/*
 * py_binding.cpp  — pybind11 薄层
 *
 * 职责：
 *   1. from_python()  — Python对象 → ClpsValue（在GIL下执行一次）
 *   2. to_python()    — ClpsValue → Python对象（在GIL下执行一次）
 *   3. 为所有核心方法添加 GIL release/acquire 包装
 *   4. 把 ClpsKezError 映射为 Python TypeError
 *
 * 核心库（core/）对本文件一无所知。
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "../core/clps_value.hpp"
#include "../core/taiji_jiejie.hpp"
#include "../core/gen_xu_lock.hpp"

namespace py = pybind11;

// std::visit helper
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Python ↔ ClpsValue 转换（GIL 保护下执行）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

static ClpsValue from_python(py::handle obj) {
    if (obj.is_none()) return ClpsValue{};
    // bool 先于 int（Python bool 是 int 的子类）
    if (py::isinstance<py::bool_>(obj))
        return ClpsValue(int64_t(obj.cast<bool>() ? 1 : 0));
    if (py::isinstance<py::float_>(obj)) return ClpsValue(obj.cast<double>());
    if (py::isinstance<py::int_>(obj))   return ClpsValue(obj.cast<int64_t>());
    if (py::isinstance<py::str>(obj))    return ClpsValue(obj.cast<std::string>());
    if (py::isinstance<py::list>(obj)) {
        auto lst = std::make_shared<ClpsList>();
        py::list py_lst = obj.cast<py::list>();
        lst->reserve(py_lst.size());
        for (auto item : py_lst) lst->push_back(from_python(item));
        return ClpsValue(std::move(lst));
    }
    if (py::isinstance<py::dict>(obj)) {
        auto mp = std::make_shared<ClpsMap>();
        py::dict py_dict = obj.cast<py::dict>();
        for (auto item : py_dict)
            (*mp)[item.first.cast<std::string>()] = from_python(item.second);
        return ClpsValue(std::move(mp));
    }
    return ClpsValue{}; // fallback: None
}

static py::object to_python(const ClpsValue& val) {
    return std::visit(overloaded{
        [](std::monostate) -> py::object
            { return py::none(); },
        [](int64_t v) -> py::object
            { return py::int_(v); },
        [](double v) -> py::object
            { return py::float_(v); },
        [](const std::string& s) -> py::object
            { return py::str(s); },
        [](const std::shared_ptr<ClpsList>& lst) -> py::object {
            py::list result;
            if (lst) for (auto& item : *lst) result.append(to_python(item));
            return result;
        },
        [](const std::shared_ptr<ClpsMap>& mp) -> py::object {
            py::dict result;
            if (mp) for (auto& [k, v] : *mp) result[py::str(k)] = to_python(v);
            return result;
        },
    }, val.data);
}

// ClpsMap → py::dict（用于 xun 返回值、resources 属性）
static py::dict clps_map_to_py(const ClpsMap& m) {
    py::dict d;
    for (auto& [k, v] : m) d[py::str(k)] = to_python(v);
    return d;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Batch ops 解析（Python list of tuples → vector<BatchOp>）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

static BatchOperand parse_batch_operand(py::handle v) {
    BatchOperand op{};
    if (py::isinstance<py::float_>(v)) {
        op.kind = BatchOperandKind::Float;
        op.fval = v.cast<double>();
    } else if (py::isinstance<py::int_>(v)) {
        op.kind = BatchOperandKind::Int;
        op.ival = v.cast<int64_t>();
    } else if (py::isinstance<py::str>(v)) {
        std::string s = v.cast<std::string>();
        if (s == "$i") op.kind = BatchOperandKind::LoopVar;
    }
    return op;
}

static std::vector<BatchOp> parse_batch_ops(py::list ops_py) {
    std::vector<BatchOp> ops;
    ops.reserve(ops_py.size());
    for (auto item : ops_py) {
        py::tuple tup = item.cast<py::tuple>();
        if (tup.size() == 0) continue;
        std::string op_name = tup[0].cast<std::string>();
        BatchOp bop{};
        if (op_name == "qian" && tup.size() >= 3) {
            bop.kind = BatchOpKind::Qian;
            bop.key  = tup[1].cast<std::string>();
            bop.val  = parse_batch_operand(tup[2]);
            bop.xing = (tup.size() > 3)
                ? wx_from_str(tup[3].cast<std::string>())
                : WX::Jin;
            ops.push_back(bop);
        } else if (op_name == "zhen" && tup.size() >= 4) {
            bop.kind = BatchOpKind::Zhen;
            bop.key  = tup[1].cast<std::string>();
            std::string op_str = tup[2].cast<std::string>();
            bop.op_char = op_str.empty() ? '+' : op_str[0];
            bop.operand = parse_batch_operand(tup[3]);
            ops.push_back(bop);
        } else if (op_name == "dui" && tup.size() >= 2) {
            bop.kind = BatchOpKind::Dui;
            bop.key  = tup[1].cast<std::string>();
            ops.push_back(bop);
        }
    }
    return ops;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  WuXing 绑定（静态工具类）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

struct WuXing {};  // 空壳，所有方法均为 staticmethod

static WX wx_from_py_handle(py::handle v) {
    if (py::isinstance<py::int_>(v) || py::isinstance<py::float_>(v)) return WX::Jin;
    if (py::isinstance<py::list>(v)) return WX::Mu;
    if (py::isinstance<py::str>(v))  return WX::Huo;
    return WX::Tu;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  PYBIND11_MODULE
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

PYBIND11_MODULE(taiji_core, m) {
    m.doc() = "taiji_core C++ 纯核心 — Python 薄绑定层";

    // ── WuXing ────────────────────────────────────────────────
    py::class_<WuXing>(m, "WuXing")
        .def(py::init<>())
        .def_static("detect", [](py::handle v) -> std::string {
            return wx_to_str(wx_from_py_handle(v));
        })
        .def_static("can_sheng", [](const std::string& a, const std::string& b) {
            return wx_can_sheng(wx_from_str(a), wx_from_str(b));
        })
        .def_static("is_ke", [](const std::string& a, const std::string& b) {
            return wx_is_ke(wx_from_str(a), wx_from_str(b));
        })
        // ── 批处理：一次穿越，五行类型检测+相生相克统计 ──
        .def_static("batch_sheng_ke", [](py::list data) -> py::tuple {
            std::vector<WX> wx_vec;
            wx_vec.reserve(data.size());
            for (auto item : data) wx_vec.push_back(wx_from_py_handle(item));
            size_t sheng = 0, ke = 0;
            for (size_t i = 0; i + 1 < wx_vec.size(); ++i) {
                if (wx_can_sheng(wx_vec[i], wx_vec[i+1])) ++sheng;
                if (wx_is_ke   (wx_vec[i], wx_vec[i+1])) ++ke;
            }
            return py::make_tuple(sheng, ke);
        })
        // ── 离+巽=风火家人 ④：C++内迭代dict，求len之和 ──
        .def_static("xun_len_sum", [](py::dict d) -> int64_t {
            int64_t total = 0;
            for (auto item : d) total += static_cast<int64_t>(py::len(item.second));
            return total;
        })
        // ── 火+巽=风火家人 ⑥：C++内UTF-8解码，字频统计 ──
        .def_static("huo_xun", [](const std::string& text) -> py::dict {
            std::unordered_map<uint32_t, int64_t> freq;
            freq.reserve(64);
            const uint8_t* p   = reinterpret_cast<const uint8_t*>(text.data());
            const uint8_t* end = p + text.size();
            while (p < end) {
                uint32_t cp;
                if      (*p < 0x80) { cp = *p++; }
                else if (*p < 0xE0) {
                    cp = (uint32_t(*p & 0x1F) << 6) | uint32_t(p[1] & 0x3F);
                    p += 2;
                } else if (*p < 0xF0) {
                    cp = (uint32_t(*p & 0x0F) << 12)
                       | (uint32_t(p[1] & 0x3F) <<  6)
                       |  uint32_t(p[2] & 0x3F);
                    p += 3;
                } else {
                    cp = (uint32_t(*p   & 0x07) << 18)
                       | (uint32_t(p[1] & 0x3F) << 12)
                       | (uint32_t(p[2] & 0x3F) <<  6)
                       |  uint32_t(p[3] & 0x3F);
                    p += 4;
                }
                freq[cp]++;
            }
            py::dict result;
            for (auto& [cp, count] : freq) {
                py::object ch = py::reinterpret_steal<py::object>(
                    PyUnicode_FromOrdinal(static_cast<int>(cp)));
                result[ch] = count;
            }
            return result;
        });

    // ── TaiJiJieJie ───────────────────────────────────────────
    py::class_<TaiJiJieJie, std::shared_ptr<TaiJiJieJie>>(m, "TaiJiJieJie")
        .def(py::init([](const std::string& name, int64_t level,
                         py::object py_parent) {
            std::shared_ptr<TaiJiJieJie> par;
            if (!py_parent.is_none())
                par = py_parent.cast<std::shared_ptr<TaiJiJieJie>>();
            return std::make_shared<TaiJiJieJie>(name, level, std::move(par));
        }), py::arg("name"), py::arg("level") = 0,
            py::arg("parent") = py::none())
        .def_readwrite("name",  &TaiJiJieJie::name)
        .def_readwrite("level", &TaiJiJieJie::level)
        .def_readwrite("alive", &TaiJiJieJie::alive)
        // resources 暴露为快照 py::dict
        .def_property_readonly("resources", [](const TaiJiJieJie& self) {
            return clps_map_to_py(self.resources);
        })
        // parent 暴露为 Python 对象或 None
        .def_property_readonly("parent", [](const TaiJiJieJie& self) -> py::object {
            if (!self.parent) return py::none();
            return py::cast(self.parent);
        })

        // ── 乾 ──
        .def("qian", [](TaiJiJieJie& self, const std::string& key,
                        py::object py_val, py::object py_xing) {
            std::optional<WX> xing;
            if (!py_xing.is_none())
                xing = wx_from_str(py_xing.cast<std::string>());
            self.qian(key, from_python(py_val), xing);
        }, py::arg("key"), py::arg("value"), py::arg("xing") = py::none())

        // ── 震 ──
        .def("zhen", [](TaiJiJieJie& self, const std::string& key,
                        const std::string& op, py::object py_val) -> py::object {
            char op_char = op.empty() ? '+' : op[0];
            try {
                return to_python(self.zhen(key, op_char, from_python(py_val)));
            } catch (const ClpsKezError& e) {
                throw py::type_error(e.what());
            }
        })

        // ── 兑 ──
        .def("dui", [](TaiJiJieJie& self, const std::string& key) -> py::object {
            return to_python(self.dui(key));
        })

        // ── 坤 ──
        .def("kun", &TaiJiJieJie::kun)

        // ── 气子 ──
        .def("qi_child", [](TaiJiJieJie& self, const std::string& name) {
            return self.qi_child(name);
        })

        // ── 巽 ──
        .def("xun", [](TaiJiJieJie& self,
                       py::object py_pat) -> py::dict {
            std::optional<std::string> pat;
            if (!py_pat.is_none()) pat = py_pat.cast<std::string>();
            return clps_map_to_py(self.xun(pat));
        }, py::arg("pattern") = py::none())

        // ── 坎 ──
        .def("kan", [](TaiJiJieJie& self, const std::string& key,
                       const std::string& op, py::object py_val) -> py::tuple {
            char op_char = op.empty() ? '+' : op[0];
            auto [ok, result] = self.kan(key, op_char, from_python(py_val));
            return py::make_tuple(ok, to_python(result));
        })

        // ── 离 ──
        .def("li", [](TaiJiJieJie& self, const std::string& key,
                      std::shared_ptr<TaiJiJieJie> src_jj,
                      const std::string& src_key) {
            self.li(key, std::move(src_jj), src_key);
        })

        // ── 爻变 ──
        .def("bian", [](TaiJiJieJie& self, const std::string& key,
                        const std::string& new_xing_str) {
            static const std::string valid[] = {"金","水","木","火","土"};
            bool ok = false;
            for (auto& v : valid) if (v == new_xing_str) { ok = true; break; }
            if (!ok)
                throw ClpsError("无效五行: '" + new_xing_str + "', 须为金水木火土之一");
            self.bian(key, wx_from_str(new_xing_str));
        })

        // ── 批处理：热路径（GIL只在解析ops时持有，热循环全程无Python）──
        .def("batch_lifecycle", [](TaiJiJieJie& self, size_t n, py::list ops_py) {
            // 1. 解析 ops（需要GIL）
            auto ops = parse_batch_ops(ops_py);
            // 2. 热路径（释放GIL）
            py::gil_scoped_release rel;
            self.batch_lifecycle(n, ops);
        })
        .def("chain_grow_shrink", [](TaiJiJieJie& self, size_t depth) {
            py::gil_scoped_release rel;
            self.chain_grow_shrink(depth);
        });

    // ── GenXuLock ─────────────────────────────────────────────
    py::class_<GenXuLock>(m, "GenXuLock")
        .def(py::init<>())
        // gen() — 释放GIL等待，避免死锁
        .def("gen", [](GenXuLock& self) {
            py::gil_scoped_release rel;
            self.gen();
        })
        // po() — 解锁
        .def("po", &GenXuLock::po)
        // ── 坎变泰：gen+zhen+po 三合一，整段无GIL ──────────────
        // 这是③的真正解法：lock/compute/unlock 全在C++，绕开Python GIL乒乓
        .def("gen_zhen_po", [](GenXuLock& self,
                               TaiJiJieJie& jj,
                               const std::string& key,
                               const std::string& op,
                               py::object py_val) {
            // 在GIL保护下完成 Python→C++ 转换
            ClpsValue val = from_python(py_val);
            char op_char = op.empty() ? '+' : op[0];
            // 释放GIL：以下全为纯C++，无Python对象
            py::gil_scoped_release rel;
            self.gen_compute([&]() {
                jj.zhen(key, op_char, std::move(val));
            });
        });
}

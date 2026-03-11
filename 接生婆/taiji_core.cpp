/*
 * taiji_core.cpp  —  C++/pybind11 重写
 * 与 Rust/PyO3 版本保持完全相同的 Python API
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <unordered_map>
#include <string>
#include <vector>
#include <optional>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <stdexcept>

namespace py = pybind11;

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  WX enum — 五行快速分类
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

enum class WX : uint8_t { Jin=0, Shui=1, Mu=2, Huo=3, Tu=4 };

static WX wx_from_str(const std::string& s) {
    if (s == "金") return WX::Jin;
    if (s == "水") return WX::Shui;
    if (s == "木") return WX::Mu;
    if (s == "火") return WX::Huo;
    return WX::Tu;
}

static bool wx_can_sheng(WX a, WX b) {
    return (a == WX::Jin  && b == WX::Shui) ||
           (a == WX::Shui && b == WX::Mu)   ||
           (a == WX::Mu   && b == WX::Huo)  ||
           (a == WX::Huo  && b == WX::Tu)   ||
           (a == WX::Tu   && b == WX::Jin);
}

static bool wx_is_ke(WX a, WX b) {
    return (a == WX::Jin  && b == WX::Mu)   ||
           (a == WX::Mu   && b == WX::Tu)   ||
           (a == WX::Tu   && b == WX::Shui) ||
           (a == WX::Shui && b == WX::Huo)  ||
           (a == WX::Huo  && b == WX::Jin);
}

// 从 Python 对象快速检测 WX（无字符串分配）
static WX detect_wx(py::handle v) {
    if (py::isinstance<py::int_>(v) || py::isinstance<py::float_>(v))
        return WX::Jin;
    if (py::isinstance<py::list>(v))
        return WX::Mu;
    if (py::isinstance<py::str>(v))
        return WX::Huo;
    return WX::Tu;
}

// 五行名称字符串（用于 meta 存储和 Python 接口）
static std::string wx_to_str(WX w) {
    switch (w) {
        case WX::Jin:  return "金";
        case WX::Shui: return "水";
        case WX::Mu:   return "木";
        case WX::Huo:  return "火";
        default:       return "土";
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Batch 操作（batch_lifecycle 内部）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

enum class BatchOperandKind { Int, Float, LoopVar };

struct BatchOperand {
    BatchOperandKind kind;
    int64_t ival;
    double  fval;

    double resolve(size_t i) const {
        switch (kind) {
            case BatchOperandKind::Int:     return static_cast<double>(ival);
            case BatchOperandKind::Float:   return fval;
            case BatchOperandKind::LoopVar: return static_cast<double>(i);
        }
        return 0.0;
    }

    WX wx() const { return WX::Jin; }  // 所有数值操作数均为金
};

static BatchOperand parse_operand(py::handle v) {
    BatchOperand op{};
    if (py::isinstance<py::float_>(v)) {
        op.kind = BatchOperandKind::Float;
        op.fval = v.cast<double>();
    } else if (py::isinstance<py::int_>(v)) {
        op.kind = BatchOperandKind::Int;
        op.ival = v.cast<int64_t>();
    } else if (py::isinstance<py::str>(v)) {
        std::string s = v.cast<std::string>();
        if (s == "$i") {
            op.kind = BatchOperandKind::LoopVar;
        } else {
            op.kind = BatchOperandKind::Int;
            op.ival = 0;
        }
    } else {
        op.kind = BatchOperandKind::Int;
        op.ival = 0;
    }
    return op;
}

enum class BatchOpKind { Qian, Zhen, Dui };

struct BatchOp {
    BatchOpKind kind;
    std::string key;
    // Qian fields
    BatchOperand val;
    WX xing;
    // Zhen fields
    char op_char;
    BatchOperand operand;
};

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  matches_pattern — 巽卦通配符匹配
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

static bool matches_pattern(const std::string& key, const std::optional<std::string>& pattern) {
    if (!pattern.has_value()) return true;
    const std::string& p = *pattern;
    if (p == "*") return true;

    bool starts = !p.empty() && p.front() == '*';
    bool ends   = !p.empty() && p.back()  == '*';

    if (starts && ends) {
        // *词* → 包含
        if (p.size() < 2) return true;
        std::string inner = p.substr(1, p.size() - 2);
        return key.find(inner) != std::string::npos;
    } else if (starts) {
        // *后缀 → ends_with
        std::string suffix = p.substr(1);
        if (key.size() < suffix.size()) return false;
        return key.compare(key.size() - suffix.size(), suffix.size(), suffix) == 0;
    } else if (ends) {
        // 前缀* → starts_with
        std::string prefix = p.substr(0, p.size() - 1);
        if (key.size() < prefix.size()) return false;
        return key.compare(0, prefix.size(), prefix) == 0;
    } else {
        return key == p;
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  WuXing 类
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

struct WuXing {
    static std::string detect(py::handle v) {
        return wx_to_str(detect_wx(v));
    }

    static bool can_sheng(const std::string& a, const std::string& b) {
        return wx_can_sheng(wx_from_str(a), wx_from_str(b));
    }

    static bool is_ke(const std::string& a, const std::string& b) {
        return wx_is_ke(wx_from_str(a), wx_from_str(b));
    }

    static py::tuple batch_sheng_ke(py::list data) {
        std::vector<WX> wx_vec;
        wx_vec.reserve(data.size());
        for (auto item : data) {
            wx_vec.push_back(detect_wx(item));
        }
        size_t sheng = 0, ke = 0;
        for (size_t i = 0; i + 1 < wx_vec.size(); ++i) {
            if (wx_can_sheng(wx_vec[i], wx_vec[i+1])) ++sheng;
            if (wx_is_ke(wx_vec[i], wx_vec[i+1]))     ++ke;
        }
        return py::make_tuple(sheng, ke);
    }

    // ── 离+巽 = 风火家人 ④ ────────────────────────────────
    // xun_len_sum(dict) -> int
    // C++ 内迭代整个 dict，对每个 value 调用 len()，一次穿越边界
    static int64_t xun_len_sum(py::dict d) {
        int64_t total = 0;
        for (auto item : d) {
            total += static_cast<int64_t>(py::len(item.second));
        }
        return total;
    }

    // ── 火+巽 = 风火家人 ⑥ ────────────────────────────────
    // huo_xun(text) -> dict   字频统计，全程在 C++ 内完成
    // 直接解码 UTF-8 字节流，无 Python 字符串迭代开销
    static py::dict huo_xun(const std::string& text) {
        std::unordered_map<uint32_t, int64_t> freq;
        freq.reserve(64);

        const uint8_t* p   = reinterpret_cast<const uint8_t*>(text.data());
        const uint8_t* end = p + text.size();

        while (p < end) {
            uint32_t cp;
            if (*p < 0x80) {
                cp = *p++;
            } else if (*p < 0xE0) {
                cp = (uint32_t(*p & 0x1F) << 6) | uint32_t(p[1] & 0x3F);
                p += 2;
            } else if (*p < 0xF0) {
                cp = (uint32_t(*p & 0x0F) << 12)
                   | (uint32_t(p[1] & 0x3F) << 6)
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

        // 仅在最终构建 Python dict 时才需要 GIL（每个唯一字符一次）
        py::dict result;
        for (auto& [cp, count] : freq) {
            py::object ch = py::reinterpret_steal<py::object>(
                PyUnicode_FromOrdinal(static_cast<int>(cp))
            );
            result[ch] = count;
        }
        return result;
    }
};

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  TaiJiJieJie 类
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

struct TaiJiJieJie {
    std::string name;
    int64_t     level;
    py::dict    resources;
    bool        alive;
    std::unordered_map<std::string, std::string> meta;
    py::object  parent;  // py::none() if no parent
    std::unordered_map<std::string, std::pair<py::object, std::string>> refs;

    TaiJiJieJie(const std::string& n, int64_t lv, py::object par)
        : name(n), level(lv), resources(), alive(true),
          parent(std::move(par))
    {}

    // ── qian ──────────────────────────────────────────────────
    void qian(const std::string& key, py::object value,
              std::optional<std::string> xing_opt) {
        std::string xing = xing_opt.has_value()
            ? *xing_opt
            : WuXing::detect(value);
        resources[py::str(key)] = value;
        meta[key] = std::move(xing);
    }

    // ── zhen ──────────────────────────────────────────────────
    py::object zhen(const std::string& key, const std::string& op,
                    py::object operand) {
        if (!alive)
            throw std::runtime_error("结界已归元");

        // 若 key 在 refs 中，读穿并断引用
        auto ref_it = refs.find(key);
        if (ref_it != refs.end()) {
            auto [src_jj, src_key] = ref_it->second;
            refs.erase(ref_it);
            py::object cur_val = src_jj.attr("dui")(src_key);
            std::string xing = WuXing::detect(cur_val);
            resources[py::str(key)] = cur_val;
            meta[key] = xing;
        }

        // 取当前值
        if (!resources.contains(py::str(key)))
            throw std::runtime_error("key '" + key + "' not found");
        py::object val = resources[py::str(key)];

        // 五行克制检查
        auto meta_it = meta.find(key);
        std::string val_x = (meta_it != meta.end())
            ? meta_it->second
            : WuXing::detect(val);
        std::string op_x = WuXing::detect(operand);

        if (WuXing::is_ke(op_x, val_x))
            throw py::type_error(op_x + "克" + val_x);

        // 执行运算
        py::object result;
        if      (op == "+") result = val.attr("__add__")(operand);
        else if (op == "-") result = val.attr("__sub__")(operand);
        else if (op == "*") result = val.attr("__mul__")(operand);
        else if (op == "/") {
            if (operand.attr("__bool__")().cast<bool>())
                result = val.attr("__truediv__")(operand);
            else
                result = py::int_(0);
        } else {
            result = val;
        }

        resources[py::str(key)] = result;
        return result;
    }

    // ── dui ───────────────────────────────────────────────────
    py::object dui(const std::string& key) {
        auto ref_it = refs.find(key);
        if (ref_it != refs.end()) {
            return ref_it->second.first.attr("dui")(ref_it->second.second);
        }
        py::str k(key);
        if (resources.contains(k))
            return resources[k];
        return py::none();
    }

    // ── kun ───────────────────────────────────────────────────
    void kun() {
        resources.clear();
        meta.clear();
        refs.clear();
        alive = false;
    }

    // ── qi_child ──────────────────────────────────────────────
    TaiJiJieJie qi_child(const std::string& child_name) {
        return TaiJiJieJie(child_name, level + 1, py::none());
    }

    // ── xun ───────────────────────────────────────────────────
    py::dict xun(std::optional<std::string> pattern) {
        py::dict result;
        if (!alive) return result;

        for (auto item : resources) {
            std::string k = item.first.cast<std::string>();
            if (matches_pattern(k, pattern))
                result[item.first] = item.second;
        }

        for (auto& [k, src_pair] : refs) {
            if (matches_pattern(k, pattern)) {
                py::object val = src_pair.first.attr("dui")(src_pair.second);
                result[py::str(k)] = val;
            }
        }
        return result;
    }

    // ── kan ───────────────────────────────────────────────────
    py::tuple kan(const std::string& key, const std::string& op,
                  py::object operand) {
        if (!alive)
            return py::make_tuple(false, py::none());

        bool in_refs = refs.count(key) > 0;
        bool in_res  = resources.contains(py::str(key));
        if (!in_refs && !in_res)
            return py::make_tuple(false, py::none());

        // 取当前值（只读）
        py::object val;
        if (in_refs) {
            auto& [src_jj, src_key] = refs.at(key);
            val = src_jj.attr("dui")(src_key);
        } else {
            val = resources[py::str(key)];
        }

        // 五行克制检查
        auto meta_it = meta.find(key);
        std::string val_x = (meta_it != meta.end())
            ? meta_it->second
            : WuXing::detect(val);
        std::string op_x = WuXing::detect(operand);
        if (WuXing::is_ke(op_x, val_x))
            return py::make_tuple(false, py::none());

        // 执行运算
        py::object result;
        if      (op == "+") result = val.attr("__add__")(operand);
        else if (op == "-") result = val.attr("__sub__")(operand);
        else if (op == "*") result = val.attr("__mul__")(operand);
        else if (op == "/") {
            if (!operand.attr("__bool__")().cast<bool>())
                return py::make_tuple(false, py::none());
            result = val.attr("__truediv__")(operand);
        } else {
            result = val;
        }

        // 写回（成功才改状态）
        if (in_refs) {
            refs.erase(key);
            std::string xing = WuXing::detect(result);
            meta[key] = xing;
        }
        resources[py::str(key)] = result;
        return py::make_tuple(true, result);
    }

    // ── li ────────────────────────────────────────────────────
    void li(const std::string& key, py::object source_jj,
            const std::string& source_key) {
        py::str k(key);
        if (resources.contains(k))
            resources.attr("__delitem__")(k);
        meta.erase(key);
        refs[key] = {std::move(source_jj), source_key};
    }

    // ── bian ──────────────────────────────────────────────────
    void bian(const std::string& key, const std::string& new_xing) {
        static const std::string valid[] = {"金","水","木","火","土"};
        bool ok = false;
        for (auto& v : valid) if (v == new_xing) { ok = true; break; }
        if (!ok)
            throw std::runtime_error("无效五行: '" + new_xing + "', 须为金水木火土之一");

        bool in_res  = resources.contains(py::str(key));
        bool in_refs = refs.count(key) > 0;
        if (!in_res && !in_refs)
            throw std::runtime_error("key '" + key + "' not found");

        meta[key] = new_xing;
    }

    // ── batch_lifecycle ───────────────────────────────────────
    void batch_lifecycle(size_t n, py::list ops_py) {
        // 1. 解析 ops（一次性，含 GIL）
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
                bop.val  = parse_operand(tup[2]);
                bop.xing = (tup.size() > 3)
                    ? wx_from_str(tup[3].cast<std::string>())
                    : WX::Jin;
                ops.push_back(bop);
            } else if (op_name == "zhen" && tup.size() >= 4) {
                bop.kind    = BatchOpKind::Zhen;
                bop.key     = tup[1].cast<std::string>();
                std::string op_str = tup[2].cast<std::string>();
                bop.op_char = op_str.empty() ? '+' : op_str[0];
                bop.operand = parse_operand(tup[3]);
                ops.push_back(bop);
            } else if (op_name == "dui" && tup.size() >= 2) {
                bop.kind = BatchOpKind::Dui;
                bop.key  = tup[1].cast<std::string>();
                ops.push_back(bop);
            }
        }

        // 2. 热路径：纯 C++ 循环，无 Python 对象
        std::unordered_map<std::string, double> state;
        std::unordered_map<std::string, WX>     wmeta;
        state.reserve(8);
        wmeta.reserve(8);

        for (size_t i = 0; i < n; ++i) {
            for (const auto& op : ops) {
                switch (op.kind) {
                    case BatchOpKind::Qian:
                        state[op.key] = op.val.resolve(i);
                        wmeta[op.key] = op.xing;
                        break;
                    case BatchOpKind::Zhen: {
                        auto sit = state.find(op.key);
                        double cur = (sit != state.end()) ? sit->second : 0.0;
                        auto mit = wmeta.find(op.key);
                        WX val_x = (mit != wmeta.end()) ? mit->second : WX::Tu;
                        WX op_x  = op.operand.wx();
                        if (wx_is_ke(op_x, val_x))
                            throw py::type_error("五行克制");
                        double rhs = op.operand.resolve(i);
                        double result;
                        switch (op.op_char) {
                            case '+': result = cur + rhs; break;
                            case '-': result = cur - rhs; break;
                            case '*': result = cur * rhs; break;
                            case '/': result = (rhs != 0.0) ? cur / rhs : 0.0; break;
                            default:  result = cur; break;
                        }
                        state[op.key] = result;
                        break;
                    }
                    case BatchOpKind::Dui:
                        break;  // 结果丢弃
                }
            }
            // 隐含 kun()
            state.clear();
            wmeta.clear();
        }
    }

    // ── chain_grow_shrink ─────────────────────────────────────
    void chain_grow_shrink(size_t depth) {
        std::vector<std::unordered_map<std::string, double>> chain;
        std::vector<std::unordered_map<std::string, WX>>     chain_meta;
        chain.reserve(depth + 1);
        chain_meta.reserve(depth + 1);

        // 根层
        chain.emplace_back();
        chain_meta.emplace_back();

        // 生长
        static const std::string lingqi = "灵气";
        for (size_t i = 1; i <= depth; ++i) {
            std::unordered_map<std::string, double> res;
            res[lingqi] = static_cast<double>(i * 10);
            std::unordered_map<std::string, WX> mta;
            mta[lingqi] = WX::Jin;
            chain.push_back(std::move(res));
            chain_meta.push_back(std::move(mta));
        }

        // 收缩（模拟 kun：pop 后自动析构）
        while (chain.size() > 1) {
            chain.pop_back();
            chain_meta.pop_back();
        }
    }
};

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  GenXuLock 类
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

struct GenXuLock {
    std::shared_ptr<std::mutex>              m;
    std::shared_ptr<std::condition_variable> cv;
    std::shared_ptr<bool>                    locked_flag;

    GenXuLock()
        : m(std::make_shared<std::mutex>()),
          cv(std::make_shared<std::condition_variable>()),
          locked_flag(std::make_shared<bool>(false))
    {}

    void gen() {
        // 释放 GIL 等待，让其他 Python 线程可以运行
        auto _m  = m;
        auto _cv = cv;
        auto _lf = locked_flag;
        py::gil_scoped_release rel;
        std::unique_lock<std::mutex> lk(*_m);
        _cv->wait(lk, [&]{ return !*_lf; });
        *_lf = true;
    }

    void po() {
        std::unique_lock<std::mutex> lk(*m);
        *locked_flag = false;
        cv->notify_one();
    }
};

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  PYBIND11_MODULE 注册
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

PYBIND11_MODULE(taiji_core, m) {
    m.doc() = "taiji_core — C++/pybind11 重写（太极结界 CLPS VM）";

    // ── WuXing ────────────────────────────────────────────────
    py::class_<WuXing>(m, "WuXing")
        .def(py::init<>())
        .def_static("detect",        &WuXing::detect,
                    "detect(value) -> str — 检测五行属性")
        .def_static("can_sheng",     &WuXing::can_sheng,
                    "can_sheng(a, b) -> bool")
        .def_static("is_ke",         &WuXing::is_ke,
                    "is_ke(a, b) -> bool")
        .def_static("batch_sheng_ke",&WuXing::batch_sheng_ke,
                    "batch_sheng_ke(data) -> (int, int)")
        .def_static("xun_len_sum",  &WuXing::xun_len_sum,
                    "xun_len_sum(dict) -> int — 离+巽：C++内迭代dict，求所有value的len之和")
        .def_static("huo_xun",      &WuXing::huo_xun,
                    "huo_xun(text) -> dict — 火+巽：C++内UTF-8字频统计");

    // ── TaiJiJieJie ───────────────────────────────────────────
    py::class_<TaiJiJieJie>(m, "TaiJiJieJie")
        .def(py::init([](const std::string& name, int64_t level,
                         py::object parent) {
                 return TaiJiJieJie(name, level, parent);
             }),
             py::arg("name"), py::arg("level") = 0,
             py::arg("parent") = py::none())
        .def_readwrite("name",      &TaiJiJieJie::name)
        .def_readwrite("level",     &TaiJiJieJie::level)
        .def_property_readonly("resources",
            [](TaiJiJieJie& self) { return self.resources; })
        .def_readwrite("alive",     &TaiJiJieJie::alive)
        .def_property_readonly("parent",
            [](TaiJiJieJie& self) { return self.parent; })
        .def("qian", &TaiJiJieJie::qian,
             py::arg("key"), py::arg("value"), py::arg("xing") = py::none())
        .def("zhen",  &TaiJiJieJie::zhen)
        .def("dui",   &TaiJiJieJie::dui)
        .def("kun",   &TaiJiJieJie::kun)
        .def("qi_child", &TaiJiJieJie::qi_child)
        .def("xun",   &TaiJiJieJie::xun,
             py::arg("pattern") = py::none())
        .def("kan",   &TaiJiJieJie::kan)
        .def("li",    &TaiJiJieJie::li)
        .def("bian",  &TaiJiJieJie::bian)
        .def("batch_lifecycle",  &TaiJiJieJie::batch_lifecycle)
        .def("chain_grow_shrink",&TaiJiJieJie::chain_grow_shrink);

    // ── GenXuLock ─────────────────────────────────────────────
    py::class_<GenXuLock>(m, "GenXuLock")
        .def(py::init<>())
        .def("gen", &GenXuLock::gen,  "加锁（释放 GIL 等待）")
        .def("po",  &GenXuLock::po,   "解锁");
}

#include "jiejie.hpp"
#include <algorithm>

// ── 通配符匹配（巽卦）────────────────────────────────────────
static bool matches_pattern(const std::string& key,
                             const std::optional<std::string>& pattern) {
    if (!pattern) return true;
    const std::string& p = *pattern;
    if (p == "*") return true;

    bool st = !p.empty() && p.front() == '*';
    bool en = !p.empty() && p.back()  == '*';

    if (st && en) {
        // *词*  → 包含
        auto inner = p.substr(1, p.size() - 2);
        return key.find(inner) != std::string::npos;
    } else if (st) {
        // *后缀 → ends_with
        auto suf = p.substr(1);
        return key.size() >= suf.size() &&
               key.compare(key.size() - suf.size(), suf.size(), suf) == 0;
    } else if (en) {
        // 前缀*  → starts_with
        auto pre = p.substr(0, p.size() - 1);
        return key.size() >= pre.size() &&
               key.compare(0, pre.size(), pre) == 0;
    } else {
        return key == p;
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  构造
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

TaiJiJieJie::TaiJiJieJie(std::string n, int64_t lv,
                           std::shared_ptr<TaiJiJieJie> par)
    : name(std::move(n)), level(lv), alive(true), parent(std::move(par))
{}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  qian — 乾：存入资源
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void TaiJiJieJie::qian(const std::string& key, ClpsValue val,
                        std::optional<WX> xing) {
    WX w = xing.value_or(val.wx());
    resources[key] = std::move(val);
    meta[key] = w;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  zhen — 震：带五行校验的运算
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

ClpsValue TaiJiJieJie::zhen(const std::string& key, char op, ClpsValue operand) {
    if (!alive) throw ClpsError("结界已归元");
    // 恒卦守护：只读key禁止修改
    if (readonly_keys.count(key))
        throw ClpsError("恒卦：key '" + key + "' 为只读常量，不可更改");

    // 离卦引用：读穿+断引用
    auto ref_it = refs.find(key);
    if (ref_it != refs.end()) {
        auto [src_jj, src_key] = ref_it->second;
        refs.erase(ref_it);
        ClpsValue cur = src_jj->dui(src_key);
        WX w = cur.wx();
        resources[key] = std::move(cur);
        meta[key] = w;
    }

    auto res_it = resources.find(key);
    if (res_it == resources.end())
        throw ClpsError("key '" + key + "' not found");

    const ClpsValue& val = res_it->second;

    // 易经·坤道：坤卦"地势坤，君子以厚德载物"——运行时承载一切，不以克制为由拒绝执行。
    // 易经·克制·中立：克制是自然规律，不是非法。震卦尝试执行，apply_op自然裁定结果。
    // 检查器（需卦）已在编译期告知程序员五行张力，运行时不再重复阻断。
    ClpsValue result = val.apply_op(op, operand);
    res_it->second = result;
    return result;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  dui — 兑：读取资源
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

ClpsValue TaiJiJieJie::dui(const std::string& key) const {
    auto ref_it = refs.find(key);
    if (ref_it != refs.end())
        return ref_it->second.first->dui(ref_it->second.second);

    auto res_it = resources.find(key);
    if (res_it != resources.end()) return res_it->second;
    return ClpsValue{}; // None
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  kun — 坤：归元
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void TaiJiJieJie::kun() {
    resources.clear();
    meta.clear();
    refs.clear();
    alive = false;
}

// ── 坤·归列：截断列表到前 count 项（坤卦归元，释放多余） ──
void TaiJiJieJie::kun_list(const std::string& key, size_t count) {
    auto it = resources.find(key);
    if (it == resources.end()) return;
    auto* lp = std::get_if<std::shared_ptr<ClpsList>>(&it->second.data);
    if (!lp) throw ClpsError("坤·归列：\"" + key + "\" 非列表");
    if (count < (*lp)->size()) (*lp)->resize(count);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  qi_child — 派生子结界
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

std::shared_ptr<TaiJiJieJie>
TaiJiJieJie::qi_child(const std::string& child_name) {
    return std::make_shared<TaiJiJieJie>(child_name, level + 1, nullptr);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  xun — 巽：遍历资源（含通配符）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

ClpsMap TaiJiJieJie::xun(std::optional<std::string> pattern) const {
    ClpsMap result;
    if (!alive) return result;

    for (auto& [k, v] : resources) {
        if (matches_pattern(k, pattern)) result[k] = v;
    }
    for (auto& [k, src_pair] : refs) {
        if (matches_pattern(k, pattern))
            result[k] = src_pair.first->dui(src_pair.second);
    }
    return result;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  kan — 坎：险中有孚，克制时静默回滚（知险而行，保守路线）
//  震·坤道：尝试执行，以apply_op裁定
//  坎·险道：克制即止，保全现状，返回(false, {})——两卦行事风格不同，各有其义
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

std::pair<bool, ClpsValue>
TaiJiJieJie::kan(const std::string& key, char op, ClpsValue operand) {
    if (!alive) return {false, ClpsValue{}};
    // 恒卦守护：只读key坎操作也静默失败
    if (readonly_keys.count(key)) return {false, ClpsValue{}};

    bool in_refs = refs.count(key) > 0;
    bool in_res  = resources.count(key) > 0;
    if (!in_refs && !in_res) return {false, ClpsValue{}};

    // 取当前值（只读）
    ClpsValue val;
    if (in_refs) {
        auto& [src_jj, src_key] = refs.at(key);
        val = src_jj->dui(src_key);
    } else {
        val = resources.at(key);
    }

    // 五行克制检查（回滚）
    auto mit = meta.find(key);
    WX val_x = (mit != meta.end()) ? mit->second : val.wx();
    WX op_x  = operand.wx();
    if (wx_is_ke(op_x, val_x)) return {false, ClpsValue{}};

    // 执行运算
    ClpsValue result;
    try {
        result = val.apply_op(op, operand);
    } catch (...) {
        return {false, ClpsValue{}};
    }

    // 写回
    if (in_refs) {
        refs.erase(key);
        meta[key] = result.wx();
    }
    resources[key] = result;
    return {true, result};
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  li — 离：引用绑定
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void TaiJiJieJie::li(const std::string& key,
                      std::shared_ptr<TaiJiJieJie> src_jj,
                      const std::string& src_key) {
    resources.erase(key);
    meta.erase(key);
    refs[key] = {std::move(src_jj), src_key};
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  bian — 爻变：修改五行属性
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void TaiJiJieJie::bian(const std::string& key, WX new_xing) {
    bool in_res  = resources.count(key) > 0;
    bool in_refs = refs.count(key) > 0;
    if (!in_res && !in_refs)
        throw ClpsError("key '" + key + "' not found");
    meta[key] = new_xing;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  batch_lifecycle — 热路径：纯C++循环，零Python对象
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void TaiJiJieJie::batch_lifecycle(size_t n, const std::vector<BatchOp>& ops) {
    std::unordered_map<std::string, double> state;
    state.reserve(8);

    for (size_t i = 0; i < n; ++i) {
        for (const auto& op : ops) {
            switch (op.kind) {
                case BatchOpKind::Qian:
                    state[op.key] = op.val.resolve(i);
                    break;
                case BatchOpKind::Zhen: {
                    // 易经·坤道：震卦热路径与主路径一致，不以克制为由中止。
                    // 需卦检查器已在编译期告知五行张力，运行时apply_op自然裁定。
                    auto sit = state.find(op.key);
                    double cur = (sit != state.end()) ? sit->second : 0.0;
                    double rhs = op.operand.resolve(i);
                    double result;
                    switch (op.op_char) {
                        case '+': result = cur + rhs; break;
                        case '-': result = cur - rhs; break;
                        case '*': result = cur * rhs; break;
                        case '/': result = rhs != 0.0 ? cur / rhs : 0.0; break;
                        default:  result = cur; break;
                    }
                    state[op.key] = result;
                    break;
                }
                case BatchOpKind::Dui:
                    break; // 结果丢弃
            }
        }
        state.clear();
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  chain_grow_shrink — 分形生长+收缩
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void TaiJiJieJie::chain_grow_shrink(size_t depth) {
    std::vector<std::unordered_map<std::string, double>> chain;
    std::vector<std::unordered_map<std::string, WX>>     wmeta;
    chain.reserve(depth + 1);
    wmeta.reserve(depth + 1);

    chain.emplace_back();
    wmeta.emplace_back();

    static const std::string lingqi = "灵气";
    for (size_t i = 1; i <= depth; ++i) {
        std::unordered_map<std::string, double> res;
        res[lingqi] = static_cast<double>(i * 10);
        std::unordered_map<std::string, WX> wm;
        wm[lingqi] = WX::Jin;
        chain.push_back(std::move(res));
        wmeta.push_back(std::move(wm));
    }

    while (chain.size() > 1) {
        chain.pop_back();
        wmeta.pop_back();
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  heng — 恒(32)：声明只读常量
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void TaiJiJieJie::heng(const std::string& key, ClpsValue val,
                        std::optional<WX> xing) {
    // 若已声明为恒，不可重新绑定
    if (readonly_keys.count(key))
        throw ClpsError("恒卦：key '" + key + "' 已为只读常量，不可重绑定");
    WX w = xing.value_or(val.wx());
    resources[key] = std::move(val);
    meta[key] = w;
    readonly_keys.insert(key);
}

// ── 小畜：辟出列表或向列表追加值 ─────────────────────────────────────────
void TaiJiJieJie::xiao_xu(const std::string& key, std::optional<ClpsValue> val) {
    auto it = resources.find(key);
    if (it != resources.end()) {
        // 已存在 — 必须是列表才能追加
        if (auto* lp = std::get_if<std::shared_ptr<ClpsList>>(&it->second.data)) {
            if (val) (*lp)->push_back(std::move(*val));
            return;
        }
        throw ClpsError("小畜：\"" + key + "\" 已存在且非列表，无法追加");
    }
    // 新建列表
    auto list = std::make_shared<ClpsList>();
    if (val) list->push_back(std::move(*val));
    resources[key] = ClpsValue(std::move(list));
}

#include "kun.hpp"
#include "tun.hpp"
#include "xu.hpp"
#include <iostream>
#include <sstream>
#include <fstream>

// ── 构造 ─────────────────────────────────────────────────────────────────
ClpsParser::ClpsParser(std::vector<Token> tokens, ClpsOutputFn output_fn)
    : toks_(std::move(tokens))
    , out_(output_fn ? std::move(output_fn)
                     : [](const std::string& s){ std::cout << s << "\n"; })
{
    root_jj = std::make_shared<TaiJiJieJie>("太极", 0, nullptr);
    scope_stack_.push_back(root_jj);
}

// ── emit ─────────────────────────────────────────────────────────────────
void ClpsParser::emit(const std::string& s) { out_(s); }

// ── Token 操作 ────────────────────────────────────────────────────────────
const Token& ClpsParser::consume() {
    const Token& t = toks_[pos_];
    if (pos_ + 1 < toks_.size()) ++pos_;
    return t;
}

const Token& ClpsParser::expect(TK t) {
    if (peek().type != t)
        throw ClpsParseError(
            std::string("期望 ") + tk_name(t) +
            "，遇到 " + tk_name(peek().type) +
            " \"" + peek().text + "\"", peek().line);
    return consume();
}

// ── parse_value ───────────────────────────────────────────────────────────
ClpsValue ClpsParser::parse_value(size_t loop_i) {
    const Token& t = peek();
    switch (t.type) {
        case TK::DOLLAR_I:
            consume();
            return ClpsValue(static_cast<int64_t>(loop_i));
        case TK::STR:
            consume();
            return ClpsValue(t.text);
        case TK::INT:
            consume();
            return ClpsValue(t.ival);
        case TK::FLOAT:
            consume();
            return ClpsValue(t.dval);
        case TK::IDENT: {
            // 标识符：从当前作用域读取
            consume();
            return cur()->dui(t.text);
        }
        default:
            throw ClpsParseError(
                std::string("期望值，遇到 ") + tk_name(t.type) +
                " \"" + t.text + "\"", t.line);
    }
}

// ── parse_wx ──────────────────────────────────────────────────────────────
WX ClpsParser::parse_wx() {
    const Token& t = peek();
    switch (t.type) {
        case TK::JIN:  consume(); return WX::Jin;
        case TK::SHUI: consume(); return WX::Shui;
        case TK::MU:   consume(); return WX::Mu;
        case TK::HUO:  consume(); return WX::Huo;
        case TK::TU:   consume(); return WX::Tu;
        default:
            throw ClpsParseError(
                std::string("期望五行（金水木火土），遇到 ") +
                tk_name(t.type), t.line);
    }
}

std::optional<WX> ClpsParser::try_parse_wx() {
    switch (peek().type) {
        case TK::JIN:  consume(); return WX::Jin;
        case TK::SHUI: consume(); return WX::Shui;
        case TK::MU:   consume(); return WX::Mu;
        case TK::HUO:  consume(); return WX::Huo;
        case TK::TU:   consume(); return WX::Tu;
        default: return std::nullopt;
    }
}

// ── parse_cmp_op ──────────────────────────────────────────────────────────
TK ClpsParser::parse_cmp_op() {
    const Token& t = peek();
    switch (t.type) {
        case TK::EQ:  consume(); return TK::EQ;
        case TK::NEQ: consume(); return TK::NEQ;
        case TK::GT:  consume(); return TK::GT;
        case TK::LT:  consume(); return TK::LT;
        case TK::GTE: consume(); return TK::GTE;
        case TK::LTE: consume(); return TK::LTE;
        default:
            throw ClpsParseError(
                std::string("期望比较运算符（== != > < >= <=），遇到 ") +
                tk_name(t.type), t.line);
    }
}

// ── compare_clps ──────────────────────────────────────────────────────────
bool ClpsParser::compare_clps(const ClpsValue& lhs, const ClpsValue& rhs, TK op) {
    // 数值比较（int / float 混用统一转 double）
    double l_num = 0, r_num = 0;
    bool l_num_ok = false, r_num_ok = false;
    if (auto* iv = std::get_if<int64_t>(&lhs.data)) { l_num = (double)*iv; l_num_ok = true; }
    else if (auto* dv = std::get_if<double>(&lhs.data)) { l_num = *dv; l_num_ok = true; }
    if (auto* iv = std::get_if<int64_t>(&rhs.data)) { r_num = (double)*iv; r_num_ok = true; }
    else if (auto* dv = std::get_if<double>(&rhs.data)) { r_num = *dv; r_num_ok = true; }

    if (l_num_ok && r_num_ok) {
        switch (op) {
            case TK::EQ:  return l_num == r_num;
            case TK::NEQ: return l_num != r_num;
            case TK::GT:  return l_num >  r_num;
            case TK::LT:  return l_num <  r_num;
            case TK::GTE: return l_num >= r_num;
            case TK::LTE: return l_num <= r_num;
            default: return false;
        }
    }

    // 字符串比较
    auto* ls = std::get_if<std::string>(&lhs.data);
    auto* rs = std::get_if<std::string>(&rhs.data);
    if (ls && rs) {
        switch (op) {
            case TK::EQ:  return *ls == *rs;
            case TK::NEQ: return *ls != *rs;
            case TK::GT:  return *ls >  *rs;
            case TK::LT:  return *ls <  *rs;
            case TK::GTE: return *ls >= *rs;
            case TK::LTE: return *ls <= *rs;
            default: return false;
        }
    }

    // 虚（None）比较：只支持 == 和 !=
    bool l_none = std::holds_alternative<std::monostate>(lhs.data);
    bool r_none = std::holds_alternative<std::monostate>(rhs.data);
    if (op == TK::EQ)  return l_none && r_none;
    if (op == TK::NEQ) return !(l_none && r_none);

    return false;
}

// ── cmp_op_str ────────────────────────────────────────────────────────────
std::string ClpsParser::cmp_op_str(TK op) {
    switch (op) {
        case TK::EQ:  return "==";
        case TK::NEQ: return "!=";
        case TK::GT:  return ">";
        case TK::LT:  return "<";
        case TK::GTE: return ">=";
        case TK::LTE: return "<=";
        default:      return "?";
    }
}

// ── parse_op ──────────────────────────────────────────────────────────────
char ClpsParser::parse_op() {
    const Token& t = peek();
    switch (t.type) {
        case TK::PLUS:    consume(); return '+';
        case TK::MINUS:   consume(); return '-';
        case TK::STAR:    consume(); return '*';
        case TK::SLASH:   consume(); return '/';
        case TK::PERCENT: consume(); return '%';
        default:
            throw ClpsParseError(
                std::string("期望运算符（+-*/%），遇到 ") +
                tk_name(t.type), t.line);
    }
}

// ═════════════════════════════════════════════════════════════════
//  各卦执行
// ═════════════════════════════════════════════════════════════════

// 乾：存入资源
//   乾 "key" value [五行]
void ClpsParser::exec_qian(size_t loop_i) {
    consume(); // 乾
    std::string key = expect(TK::STR).text;
    ClpsValue   val = parse_value(loop_i);
    auto        wx  = try_parse_wx();
    cur()->qian(key, std::move(val), wx);
}

// 震：运算（克制则抛异常）或万物库调用
//
//   算术：震 "key" op value
//   库（有结果）：震 "key"  【模块名】 函数名 [参数...]
//   库（纯副作用）：震       【模块名】 函数名 [参数...]
void ClpsParser::exec_zhen(size_t loop_i) {
    consume(); // 震

    // ── 路径判断 ──────────────────────────────────────────
    // 下一词元是 MODULE → 纯副作用库调用（无返回存储）
    if (check(TK::MODULE)) {
        exec_zhen_module("", loop_i);
        return;
    }
    // 下一词元是 STR
    if (check(TK::STR)) {
        std::string key = consume().text;
        // key 后紧跟 MODULE → 有返回值的库调用
        if (check(TK::MODULE)) {
            exec_zhen_module(key, loop_i);
            return;
        }
        // 否则是算术运算
        char      op  = parse_op();
        ClpsValue val = parse_value(loop_i);
        cur()->zhen(key, op, std::move(val));
        return;
    }
    throw ClpsParseError("震卦：期望 key 或 【模块名】", peek().line);
}

// 万物库调用实现
void ClpsParser::exec_zhen_module(const std::string& result_key, size_t loop_i) {
    std::string mod_name = expect(TK::MODULE).text;

    // 函数名（语境坍缩：词法层折叠的多义词，在此位置坍缩为函数名）
    if (check(TK::END) || check(TK::RBRACE))
        throw ClpsParseError("震卦·万物库：期望函数名", peek().line);
    std::string fn_name = consume().text;

    // 收集参数（直到遇到关键字、}、END）
    std::vector<ClpsValue> args;
    while (true) {
        TK t = peek().type;
        if (t == TK::END || t == TK::RBRACE)     break;
        if (t == TK::JIEJIE  || t == TK::QIAN    ||
            t == TK::ZHEN    || t == TK::KAN     ||
            t == TK::DUI     || t == TK::KUN     ||
            t == TK::LI      || t == TK::XUN     ||
            t == TK::GEN     || t == TK::BIAN    ||
            t == TK::FU      || t == TK::SONG    ||
            t == TK::JIAREN  || t == TK::SHI     ||
            t == TK::HE      || t == TK::XIAO_XU ||
            t == TK::HENG    || t == TK::JIAN ||
            t == TK::YI) break;
        args.push_back(parse_value(loop_i));
    }

    // ── 鼎执行(50)：子鼎隔离执行 ─────────────────────────────────────
    // 震 "result" 【雷地豫卦】 鼎执行 child_ding_name
    // → C++ 栈帧保存母鼎 g_tok_str 等状态，执行子鼎 tokens，完毕恢复
    // 每层自己的指令流在 C++ 栈上隔离，不共享 g_tok_str，母永远安全
    if (mod_name == "雷地豫卦" && fn_name == "鼎执行") {
        std::string child_ding;
        if (!args.empty()) {
            if (auto* sv = std::get_if<std::string>(&args[0].data))
                child_ding = *sv;
        }
        if (child_ding.empty()) {
            if (!result_key.empty())
                cur()->qian(result_key, ClpsValue(int64_t(0)));
            return;
        }

        // 读取子鼎 __tok__
        ClpsValue child_tok_val = WanWuKu::instance().call(
            "雷地豫卦", "缓存取",
            {ClpsValue(child_ding), ClpsValue(std::string("__tok__"))});
        std::string child_tok;
        if (auto* sv = std::get_if<std::string>(&child_tok_val.data))
            child_tok = *sv;
        if (child_tok.empty()) {
            if (!result_key.empty())
                cur()->qian(result_key, ClpsValue(int64_t(0)));
            return;
        }

        // ── 保存母鼎状态（C++ 栈帧隔离，母的井水不被子搅浑）──
        // 基础设施级原语，不走普通 WanWuKu 分发
        // 接口层用豫卦缓存（__tok__ 存取），实现层用 C++ 栈（save/restore）
        // ⚠️ 凡新增 native scope 基础设施变量，必须同步加入此 save/restore 列表。
        // 当前清单（7项）：g_tok_str / g_cur_type / g_cur_text / g_exec_done /
        //                  g_exec_use_cache / g_yi_stack + 井["g_井名"]
        ClpsValue saved_tok      = cur()->dui("g_tok_str");
        ClpsValue saved_type     = cur()->dui("g_cur_type");
        ClpsValue saved_text     = cur()->dui("g_cur_text");
        ClpsValue saved_done     = cur()->dui("g_exec_done");
        ClpsValue saved_cache    = cur()->dui("g_exec_use_cache");
        ClpsValue saved_yi_stack = cur()->dui("g_yi_stack");
        ClpsValue saved_well  = WanWuKu::instance().call(
            "雷地豫卦", "缓存取",
            {ClpsValue(std::string("井")), ClpsValue(std::string("g_井名"))});

        // ── 切换到子鼎 ──
        cur()->qian("g_tok_str", ClpsValue(child_tok));
        cur()->qian("g_exec_done", ClpsValue(int64_t(0)));
        WanWuKu::instance().call(
            "雷地豫卦", "缓存存",
            {ClpsValue(std::string("井")), ClpsValue(std::string("g_井名")),
             ClpsValue(child_ding)});

        // ── 执行子鼎 tokens（复用编译好的 exec_步）──
        auto yi_it = yi_defs_.find("exec_步");
        if (yi_it != yi_defs_.end()) {
            const YiDef& edef = yi_it->second;
            try {
                for (int i = 0; i < 50000; ++i) {
                    ClpsValue dv = cur()->dui("g_exec_done");
                    if (auto* iv = std::get_if<int64_t>(&dv.data)) {
                        if (*iv == 1) break;
                    }
                    size_t sp = pos_;
                    pos_ = edef.body_start;
                    while (pos_ < edef.body_end && !check(TK::END))
                        exec_one(0);
                    pos_ = sp;
                }
            } catch (...) {
                // 异常时先恢复母鼎状态，再 rethrow
                cur()->qian("g_tok_str", saved_tok);
                cur()->qian("g_cur_type", saved_type);
                cur()->qian("g_cur_text", saved_text);
                cur()->qian("g_exec_done", saved_done);
                cur()->qian("g_exec_use_cache", saved_cache);
                cur()->qian("g_yi_stack", saved_yi_stack);
                WanWuKu::instance().call(
                    "雷地豫卦", "缓存存",
                    {ClpsValue(std::string("井")), ClpsValue(std::string("g_井名")),
                     saved_well});
                throw;
            }
        }

        // ── 恢复母鼎状态 ──
        cur()->qian("g_tok_str", saved_tok);
        cur()->qian("g_cur_type", saved_type);
        cur()->qian("g_cur_text", saved_text);
        cur()->qian("g_exec_done", saved_done);
        cur()->qian("g_exec_use_cache", saved_cache);
        cur()->qian("g_yi_stack", saved_yi_stack);
        WanWuKu::instance().call(
            "雷地豫卦", "缓存存",
            {ClpsValue(std::string("井")), ClpsValue(std::string("g_井名")),
             saved_well});

        if (!result_key.empty())
            cur()->qian(result_key, ClpsValue(int64_t(1)));
        return;
    }

    // 调用
    ClpsValue result = WanWuKu::instance().call(mod_name, fn_name, args);

    // 存储结果
    if (!result_key.empty()) {
        cur()->qian(result_key, std::move(result));
    }
}

// 坎：安全运算（克制则静默）
//   坎 "key" op value
void ClpsParser::exec_kan(size_t loop_i) {
    consume(); // 坎
    std::string key = expect(TK::STR).text;
    char        op  = parse_op();
    ClpsValue   val = parse_value(loop_i);
    cur()->kan(key, op, std::move(val));
}

// 兑：读取/输出
//   兑 "key"           — 查变量输出；找不到则以 key 本身为字面量输出
//
// 易经·兑·丽泽，君子以朋友讲习。
// 口开了，说什么都行。变量是变量，字面量是字面量，兑不区分——说出去就是了。
void ClpsParser::exec_dui(size_t /*loop_i*/) {
    consume(); // 兑
    std::string key = expect(TK::STR).text;

    // 兑卦·字面量兜底：key 不在当前作用域 → 以 key 本名直接输出
    auto* scope = cur().get();
    bool found = (scope->resources.count(key) > 0 ||
                  scope->refs.count(key) > 0);
    if (!found) {
        emit("[兑·显像] " + key);
        return;
    }

    ClpsValue   val = scope->dui(key);
    // 坍缩为实象，输出
    std::ostringstream oss;
    oss << "[兑·显像] " << key << " => ";
    if (std::holds_alternative<std::monostate>(val.data))
        oss << "虚（None）";
    else if (auto* iv = std::get_if<int64_t>(&val.data))
        oss << *iv;
    else if (auto* dv = std::get_if<double>(&val.data))
        oss << *dv;
    else if (auto* sv = std::get_if<std::string>(&val.data))
        oss << "\"" << *sv << "\"";
    else if (auto* lp = std::get_if<std::shared_ptr<ClpsList>>(&val.data)) {
        oss << "[列表] " << (*lp)->size() << "项";
        emit(oss.str());
        for (size_t i = 0; i < (*lp)->size(); ++i) {
            std::ostringstream item;
            item << "  [" << i << "] ";
            const ClpsValue& v = (**lp)[i];
            if (auto* iv = std::get_if<int64_t>(&v.data))        item << *iv;
            else if (auto* dv = std::get_if<double>(&v.data))     item << *dv;
            else if (auto* sv = std::get_if<std::string>(&v.data)) item << "\"" << *sv << "\"";
            else item << "(复合)";
            emit(item.str());
        }
        return;
    }
    else if (std::holds_alternative<std::shared_ptr<ClpsMap>>(val.data))
        oss << "{图}";
    emit(oss.str());
}

// 坤：归元
//   坤              — 清空当前作用域
//   坤 "列表名" N  — 截断列表到前 N 项（坤·归列）
void ClpsParser::exec_kun(size_t loop_i) {
    consume(); // 坤
    TK t = peek().type;
    if (t == TK::STR) {
        std::string key = expect(TK::STR).text;
        auto count_val = parse_value(loop_i);
        int64_t n = std::get<int64_t>(count_val.data);
        cur()->kun_list(key, static_cast<size_t>(n));
        emit("[坤·归列] \"" + key + "\" 截断至 " + std::to_string(n) + " 项");
    } else {
        cur()->kun();
        emit("[坤·归元] 结界资源已散化");
    }
}

// 离：跨界引用
//   离 "key" 源结界名 "src_key"
void ClpsParser::exec_li(size_t /*loop_i*/) {
    consume(); // 离
    std::string key      = expect(TK::STR).text;
    std::string src_name = expect(TK::IDENT).text;
    std::string src_key  = expect(TK::STR).text;

    // 在作用域栈中找名为 src_name 的结界
    std::shared_ptr<TaiJiJieJie> src_jj;
    for (auto it = scope_stack_.rbegin(); it != scope_stack_.rend(); ++it) {
        if ((*it)->name == src_name) { src_jj = *it; break; }
    }
    // 未在作用域栈中找到 → 查家人模块表
    if (!src_jj) {
        auto it = modules_.find(src_name);
        if (it != modules_.end()) {
            src_jj = it->second;
        }
    }
    if (!src_jj) {
        throw ClpsParseError("离卦：找不到结界或模块 \"" + src_name + "\"", peek().line);
    }
    cur()->li(key, src_jj, src_key);
}

// 巽：遍历
//   巽 [pattern]                  — 输出所有匹配 key（资源图遍历）
//   巽 "list_key" "elem" { 块 }   — 列表遍历，当前元素存入 "elem"
void ClpsParser::exec_xun(size_t loop_i) {
    consume(); // 巽

    // ── 判断是否为列表遍历模式：巽 STR STR LBRACE ──
    if (check(TK::STR)) {
        size_t saved_pos = pos_;
        std::string first = consume().text;

        if (check(TK::STR)) {
            std::string elem_var = consume().text;
            if (check(TK::LBRACE)) {
                // 列表遍历模式
                ClpsValue list_val = cur()->dui(first);
                auto* lp = std::get_if<std::shared_ptr<ClpsList>>(&list_val.data);
                if (!lp)
                    throw ClpsParseError("巽卦：\"" + first + "\" 非列表，无法遍历", peek().line);

                consume(); // {
                size_t body_start = pos_;
                // 扫描找到匹配的 }
                size_t body_end = pos_;
                {
                    size_t scan = pos_, depth = 1;
                    while (scan < toks_.size() && depth > 0) {
                        if      (toks_[scan].type == TK::LBRACE) ++depth;
                        else if (toks_[scan].type == TK::RBRACE) --depth;
                        ++scan;
                    }
                    body_end = scan - 1; // 指向 }
                }

                emit("[巽·列表] \"" + first + "\" 共 " + std::to_string((*lp)->size()) + " 项");
                for (size_t i = 0; i < (*lp)->size(); ++i) {
                    cur()->qian(elem_var, (**lp)[i]);   // 当前元素存入 elem_var
                    pos_ = body_start;
                    while (pos_ < body_end) {
                        if (check(TK::END)) break;
                        exec_one(i);
                    }
                }
                pos_ = body_end;
                expect(TK::RBRACE);
                return;
            }
            // 不是 LBRACE，回退
            pos_ = saved_pos;
        } else {
            // 只有一个 STR，可能是普通 pattern，回退重新处理
            pos_ = saved_pos;
        }
    }

    // ── 资源图遍历模式（原有行为）──
    std::optional<std::string> pat;
    if (check(TK::STR))           pat = consume().text;
    else if (check(TK::WILDCARD)) pat = consume().text;
    else if (check(TK::IDENT))    pat = consume().text;

    ClpsMap result = cur()->xun(pat);
    emit("[巽·遍历] 共 " + std::to_string(result.size()) + " 项");
    for (auto& [k, v] : result) {
        std::ostringstream oss;
        oss << "  " << k << " = ";
        if (auto* iv = std::get_if<int64_t>(&v.data))        oss << *iv;
        else if (auto* dv = std::get_if<double>(&v.data))     oss << *dv;
        else if (auto* sv = std::get_if<std::string>(&v.data)) oss << "\"" << *sv << "\"";
        else oss << "(复合)";
        emit(oss.str());
    }
}

// 变：爻变（修改五行）
//   变 "key" 五行
void ClpsParser::exec_bian() {
    consume(); // 变
    std::string key = expect(TK::STR).text;
    WX          wx  = parse_wx();
    cur()->bian(key, wx);
    emit("[变·爻变] \"" + key + "\" 五行已更新");
}

// 复：循环
//   复 N { statements }
void ClpsParser::exec_fu(size_t /*outer_loop_i*/) {
    consume(); // 复
    // N 可以是整数字面量或 IDENT（从当前作用域读）
    size_t n = 0;
    if (check(TK::INT)) {
        n = static_cast<size_t>(consume().ival);
    } else if (check(TK::IDENT)) {
        ClpsValue v = cur()->dui(consume().text);
        if (auto* iv = std::get_if<int64_t>(&v.data)) n = static_cast<size_t>(*iv);
        else throw ClpsParseError("复卦：循环次数必须为整数");
    } else {
        throw ClpsParseError("复卦：期望循环次数", peek().line);
    }

    expect(TK::LBRACE);

    // 记录 { 后的位置，以便多次循环
    size_t body_start = pos_;
    size_t brace_depth = 1;

    // 先扫描找到对应 }
    size_t body_end = pos_;
    {
        size_t scan = pos_;
        size_t depth = 1;
        while (scan < toks_.size() && depth > 0) {
            if (toks_[scan].type == TK::LBRACE) ++depth;
            else if (toks_[scan].type == TK::RBRACE) --depth;
            ++scan;
        }
        body_end = scan - 1; // 位置指向 }
    }

    // 扁平坍缩法：循环体每次从 body_start 执行到 body_end（}之前）
    for (size_t i = 0; i < n; ++i) {
        pos_ = body_start;
        while (pos_ < body_end) {
            if (check(TK::END)) break;
            exec_one(i);
        }
    }
    pos_ = body_end; // 跳到 }
    expect(TK::RBRACE);
}

// 结界：作用域
//   结界 "name" { statements }
//   内卦成型（}）→ 立即坍缩（执行），结果归并外卦
void ClpsParser::exec_jiejie(size_t loop_i) {
    consume(); // 结界
    std::string name = expect(TK::STR).text;
    expect(TK::LBRACE);

    // 压帧（内卦）
    auto child = std::make_shared<TaiJiJieJie>(name, (int64_t)scope_stack_.size(), nullptr);
    scope_stack_.push_back(child);

    // 执行内部语句（扁平坍缩：遇到 } 停止）
    while (!check(TK::RBRACE) && !check(TK::END)) {
        exec_one(loop_i);
    }
    expect(TK::RBRACE);

    // 坍缩：弹帧，内卦归元（资源由外卦继承可选，此版本内卦独立生命周期）
    scope_stack_.pop_back();
    emit("[内卦坍缩] 结界 \"" + name + "\" 已归元");
}

// 艮：条件分支（止·门控）
//   艮 "key" cmpop value { statements }
//   艮 "key" cmpop value { statements } 否 { statements }
//
//   条件为真  → 执行 艮 块，扁平跳过 否 块（若有）
//   条件为假  → 扁平跳过 艮 块，执行 否 块（若有）
//
// 易经·艮否：艮(52)止·门控；否(12)天地不交·封闭路径。一通一塞，阴阳对立。
void ClpsParser::exec_gen(size_t loop_i) {
    consume(); // 艮
    std::string key = expect(TK::STR).text;
    TK          op  = parse_cmp_op();
    ClpsValue   rhs = parse_value(loop_i);
    expect(TK::LBRACE);

    ClpsValue lhs = cur()->dui(key);
    bool cond = compare_clps(lhs, rhs, op);

    // 辅助：扁平跳过一个 { ... } 块（调用前 { 已消费）
    auto skip_block = [&]() {
        size_t depth = 1;
        while (pos_ < toks_.size() && depth > 0) {
            if      (toks_[pos_].type == TK::LBRACE) ++depth;
            else if (toks_[pos_].type == TK::RBRACE) { if (--depth == 0) { ++pos_; break; } }
            ++pos_;
        }
    };

    if (cond) {
        // 执行 艮 块
        while (!check(TK::RBRACE) && !check(TK::END)) exec_one(loop_i);
        expect(TK::RBRACE);
        // 跳过 否 块（若有）
        if (check(TK::POU)) {
            consume(); // 否
            expect(TK::LBRACE);
            skip_block();
        }
    } else {
        // 跳过 艮 块
        skip_block();
        // 执行 否 块（若有）
        if (check(TK::POU)) {
            consume(); // 否
            expect(TK::LBRACE);
            while (!check(TK::RBRACE) && !check(TK::END)) exec_one(loop_i);
            expect(TK::RBRACE);
        }
    }
}

// 讼：断言（天与水违行，君子以作事谋始）
//   讼 "key" cmpop value
//   条件为真 → [讼·验] 通过；为假 → 抛 ClpsError
void ClpsParser::exec_song(size_t loop_i) {
    int line = peek().line;
    consume(); // 讼
    std::string key = expect(TK::STR).text;
    TK          op  = parse_cmp_op();
    ClpsValue   rhs = parse_value(loop_i);

    ClpsValue lhs = cur()->dui(key);
    bool ok = compare_clps(lhs, rhs, op);

    if (ok) {
        emit("[讼·验] \"" + key + "\" " + cmp_op_str(op) + " " + rhs.repr() + " → 通过");
    } else {
        std::ostringstream oss;
        oss << "[讼·断] 断言失败（第" << line << "行）: \""
            << key << "\" (= " << lhs.repr() << ") "
            << cmp_op_str(op) << " " << rhs.repr();
        throw ClpsError(oss.str());
    }
}

// 家人：加载外部 .dao 文件，在独立结界内运行，结果存入 modules_
//   家人 "模块名" "文件路径"
//   之后可用：离 "key" 模块名 "src_key"
void ClpsParser::exec_jiaren(size_t /*loop_i*/) {
    int line = peek().line;
    consume(); // 家人

    std::string mod_name = expect(TK::STR).text;
    std::string filepath  = expect(TK::STR).text;

    // 读取文件
    std::ifstream f(filepath);
    if (!f) {
        throw ClpsParseError("家人：无法打开文件 \"" + filepath + "\"", line);
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string src = ss.str();

    // 词法分析
    ClpsLexer lexer(src);
    std::vector<Token> tokens;
    try {
        tokens = lexer.tokenize();
    } catch (const ClpsLexError& e) {
        throw ClpsParseError(
            "家人：文件 \"" + filepath + "\" 词法错误 第" +
            std::to_string(e.line) + "行: " + e.what(), line);
    }

    // 静态检查（继承主程序的检查策略）
    ClpsChecker checker(tokens);
    auto diags = checker.check();
    for (auto& d : diags) {
        if (d.is_error()) {
            throw ClpsParseError(
                "家人：文件 \"" + filepath + "\" 静态错误 第" +
                std::to_string(d.line) + "行: " + d.msg, line);
        }
    }

    // 创建独立 Parser，根结界命名为 mod_name
    // 子程序的输出通过 emit 回调传递到当前输出流
    ClpsParser child(tokens, out_);
    child.root_jj->name = mod_name;

    // 执行子程序（外部文件在独立结界内坍缩，跑完后不再运行）
    try {
        child.run();
    } catch (const ClpsParseError& e) {
        throw ClpsParseError(
            "家人：文件 \"" + filepath + "\" 解析错误 第" +
            std::to_string(e.line) + "行: " + e.what(), line);
    }

    // 将子程序根结界存入模块表，供 离 跨界引用
    modules_[mod_name] = child.root_jj;
    emit("[家人·归] 模块 \"" + mod_name + "\" 已加载（" + filepath + "）");
}

// ── 师·分兵（解释器模式）────────────────────────────────────────────────
void ClpsParser::exec_shi(size_t /*loop_i*/) {
    int line = peek().line;
    consume(); // 师
    std::string task_name = expect(TK::STR).text;
    std::string filepath  = expect(TK::STR).text;

    // 预读 + 编译（主线程完成）
    std::ifstream f(filepath);
    if (!f) throw ClpsParseError("师·分兵：无法打开文件 \"" + filepath + "\"", line);
    std::ostringstream ss; ss << f.rdbuf();
    std::string src = ss.str();

    ClpsLexer lexer(src);
    auto tokens = lexer.tokenize();
    auto child_bc = std::make_shared<std::vector<Token>>(std::move(tokens));

    // 输出缓冲
    auto buf = std::make_shared<std::vector<std::string>>();
    auto buf_out = [buf](const std::string& s){ buf->push_back(s); };

    // 异步执行（解释器模式：子 ClpsParser）
    auto fut = std::async(std::launch::async,
        [child_bc, buf_out, task_name]() mutable
            -> std::shared_ptr<TaiJiJieJie>
        {
            ClpsParser child(*child_bc, buf_out);
            child.root_jj->name = task_name;
            child.run();
            return child.root_jj;
        });

    pending_shi_[task_name] = { std::move(fut), buf };
    emit("[师·分兵] 任务 \"" + task_name + "\" 已启动（" + filepath + "）");
}

// ── 小畜：列表积累（小畜卦·密云不雨）────────────────────────────────────
//   小畜 "名"       — 辟出空列表
//   小畜 "名" 值    — 向列表追加值（列表不存在则创建）
void ClpsParser::exec_xiao_xu(size_t loop_i) {
    consume(); // 小畜
    std::string key = expect(TK::STR).text;

    // 检查后续是否有值
    std::optional<ClpsValue> val;
    TK t = peek().type;
    if (t == TK::STR    || t == TK::INT  || t == TK::FLOAT ||
        t == TK::DOLLAR_I || t == TK::IDENT) {
        val = parse_value(loop_i);
    }

    cur()->xiao_xu(key, std::move(val));

    if (val)
        emit("[小畜·积] \"" + key + "\" ← 一项（共 " +
             std::to_string(std::get<std::shared_ptr<ClpsList>>(
                 cur()->dui(key).data)->size()) + "）");
    else
        emit("[小畜·辟] \"" + key + "\" 空列已辟");
}

// ── 恒(32)：只读常量声明 ─────────────────────────────────────────────────
//   恒 "key" value [五行]
void ClpsParser::exec_heng(size_t loop_i) {
    consume(); // 恒
    std::string key = expect(TK::STR).text;
    ClpsValue   val = parse_value(loop_i);
    auto        wx  = try_parse_wx();
    cur()->heng(key, std::move(val), wx);
    emit("[恒·常] \"" + key + "\" 已锁为只读常量");
}

// ── 蹇(39)：错误恢复（水山蹇·难行）──────────────────────────────────────
//   蹇 { ... } 化 "errvar" { ... }
//   蹇 { ... }              ← 化块可选
void ClpsParser::exec_jian(size_t loop_i) {
    consume(); // 蹇
    expect(TK::LBRACE);

    // 记录 try 块起始位置，收集 token 范围
    // 直接在此捕获异常：先保存 pos_，尝试执行，失败则跳过
    size_t try_start = pos_;  // 进入 try 块 token 开始处

    // 辅助：跳过一个 { ... } 块（含嵌套大括号）
    auto skip_braced = [&]() {
        int depth = 1;
        while (!check(TK::END) && depth > 0) {
            if (peek().type == TK::LBRACE) { depth++; consume(); }
            else if (peek().type == TK::RBRACE) { depth--; if (depth > 0) consume(); }
            else consume();
        }
        if (check(TK::RBRACE)) consume(); // 消费最终 }
    };

    // 执行 try 块，捕获任何异常
    std::string caught_msg;
    bool caught = false;
    size_t saved_pos = pos_;
    try {
        while (!check(TK::RBRACE) && !check(TK::END)) {
            exec_one(loop_i);
        }
        expect(TK::RBRACE);
    } catch (const std::exception& e) {
        caught = true;
        caught_msg = e.what();
        // 跳到 try 块末尾的 }
        int depth = 1;
        while (!check(TK::END) && depth > 0) {
            if (peek().type == TK::LBRACE) { depth++; consume(); }
            else if (peek().type == TK::RBRACE) {
                depth--;
                if (depth == 0) { consume(); break; }
                consume();
            } else {
                consume();
            }
        }
    }

    // 检查是否有 化 块
    if (!check(TK::HUA)) {
        if (caught) emit("[蹇·化] 错误已化解：" + caught_msg);
        return;
    }
    consume(); // 化

    std::string err_key = expect(TK::STR).text;
    expect(TK::LBRACE);

    if (caught) {
        // 将错误消息存入 err_key，执行化块
        cur()->qian(err_key, ClpsValue(caught_msg));
        emit("[蹇·化] 捕获错误 → \"" + err_key + "\"：" + caught_msg);
        while (!check(TK::RBRACE) && !check(TK::END)) {
            exec_one(loop_i);
        }
        expect(TK::RBRACE);
    } else {
        // 无错误，跳过化块
        int depth = 1;
        while (!check(TK::END) && depth > 0) {
            if (peek().type == TK::LBRACE) { depth++; consume(); }
            else if (peek().type == TK::RBRACE) {
                depth--;
                if (depth == 0) { consume(); break; }
                consume();
            } else {
                consume();
            }
        }
    }
}

// ── 师·合师（解释器模式）────────────────────────────────────────────────
void ClpsParser::exec_he(size_t /*loop_i*/) {
    consume(); // 合
    if (pending_shi_.empty()) { emit("[师·合师] 无待命部队"); return; }
    for (auto& [name, task] : pending_shi_) {
        auto scope = task.fut.get();
        for (auto& line : *task.buf) emit(line);
        modules_[name] = scope;
        emit("[师·归] 任务 \"" + name + "\" 已完成");
    }
    pending_shi_.clear();
}

// ── 益(42)：用户自定义子程序（风雷益·损上益下）───────────────────────────
//
//   定义（无参）：益 "name" { statements }
//   定义（有参）：益 "name" "p1" "p2" { statements }
//     → 记录 {params, body_start, body_end} 到 yi_defs_，跳过 body
//
//   调用（无参）：益 "name"
//   调用（有参）：益 "name" val1 val2
//     → 按顺序将 val_i 绑定到 param_i（乾的语法糖，不隔离作用域）
//     → 跳转执行 body，结束后恢复 pos_
//
// 易经·扁平坍缩法：参数绑定 = 自动 乾，共享作用域，无隐式调用栈。
// 益之义：损上（调用者提供值）益下（被调用者获得参数），利有攸往。
void ClpsParser::exec_yi(size_t loop_i) {
    consume(); // 益
    int line = peek().line;
    std::string name = expect(TK::STR).text;

    if (check(TK::LBRACE)) {
        // ── 定义模式（无参）──
        consume(); // {
        size_t body_start = pos_;
        size_t body_end = pos_;
        {
            size_t scan = pos_, depth = 1;
            while (scan < toks_.size() && depth > 0) {
                if      (toks_[scan].type == TK::LBRACE) ++depth;
                else if (toks_[scan].type == TK::RBRACE) --depth;
                ++scan;
            }
            body_end = scan - 1; // 指向 }
        }
        pos_ = body_end;
        expect(TK::RBRACE);
        yi_defs_[name] = YiDef{{}, body_start, body_end};
        emit("[益·定] 子程序 \"" + name + "\" 已定义");
        return;
    }

    if (check(TK::STR)) {
        // ── 可能是定义（有参）或调用（带字符串字面量参数）──
        // 规则：若连续 STR 序列后紧跟 LBRACE → 有参定义
        //       否则 → 调用（STR 字面量作为参数值）
        size_t saved_pos = pos_;
        std::vector<std::string> candidate_params;
        while (check(TK::STR)) {
            candidate_params.push_back(consume().text);
        }
        if (check(TK::LBRACE)) {
            // ── 定义模式（有参）──
            consume(); // {
            size_t body_start = pos_;
            size_t body_end = pos_;
            {
                size_t scan = pos_, depth = 1;
                while (scan < toks_.size() && depth > 0) {
                    if      (toks_[scan].type == TK::LBRACE) ++depth;
                    else if (toks_[scan].type == TK::RBRACE) --depth;
                    ++scan;
                }
                body_end = scan - 1;
            }
            pos_ = body_end;
            expect(TK::RBRACE);
            yi_defs_[name] = YiDef{candidate_params, body_start, body_end};
            std::string plist;
            for (auto& p : candidate_params) plist += " \"" + p + "\"";
            emit("[益·定] 子程序 \"" + name + "\"" + plist + " 已定义");
            return;
        }
        // 不是定义 → 回退，按调用处理（candidate_params 里是字符串字面量值）
        // 已消费的 STR token 当作字面量值传入
        auto it = yi_defs_.find(name);
        if (it == yi_defs_.end())
            throw ClpsParseError("益卦：未定义的子程序 \"" + name + "\"", line);
        const YiDef& def = it->second;

        // 绑定字符串字面量参数（已消费）
        for (size_t i = 0; i < candidate_params.size() && i < def.params.size(); ++i) {
            cur()->qian(def.params[i], ClpsValue(candidate_params[i]));
        }
        // 继续收集剩余非字符串参数（INT/FLOAT/IDENT/$i）
        size_t bound = candidate_params.size();
        while (bound < def.params.size()) {
            TK t = peek().type;
            if (t == TK::END || t == TK::RBRACE) break;
            if (t == TK::INT || t == TK::FLOAT || t == TK::IDENT || t == TK::DOLLAR_I) {
                cur()->qian(def.params[bound], parse_value(loop_i));
                ++bound;
            } else break;
        }
        emit("[益·召] 子程序 \"" + name + "\" 开始");
        size_t saved_exec = pos_;
        pos_ = def.body_start;
        while (pos_ < def.body_end && !check(TK::END)) exec_one(loop_i);
        pos_ = saved_exec;
        emit("[益·归] 子程序 \"" + name + "\" 结束");
        return;
    }

    // ── 调用模式（无 STR 前导，直接是数字/标识符/无参）──
    auto it = yi_defs_.find(name);
    if (it == yi_defs_.end())
        throw ClpsParseError("益卦：未定义的子程序 \"" + name + "\"", line);
    const YiDef& def = it->second;

    // 收集参数值（按 def.params 数量，遇关键字/}结束）
    for (size_t i = 0; i < def.params.size(); ++i) {
        TK t = peek().type;
        if (t == TK::END || t == TK::RBRACE) break;
        if (t == TK::INT || t == TK::FLOAT || t == TK::IDENT || t == TK::DOLLAR_I) {
            cur()->qian(def.params[i], parse_value(loop_i));
        } else break;
    }

    emit("[益·召] 子程序 \"" + name + "\" 开始");
    size_t saved_pos = pos_;
    pos_ = def.body_start;
    while (pos_ < def.body_end && !check(TK::END)) exec_one(loop_i);
    pos_ = saved_pos;
    emit("[益·归] 子程序 \"" + name + "\" 结束");
}

// ── exec_one：执行当前 Token 对应的语句 ──────────────────────────────────
void ClpsParser::exec_one(size_t loop_i) {
    switch (peek().type) {
        case TK::JIEJIE: exec_jiejie(loop_i); break;
        case TK::QIAN:   exec_qian(loop_i);   break;
        case TK::ZHEN:   exec_zhen(loop_i);   break;
        case TK::KAN:    exec_kan(loop_i);     break;
        case TK::DUI:    exec_dui(loop_i);     break;
        case TK::KUN:    exec_kun(loop_i);     break;
        case TK::LI:     exec_li(loop_i);      break;
        case TK::XUN:    exec_xun(loop_i);     break;
        case TK::BIAN:   exec_bian();          break;
        case TK::FU:     exec_fu(loop_i);      break;
        case TK::GEN:    exec_gen(loop_i);     break;
        case TK::SONG:   exec_song(loop_i);    break;
        case TK::JIAREN: exec_jiaren(loop_i);  break;
        case TK::SHI:     exec_shi    (loop_i);  break;
        case TK::HE:      exec_he     (loop_i);  break;
        case TK::XIAO_XU: exec_xiao_xu(loop_i);  break;
        case TK::HENG:    exec_heng   (loop_i);  break;
        case TK::JIAN:    exec_jian   (loop_i);  break;
        case TK::YI:      exec_yi     (loop_i);  break;
        default:
            throw ClpsParseError(
                std::string("意外的词元: ") + tk_name(peek().type) +
                " \"" + peek().text + "\"", peek().line);
    }
}

// ── 顶层入口 ──────────────────────────────────────────────────────────────
void ClpsParser::run() {
    while (!check(TK::END)) {
        exec_one(0);
    }
}

// ── REPL 专用：注入新词元，在持久作用域内执行 ────────────────────────────
void ClpsParser::exec_snippet(const std::vector<Token>& new_tokens) {
    // 替换词元流，重置位置
    toks_ = new_tokens;
    pos_  = 0;
    // 作用域栈确保根作用域存在
    while (scope_stack_.size() > 1) scope_stack_.pop_back();
    // 执行直到 END
    while (!check(TK::END)) {
        exec_one(0);
    }
}

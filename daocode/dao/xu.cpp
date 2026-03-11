#include "xu.hpp"
#include "wanwu.hpp"
#include <iostream>
#include <sstream>

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  构造 / 诊断辅助
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

ClpsChecker::ClpsChecker(const std::vector<Token>& tokens)
    : toks_(tokens)
{
    // 顶层作用域
    scopes_.push_back({"太极", {}});
}

void ClpsChecker::error  (int line, const std::string& msg) {
    diags_.push_back({ClpsDiag::Level::Error,   line, msg});
}
void ClpsChecker::warning(int line, const std::string& msg) {
    diags_.push_back({ClpsDiag::Level::Warning, line, msg});
}
void ClpsChecker::info   (int line, const std::string& msg) {
    diags_.push_back({ClpsDiag::Level::Info,    line, msg});
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Token 操作
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

const Token& ClpsChecker::consume() {
    const Token& t = toks_[pos_];
    if (pos_ + 1 < toks_.size()) ++pos_;
    return t;
}

bool ClpsChecker::try_consume(TK t) {
    if (check_tk(t)) { consume(); return true; }
    return false;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  五行推断
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// 从当前 token 推断操作数的五行，同时消费该 token
std::optional<WX> ClpsChecker::infer_wx_of_value() {
    const Token& t = peek();
    switch (t.type) {
        case TK::INT:      consume(); return WX::Jin;   // 整数 → 金
        case TK::FLOAT:    consume(); return WX::Shui;  // 浮点 → 水
        case TK::STR:      consume(); return WX::Huo;   // 字符串 → 火
        case TK::DOLLAR_I: consume(); return WX::Jin;   // $i → 金（整数索引）
        case TK::IDENT: {
            // 查变量表
            std::string key = consume().text;
            return lookup_var_wx(key);  // 可能未知
        }
        // 其他类型（TK::END等）→ 未知
        default: return std::nullopt;
    }
}

// 从内到外查找变量的五行
std::optional<WX> ClpsChecker::lookup_var_wx(const std::string& key) const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->vars.find(key);
        if (found != it->vars.end()) return found->second;
    }
    return std::nullopt;
}

// 在当前作用域设置变量五行
void ClpsChecker::set_var_wx(const std::string& key, WX wx) {
    scopes_.back().vars[key] = wx;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  跳过 { ... } 块（含嵌套）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void ClpsChecker::skip_block() {
    // 调用前 { 已被消费
    int depth = 1;
    while (!check_tk(TK::END) && depth > 0) {
        if      (check_tk(TK::LBRACE)) ++depth;
        else if (check_tk(TK::RBRACE)) --depth;
        consume();
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  各语句检查
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// 乾：存入资源
// 乾 "key" value [五行]
void ClpsChecker::check_qian() {
    int line = peek().line;
    consume(); // 乾

    if (!check_tk(TK::STR)) { consume(); return; } // 容错
    std::string key = consume().text;

    // 推断值的五行（用于默认）
    std::optional<WX> val_wx = infer_wx_of_value();

    // 显式五行覆盖
    WX declared_wx;
    bool has_explicit = false;
    switch (peek().type) {
        case TK::JIN:  consume(); declared_wx = WX::Jin;  has_explicit = true; break;
        case TK::SHUI: consume(); declared_wx = WX::Shui; has_explicit = true; break;
        case TK::MU:   consume(); declared_wx = WX::Mu;   has_explicit = true; break;
        case TK::HUO:  consume(); declared_wx = WX::Huo;  has_explicit = true; break;
        case TK::TU:   consume(); declared_wx = WX::Tu;   has_explicit = true; break;
        default: break;
    }

    // 记录五行
    WX final_wx;
    if (has_explicit) {
        final_wx = declared_wx;
        // 如果显式五行与值类型五行矛盾，发出提示
        if (val_wx && *val_wx != declared_wx) {
            info(line, "「乾」\"" + key + "\"：值类型为" +
                 wx_to_str(*val_wx) + "，显式声明为" + wx_to_str(declared_wx) +
                 "（以显式声明为准）");
        }
    } else if (val_wx) {
        final_wx = *val_wx;
    } else {
        return; // 未知五行，无法追踪
    }
    set_var_wx(key, final_wx);
}

// 震：五行检查（克制→警告·中立；相生→INFO·顺天道）或万物库调用
void ClpsChecker::check_zhen() {
    int line = peek().line;
    consume(); // 震

    // ── 万物库调用（纯副作用）：震 【模块】 函数名 [参数]
    if (check_tk(TK::MODULE)) {
        std::string mod  = consume().text;
        std::string fn   = (!check_tk(TK::END) && !check_tk(TK::RBRACE)) ? consume().text : ""; // 语境坍缩
        // 跳过参数直到下一语句
        while (!check_tk(TK::END) && !check_tk(TK::RBRACE)) {
            TK t = peek().type;
            if (t == TK::JIEJIE || t == TK::QIAN || t == TK::ZHEN ||
                t == TK::KAN    || t == TK::DUI  || t == TK::KUN  ||
                t == TK::LI     || t == TK::XUN  || t == TK::GEN  ||
                t == TK::BIAN   || t == TK::FU   || t == TK::SONG ||
                t == TK::JIAREN || t == TK::YI   || t == TK::SHI  ||
                t == TK::HE     || t == TK::HENG || t == TK::JIAN ||
                t == TK::XIAO_XU) break;
            consume();
        }
        return;
    }

    if (!check_tk(TK::STR)) { consume(); return; }
    std::string key = consume().text;

    // ── 万物库调用（有返回值）：震 "key" 【模块】 函数名 [参数]
    if (check_tk(TK::MODULE)) {
        std::string mod = consume().text;
        std::string fn  = (!check_tk(TK::END) && !check_tk(TK::RBRACE)) ? consume().text : ""; // 语境坍缩
        // 推断返回值五行，记入符号表
        auto rx = WanWuKu::instance().return_wx(mod, fn);
        if (rx) set_var_wx(key, *rx);
        // 跳过参数
        while (!check_tk(TK::END) && !check_tk(TK::RBRACE)) {
            TK t = peek().type;
            if (t == TK::JIEJIE || t == TK::QIAN || t == TK::ZHEN ||
                t == TK::KAN    || t == TK::DUI  || t == TK::KUN  ||
                t == TK::LI     || t == TK::XUN  || t == TK::GEN  ||
                t == TK::BIAN   || t == TK::FU   || t == TK::SONG ||
                t == TK::JIAREN || t == TK::YI   || t == TK::SHI  ||
                t == TK::HE     || t == TK::HENG || t == TK::JIAN ||
                t == TK::XIAO_XU) break;
            consume();
        }
        return;
    }

    // ── 算术运算：震 "key" op value
    try_consume(TK::PLUS);
    try_consume(TK::MINUS);
    try_consume(TK::STAR);
    try_consume(TK::SLASH);
    try_consume(TK::PERCENT);

    std::optional<WX> op_wx  = infer_wx_of_value();
    std::optional<WX> var_wx = lookup_var_wx(key);

    if (!op_wx || !var_wx) return;

    if (wx_is_ke(*op_wx, *var_wx)) {
        // 易经·原则二：克制·中立。水克火不是水的错，克制是自然规律，不是非法。
        // 检查器告知程序员存在张力，但不阻断。运行时将尝试执行，坤道承载一切。
        warning(line,
            "【克制·中立】震 \"" + key + "\"："
            + wx_to_str(*op_wx) + "克" + wx_to_str(*var_wx) +
            "，自然张力，运行时将尝试执行（确认是否有意为之？）");
    } else if (wx_can_sheng(*op_wx, *var_wx)) {
        info(line,
            "【相生·顺天】震 \"" + key + "\"："
            + wx_to_str(*op_wx) + "生" + wx_to_str(*var_wx) +
            "，顺天道，运算自然流通");
    }
}

// 坎：安全运算（克制→警告，运行时静默）
// 坎 "key" op value
void ClpsChecker::check_kan() {
    int line = peek().line;
    consume(); // 坎

    if (!check_tk(TK::STR)) { consume(); return; }
    std::string key = consume().text;

    try_consume(TK::PLUS);
    try_consume(TK::MINUS);
    try_consume(TK::STAR);
    try_consume(TK::SLASH);
    try_consume(TK::PERCENT);

    std::optional<WX> op_wx  = infer_wx_of_value();
    std::optional<WX> var_wx = lookup_var_wx(key);

    if (!op_wx || !var_wx) return;

    if (wx_is_ke(*op_wx, *var_wx)) {
        warning(line,
            "【险中有孚·编译期】坎 \"" + key + "\"："
            "操作数为" + wx_to_str(*op_wx) +
            "，变量为" + wx_to_str(*var_wx) +
            "，" + wx_to_str(*op_wx) + "克" + wx_to_str(*var_wx) +
            "（运行时将静默跳过，确认是否有意为之？）");
    }
}

// 兑：读取（消费 key token）
void ClpsChecker::check_dui() {
    consume(); // 兑
    if (check_tk(TK::STR)) consume();
}

// 坤：归元 / 归列
//   坤              — 清空作用域
//   坤 "列表名" N  — 截断列表到前 N 项
void ClpsChecker::check_kun() {
    consume();
    TK t = peek().type;
    if (t == TK::STR) {
        consume(); // 列表名
        // N：INT / IDENT / $i
        TK v = peek().type;
        if (v == TK::INT || v == TK::FLOAT || v == TK::IDENT || v == TK::DOLLAR_I)
            consume();
    }
}

// 离：跨界引用
// 离 "key" 结界名 "src_key"
void ClpsChecker::check_li() {
    int line = peek().line;
    consume(); // 离

    if (!check_tk(TK::STR)) { consume(); return; }
    std::string key = consume().text;

    std::string src_name;
    if (check_tk(TK::IDENT)) src_name = consume().text;

    std::string src_key;
    if (check_tk(TK::STR)) src_key = consume().text;

    // 尝试推断 src_key 的五行（跨结界，可能在外层作用域）
    std::optional<WX> src_wx = lookup_var_wx(src_key);
    if (src_wx) {
        set_var_wx(key, *src_wx);
        info(line, "「离」\"" + key + "\" 引用 \"" + src_key + "\"，五行继承为" + wx_to_str(*src_wx));
    }
    // 若找不到，标为未知（保守，不报错）
}

// 巽：遍历
void ClpsChecker::check_xun() {
    consume(); // 巽
    // 可选 pattern
    if (check_tk(TK::STR) || check_tk(TK::WILDCARD) || check_tk(TK::IDENT))
        consume();
}

// 变：爻变（更新符号表）
// 变 "key" 五行
void ClpsChecker::check_bian() {
    int line = peek().line;
    consume(); // 变

    if (!check_tk(TK::STR)) { consume(); return; }
    std::string key = consume().text;

    // 读新五行
    WX new_wx;
    bool found = true;
    switch (peek().type) {
        case TK::JIN:  consume(); new_wx = WX::Jin;  break;
        case TK::SHUI: consume(); new_wx = WX::Shui; break;
        case TK::MU:   consume(); new_wx = WX::Mu;   break;
        case TK::HUO:  consume(); new_wx = WX::Huo;  break;
        case TK::TU:   consume(); new_wx = WX::Tu;   break;
        default: found = false; break;
    }

    if (!found) return;

    std::optional<WX> old_wx = lookup_var_wx(key);
    if (old_wx) {
        info(line, "「变」\"" + key + "\" 五行：" +
             wx_to_str(*old_wx) + " → " + wx_to_str(new_wx));
    }
    set_var_wx(key, new_wx);
}

// 复：循环（检查一遍块体，不展开）
// 复 N { ... }
void ClpsChecker::check_fu() {
    consume(); // 复
    // 跳过循环次数
    if (check_tk(TK::INT) || check_tk(TK::IDENT)) consume();

    if (!try_consume(TK::LBRACE)) return;
    // 进入循环体（检查，不展开）
    check_block();
}

// 结界：新作用域
// 结界 "name" { ... }
void ClpsChecker::check_jiejie() {
    consume(); // 结界
    std::string name;
    if (check_tk(TK::STR)) name = consume().text;

    if (!try_consume(TK::LBRACE)) return;

    // 压作用域
    scopes_.push_back({name, {}});
    check_block();
    scopes_.pop_back();
}

// 家人：模块加载（跨文件五行未知，保守放行，不追踪子程序内部）
// 家人 "模块名" "文件路径"
void ClpsChecker::check_jiaren() {
    consume(); // 家人
    if (check_tk(TK::STR)) consume(); // 模块名
    if (check_tk(TK::STR)) consume(); // 文件路径
    // 跨文件五行无法静态追踪：保守放行，子程序由其自身的 checker 负责
}

// 师：异步分兵（结构同家人，保守放行）
// 师 "任务名" "文件路径"
void ClpsChecker::check_shi() {
    consume(); // 师
    if (check_tk(TK::STR)) consume(); // 任务名
    if (check_tk(TK::STR)) consume(); // 文件路径
}

// 合：合师（无操作数）
void ClpsChecker::check_he() {
    consume(); // 合
}

// 小畜：列表积累（保守直通）
// 小畜 "名" [值]
void ClpsChecker::check_xiao_xu() {
    consume(); // 小畜
    if (check_tk(TK::STR)) consume(); // 列表名
    // 可选值：STR INT FLOAT $i IDENT
    TK t = peek().type;
    if (t == TK::STR || t == TK::INT || t == TK::FLOAT ||
        t == TK::DOLLAR_I || t == TK::IDENT)
        consume();
}

// 艮：条件分支（止·门控）
// 艮 "key" cmpop value { ... } [否 { ... }]
// 静态检查：艮块和否块都检查一遍（两条路都可能执行）
void ClpsChecker::check_gen() {
    consume(); // 艮
    if (check_tk(TK::STR)) consume(); // key
    // 消费比较运算符
    switch (peek().type) {
        case TK::EQ: case TK::NEQ: case TK::GT:
        case TK::LT: case TK::GTE: case TK::LTE:
            consume(); break;
        default: break;
    }
    // 消费比较值
    switch (peek().type) {
        case TK::STR: case TK::INT: case TK::FLOAT:
        case TK::DOLLAR_I: case TK::IDENT:
            consume(); break;
        default: break;
    }
    // 检查 艮 块
    if (!try_consume(TK::LBRACE)) return;
    check_block();
    // 检查 否 块（可选）
    if (check_tk(TK::POU)) {
        consume(); // 否
        if (try_consume(TK::LBRACE)) check_block();
    }
}

// 讼：断言
// 讼 "key" cmpop value
void ClpsChecker::check_song() {
    consume(); // 讼
    if (check_tk(TK::STR)) consume(); // key
    // 消费比较运算符
    switch (peek().type) {
        case TK::EQ: case TK::NEQ: case TK::GT:
        case TK::LT: case TK::GTE: case TK::LTE:
            consume(); break;
        default: break;
    }
    // 消费比较值
    switch (peek().type) {
        case TK::STR: case TK::INT: case TK::FLOAT:
        case TK::DOLLAR_I: case TK::IDENT:
            consume(); break;
        default: break;
    }
}

// 恒(32)：只读常量声明（语法同乾）
// 恒 "key" value [五行]
void ClpsChecker::check_heng() {
    int line = peek().line;
    consume(); // 恒
    if (!check_tk(TK::STR)) { consume(); return; }
    std::string key = consume().text;

    std::optional<WX> val_wx = infer_wx_of_value(); // 推断并消费值token

    // 可选显式五行
    WX declared_wx;
    bool has_explicit = false;
    switch (peek().type) {
        case TK::JIN:  consume(); declared_wx = WX::Jin;  has_explicit = true; break;
        case TK::SHUI: consume(); declared_wx = WX::Shui; has_explicit = true; break;
        case TK::MU:   consume(); declared_wx = WX::Mu;   has_explicit = true; break;
        case TK::HUO:  consume(); declared_wx = WX::Huo;  has_explicit = true; break;
        case TK::TU:   consume(); declared_wx = WX::Tu;   has_explicit = true; break;
        default: break;
    }

    WX final_wx;
    if (has_explicit)      final_wx = declared_wx;
    else if (val_wx)       final_wx = *val_wx;
    else                   return;
    set_var_wx(key, final_wx);
}

// 益(42)：用户自定义子程序（定义或调用）
// 定义：益 "name" ["p1" ...] { body }
// 调用：益 "name" [val1 val2 ...]
void ClpsChecker::check_yi() {
    consume(); // 益
    if (!check_tk(TK::STR)) { consume(); return; }
    consume(); // name

    // 先吞掉连续 STR（可能是参数名或字符串实参）
    while (check_tk(TK::STR)) consume();

    if (try_consume(TK::LBRACE)) {
        // 定义：进入块体检查（参数类型未知→保守放行）
        check_block();
        return;
    }

    // 调用：吞掉非语句词元（整数/浮点/标识符/$i）
    while (!check_tk(TK::END) && !check_tk(TK::RBRACE)) {
        TK t = peek().type;
        if (t == TK::JIEJIE || t == TK::QIAN || t == TK::ZHEN ||
            t == TK::KAN    || t == TK::DUI  || t == TK::KUN  ||
            t == TK::LI     || t == TK::XUN  || t == TK::GEN  ||
            t == TK::BIAN   || t == TK::FU   || t == TK::SONG ||
            t == TK::JIAREN || t == TK::YI   || t == TK::SHI  ||
            t == TK::HE     || t == TK::HENG || t == TK::JIAN ||
            t == TK::XIAO_XU) break;
        consume();
    }
}

// 蹇(39)：错误恢复（消费 try 块 + 可选 化 "errvar" 块）
// 蹇 { ... } 化 "errvar" { ... }
void ClpsChecker::check_jian() {
    consume(); // 蹇
    if (try_consume(TK::LBRACE)) check_block();
    if (check_tk(TK::HUA)) {
        consume(); // 化
        if (check_tk(TK::STR)) { consume(); } // errvar
        if (try_consume(TK::LBRACE)) check_block();
    }
}

// ── 块体检查（不含 {，直到 }）────────────────────────────────────────────
void ClpsChecker::check_block() {
    while (!check_tk(TK::RBRACE) && !check_tk(TK::END)) {
        check_stmt();
    }
    try_consume(TK::RBRACE);
}

// ── 单语句分发 ────────────────────────────────────────────────────────────
void ClpsChecker::check_stmt() {
    switch (peek().type) {
        case TK::JIEJIE: check_jiejie(); break;
        case TK::QIAN:   check_qian();   break;
        case TK::ZHEN:   check_zhen();   break;
        case TK::KAN:    check_kan();    break;
        case TK::DUI:    check_dui();    break;
        case TK::KUN:    check_kun();    break;
        case TK::LI:     check_li();     break;
        case TK::XUN:    check_xun();    break;
        case TK::BIAN:   check_bian();   break;
        case TK::FU:     check_fu();     break;
        case TK::GEN:    check_gen();    break;
        case TK::SONG:   check_song();   break;
        case TK::JIAREN: check_jiaren(); break;
        case TK::SHI:     check_shi();     break;
        case TK::HE:      check_he();      break;
        case TK::XIAO_XU: check_xiao_xu(); break;
        case TK::HENG:    check_heng();    break;
        case TK::JIAN:    check_jian();    break;
        case TK::YI:      check_yi();      break;
        default:
            // 未知 token：跳过，继续（容错）
            consume();
            break;
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  入口
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

std::vector<ClpsDiag> ClpsChecker::check() {
    while (!check_tk(TK::END)) {
        check_stmt();
    }
    return diags_;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  打印工具
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void print_diags(const std::vector<ClpsDiag>& diags, const std::string& filename) {
    int errors = 0, warnings = 0, infos = 0;
    for (auto& d : diags) {
        std::string prefix;
        switch (d.level) {
            case ClpsDiag::Level::Error:
                prefix = "\033[1;31m[错误]\033[0m";   ++errors;   break;
            case ClpsDiag::Level::Warning:
                prefix = "\033[1;33m[警告]\033[0m";   ++warnings; break;
            case ClpsDiag::Level::Info:
                prefix = "\033[1;36m[提示]\033[0m";   ++infos;    break;
        }
        std::cerr << prefix;
        if (!filename.empty()) std::cerr << " " << filename;
        std::cerr << " 第" << d.line << "行: " << d.msg << "\n";
    }

    if (!diags.empty()) {
        std::cerr << "\n\033[1m需卦检查结果：\033[0m ";
        if (errors > 0)   std::cerr << "\033[1;31m" << errors   << " 错误\033[0m  ";
        if (warnings > 0) std::cerr << "\033[1;33m" << warnings << " 警告\033[0m  ";
        if (infos > 0)    std::cerr << "\033[1;36m" << infos    << " 提示\033[0m";
        std::cerr << "\n";
    }
}

bool diags_has_error(const std::vector<ClpsDiag>& diags) {
    for (auto& d : diags)
        if (d.is_error()) return true;
    return false;
}

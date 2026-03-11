#include "dui.hpp"
#include "wanwu.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>

// ── ANSI 颜色 ─────────────────────────────────────────────────────────────
static const char* C_RESET  = "\033[0m";
static const char* C_CYAN   = "\033[1;36m";
static const char* C_YELLOW = "\033[1;33m";
static const char* C_RED    = "\033[1;31m";
static const char* C_GREEN  = "\033[1;32m";
static const char* C_DIM    = "\033[2m";

// ── 提示符 ────────────────────────────────────────────────────────────────
static void print_prompt(bool continuation) {
    if (continuation)
        std::cout << C_DIM << "  ☯  " << C_RESET;
    else
        std::cout << C_CYAN << "道·码" << C_RESET << " ☯  ";
    std::cout << std::flush;
}

// ── 欢迎语 ────────────────────────────────────────────────────────────────
static void print_welcome() {
    std::cout << "\n";
    std::cout << C_CYAN;
    std::cout << "  ╔═══════════════════════════════════════╗\n";
    std::cout << "  ║   道·码 (CLPS) 交互式命令行 · 兑卦   ║\n";
    std::cout << "  ║   丽泽，兑；君子以朋友讲习            ║\n";
    std::cout << "  ╚═══════════════════════════════════════╝\n";
    std::cout << C_RESET;
    std::cout << C_DIM;
    std::cout << "  :q/:退出   :clear/:清空/:坤   :wx/:五行   :ls/:查看/:巽   （: 与 ： 均可）\n";
    std::cout << C_RESET << "\n";
}

// ── :wx 五行表 ────────────────────────────────────────────────────────────
static void print_wx_table() {
    std::cout << C_YELLOW << "\n  ── 五行克制表 ──\n" << C_RESET;
    std::cout << "  金克木  水克火  木克土  火克金  土克水\n";
    std::cout << "  金→水  水→木  木→火  火→土  土→金  （相生）\n\n";
    std::cout << C_YELLOW << "  ── 值类型五行 ──\n" << C_RESET;
    std::cout << "  整数 → 金    浮点 → 水    字符串 → 火\n";
    std::cout << "  列表 → 木    字典 → 土    无    → 土\n\n";
}

// ── 浮点去尾零格式化 ──────────────────────────────────────────────────────
static std::string fmt_double(double v) {
    std::ostringstream oss;
    oss << std::setprecision(10) << v;
    return oss.str();
}

// ── ClpsValue 单值展示（不含五行标签）────────────────────────────────────
static std::string fmt_val(const ClpsValue& v, int depth = 0) {
    if (auto* iv = std::get_if<int64_t>(&v.data))
        return std::to_string(*iv);
    if (auto* dv = std::get_if<double>(&v.data))
        return fmt_double(*dv);
    if (auto* sv = std::get_if<std::string>(&v.data))
        return "\"" + *sv + "\"";
    if (std::holds_alternative<std::monostate>(v.data))
        return "虚";
    if (auto* lp = std::get_if<std::shared_ptr<ClpsList>>(&v.data)) {
        const auto& lst = **lp;
        if (lst.empty()) return "[]";
        std::string s = "[";
        size_t show = std::min(lst.size(), size_t(5));
        for (size_t i = 0; i < show; ++i) {
            if (i) s += ", ";
            s += (depth < 1) ? fmt_val(lst[i], depth + 1) : "…";
        }
        if (lst.size() > show) s += ", …(" + std::to_string(lst.size()) + "项)";
        return s + "]";
    }
    if (auto* mp = std::get_if<std::shared_ptr<ClpsMap>>(&v.data)) {
        const auto& map = **mp;
        if (map.empty()) return "{}";
        std::string s = "{";
        size_t cnt = 0;
        for (auto& [mk, mv] : map) {
            if (cnt) s += ", ";
            s += mk + ": " + ((depth < 1) ? fmt_val(mv, depth + 1) : "…");
            if (++cnt >= 5) { s += ", …(" + std::to_string(map.size()) + "项)"; break; }
        }
        return s + "}";
    }
    return "?";
}

// ── :ls 显示当前作用域变量 ───────────────────────────────────────────────
static void print_vars(const std::shared_ptr<TaiJiJieJie>& jj) {
    bool empty = jj->resources.empty() && jj->refs.empty();
    if (empty) {
        std::cout << C_DIM << "  （作用域为空）\n" << C_RESET;
        return;
    }
    std::cout << C_YELLOW << "  ── 当前结界：" << jj->name << " ──\n" << C_RESET;

    // 普通资源
    for (auto& [k, v] : jj->resources) {
        bool ro = jj->readonly_keys.count(k) > 0;
        WX wx = jj->meta.count(k) ? jj->meta.at(k) : v.wx();

        std::cout << "  " << k << " = " << fmt_val(v);
        std::cout << C_DIM << "  (" << wx_to_str(wx);
        if (ro) std::cout << "·恒";
        std::cout << ")" << C_RESET << "\n";
    }

    // 离卦引用
    for (auto& [k, ref] : jj->refs) {
        auto& [src_jj, src_key] = ref;
        std::cout << "  " << k << C_DIM << " → " << src_jj->name
                  << "." << src_key << "  (离·引用)" << C_RESET << "\n";
    }

    std::cout << "\n";
}

// ── 检查括号是否平衡（用于多行输入检测）────────────────────────────────
static int brace_balance(const std::string& src) {
    int depth = 0;
    bool in_str = false;
    for (char c : src) {
        if (c == '"') in_str = !in_str;
        if (!in_str) {
            if (c == '{') ++depth;
            else if (c == '}') --depth;
        }
    }
    return depth;
}

// ── 执行一段源码（词法→静态检查→执行），显示诊断 ──────────────────────
static bool exec_snippet(const std::string& src, ClpsParser& parser) {
    // 词法
    ClpsLexer lexer(src);
    std::vector<Token> tokens;
    try {
        tokens = lexer.tokenize();
    } catch (const ClpsLexError& e) {
        std::cerr << C_RED << "[词法错误] 第" << e.line << "行: "
                  << e.what() << C_RESET << "\n";
        return false;
    }

    if (tokens.size() <= 1) return true; // 只有 END

    // 静态检查（REPL 模式：只报 Error，Warning 显示但不阻止）
    ClpsChecker checker(tokens);
    auto diags = checker.check();
    for (auto& d : diags) {
        if (d.is_error()) {
            std::cerr << C_RED << "[需卦·编译期] 第" << d.line << "行: "
                      << d.msg << C_RESET << "\n";
        } else if (d.is_warning()) {
            std::cerr << C_YELLOW << "[警告] 第" << d.line << "行: "
                      << d.msg << C_RESET << "\n";
        }
        // INFO 在 REPL 模式下静默，不打扰
    }
    if (diags_has_error(diags)) return false;

    // 执行（注入到持久 parser 的词元流）
    // ClpsParser::run() 从头跑——REPL 需要将新 tokens 注入并在当前作用域执行
    // 实现：用 parser 的 exec_snippet_into 方法（见 clps_parser 扩展）
    try {
        parser.exec_snippet(tokens);
    } catch (const ClpsKezError& e) {
        std::cout.flush();
        std::cerr << C_RED << "[天道阻断] " << e.what() << C_RESET << "\n";
        return false;
    } catch (const ClpsError& e) {
        std::cout.flush();
        std::cerr << C_RED << "[CLPS错误] " << e.what() << C_RESET << "\n";
        return false;
    } catch (const ClpsParseError& e) {
        std::cout.flush();
        std::cerr << C_RED << "[解析错误] 第" << e.line << "行: "
                  << e.what() << C_RESET << "\n";
        return false;
    }
    return true;
}

// ── REPL 主循环 ───────────────────────────────────────────────────────────
void run_repl() {
    print_welcome();

    // 持久解析器——root_jj 在整个会话内保持
    ClpsParser parser({Token{TK::END, "", 0, 0.0, 1}});

    std::string buffer;   // 多行输入缓冲
    int depth = 0;        // 未闭合 { 的层数

    while (true) {
        print_prompt(depth > 0);

        std::string line;
        if (!std::getline(std::cin, line)) break; // EOF

        // 全角冒号兼容：将行首 ：（U+FF1A）替换为 :（U+003A），两种输入均接受
        if (line.size() >= 3 &&
            (unsigned char)line[0] == 0xEF &&
            (unsigned char)line[1] == 0xBC &&
            (unsigned char)line[2] == 0x9A) {
            line = ":" + line.substr(3);
        }

        // 特殊命令
        if (depth == 0) {
            if (line == ":q" || line == ":quit" || line == ":exit" ||
                line == ":退出" || line == ":再见" || line == ":归元") break;
            if (line == ":clear" || line == ":清空" || line == ":坤") {
                parser.root_jj->kun();
                parser.root_jj->alive = true; // 重置 alive
                std::cout << C_GREEN << "  作用域已清空。\n" << C_RESET;
                continue;
            }
            if (line == ":wx" || line == ":五行") { print_wx_table(); continue; }
            if (line == ":ls" || line == ":查看" || line == ":巽") { print_vars(parser.root_jj); continue; }
            if (line.empty()) continue;
        }

        buffer += line + "\n";
        depth += brace_balance(line);

        // 括号未平衡→继续读取
        if (depth > 0) continue;
        if (depth < 0) {
            std::cerr << C_RED << "  括号不匹配（多余的 }）\n" << C_RESET;
            buffer.clear(); depth = 0;
            continue;
        }

        // 执行缓冲区
        exec_snippet(buffer, parser);
        buffer.clear();
    }

    std::cout << "\n" << C_DIM << "  [兑·归元] 道别。\n" << C_RESET << "\n";
}

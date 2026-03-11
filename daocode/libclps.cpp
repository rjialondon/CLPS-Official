/*
 * libclps.cpp — 道·码 CLPS 库实现
 *
 * 流水线（解释模式）：
 *   源码 → 词法（ClpsLexer）→ 需卦检查（ClpsChecker）→ 执行（ClpsParser）
 *
 * 初始化（万物库）：std::call_once 保证单次，可安全多次调用 clps_exec_*
 */

#include "libclps.hpp"

#include "dao/tun.hpp"
#include "dao/xu.hpp"
#include "dao/kun.hpp"
#include "dao/wanwu.hpp"
#include "dao/dui.hpp"
#include "dao/xisui.hpp"
#include "dao/gen.hpp"

#include <fstream>
#include <sstream>
#include <iostream>
#include <mutex>
#include <filesystem>
#include <cstdlib>

// ── 版本 ─────────────────────────────────────────────────────────────────
const char* clps_version() {
    return "道·码 CLPS 1.0.0";
}

// ── 艮卦·记忆·磁盘辅助 ───────────────────────────────────────────────────
namespace fs = std::filesystem;

// scope → 磁盘安全文件名（仅保留字母/数字/点/横线，其余换 _，取尾 120 字符）
static std::string scope_to_filename(const std::string& scope) {
    std::string name;
    for (unsigned char c : scope) {
        if (std::isalnum(c) || c == '.' || c == '-') name += (char)c;
        else name += '_';
    }
    if (name.size() > 120) name = name.substr(name.size() - 120);
    return name + ".kv";
}

// 记忆目录：~/.clps/memory/（不存在则自动创建）
static std::string clps_memory_dir() {
    const char* home = std::getenv("HOME");
    std::string dir = home ? std::string(home) + "/.clps/memory"
                           : "/tmp/.clps_memory";
    fs::create_directories(dir);
    return dir;
}

// ── ClpsMemory 工厂 ───────────────────────────────────────────────────────
ClpsMemory ClpsMemory::for_file(const std::string& path) {
    ClpsMemory m;
    m.scope = path;
    return m;
}

ClpsMemory ClpsMemory::for_scope(const std::string& name) {
    ClpsMemory m;
    m.scope = name;
    return m;
}

// ── ClpsMemory 磁盘操作 ───────────────────────────────────────────────────
bool ClpsMemory::load() {
    std::string path = clps_memory_dir() + "/" + scope_to_filename(scope);
    std::ifstream f(path);
    if (!f) return false;
    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        store[line.substr(0, eq)] = line.substr(eq + 1);
    }
    return true;
}

bool ClpsMemory::save() const {
    std::string path = clps_memory_dir() + "/" + scope_to_filename(scope);
    std::ofstream f(path);
    if (!f) return false;
    for (auto& [k, v] : store) f << k << "=" << v << "\n";
    return true;
}

// ── ClpsMemory 热记忆读写 ─────────────────────────────────────────────────
std::string ClpsMemory::get(const std::string& key, const std::string& def) const {
    auto it = store.find(key);
    return it == store.end() ? def : it->second;
}

void ClpsMemory::set(const std::string& key, const std::string& value) {
    store[key] = value;
}

void ClpsMemory::erase(const std::string& key) {
    store.erase(key);
}

bool ClpsMemory::has(const std::string& key) const {
    return store.count(key) > 0;
}

// ── 万物库单次初始化 ──────────────────────────────────────────────────────
static void ensure_init() {
    static std::once_flag flag;
    std::call_once(flag, []() {
        xi_sui_dan();
        wan_wu_ku_init();
    });
}

// ── 内部：源码 → ClpsResult ───────────────────────────────────────────────
static ClpsResult run_source(
    const std::string&              source,
    const std::string&              label,    // 用于错误信息的文件名或"<string>"
    const std::vector<std::string>& args,
    ClpsLibOutputFn                 output,
    ClpsMemory*                     memory)
{
    ensure_init();
    wan_wu_ku_set_args(args);
    wan_wu_ku_set_memory(memory);  // 艮卦·注入记忆上下文（nullptr = 无记忆模式）

    // ── 词法 ──────────────────────────────────────────────────────────────
    std::vector<Token> tokens;
    try {
        ClpsLexer lexer(source);
        tokens = lexer.tokenize();
    } catch (const ClpsLexError& e) {
        ClpsResult r;
        r.status = ClpsStatus::LEX_ERROR;
        r.error  = "[词法错误] " + label + " 第" + std::to_string(e.line) + "行: " + e.what();
        r.line   = e.line;
        return r;
    }

    // ── 需卦静态检查 ──────────────────────────────────────────────────────
    {
        ClpsChecker checker(tokens);
        auto diags = checker.check();
        if (diags_has_error(diags)) {
            std::string msg;
            for (auto& d : diags) {
                if (d.is_error())
                    msg += label + " 第" + std::to_string(d.line) + "行: " + d.msg + "\n";
            }
            ClpsResult r;
            r.status = ClpsStatus::TYPE_ERROR;
            r.error  = msg;
            return r;
        }
    }

    // ── 扁平坍缩法执行 ────────────────────────────────────────────────────
    // ClpsOutputFn 与 ClpsLibOutputFn 签名相同，直接转发
    ClpsOutputFn parser_out = output ? ClpsOutputFn(output) : ClpsOutputFn(nullptr);

    try {
        ClpsParser parser(tokens, parser_out);
        parser.run();
    } catch (const ClpsParseError& e) {
        std::cout.flush();
        ClpsResult r;
        r.status = ClpsStatus::RUN_ERROR;
        r.error  = "[解析错误] 第" + std::to_string(e.line) + "行: " + e.what();
        r.line   = e.line;
        return r;
    } catch (const ClpsKezError& e) {
        std::cout.flush();
        ClpsResult r;
        r.status = ClpsStatus::RUN_ERROR;
        r.error  = std::string("[天道阻断] ") + e.what();
        return r;
    } catch (const ClpsError& e) {
        std::cout.flush();
        ClpsResult r;
        r.status = ClpsStatus::RUN_ERROR;
        r.error  = std::string("[CLPS错误] ") + e.what();
        return r;
    }

    return ClpsResult{};  // ClpsStatus::OK
}

// ── clps_exec_file ────────────────────────────────────────────────────────
ClpsResult clps_exec_file(
    const std::string&              path,
    const std::vector<std::string>& args,
    ClpsLibOutputFn                 output,
    ClpsMemory*                     memory)
{
    std::ifstream f(path);
    if (!f)
        return {ClpsStatus::IO_ERROR,
                "[IO错误] 无法读取文件: " + path, 0};

    std::ostringstream ss;
    ss << f.rdbuf();
    return run_source(ss.str(), path, args, output, memory);
}

// ── clps_exec_string ──────────────────────────────────────────────────────
ClpsResult clps_exec_string(
    const std::string&              source,
    const std::vector<std::string>& args,
    ClpsLibOutputFn                 output,
    ClpsMemory*                     memory)
{
    return run_source(source, "<string>", args, output, memory);
}

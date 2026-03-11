#pragma once
/*
 * libclps.hpp — 道·码 CLPS 库入口
 *
 * 对外暴露接口：
 *   clps_version()      — 查询版本字符串
 *   clps_exec_file()    — 执行 .dao 文件
 *   clps_exec_string()  — 执行字符串源码
 *   ClpsMemory          — 艮卦·程序私有持久记忆
 *
 * 设计原则（易经宪法）：
 *   乾卦·对外强健：接口最小化，不泄露内部类型
 *   坤卦·对内承载：初始化、词法、检查、执行全部内藏
 *   需卦·守正等待：错误以结构体返回，不跨界抛异常
 *   艮卦·守界止于所：记忆归属各程序，不跨界共享
 */

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

// ── 版本 ─────────────────────────────────────────────────────────────────
const char* clps_version();

// ── 输出回调（nullptr = 直接写 stdout）───────────────────────────────────
using ClpsLibOutputFn = std::function<void(const std::string&)>;

// ── 执行状态码 ────────────────────────────────────────────────────────────
enum class ClpsStatus {
    OK         = 0,  // 成功
    IO_ERROR   = 1,  // 文件无法读取
    LEX_ERROR  = 2,  // 词法错误
    TYPE_ERROR = 3,  // 需卦·五行克制错误（编译期）
    RUN_ERROR  = 4,  // 运行时错误
};

// ── 执行结果 ──────────────────────────────────────────────────────────────
struct ClpsResult {
    ClpsStatus  status = ClpsStatus::OK;
    std::string error;    // 错误描述（status != OK 时有效）
    int         line = 0; // 错误行号（词法/运行时错误时有效）

    bool ok()   const { return status == ClpsStatus::OK; }
    operator bool() const { return ok(); }
};

// ── 艮卦·记忆（程序私有持久状态）────────────────────────────────────────────
/*
 * 艮卦（☶☶）— 兼山，时止则止，止于其所。
 *
 * 用法示例：
 *   ClpsMemory mem = ClpsMemory::for_file("/path/to/prog.dao");
 *   mem.load();                             // 从磁盘恢复（艮·归位）
 *   clps_exec_file(path, {}, nullptr, &mem);
 *   mem.save();                             // 持久化到磁盘（艮·止于所）
 *
 * 艮德三守：
 *   守界 — scope 唯一标识记忆域，不跨程序共享
 *   守时 — 不自动写盘，由调用方决定何时 save()
 *   守正 — shared 留口，天火同人卦握手协议待实现
 *
 * 存储路径：~/.clps/memory/<scope>.kv
 */
struct ClpsMemory {
    std::string scope;              // 记忆域标识（艮·守界）
    ClpsMemory* shared = nullptr;  // 共享层留口（天火同人卦，未激活）
    std::unordered_map<std::string, std::string> store;  // 热记忆表

    // 工厂：以文件路径为 scope（常规用法）
    static ClpsMemory for_file(const std::string& path);
    // 工厂：自定义 scope 名（命名记忆域）
    static ClpsMemory for_scope(const std::string& name);

    bool load();        // 从磁盘恢复热记忆（艮·归位）
    bool save() const;  // 持久化热记忆到磁盘（艮·止于所）

    std::string get(const std::string& key, const std::string& def = "") const;
    void        set(const std::string& key, const std::string& value);
    void        erase(const std::string& key);
    bool        has(const std::string& key) const;
};

// ── 主接口 ────────────────────────────────────────────────────────────────

// 执行 .dao 源文件
ClpsResult clps_exec_file(
    const std::string&              path,
    const std::vector<std::string>& args   = {},
    ClpsLibOutputFn                 output = nullptr,
    ClpsMemory*                     memory = nullptr   // 艮卦·记忆（可选）
);

// 执行字符串源码（source 即为 .dao 内容）
ClpsResult clps_exec_string(
    const std::string&              source,
    const std::vector<std::string>& args   = {},
    ClpsLibOutputFn                 output = nullptr,
    ClpsMemory*                     memory = nullptr   // 艮卦·记忆（可选）
);

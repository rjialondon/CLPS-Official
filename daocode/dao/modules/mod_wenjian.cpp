// mod_wenjian.cpp — 道·码万物库子模块
#include "../wanwu.hpp"
#include "../gen.hpp"
#include "../../libclps.hpp"
#include "mod_helpers.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>

static const char* DEFAULT_LOG = "道·码记录.txt";

// ── 艮卦·记忆注入（由 libclps.cpp 在每次 run_source() 前调用）─────────────
static ClpsMemory* g_clps_memory = nullptr;

void wan_wu_ku_set_memory(ClpsMemory* memory) {
    g_clps_memory = memory;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【厚土艮卦】— 艮卦(52)，文件读写 + 程序私有持久记忆
//  卦义：兼山，艮；君子以思不出其位（时止则止，止于其所）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// 记忆子组：六个函数，艮德三守（守界·守时·守正）
static void reg_gen_jiyi() {
    auto& ku = WanWuKu::instance();

    // 记忆读 "key" ["默认值"] → string（艮·察境，读取已止之知）
    ku.reg("厚土艮卦", "记忆读", WX::Huo, [](const std::vector<ClpsValue>& a) {
        if (!g_clps_memory)
            throw ClpsError("厚土艮卦·记忆读：当前无记忆上下文（未传入 ClpsMemory）");
        std::string key = a.empty() ? "" : to_str(a[0]);
        std::string def = a.size() >= 2 ? to_str(a[1]) : "";
        return ClpsValue(g_clps_memory->get(key, def));
    });

    // 记忆写 "key" "value"（艮·归位，将新知落定）
    ku.reg("厚土艮卦", "记忆写", WX::Tu, [](const std::vector<ClpsValue>& a) {
        if (!g_clps_memory)
            throw ClpsError("厚土艮卦·记忆写：当前无记忆上下文");
        std::string key = a.size() >= 1 ? to_str(a[0]) : "";
        std::string val = a.size() >= 2 ? to_str(a[1]) : "";
        g_clps_memory->set(key, val);
        return ClpsValue{};
    });

    // 记忆删 "key"（艮·净场，清除已止之迹）
    ku.reg("厚土艮卦", "记忆删", WX::Tu, [](const std::vector<ClpsValue>& a) {
        if (!g_clps_memory)
            throw ClpsError("厚土艮卦·记忆删：当前无记忆上下文");
        std::string key = a.empty() ? "" : to_str(a[0]);
        g_clps_memory->erase(key);
        return ClpsValue{};
    });

    // 记忆有 "key" → 1/0（艮·察境，无需记忆上下文时返回 0）
    ku.reg("厚土艮卦", "记忆有", WX::Jin, [](const std::vector<ClpsValue>& a) {
        if (!g_clps_memory) return ClpsValue(int64_t(0));
        std::string key = a.empty() ? "" : to_str(a[0]);
        return ClpsValue(static_cast<int64_t>(g_clps_memory->has(key) ? 1 : 0));
    });

    // 记忆存 → 1=成功 0=失败（艮·止于所，持久化热记忆到磁盘）
    ku.reg("厚土艮卦", "记忆存", WX::Jin, [](const std::vector<ClpsValue>&) {
        if (!g_clps_memory) return ClpsValue(int64_t(0));
        return ClpsValue(static_cast<int64_t>(g_clps_memory->save() ? 1 : 0));
    });

    // 记忆载 → 1=成功 0=失败（艮·归位，从磁盘恢复热记忆）
    ku.reg("厚土艮卦", "记忆载", WX::Jin, [](const std::vector<ClpsValue>&) {
        if (!g_clps_memory) return ClpsValue(int64_t(0));
        return ClpsValue(static_cast<int64_t>(g_clps_memory->load() ? 1 : 0));
    });
}

static void reg_hou_tu() {
    auto& ku = WanWuKu::instance();

    // 刻石记录 "内容" ["文件名"] — 追加写入文件
    ku.reg("厚土艮卦", "刻石记录", WX::Tu, [](const std::vector<ClpsValue>& args) {
        std::string text = args.empty() ? "" : to_str(args[0]);
        std::string path = (args.size() >= 2) ? to_str(args[1]) : DEFAULT_LOG;
        std::ofstream f(path, std::ios::app);
        if (!f) throw ClpsError("厚土艮卦：无法写入文件 " + path);
        f << text << "\n";
        return ClpsValue{};  // None
    });

    // 开卷展读 "文件名" — 读取文件全部内容，返回字符串
    ku.reg("厚土艮卦", "开卷展读", WX::Huo, [](const std::vector<ClpsValue>& args) {
        std::string path = args.empty() ? DEFAULT_LOG : to_str(args[0]);
        std::ifstream f(path);
        if (!f) throw ClpsError("厚土艮卦：无法读取文件 " + path);
        std::ostringstream ss;
        ss << f.rdbuf();
        return ClpsValue(ss.str());
    });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【厚土井卦】— 井卦(48)，标准输入
//  卦义：木上有水，井；君子以劳民劝相（汲取、给予）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

static void reg_jing() {
    auto& ku = WanWuKu::instance();

    // 汲水 ["提示"] — 读取一行标准输入
    ku.reg("厚土井卦", "汲水", WX::Huo, [](const std::vector<ClpsValue>& args) {
        if (!args.empty()) {
            std::cout << to_str(args[0]) << std::flush;
        }
        std::string line;
        std::getline(std::cin, line);
        return ClpsValue(line);
    });

    // 倾诉 "内容" — 输出到 stdout（不换行的兑卦变体）
    ku.reg("厚土井卦", "倾诉", WX::Tu, [](const std::vector<ClpsValue>& args) {
        for (auto& a : args) std::cout << to_str(a);
        std::cout << std::flush;
        return ClpsValue{};
    });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【地水比卦】— 比卦(8)，网络通信
//  卦义：地上有水，比；先王以建万国，亲诸侯
//  实现：POSIX TCP socket（单次发/收，无状态）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

static void reg_huo_di_jin() {
    auto& ku = WanWuKu::instance();
    namespace fs = std::filesystem;

    ku.reg("火地晋卦", "存在", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string path = a.empty() ? "" : to_str(a[0]);
        return ClpsValue(static_cast<int64_t>(fs::exists(path) ? 1 : 0));
    });
    ku.reg("火地晋卦", "大小", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string path = a.empty() ? "" : to_str(a[0]);
        std::error_code ec;
        auto sz = fs::file_size(path, ec);
        return ClpsValue(static_cast<int64_t>(ec ? -1 : (int64_t)sz));
    });
    ku.reg("火地晋卦", "列目录", WX::Huo, [](const std::vector<ClpsValue>& a) {
        std::string path = a.empty() ? "." : to_str(a[0]);
        std::string result;
        std::error_code ec;
        for (auto& entry : fs::directory_iterator(path, ec)) {
            if (!result.empty()) result += "\n";
            result += entry.path().filename().string();
        }
        return ClpsValue(result);
    });
    ku.reg("火地晋卦", "建目录", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string path = a.empty() ? "" : to_str(a[0]);
        std::error_code ec;
        fs::create_directories(path, ec);
        return ClpsValue(static_cast<int64_t>(ec ? 0 : 1));
    });
    ku.reg("火地晋卦", "删除", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string path = a.empty() ? "" : to_str(a[0]);
        std::error_code ec;
        auto n = fs::remove_all(path, ec);
        return ClpsValue(static_cast<int64_t>(ec ? -1 : (int64_t)n));
    });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【离为火卦】— 离(30)，字符串操作
//  卦义：离，丽也；日月丽乎天，百谷草木丽乎土
//  函数：大写/小写/去空/子串/查找/长度/替换
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【坤为地卦】— 坤(2)，INI 配置文件读写
//  卦义：地势坤，君子以厚德载物。
//  坤=地=承载；承载配置，厚德=宽容格式差异
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

struct KunConfig {
    std::mutex mx;
    // section → { key → value }
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> data;

    bool load(const std::string& path) {
        std::ifstream f(path);
        if (!f) return false;
        std::string line, section = "default";
        while (std::getline(f, line)) {
            // trim
            size_t s = line.find_first_not_of(" \t");
            size_t e = line.find_last_not_of(" \t\r\n");
            if (s == std::string::npos) continue;
            line = line.substr(s, e - s + 1);
            if (line[0]=='#'||line[0]==';') continue;
            if (line[0]=='[') {
                auto end = line.find(']');
                if (end != std::string::npos) section = line.substr(1, end - 1);
                continue;
            }
            auto eq = line.find('=');
            if (eq != std::string::npos) {
                std::string k = line.substr(0, eq);
                std::string v = line.substr(eq + 1);
                while (!k.empty() && k.back()==' ') k.pop_back();
                size_t vs = v.find_first_not_of(" ");
                if (vs != std::string::npos) v = v.substr(vs); else v = "";
                std::lock_guard<std::mutex> lk(mx);
                data[section][k] = v;
            }
        }
        return true;
    }

    bool save(const std::string& path) {
        std::ofstream f(path);
        if (!f) return false;
        std::lock_guard<std::mutex> lk(mx);
        for (auto& [sec, kv] : data) {
            f << "[" << sec << "]\n";
            for (auto& [k, v] : kv) f << k << "=" << v << "\n";
            f << "\n";
        }
        return true;
    }

    std::string get(const std::string& sec, const std::string& key) {
        std::lock_guard<std::mutex> lk(mx);
        auto sit = data.find(sec);
        if (sit == data.end()) return "";
        auto kit = sit->second.find(key);
        return kit == sit->second.end() ? "" : kit->second;
    }

    void set(const std::string& sec, const std::string& key, const std::string& val) {
        std::lock_guard<std::mutex> lk(mx);
        data[sec][key] = val;
    }
};

static KunConfig g_kun;

// 解析 "section.key" → (section, key)；无点号则 section="default"
static std::pair<std::string,std::string> split_kun_key(const std::string& sk) {
    auto dot = sk.find('.');
    if (dot == std::string::npos) return {"default", sk};
    return {sk.substr(0, dot), sk.substr(dot + 1)};
}


static void reg_kun_di() {
    auto& ku = WanWuKu::instance();

    // 载配 "filename" → 1=成功 0=失败（读入 INI 文件）
    ku.reg("坤为地卦", "载配", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string path = a.empty() ? "config.ini" : to_str(a[0]);
        return ClpsValue(static_cast<int64_t>(g_kun.load(path) ? 1 : 0));
    });

    // 读配 "section.key" → value（未找到返回""）
    ku.reg("坤为地卦", "读配", WX::Huo, [](const std::vector<ClpsValue>& a) {
        auto [sec, key] = split_kun_key(a.empty() ? "" : to_str(a[0]));
        return ClpsValue(g_kun.get(sec, key));
    });

    // 写配 "section.key" "value" → 1（仅改内存，需存配写入文件）
    ku.reg("坤为地卦", "写配", WX::Jin, [](const std::vector<ClpsValue>& a) {
        auto [sec, key] = split_kun_key(a.size() >= 1 ? to_str(a[0]) : "");
        std::string val = a.size() >= 2 ? to_str(a[1]) : "";
        g_kun.set(sec, key, val);
        return ClpsValue(int64_t(1));
    });

    // 存配 "filename" → 1=成功 0=失败（将内存配置写入文件）
    ku.reg("坤为地卦", "存配", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string path = a.empty() ? "config.ini" : to_str(a[0]);
        return ClpsValue(static_cast<int64_t>(g_kun.save(path) ? 1 : 0));
    });

    // 列配 → ClpsList of "section.key=value" 字符串
    ku.reg("坤为地卦", "列配", WX::Mu, [](const std::vector<ClpsValue>&) {
        auto lst = std::make_shared<ClpsList>();
        std::lock_guard<std::mutex> lk(g_kun.mx);
        for (auto& [sec, kv] : g_kun.data)
            for (auto& [k, v] : kv)
                lst->push_back(ClpsValue(sec + "." + k + "=" + v));
        return ClpsValue(lst);
    });

    // 清配 → 清空内存配置
    ku.reg("坤为地卦", "清配", WX::Tu, [](const std::vector<ClpsValue>&) {
        std::lock_guard<std::mutex> lk(g_kun.mx);
        g_kun.data.clear();
        return ClpsValue{};
    });
}

void init_mod_wenjian() {
    reg_gen_jiyi();   // 艮卦·记忆（六函数）
    reg_hou_tu();
    reg_jing();
    reg_huo_di_jin();
    reg_kun_di();
}

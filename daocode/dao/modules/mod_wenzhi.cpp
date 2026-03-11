// mod_wenzhi.cpp — 山水蒙·问智卦（☶☵·4）
//
// 卦象：山下有水，蒙以养正。童蒙求我，非我求童蒙。
// 宪法三律：
//   蒙律·发蒙  — 最多教三次（初筮告，再三渎则不告）
//   艮律·守记  — 成功模式存艮卦记忆，下次参考
//   讼律·预判  — 执行前静态审查，明显错误先拦
//
// 万物库注册：山水蒙卦
// 函数：问智·配  问智·问  问智·码  问智·生

#include "../wanwu.hpp"
#include "mod_helpers.hpp"
#include "../../libclps.hpp"
#include <curl/curl.h>
#include <sstream>
#include <string>
#include <vector>
#include <mutex>
#include <regex>

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  静态配置（艮德·守界）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
static std::string g_backend = "gemini";
static std::string g_api_key;
static std::string g_model;
static std::mutex  g_mu;

// 蒙卦律：最多教正三次（初筮告，再三渎则不告）
static constexpr int MENG_MAX_RETRIES = 3;

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  CLPS 宪法提示词（嵌入模块）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
static const char* CLPS_CONSTITUTION = R"CONST(
你是 CLPS（中文逻辑编程体系）代码生成器。只输出纯 CLPS 代码，不要任何解释或代码块标记。

【一、引号规则——最重要，违反必报错】
赋值目标（写入变量）：必须加引号    乾 "变量名" ...
读取变量的值：不加任何引号          震 "总和" + 计数
字符串字面量：加引号                乾 "状态" "成功"
数字字面量：不加引号                乾 "计数" 0

【二、每行只有一条卦命令，命令之间不嵌套】
乾 "变量" 值或变量名           → 赋值（只能赋单个值或单个变量名）
震 "变量" 运算符 值或变量名    → 算术修改（+ - * / %）
坎 "变量" 运算符 值或变量名    → 安全算术（同震，但不抛错）
兑 "变量"                     → 输出变量
艮 "变量" 比较符 值或变量名 {  → 条件（== != > < >= <=）
    ...卦命令...
} 否 {
    ...卦命令...
}
复 整数 {                     → 循环，$i = 当前轮次（从0开始）
    ...卦命令...
}
讼 "变量" 比较符 值            → 断言，失败则报错

【三、严禁写法（写了必报错）】
❌ 震 "变量"                  — 震后面必须有运算符，震不是读取命令
❌ 乾 "y" 震 "x"              — 乾的值不能是命令
❌ 乾 "余数" x % 3            — 乾不支持表达式，只能赋单值
❌ 乾 "sum" 0 震 "sum" + 1    — 每行只有一条命令
❌ 乾 "result" ...            — 变量名必须是中文，不能是英文

【四、计算后赋值的正确模式】
需要计算再存变量时，分两步：
  乾 "余数" 被除数   ← 第一步：复制原值
  坎 "余数" % 3      ← 第二步：计算修改

【五、完整示例】
# 累加1到10
乾 "总和" 0
复 10 {
    乾 "临时" $i
    震 "临时" + 1
    震 "总和" + 临时
}
艮 "总和" > 40 {
    乾 "结论" "大于40"
} 否 {
    乾 "结论" "不大于40"
}
兑 "总和"
兑 "结论"

# 判断整除
乾 "余数" 被除数
坎 "余数" % 3
艮 "余数" == 0 {
    乾 "答" "整除"
} 否 {
    乾 "答" "不整除"
}
兑 "答"
)CONST";

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  讼律·静态预审（执行前拦截明显错误）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
struct SongVerdict {
    bool ok;
    std::string reason; // 违宪原因（空=通过）
};

static SongVerdict song_precheck(const std::string& code) {
    std::istringstream ss(code);
    std::string line;
    int lineno = 0;
    while (std::getline(ss, line)) {
        ++lineno;
        // 跳过空行和注释
        size_t p = line.find_first_not_of(" \t");
        if (p == std::string::npos || line[p] == '#') continue;

        // 检测：英文变量名（乾/震/坎/兑 后紧跟 "英文"）
        // 模式：卦字 空格 "英文字母开头的变量名"
        static const std::regex en_var(
            u8R"((乾|震|坎|兑|讼|艮|恒|小畜)\s+"[a-zA-Z][^"]*")",
            std::regex::ECMAScript);
        if (std::regex_search(line, en_var)) {
            return {false, "第" + std::to_string(lineno) + "行：变量名必须用中文，不能用英文"};
        }

        // 检测：震 "变量" 行尾没有运算符（震被当 getter 用）
        // 模式：行首是 震，后面有 "...", 之后没有 +/-/*///%
        static const std::regex zhen_no_op(
            u8R"(^\s*震\s+"[^"]+"\s*$)",
            std::regex::ECMAScript);
        if (std::regex_search(line, zhen_no_op)) {
            return {false, "第" + std::to_string(lineno) + "行：震后必须有运算符（+ - * / %），震不是读取命令"};
        }

        // 检测：乾 "变量" ... 震（乾的值是命令）
        static const std::regex qian_cmd(
            u8R"(乾\s+"[^"]+"\s+震)",
            std::regex::ECMAScript);
        if (std::regex_search(line, qian_cmd)) {
            return {false, "第" + std::to_string(lineno) + "行：乾的值不能是命令（不能写 乾 \"x\" 震 ...）"};
        }
    }
    return {true, ""};
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  艮律·记忆接口（守记成功模式）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
static ClpsMemory g_memory = ClpsMemory::for_scope("问智·学习");
static bool       g_memory_loaded = false;

static void mem_ensure_loaded() {
    if (!g_memory_loaded) {
        g_memory.load();
        g_memory_loaded = true;
    }
}

// 任务摘要（取前40字节作为key）
static std::string task_key(const std::string& task) {
    std::string k = task.substr(0, 40);
    // 替换非法 key 字符
    for (auto& c : k) if (c == '\n' || c == '=' || c == '\r') c = '_';
    return "模式·" + k;
}

static std::string mem_get_pattern(const std::string& task) {
    mem_ensure_loaded();
    return g_memory.get(task_key(task));
}

static void mem_save_pattern(const std::string& task, const std::string& code) {
    mem_ensure_loaded();
    g_memory.set(task_key(task), code);
    g_memory.save();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  libcurl 辅助
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
static size_t curl_write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    static_cast<std::string*>(userdata)->append(ptr, size * nmemb);
    return size * nmemb;
}

static std::string json_esc(const std::string& s) {
    std::string r;
    for (unsigned char c : s) {
        switch (c) {
            case '"':  r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\n': r += "\\n";  break;
            case '\r': r += "\\r";  break;
            case '\t': r += "\\t";  break;
            default:
                if (c < 0x20) { // 其他控制字符
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    r += buf;
                } else {
                    r += (char)c;
                }
        }
    }
    return r;
}

static std::string json_extract_text(const std::string& json) {
    // 提取第一个 "text": "..." 字段
    const std::string key = "\"text\"";
    size_t p = json.find(key);
    if (p == std::string::npos) return "";
    p = json.find(':', p + key.size());
    if (p == std::string::npos) return "";
    p = json.find('"', p + 1);
    if (p == std::string::npos) return "";
    std::string result;
    size_t i = p + 1;
    while (i < json.size()) {
        if (json[i] == '"' && (i == 0 || json[i-1] != '\\')) break;
        if (json[i] == '\\' && i + 1 < json.size()) {
            switch (json[++i]) {
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case '"': result += '"';  break;
                case '\\':result += '\\'; break;
                default:  result += json[i];
            }
        } else {
            result += json[i];
        }
        ++i;
    }
    return result;
}

// 对话轮次
struct MengTurn {
    std::string role;    // "user" / "assistant" / "model"
    std::string content;
};

static std::string http_post(const std::string& url,
                              const std::string& body,
                              const std::vector<std::string>& hdrs) {
    CURL* curl = curl_easy_init();
    if (!curl) throw ClpsError("问智：libcurl 初始化失败");
    std::string resp;
    struct curl_slist* hlist = nullptr;
    for (auto& h : hdrs) hlist = curl_slist_append(hlist, h.c_str());
    curl_easy_setopt(curl, CURLOPT_URL,           url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,    body.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,    hlist);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &resp);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,       30L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    CURLcode rc = curl_easy_perform(curl);
    curl_slist_free_all(hlist);
    curl_easy_cleanup(curl);
    if (rc != CURLE_OK)
        throw ClpsError(std::string("问智·网：") + curl_easy_strerror(rc));
    return resp;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  各后端·多轮对话
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
static std::string call_gemini_chat(const std::string& system,
                                     const std::vector<MengTurn>& turns) {
    std::string model = g_model.empty() ? "gemini-2.5-flash" : g_model;
    std::string url   = "https://generativelanguage.googleapis.com/v1beta/models/"
                      + model + ":generateContent?key=" + g_api_key;

    // 构建 contents 数组
    std::string contents = "[";
    for (size_t i = 0; i < turns.size(); ++i) {
        if (i) contents += ",";
        // Gemini：user/model
        std::string role = (turns[i].role == "assistant") ? "model" : "user";
        contents += "{\"role\":\"" + role + "\","
                    "\"parts\":[{\"text\":\"" + json_esc(turns[i].content) + "\"}]}";
    }
    contents += "]";

    std::string body = "{\"system_instruction\":{\"parts\":[{\"text\":\""
                     + json_esc(system) + "\"}]},"
                     "\"contents\":" + contents + ","
                     "\"generationConfig\":{\"temperature\":0.1}}";

    auto resp = http_post(url, body, {"Content-Type: application/json"});
    std::string text = json_extract_text(resp);
    if (text.empty())
        throw ClpsError("问智·Gemini：响应解析失败\n" + resp.substr(0, 400));
    return text;
}

static std::string call_claude_chat(const std::string& system,
                                     const std::vector<MengTurn>& turns) {
    std::string model = g_model.empty() ? "claude-sonnet-4-6" : g_model;

    std::string msgs = "[";
    for (size_t i = 0; i < turns.size(); ++i) {
        if (i) msgs += ",";
        std::string role = turns[i].role; // user/assistant
        msgs += "{\"role\":\"" + role + "\","
                "\"content\":\"" + json_esc(turns[i].content) + "\"}";
    }
    msgs += "]";

    std::string body = "{\"model\":\"" + model + "\","
                       "\"max_tokens\":1024,"
                       "\"system\":\"" + json_esc(system) + "\","
                       "\"messages\":" + msgs + "}";

    auto resp = http_post("https://api.anthropic.com/v1/messages", body, {
        "Content-Type: application/json",
        "x-api-key: " + g_api_key,
        "anthropic-version: 2023-06-01"
    });
    std::string text = json_extract_text(resp);
    if (text.empty())
        throw ClpsError("问智·Claude：响应解析失败\n" + resp.substr(0, 400));
    return text;
}

static std::string call_ollama_chat(const std::string& system,
                                     const std::vector<MengTurn>& turns) {
    std::string model = g_model.empty() ? "llama3" : g_model;
    // Ollama：把对话历史拼接成一段 prompt
    std::string prompt = system + "\n\n";
    for (auto& t : turns) {
        prompt += (t.role == "user" ? "用户：" : "助手：");
        prompt += t.content + "\n";
    }

    std::string body = "{\"model\":\"" + model + "\","
                       "\"prompt\":\"" + json_esc(prompt) + "\","
                       "\"stream\":false}";
    auto resp = http_post("http://localhost:11434/api/generate", body,
                          {"Content-Type: application/json"});
    // Ollama 返回 "response" 字段
    const std::string key = "\"response\"";
    size_t p = resp.find(key);
    if (p == std::string::npos) throw ClpsError("问智·Ollama：响应解析失败");
    // 复用 json_extract_text（response 字段也是字符串）
    // 临时替换为 text 字段格式来复用
    std::string faked = resp.substr(p);
    faked[1] = 't'; faked[2] = 'e'; faked[3] = 'x'; faked[4] = 't';
    return json_extract_text(faked);
}

// ── 统一多轮调用 ─────────────────────────────────────
static std::string llm_chat(const std::string& system,
                              const std::vector<MengTurn>& turns) {
    std::lock_guard<std::mutex> lk(g_mu);
    if (g_backend == "gemini") return call_gemini_chat(system, turns);
    if (g_backend == "claude") return call_claude_chat(system, turns);
    if (g_backend == "ollama") return call_ollama_chat(system, turns);
    throw ClpsError("问智：未知后端 \"" + g_backend + "\"（支持 gemini/claude/ollama）");
}

// 单轮便利封装
static std::string llm_once(const std::string& system, const std::string& msg) {
    return llm_chat(system, {{"user", msg}});
}

// 清理 markdown 代码块标记
static std::string strip_markdown(const std::string& s) {
    std::istringstream ss(s);
    std::string line, result;
    while (std::getline(ss, line)) {
        std::string t = line;
        size_t p = t.find_first_not_of(" \t");
        if (p != std::string::npos && t[p] == '`') continue;
        if (!result.empty()) result += '\n';
        result += line;
    }
    return result;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  蒙卦·发蒙核心（多轮教正，最多三次）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 返回：{成功的CLPS代码, 执行输出}
// 失败则抛 ClpsError
static std::pair<std::string,std::string>
meng_fa_meng(const std::string& task) {

    // ── 艮律·快速路径：记忆命中则直接执行，跳过 LLM ──
    {
        std::lock_guard<std::mutex> lk(g_mu);
        std::string cached = mem_get_pattern(task);
        if (!cached.empty()) {
            std::string output;
            auto r = clps_exec_string(cached, {},
                [&](const std::string& s){ output += s + "\n"; });
            if (r) return {cached, output}; // 命中且成功，直接返回
            // 缓存失效（可能语言版本变化），清除后走正常流程
            g_memory.erase(task_key(task));
            g_memory.save();
        }
    }

    // ── 艮律：记忆未命中，生成时把近似模式作示例注入 ──
    std::string memory_hint;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        memory_hint = mem_get_pattern(task);
    }

    std::string system = CLPS_CONSTITUTION;
    if (!memory_hint.empty()) {
        system += "\n\n【记忆参考·相近成功模式】\n"
               + memory_hint
               + "\n（以上为参考，根据当前任务灵活调整）\n";
    }

    // 对话历史（蒙卦·师生问答）
    std::vector<MengTurn> history;
    history.push_back({"user", task});

    std::string last_error;

    // 蒙律：最多三次（初筮告，再三渎则不告）
    for (int attempt = 1; attempt <= MENG_MAX_RETRIES; ++attempt) {

        // 生成代码
        std::string code = llm_chat(system, history);
        code = strip_markdown(code);

        // 讼律·预审
        auto verdict = song_precheck(code);
        if (!verdict.ok) {
            // 预审失败：告知 LLM 具体违宪原因，加入历史
            history.push_back({"assistant", code});
            history.push_back({"user",
                "代码违反 CLPS 宪法：" + verdict.reason +
                "\n请只修正这一个错误，重新输出完整的 CLPS 代码。"});
            last_error = verdict.reason;
            continue;
        }

        // 执行
        std::string output;
        auto result = clps_exec_string(code, {},
            [&](const std::string& s){ output += s + "\n"; });

        if (result) {
            // ── 成功：艮律·存记忆 ───────────────────
            std::lock_guard<std::mutex> lk(g_mu);
            mem_save_pattern(task, code);
            return {code, output};
        }

        // 执行失败：把错误带入下轮（蒙卦·发蒙）
        last_error = result.error;
        history.push_back({"assistant", code});
        history.push_back({"user",
            "执行报错：" + result.error +
            "\n请分析错误原因，修正后只输出完整的 CLPS 代码。"});
    }

    // 三次皆失：渎则不告，抛错
    throw ClpsError(
        "问智·蒙卦：三次发蒙皆失，任务无法完成。\n"
        "最后错误：" + last_error);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  万物库注册
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void init_mod_wenzhi() {
    auto& ku = WanWuKu::instance();

    // ── 问智·配  "后端" "key" ["模型"] ─────────────
    // 配置 LLM 后端和 API key
    // 后端：gemini / claude / ollama
    ku.reg("山水蒙卦", "问智·配", WX::Tu,
        [](const std::vector<ClpsValue>& args) -> ClpsValue {
            if (args.size() < 2)
                throw ClpsError("问智·配：需要 后端 key [模型]");
            std::lock_guard<std::mutex> lk(g_mu);
            g_backend = to_str(args[0]);
            g_api_key = to_str(args[1]);
            if (args.size() >= 3) g_model = to_str(args[2]);
            g_memory_loaded = false; // 重置记忆加载状态
            return ClpsValue{};
        });

    // ── 问智·问  "问题" ─────────────────────────────
    // 直接问 LLM，返回自然语言回答（无 CLPS 宪法约束）
    // 返回：字符串（火·Huo）
    ku.reg("山水蒙卦", "问智·问", WX::Huo,
        [](const std::vector<ClpsValue>& args) -> ClpsValue {
            if (args.empty())
                throw ClpsError("问智·问：需要问题参数");
            std::string q = to_str(args[0]);
            std::string sys = "你是知识渊博的助手，用中文简洁回答。";
            return ClpsValue(llm_once(sys, q));
        });

    // ── 问智·码  "任务描述" ──────────────────────────
    // 生成 CLPS 代码（带蒙卦发蒙，但不执行）
    // 返回：CLPS 代码字符串（火·Huo）
    ku.reg("山水蒙卦", "问智·码", WX::Huo,
        [](const std::vector<ClpsValue>& args) -> ClpsValue {
            if (args.empty())
                throw ClpsError("问智·码：需要任务描述参数");
            std::string task = to_str(args[0]);
            auto [code, _] = meng_fa_meng(task);
            return ClpsValue(code);
        });

    // ── 问智·生  "任务描述" ──────────────────────────
    // 完整闭环：自然语言 → CLPS（蒙卦·三次发蒙）→ 执行 → 结果
    // 成功模式存艮卦记忆，下次同类任务参考
    // 返回：执行输出字符串（火·Huo）
    ku.reg("山水蒙卦", "问智·生", WX::Huo,
        [](const std::vector<ClpsValue>& args) -> ClpsValue {
            if (args.empty())
                throw ClpsError("问智·生：需要任务描述参数");
            std::string task = to_str(args[0]);
            auto [code, output] = meng_fa_meng(task);
            (void)code; // 代码已存艮卦，此处只返回输出
            return ClpsValue(output.empty() ? "(无输出)" : output);
        });
}

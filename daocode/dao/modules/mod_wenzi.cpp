// mod_wenzi.cpp — 道·码万物库子模块
#include "../wanwu.hpp"
#include "mod_helpers.hpp"
#include <string>
#include <sstream>
#include <algorithm>
#include <regex>
#include <iomanip>
#include <cctype>

static void reg_li_wei_huo() {
    auto& ku = WanWuKu::instance();

    ku.reg("离为火卦", "大写", WX::Huo, [](const std::vector<ClpsValue>& a) {
        std::string s = a.empty() ? "" : to_str(a[0]);
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
        return ClpsValue(s);
    });
    ku.reg("离为火卦", "小写", WX::Huo, [](const std::vector<ClpsValue>& a) {
        std::string s = a.empty() ? "" : to_str(a[0]);
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return ClpsValue(s);
    });
    ku.reg("离为火卦", "去空", WX::Huo, [](const std::vector<ClpsValue>& a) {
        std::string s = a.empty() ? "" : to_str(a[0]);
        size_t l = s.find_first_not_of(" \t\r\n");
        if (l == std::string::npos) return ClpsValue(std::string(""));
        size_t r = s.find_last_not_of(" \t\r\n");
        return ClpsValue(s.substr(l, r - l + 1));
    });
    ku.reg("离为火卦", "子串", WX::Huo, [](const std::vector<ClpsValue>& a) {
        std::string s = a.empty() ? "" : to_str(a[0]);
        int64_t start = a.size() >= 2 ? to_int(a[1]) : 0;
        int64_t len   = a.size() >= 3 ? to_int(a[2]) : (int64_t)s.size();
        if (start < 0) start = 0;
        if (start >= (int64_t)s.size()) return ClpsValue(std::string(""));
        return ClpsValue(s.substr((size_t)start, (size_t)len));
    });
    ku.reg("离为火卦", "查找", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string s   = a.size() >= 1 ? to_str(a[0]) : "";
        std::string sub = a.size() >= 2 ? to_str(a[1]) : "";
        size_t pos = s.find(sub);
        return ClpsValue(static_cast<int64_t>(pos == std::string::npos ? -1 : (int64_t)pos));
    });
    ku.reg("离为火卦", "长度", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string s = a.empty() ? "" : to_str(a[0]);
        return ClpsValue(static_cast<int64_t>(s.size()));
    });
    ku.reg("离为火卦", "替换", WX::Huo, [](const std::vector<ClpsValue>& a) {
        std::string s    = a.size() >= 1 ? to_str(a[0]) : "";
        std::string from = a.size() >= 2 ? to_str(a[1]) : "";
        std::string to_s = a.size() >= 3 ? to_str(a[2]) : "";
        if (from.empty()) return ClpsValue(s);
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.size(), to_s);
            pos += to_s.size();
        }
        return ClpsValue(s);
    });

    // 转文 — 将任意 ClpsValue 转为其字符串表示（repr 去引号版）
    // 卦义：离·附丽，使万象皆可言说（火=字符串·可传播之象）
    // 整数 42 → "42"，浮点 3.14 → "3.14"，字符串不变，None → "虚"
    ku.reg("离为火卦", "转文", WX::Huo, [](const std::vector<ClpsValue>& a) {
        if (a.empty()) return ClpsValue(std::string(""));
        const ClpsValue& v = a[0];
        if (std::holds_alternative<std::monostate>(v.data))
            return ClpsValue(std::string("虚"));
        if (auto* p = std::get_if<int64_t>(&v.data))
            return ClpsValue(std::to_string(*p));
        if (auto* p = std::get_if<double>(&v.data)) {
            std::ostringstream oss; oss << *p;
            return ClpsValue(oss.str());
        }
        if (auto* p = std::get_if<std::string>(&v.data))
            return ClpsValue(*p);
        if (auto* p = std::get_if<std::shared_ptr<ClpsList>>(&v.data))
            return ClpsValue(std::string("[列表 ") +
                std::to_string(*p ? (*p)->size() : 0) + "项]");
        return ClpsValue(std::string("{图}"));
    });

    // 拼 — 将两个字符串（或任意值转文后）拼接
    // 卦义：离·重明，两光相续，丽泽相连
    // 用法：震 "s" 【离为火卦】 拼 "前缀" 其他变量
    ku.reg("离为火卦", "拼", WX::Huo, [](const std::vector<ClpsValue>& a) {
        std::string result;
        for (const auto& v : a) result += to_str(v);
        return ClpsValue(result);
    });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【雷火丰卦】— 丰(55)，格式化输出
//  卦义：丰，大也；君子以折狱致刑
//  函数：格式化（sprintf风格，只支持%d%f%s%g）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

static void reg_lei_huo_feng() {
    auto& ku = WanWuKu::instance();

    ku.reg("雷火丰卦", "格式化", WX::Huo, [](const std::vector<ClpsValue>& a) {
        if (a.empty()) return ClpsValue(std::string(""));
        std::string fmt = to_str(a[0]);
        std::string result;
        result.reserve(fmt.size() + 64);
        size_t arg_idx = 1;
        for (size_t i = 0; i < fmt.size(); ) {
            if (fmt[i] != '%') { result += fmt[i++]; continue; }
            i++;
            if (i >= fmt.size()) { result += '%'; break; }
            char spec = fmt[i++];
            if (spec == '%') { result += '%'; continue; }
            // 简单格式：%d %i %f %g %s
            if (arg_idx >= a.size()) { result += "?"; continue; }
            const ClpsValue& arg = a[arg_idx++];
            char buf[64];
            if (spec == 'd' || spec == 'i') {
                snprintf(buf, sizeof(buf), "%lld", (long long)to_int(arg));
            } else if (spec == 'f') {
                double v = 0;
                if (auto* p = std::get_if<double>(&arg.data)) v = *p;
                else if (auto* p = std::get_if<int64_t>(&arg.data)) v = (double)*p;
                snprintf(buf, sizeof(buf), "%f", v);
            } else if (spec == 'g') {
                double v = 0;
                if (auto* p = std::get_if<double>(&arg.data)) v = *p;
                else if (auto* p = std::get_if<int64_t>(&arg.data)) v = (double)*p;
                snprintf(buf, sizeof(buf), "%g", v);
            } else if (spec == 's') {
                snprintf(buf, sizeof(buf), "%s", to_str(arg).c_str());
            } else {
                buf[0] = '%'; buf[1] = spec; buf[2] = '\0';
            }
            result += buf;
        }
        return ClpsValue(result);
    });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【火雷噬嗑卦】— 噬嗑(21)，正则表达式
//  卦义：颐中有物，噬嗑；先王以明罚敕法
//  函数：匹配/搜寻/替换/分割
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

static void reg_huo_lei_shi_ke() {
    auto& ku = WanWuKu::instance();

    ku.reg("火雷噬嗑卦", "匹配", WX::Jin, [](const std::vector<ClpsValue>& a) {
        if (a.size() < 2) return ClpsValue(int64_t(0));
        try {
            std::regex re(to_str(a[0]));
            std::string text = to_str(a[1]);
            return ClpsValue(static_cast<int64_t>(
                std::regex_search(text, re) ? 1 : 0));
        } catch (...) { return ClpsValue(int64_t(0)); }
    });
    ku.reg("火雷噬嗑卦", "搜寻", WX::Huo, [](const std::vector<ClpsValue>& a) {
        if (a.size() < 2) return ClpsValue(std::string(""));
        try {
            std::regex re(to_str(a[0]));
            std::string text = to_str(a[1]);
            std::smatch m;
            if (std::regex_search(text, m, re)) return ClpsValue(m[0].str());
        } catch (...) {}
        return ClpsValue(std::string(""));
    });
    ku.reg("火雷噬嗑卦", "替换", WX::Huo, [](const std::vector<ClpsValue>& a) {
        if (a.size() < 3) return ClpsValue(std::string(""));
        try {
            std::regex re(to_str(a[0]));
            std::string text    = to_str(a[1]);
            std::string replace = to_str(a[2]);
            return ClpsValue(std::regex_replace(text, re, replace));
        } catch (...) { return ClpsValue(a.size() > 1 ? to_str(a[1]) : std::string("")); }
    });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【山火贲卦】— 贲(22)，JSON序列化
//  卦义：山下有火，贲；君子以明庶政，无敢折狱
//  函数：序列化（ClpsValue→JSON字符串）/ 反序列化（JSON→ClpsValue）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// 轻量级 JSON 序列化（无依赖）

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【屯卦辞海】— 屯(3)，动态近义词表
//  卦义：屯，难生。万物萌芽，词语寻形。
//  函数：
//    辞海·添 "族名" "词"  → 土（记入动态表并持久化到文件）
//    辞海·查 "词"         → 火（返回该词所属族名，未知返回空串）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#include "../cihai.hpp"

static void reg_tun_cihai() {
    auto& ku = WanWuKu::instance();

    // 辞海·添 "族名" "词" → 土
    ku.reg("屯卦辞海", "辞海·添", WX::Tu, [](const std::vector<ClpsValue>& a) -> ClpsValue {
        if (a.size() < 2)
            throw ClpsError("辞海·添：需要 族名 词 两个参数");
        std::string clan = to_str(a[0]);
        std::string word = to_str(a[1]);
        bool ok = cihai_add_word(clan, word);
        if (!ok) throw ClpsError("辞海·添：未知族名「" + clan + "」，可用：乾兑震坎离巽艮坤…");
        return ClpsValue(); // 土·无返回值
    });

    // 辞海·查 "词" → 火（族名字符串）
    ku.reg("屯卦辞海", "辞海·查", WX::Huo, [](const std::vector<ClpsValue>& a) -> ClpsValue {
        if (a.empty()) throw ClpsError("辞海·查：需要一个词");
        std::string word = to_str(a[0]);
        // 先查静态表
        TK tk = cihai_lookup(word);
        if (tk == TK::END)
            return ClpsValue(std::string("")); // 未知
        return ClpsValue(cihai_tk_to_clan(tk));
    });
}

void init_mod_wenzi() {
    reg_li_wei_huo();
    reg_lei_huo_feng();
    reg_huo_lei_shi_ke();
    reg_tun_cihai();
}

// mod_geshi.cpp — 道·码万物库子模块
#include "../wanwu.hpp"
#include "mod_helpers.hpp"
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <deque>

static std::string clps_to_json(const ClpsValue& v) {
    if (std::holds_alternative<std::monostate>(v.data)) return "null";
    if (auto* x = std::get_if<int64_t>(&v.data)) return std::to_string(*x);
    if (auto* x = std::get_if<double>(&v.data)) {
        char b[32]; snprintf(b, 32, "%g", *x); return std::string(b);
    }
    if (auto* x = std::get_if<std::string>(&v.data)) {
        std::string r = "\"";
        for (char c : *x) {
            if (c == '"')  r += "\\\"";
            else if (c == '\\') r += "\\\\";
            else if (c == '\n') r += "\\n";
            else if (c == '\r') r += "\\r";
            else if (c == '\t') r += "\\t";
            else r += c;
        }
        r += '"'; return r;
    }
    if (auto* x = std::get_if<std::shared_ptr<ClpsList>>(&v.data)) {
        std::string r = "[";
        for (size_t i = 0; i < (*x)->size(); ++i) {
            if (i) r += ",";
            r += clps_to_json((**x)[i]);
        }
        r += "]"; return r;
    }
    if (auto* x = std::get_if<std::shared_ptr<ClpsMap>>(&v.data)) {
        std::string r = "{";
        bool first = true;
        for (auto& [k, v2] : **x) {
            if (!first) r += ",";
            r += "\"" + k + "\":" + clps_to_json(v2);
            first = false;
        }
        r += "}"; return r;
    }
    return "null";
}

// 轻量级 JSON 反序列化（只支持 null/bool/number/string）
static ClpsValue json_to_clps(const std::string& s, size_t& i) {
    while (i < s.size() && isspace(s[i])) i++;
    if (i >= s.size()) return ClpsValue{};
    char c = s[i];
    if (c == 'n') { i += 4; return ClpsValue{}; }
    if (c == 't') { i += 4; return ClpsValue(int64_t(1)); }
    if (c == 'f') { i += 5; return ClpsValue(int64_t(0)); }
    if (c == '"') {
        i++;
        std::string r;
        while (i < s.size() && s[i] != '"') {
            if (s[i] == '\\' && i+1 < s.size()) {
                i++;
                switch (s[i]) {
                    case 'n': r += '\n'; break;
                    case 'r': r += '\r'; break;
                    case 't': r += '\t'; break;
                    default:  r += s[i]; break;
                }
            } else { r += s[i]; }
            i++;
        }
        if (i < s.size()) i++; // consume closing "
        return ClpsValue(r);
    }
    if (c == '-' || isdigit(c)) {
        size_t start = i;
        bool is_float = false;
        if (s[i] == '-') i++;
        while (i < s.size() && isdigit(s[i])) i++;
        if (i < s.size() && (s[i] == '.' || s[i] == 'e' || s[i] == 'E')) {
            is_float = true;
            while (i < s.size() && (isdigit(s[i]) || s[i]=='.' || s[i]=='e' || s[i]=='E' || s[i]=='+' || s[i]=='-')) i++;
        }
        std::string num = s.substr(start, i - start);
        if (is_float) return ClpsValue(std::stod(num));
        return ClpsValue((int64_t)std::stoll(num));
    }
    i++; // skip unknown
    return ClpsValue{};
}



static void reg_shan_huo_bi() {
    auto& ku = WanWuKu::instance();

    ku.reg("山火贲卦", "序列化", WX::Huo, [](const std::vector<ClpsValue>& a) {
        if (a.empty()) return ClpsValue(std::string("null"));
        return ClpsValue(clps_to_json(a[0]));
    });
    ku.reg("山火贲卦", "反序列化", WX::Tu, [](const std::vector<ClpsValue>& a) {
        if (a.empty()) return ClpsValue{};
        std::string s = to_str(a[0]);
        size_t i = 0;
        return json_to_clps(s, i);
    });
}
static void reg_shui_huo_jiji() {
    auto& ku = WanWuKu::instance();

    // 与 a b → a & b
    ku.reg("水火既济卦", "与", WX::Jin, [](const std::vector<ClpsValue>& a) {
        int64_t x = a.size() >= 1 ? to_int(a[0]) : 0;
        int64_t y = a.size() >= 2 ? to_int(a[1]) : 0;
        return ClpsValue(x & y);
    });
    // 或 a b → a | b
    ku.reg("水火既济卦", "或", WX::Jin, [](const std::vector<ClpsValue>& a) {
        int64_t x = a.size() >= 1 ? to_int(a[0]) : 0;
        int64_t y = a.size() >= 2 ? to_int(a[1]) : 0;
        return ClpsValue(x | y);
    });
    // 异 a b → a ^ b
    ku.reg("水火既济卦", "异", WX::Jin, [](const std::vector<ClpsValue>& a) {
        int64_t x = a.size() >= 1 ? to_int(a[0]) : 0;
        int64_t y = a.size() >= 2 ? to_int(a[1]) : 0;
        return ClpsValue(x ^ y);
    });
    // 非 a → ~a（按位取反）
    ku.reg("水火既济卦", "非", WX::Jin, [](const std::vector<ClpsValue>& a) {
        int64_t x = a.empty() ? 0 : to_int(a[0]);
        return ClpsValue(~x);
    });
    // 左 a n → a << n
    ku.reg("水火既济卦", "左", WX::Jin, [](const std::vector<ClpsValue>& a) {
        int64_t x = a.size() >= 1 ? to_int(a[0]) : 0;
        int64_t n = a.size() >= 2 ? to_int(a[1]) : 0;
        return ClpsValue((n >= 0 && n < 64) ? (x << n) : int64_t(0));
    });
    // 右 a n → a >> n（逻辑右移，用 uint64_t）
    ku.reg("水火既济卦", "右", WX::Jin, [](const std::vector<ClpsValue>& a) {
        int64_t x = a.size() >= 1 ? to_int(a[0]) : 0;
        int64_t n = a.size() >= 2 ? to_int(a[1]) : 0;
        return ClpsValue(static_cast<int64_t>((n >= 0 && n < 64)
            ? (static_cast<uint64_t>(x) >> n) : uint64_t(0)));
    });
    // 算右 a n → a >> n（算术右移，保持符号位）
    ku.reg("水火既济卦", "算右", WX::Jin, [](const std::vector<ClpsValue>& a) {
        int64_t x = a.size() >= 1 ? to_int(a[0]) : 0;
        int64_t n = a.size() >= 2 ? to_int(a[1]) : 0;
        return ClpsValue((n >= 0 && n < 64) ? (x >> n) : (x < 0 ? int64_t(-1) : int64_t(0)));
    });
    // 数一 a → popcount（1位数量）
    ku.reg("水火既济卦", "数一", WX::Jin, [](const std::vector<ClpsValue>& a) {
        uint64_t x = (uint64_t)(a.empty() ? 0 : to_int(a[0]));
        return ClpsValue(static_cast<int64_t>(__builtin_popcountll(x)));
    });
    // 前零 a → count leading zeros（以32位整数）
    ku.reg("水火既济卦", "前零", WX::Jin, [](const std::vector<ClpsValue>& a) {
        uint32_t x = (uint32_t)(a.empty() ? 0 : to_int(a[0]));
        return ClpsValue(static_cast<int64_t>(x == 0 ? 32 : __builtin_clz(x)));
    });
    // 后零 a → count trailing zeros
    ku.reg("水火既济卦", "后零", WX::Jin, [](const std::vector<ClpsValue>& a) {
        uint32_t x = (uint32_t)(a.empty() ? 0 : to_int(a[0]));
        return ClpsValue(static_cast<int64_t>(x == 0 ? 32 : __builtin_ctz(x)));
    });
    // 掩 a mask → (a & mask) != 0 ? 1 : 0  掩码测试
    ku.reg("水火既济卦", "掩", WX::Jin, [](const std::vector<ClpsValue>& a) {
        int64_t x    = a.size() >= 1 ? to_int(a[0]) : 0;
        int64_t mask = a.size() >= 2 ? to_int(a[1]) : 0;
        return ClpsValue(int64_t((x & mask) != 0 ? 1 : 0));
    });
    // 置位 a n → a | (1<<n)  置第n位
    ku.reg("水火既济卦", "置位", WX::Jin, [](const std::vector<ClpsValue>& a) {
        int64_t x = a.size() >= 1 ? to_int(a[0]) : 0;
        int64_t n = a.size() >= 2 ? to_int(a[1]) : 0;
        return ClpsValue(x | (int64_t(1) << n));
    });
    // 清位 a n → a & ~(1<<n)  清第n位
    ku.reg("水火既济卦", "清位", WX::Jin, [](const std::vector<ClpsValue>& a) {
        int64_t x = a.size() >= 1 ? to_int(a[0]) : 0;
        int64_t n = a.size() >= 2 ? to_int(a[1]) : 0;
        return ClpsValue(x & ~(int64_t(1) << n));
    });
    // 测位 a n → (a >> n) & 1
    ku.reg("水火既济卦", "测位", WX::Jin, [](const std::vector<ClpsValue>& a) {
        int64_t x = a.size() >= 1 ? to_int(a[0]) : 0;
        int64_t n = a.size() >= 2 ? to_int(a[1]) : 0;
        return ClpsValue(int64_t((x >> n) & 1));
    });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【风泽中孚卦】— 中孚(61)，数据校验
//  卦义：中孚，豚鱼吉，利涉大川，利贞。
//  中孚=诚信=数据完整性；校验和如君子之信，不可欺
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// CRC-8 (多项式 0x07，无反转)
static uint8_t crc8_byte(uint8_t crc, uint8_t byte) {
    crc ^= byte;
    for (int i = 0; i < 8; i++) crc = (crc & 0x80) ? ((crc << 1) ^ 0x07) : (crc << 1);
    return crc;
}

// CRC-16/CCITT-FALSE (多项式 0x1021, init 0xFFFF, 无反转)
static uint16_t crc16_byte(uint16_t crc, uint8_t byte) {
    crc ^= (uint16_t)byte << 8;
    for (int i = 0; i < 8; i++) crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1);
    return crc;
}

// CRC-32 (IEEE 802.3, 多项式 0xEDB88320, 反转输入/输出)
static uint32_t crc32_compute(const std::string& data) {
    uint32_t crc = 0xFFFFFFFF;
    for (unsigned char c : data) {
        crc ^= c;
        for (int i = 0; i < 8; i++) crc = (crc & 1) ? ((crc >> 1) ^ 0xEDB88320) : (crc >> 1);
    }
    return crc ^ 0xFFFFFFFF;
}

// Adler-32
static uint32_t adler32_compute(const std::string& data) {
    uint32_t s1 = 1, s2 = 0;
    for (unsigned char c : data) { s1 = (s1 + c) % 65521; s2 = (s2 + s1) % 65521; }
    return (s2 << 16) | s1;
}

static void reg_feng_ze_zhongfu() {
    auto& ku = WanWuKu::instance();

    // crc8 "data" → int  CRC-8校验值
    ku.reg("风泽中孚卦", "crc8", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string s = a.empty() ? "" : to_str(a[0]);
        uint8_t crc = 0;
        for (unsigned char c : s) crc = crc8_byte(crc, c);
        return ClpsValue(int64_t(crc));
    });

    // crc16 "data" → int  CRC-16/CCITT-FALSE
    ku.reg("风泽中孚卦", "crc16", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string s = a.empty() ? "" : to_str(a[0]);
        uint16_t crc = 0xFFFF;
        for (unsigned char c : s) crc = crc16_byte(crc, c);
        return ClpsValue(int64_t(crc));
    });

    // crc32 "data" → int  CRC-32 (IEEE 802.3)
    ku.reg("风泽中孚卦", "crc32", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string s = a.empty() ? "" : to_str(a[0]);
        return ClpsValue(int64_t(crc32_compute(s)));
    });

    // adler32 "data" → int  Adler-32
    ku.reg("风泽中孚卦", "adler32", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string s = a.empty() ? "" : to_str(a[0]);
        return ClpsValue(int64_t(adler32_compute(s)));
    });

    // 异或和 "data" → int  所有字节XOR之和
    ku.reg("风泽中孚卦", "异或和", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string s = a.empty() ? "" : to_str(a[0]);
        uint8_t x = 0;
        for (unsigned char c : s) x ^= c;
        return ClpsValue(int64_t(x));
    });

    // 字节和 "data" → int  所有字节相加（截断到8位）
    ku.reg("风泽中孚卦", "字节和", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string s = a.empty() ? "" : to_str(a[0]);
        uint8_t sum = 0;
        for (unsigned char c : s) sum += c;
        return ClpsValue(int64_t(sum));
    });

    // 奇偶 "data" → 0=偶校验 1=奇校验（总1位数）
    ku.reg("风泽中孚卦", "奇偶", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string s = a.empty() ? "" : to_str(a[0]);
        int cnt = 0;
        for (unsigned char c : s) cnt += __builtin_popcount(c);
        return ClpsValue(int64_t(cnt & 1));
    });

    // 十六进制 "data" → hex string  字节转十六进制（小写）
    ku.reg("风泽中孚卦", "十六进制", WX::Huo, [](const std::vector<ClpsValue>& a) {
        std::string s = a.empty() ? "" : to_str(a[0]);
        static const char* hex = "0123456789abcdef";
        std::string out;
        out.reserve(s.size() * 2);
        for (unsigned char c : s) { out += hex[c >> 4]; out += hex[c & 0xF]; }
        return ClpsValue(out);
    });

    // 校验相等 "data" crc → 1=通过 0=失败（与crc32比较）
    ku.reg("风泽中孚卦", "校验", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string s    = a.size() >= 1 ? to_str(a[0]) : "";
        int64_t expected = a.size() >= 2 ? to_int(a[1]) : -1;
        return ClpsValue(int64_t((int64_t)crc32_compute(s) == expected ? 1 : 0));
    });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【山泽损卦】— 损(41)，滑动窗口统计
//  卦义：损，有孚，元吉，无咎，可贞，利有攸往。
//  损=减少=过滤旧数据；滑动窗口即损去旧、留存新的过程
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

struct SunWindow {
    std::deque<double> data;
    size_t capacity;  // 0=无限
    std::mutex mx;

    explicit SunWindow(size_t cap = 0) : capacity(cap) {}

    void push(double v) {
        std::lock_guard<std::mutex> lk(mx);
        data.push_back(v);
        if (capacity > 0 && data.size() > capacity) data.pop_front();
    }

    void clear_data() { std::lock_guard<std::mutex> lk(mx); data.clear(); }

    int64_t count() { std::lock_guard<std::mutex> lk(mx); return (int64_t)data.size(); }

    double sum_v() { std::lock_guard<std::mutex> lk(mx);
        double s=0; for(auto v:data) s+=v; return s; }

    double mean_v() { std::lock_guard<std::mutex> lk(mx);
        if(data.empty()) return 0.0;
        double s=0; for(auto v:data) s+=v; return s/data.size(); }

    double min_v() { std::lock_guard<std::mutex> lk(mx);
        if(data.empty()) return 0.0;
        return *std::min_element(data.begin(),data.end()); }

    double max_v() { std::lock_guard<std::mutex> lk(mx);
        if(data.empty()) return 0.0;
        return *std::max_element(data.begin(),data.end()); }

    double variance_v() { std::lock_guard<std::mutex> lk(mx);
        if(data.size()<2) return 0.0;
        double m=0; for(auto v:data) m+=v; m/=data.size();
        double s=0; for(auto v:data) { double d=v-m; s+=d*d; }
        return s/data.size(); }

    // 返回最近 n 个（n<=0 = 全部）
    std::shared_ptr<ClpsList> tail_n(int64_t n) {
        std::lock_guard<std::mutex> lk(mx);
        auto lst = std::make_shared<ClpsList>();
        size_t start = (n > 0 && (size_t)n < data.size()) ? data.size() - n : 0;
        for (size_t i = start; i < data.size(); i++) lst->push_back(ClpsValue(data[i]));
        return lst;
    }
};

static std::mutex g_sun_mx;
static std::unordered_map<std::string, std::shared_ptr<SunWindow>> g_sun_windows;

static std::shared_ptr<SunWindow> sun_get(const std::string& name, size_t cap=0) {
    std::lock_guard<std::mutex> lk(g_sun_mx);
    auto& p = g_sun_windows[name];
    if (!p) p = std::make_shared<SunWindow>(cap);
    return p;
}

static void reg_huo_feng_ding() {
    auto& ku = WanWuKu::instance();

    // 十六 n → hex字符串（整数转十六进制）
    ku.reg("火风鼎卦", "十六", WX::Huo, [](const std::vector<ClpsValue>& a) {
        int64_t n = a.empty() ? 0 : to_int(a[0]);
        char buf[32]; snprintf(buf, sizeof(buf), "%llx", (unsigned long long)(uint64_t)n);
        return ClpsValue(std::string(buf));
    });

    // 八进 n → oct字符串
    ku.reg("火风鼎卦", "八进", WX::Huo, [](const std::vector<ClpsValue>& a) {
        int64_t n = a.empty() ? 0 : to_int(a[0]);
        char buf[32]; snprintf(buf, sizeof(buf), "%llo", (unsigned long long)(uint64_t)n);
        return ClpsValue(std::string(buf));
    });

    // 二进 n [width] → bin字符串（width=0不填充）
    ku.reg("火风鼎卦", "二进", WX::Huo, [](const std::vector<ClpsValue>& a) {
        uint64_t n = (uint64_t)(a.empty() ? 0 : to_int(a[0]));
        int width  = (int)(a.size()>=2 ? to_int(a[1]) : 0);
        std::string s;
        for (int i = 63; i >= 0; i--) if ((n >> i)&1 || !s.empty()) s += (char)('0' + ((n>>i)&1));
        if (s.empty()) s = "0";
        while ((int)s.size() < width) s = "0" + s;
        return ClpsValue(s);
    });

    // 解十六 str → int  十六进制字符串→整数
    ku.reg("火风鼎卦", "解十六", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string s = a.empty() ? "0" : to_str(a[0]);
        try { return ClpsValue((int64_t)std::stoull(s, nullptr, 16)); }
        catch (...) { return ClpsValue(int64_t(0)); }
    });

    // 解八进 str → int
    ku.reg("火风鼎卦", "解八进", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string s = a.empty() ? "0" : to_str(a[0]);
        try { return ClpsValue((int64_t)std::stoull(s, nullptr, 8)); }
        catch (...) { return ClpsValue(int64_t(0)); }
    });

    // 解二进 str → int
    ku.reg("火风鼎卦", "解二进", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string s = a.empty() ? "0" : to_str(a[0]);
        try { return ClpsValue((int64_t)std::stoull(s, nullptr, 2)); }
        catch (...) { return ClpsValue(int64_t(0)); }
    });

    // 字节 n [size] → ClpsList of bytes (little-endian, default 8)
    ku.reg("火风鼎卦", "字节", WX::Mu, [](const std::vector<ClpsValue>& a) {
        uint64_t n   = (uint64_t)(a.empty() ? 0 : to_int(a[0]));
        int64_t size = a.size()>=2 ? to_int(a[1]) : 8;
        if (size<1) size=1; if (size>8) size=8;
        auto lst = std::make_shared<ClpsList>();
        for (int64_t i=0;i<size;i++) lst->push_back(ClpsValue((int64_t)((n>>(i*8))&0xFF)));
        return ClpsValue(lst);
    });

    // 字节合 list → int  字节列表→整数（little-endian）
    ku.reg("火风鼎卦", "字节合", WX::Jin, [](const std::vector<ClpsValue>& a) {
        if (a.empty()) return ClpsValue(int64_t(0));
        auto src = xun_get_list(a[0]);
        uint64_t n = 0;
        for (size_t i=0;i<src->size() && i<8;i++)
            n |= (uint64_t)(to_int((*src)[i]) & 0xFF) << (i*8);
        return ClpsValue((int64_t)n);
    });

    // 字节大端 n [size] → ClpsList  大端序字节列表
    ku.reg("火风鼎卦", "字节大端", WX::Mu, [](const std::vector<ClpsValue>& a) {
        uint64_t n   = (uint64_t)(a.empty() ? 0 : to_int(a[0]));
        int64_t size = a.size()>=2 ? to_int(a[1]) : 8;
        if (size<1) size=1; if (size>8) size=8;
        auto lst = std::make_shared<ClpsList>();
        for (int64_t i=size-1;i>=0;i--) lst->push_back(ClpsValue((int64_t)((n>>(i*8))&0xFF)));
        return ClpsValue(lst);
    });

    // 字节序反转 n [size] → n  按size字节反转字节序
    ku.reg("火风鼎卦", "字节序反转", WX::Jin, [](const std::vector<ClpsValue>& a) {
        uint64_t n = (uint64_t)(a.empty() ? 0 : to_int(a[0]));
        int64_t size = a.size()>=2 ? to_int(a[1]) : 8;
        if (size==2) n = ((n&0xFF)<<8)|((n>>8)&0xFF);
        else if (size==4) n = ((n&0xFF)<<24)|((n&0xFF00)<<8)|((n&0xFF0000)>>8)|((n>>24)&0xFF);
        else if (size==8) {
            n = ((n&0xFF)<<56)|((n&0xFF00)<<40)|((n&0xFF0000)<<24)|((n&0xFF000000LL)<<8)
               |((n>>8)&0xFF000000LL)|((n>>24)&0xFF0000)|((n>>40)&0xFF00)|((n>>56)&0xFF);
        }
        return ClpsValue((int64_t)n);
    });

    // 浮点字节 f → ClpsList (8字节 IEEE-754 双精度, little-endian)
    ku.reg("火风鼎卦", "浮点字节", WX::Mu, [](const std::vector<ClpsValue>& a) {
        double d = a.empty() ? 0.0 : to_double(a[0]);
        uint64_t bits; std::memcpy(&bits, &d, 8);
        auto lst = std::make_shared<ClpsList>();
        for (int i=0;i<8;i++) lst->push_back(ClpsValue((int64_t)((bits>>(i*8))&0xFF)));
        return ClpsValue(lst);
    });

    // 字节浮点 list → double (8字节 IEEE-754, little-endian)
    ku.reg("火风鼎卦", "字节浮点", WX::Shui, [](const std::vector<ClpsValue>& a) {
        if (a.empty()) return ClpsValue(0.0);
        auto src = xun_get_list(a[0]);
        uint64_t bits = 0;
        for (size_t i=0;i<src->size() && i<8;i++)
            bits |= (uint64_t)(to_int((*src)[i]) & 0xFF) << (i*8);
        double d; std::memcpy(&d, &bits, 8);
        return ClpsValue(d);
    });

    // 整转浮 n → float  整数位模式→浮点（32位，IEEE-754单精度）
    ku.reg("火风鼎卦", "整转浮", WX::Shui, [](const std::vector<ClpsValue>& a) {
        uint32_t n = (uint32_t)(a.empty() ? 0 : to_int(a[0]));
        float f; std::memcpy(&f, &n, 4);
        return ClpsValue((double)f);
    });

    // 浮转整 f → int  单精度浮点→整数位模式
    ku.reg("火风鼎卦", "浮转整", WX::Jin, [](const std::vector<ClpsValue>& a) {
        float f = (float)(a.empty() ? 0.0 : to_double(a[0]));
        uint32_t n; std::memcpy(&n, &f, 4);
        return ClpsValue((int64_t)n);
    });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【水雷屯卦】— 屯(3)，字节流操作
//  卦义：屯，元亨利贞，勿用有攸往，利建侯。
//  屯=积聚=字节的聚散；pack/unpack是屯卦之道
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// varint encode (LEB128 unsigned)
static std::string varint_encode(uint64_t v) {
    std::string out;
    do { uint8_t b = v & 0x7F; v >>= 7; if (v) b |= 0x80; out += (char)b; } while (v);
    return out;
}

static uint64_t varint_decode(const std::string& data, size_t& pos) {
    uint64_t result = 0; int shift = 0;
    while (pos < data.size()) {
        uint8_t b = (uint8_t)data[pos++];
        result |= (uint64_t)(b & 0x7F) << shift;
        if (!(b & 0x80)) break;
        shift += 7;
    }
    return result;
}

static void reg_shui_lei_zhun() {
    auto& ku = WanWuKu::instance();

    // 打包 "格式" val [val...] → string  二进制打包
    // 格式字符: b=int8, B=uint8, h=int16, H=uint16, i=int32, I=uint32, q=int64, f=float32, d=double64
    // 默认小端；> 前缀=大端
    ku.reg("水雷屯卦", "打包", WX::Huo, [](const std::vector<ClpsValue>& a) {
        if (a.empty()) return ClpsValue(std::string{});
        std::string fmt = to_str(a[0]);
        bool big = !fmt.empty() && fmt[0] == '>';
        if (big) fmt = fmt.substr(1);
        std::string out;
        size_t ai = 1;
        for (char c : fmt) {
            if (ai >= a.size()) break;
            auto write = [&](const void* data, size_t size) {
                const char* p = (const char*)data;
                if (big) for (int i=(int)size-1;i>=0;i--) out += p[i];
                else for (size_t i=0;i<size;i++) out += p[i];
            };
            switch (c) {
                case 'b': { int8_t  v=(int8_t) to_int(a[ai++]); write(&v,1); break; }
                case 'B': { uint8_t v=(uint8_t)to_int(a[ai++]); write(&v,1); break; }
                case 'h': { int16_t v=(int16_t)to_int(a[ai++]); write(&v,2); break; }
                case 'H': { uint16_t v=(uint16_t)to_int(a[ai++]); write(&v,2); break; }
                case 'i': { int32_t v=(int32_t)to_int(a[ai++]); write(&v,4); break; }
                case 'I': { uint32_t v=(uint32_t)to_int(a[ai++]); write(&v,4); break; }
                case 'q': { int64_t v=to_int(a[ai++]); write(&v,8); break; }
                case 'f': { float v=(float)to_double(a[ai++]); write(&v,4); break; }
                case 'd': { double v=to_double(a[ai++]); write(&v,8); break; }
                default: break;
            }
        }
        return ClpsValue(out);
    });

    // 解包 "格式" str → ClpsList  二进制解包（同格式规则）
    ku.reg("水雷屯卦", "解包", WX::Mu, [](const std::vector<ClpsValue>& a) {
        auto lst = std::make_shared<ClpsList>();
        if (a.size() < 2) return ClpsValue(lst);
        std::string fmt = to_str(a[0]);
        std::string data = to_str(a[1]);
        bool big = !fmt.empty() && fmt[0] == '>';
        if (big) fmt = fmt.substr(1);
        size_t pos = 0;
        auto read_bytes = [&](void* out, size_t size) -> bool {
            if (pos + size > data.size()) return false;
            if (big) { for (size_t i=0;i<size;i++) ((char*)out)[size-1-i]=data[pos+i]; }
            else { std::memcpy(out, data.data()+pos, size); }
            pos += size; return true;
        };
        for (char c : fmt) {
            switch (c) {
                case 'b': { int8_t v=0;  if(read_bytes(&v,1)) lst->push_back(ClpsValue((int64_t)v)); break; }
                case 'B': { uint8_t v=0; if(read_bytes(&v,1)) lst->push_back(ClpsValue((int64_t)v)); break; }
                case 'h': { int16_t v=0; if(read_bytes(&v,2)) lst->push_back(ClpsValue((int64_t)v)); break; }
                case 'H': { uint16_t v=0; if(read_bytes(&v,2)) lst->push_back(ClpsValue((int64_t)v)); break; }
                case 'i': { int32_t v=0; if(read_bytes(&v,4)) lst->push_back(ClpsValue((int64_t)v)); break; }
                case 'I': { uint32_t v=0; if(read_bytes(&v,4)) lst->push_back(ClpsValue((int64_t)v)); break; }
                case 'q': { int64_t v=0; if(read_bytes(&v,8)) lst->push_back(ClpsValue(v)); break; }
                case 'f': { float v=0;  if(read_bytes(&v,4)) lst->push_back(ClpsValue((double)v)); break; }
                case 'd': { double v=0; if(read_bytes(&v,8)) lst->push_back(ClpsValue(v)); break; }
                default: break;
            }
        }
        return ClpsValue(lst);
    });

    // 可变编 n → string  varint LEB128编码（字节字符串）
    ku.reg("水雷屯卦", "可变编", WX::Huo, [](const std::vector<ClpsValue>& a) {
        uint64_t n = (uint64_t)(a.empty() ? 0 : to_int(a[0]));
        return ClpsValue(varint_encode(n));
    });

    // 可变解 str → int  varint LEB128解码
    ku.reg("水雷屯卦", "可变解", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string s = a.empty() ? "" : to_str(a[0]);
        size_t pos = 0;
        return ClpsValue((int64_t)varint_decode(s, pos));
    });

    // 字节获 str idx → int  取字符串第idx字节值（0-indexed）
    ku.reg("水雷屯卦", "字节获", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string s = a.size()>=1 ? to_str(a[0]) : "";
        int64_t idx = a.size()>=2 ? to_int(a[1]) : 0;
        if (idx<0 || idx>=(int64_t)s.size()) return ClpsValue(int64_t(-1));
        return ClpsValue((int64_t)(uint8_t)s[idx]);
    });

    // 字节置 str idx val → str  设置字符串第idx字节
    ku.reg("水雷屯卦", "字节置", WX::Huo, [](const std::vector<ClpsValue>& a) {
        std::string s = a.size()>=1 ? to_str(a[0]) : "";
        int64_t idx = a.size()>=2 ? to_int(a[1]) : 0;
        int64_t val = a.size()>=3 ? to_int(a[2]) : 0;
        if (idx>=0 && idx<(int64_t)s.size()) s[idx] = (char)(uint8_t)(val & 0xFF);
        return ClpsValue(s);
    });

    // 字节长 str → int  字节数（len，支持中文等多字节）
    ku.reg("水雷屯卦", "字节长", WX::Jin, [](const std::vector<ClpsValue>& a) {
        return ClpsValue((int64_t)(a.empty() ? 0 : to_str(a[0]).size()));
    });

    // 字节列 str → ClpsList of int  字符串转字节列表
    ku.reg("水雷屯卦", "字节列", WX::Mu, [](const std::vector<ClpsValue>& a) {
        std::string s = a.empty() ? "" : to_str(a[0]);
        auto lst = std::make_shared<ClpsList>();
        for (unsigned char c : s) lst->push_back(ClpsValue((int64_t)c));
        return ClpsValue(lst);
    });

    // 列字节 list → str  字节列表转字符串
    ku.reg("水雷屯卦", "列字节", WX::Huo, [](const std::vector<ClpsValue>& a) {
        if (a.empty()) return ClpsValue(std::string{});
        auto src = xun_get_list(a[0]);
        std::string out;
        for (auto& item : *src) out += (char)(uint8_t)(to_int(item) & 0xFF);
        return ClpsValue(out);
    });

    // 零填 n → string  n个零字节
    ku.reg("水雷屯卦", "零填", WX::Huo, [](const std::vector<ClpsValue>& a) {
        int64_t n = a.empty() ? 0 : to_int(a[0]);
        if (n<0) n=0;
        return ClpsValue(std::string((size_t)n, '\0'));
    });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  总入口：启动时调用一次
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━


void init_mod_geshi() {
    reg_shan_huo_bi();
    reg_shui_huo_jiji();
    reg_feng_ze_zhongfu();
    reg_huo_feng_ding();
    reg_shui_lei_zhun();
}

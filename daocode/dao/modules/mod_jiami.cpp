// mod_jiami.cpp — 道·码万物库子模块
#include "../wanwu.hpp"
#include "mod_helpers.hpp"

#include <sstream>
#include <iomanip>
#include <cstring>
#include <thread>
#include <chrono>

// ── 全局：用户 CLI 参数（由 wan_wu_ku_set_args() 注入）──────────────────────
static std::vector<std::string> g_clpsd_args;

void wan_wu_ku_set_args(const std::vector<std::string>& args) {
    g_clpsd_args = args;
}

static void reg_ze_tian_guai() {
    auto& ku = WanWuKu::instance();

    // 取参 → list（所有参数）
    ku.reg("泽天夬卦", "取参", WX::Mu, [](const std::vector<ClpsValue>&) {
        auto lst = std::make_shared<ClpsList>();
        for (auto& s : g_clpsd_args) lst->push_back(ClpsValue(s));
        return ClpsValue(lst);
    });

    // 参数 → int（参数个数）
    ku.reg("泽天夬卦", "参数", WX::Jin, [](const std::vector<ClpsValue>&) {
        return ClpsValue(int64_t(g_clpsd_args.size()));
    });

    // 取位 n → string（第 n 个参数，超出返回 None）
    ku.reg("泽天夬卦", "取位", WX::Huo, [](const std::vector<ClpsValue>& a) {
        int64_t n = a.empty() ? 0 : to_int(a[0], 0);
        if (n < 0 || (size_t)n >= g_clpsd_args.size()) return ClpsValue{};
        return ClpsValue(g_clpsd_args[(size_t)n]);
    });

    // 有参 name → int（1=存在 --name，0=不存在）
    ku.reg("泽天夬卦", "有参", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string name = a.empty() ? "" : to_str(a[0]);
        std::string flag = "--" + name;
        for (auto& s : g_clpsd_args) {
            if (s == flag || s.rfind(flag + "=", 0) == 0) return ClpsValue(int64_t(1));
        }
        return ClpsValue(int64_t(0));
    });

    // 取值 name → string（--name=value 或 --name value）
    ku.reg("泽天夬卦", "取值", WX::Huo, [](const std::vector<ClpsValue>& a) {
        std::string name = a.empty() ? "" : to_str(a[0]);
        std::string prefix = "--" + name + "=";
        for (size_t i = 0; i < g_clpsd_args.size(); ++i) {
            auto& s = g_clpsd_args[i];
            if (s.rfind(prefix, 0) == 0)
                return ClpsValue(s.substr(prefix.size()));
            if (s == "--" + name && i + 1 < g_clpsd_args.size())
                return ClpsValue(g_clpsd_args[i + 1]);
        }
        return ClpsValue{}; // 未找到 → None
    });
}


// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【习坎卦】— 坎(29)，安全防护
//  卦义：习坎，有孚，维心亨，行有尚。
//  险中求通：Base64编解码 + SHA-256 + HMAC-SHA256 + URL编解码
//  纯C++零依赖，不链接 OpenSSL
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// ── Base64 ───────────────────────────────────────────
static const char KAN_B64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string kan_b64_encode(const std::string& in) {
    std::string out;
    out.reserve(((in.size() + 2) / 3) * 4);
    int val = 0, bits = -6;
    for (unsigned char c : in) {
        val = (val << 8) | c;
        bits += 8;
        while (bits >= 0) {
            out.push_back(KAN_B64[(val >> bits) & 0x3F]);
            bits -= 6;
        }
    }
    if (bits > -6) out.push_back(KAN_B64[((val << 8) >> (bits + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

static std::string kan_b64_decode(const std::string& in) {
    static const int T[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };
    std::string out;
    int val = 0, bits = -8;
    for (unsigned char c : in) {
        if (T[c] == -1) break;
        val = (val << 6) | T[c];
        bits += 6;
        if (bits >= 0) { out.push_back(char((val >> bits) & 0xFF)); bits -= 8; }
    }
    return out;
}

// ── SHA-256（FIPS 180-4）────────────────────────────
static inline uint32_t rotr32(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

static const uint32_t SHA256_K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static std::vector<uint8_t> sha256_raw(const std::string& input) {
    uint32_t h[8] = {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    };
    std::vector<uint8_t> msg(input.begin(), input.end());
    uint64_t bit_len = (uint64_t)msg.size() * 8;
    msg.push_back(0x80);
    while (msg.size() % 64 != 56) msg.push_back(0x00);
    for (int i = 7; i >= 0; i--) msg.push_back((uint8_t)(bit_len >> (i * 8)));

    for (size_t i = 0; i < msg.size(); i += 64) {
        uint32_t w[64];
        for (int j = 0; j < 16; j++)
            w[j] = ((uint32_t)msg[i+j*4]<<24)|((uint32_t)msg[i+j*4+1]<<16)
                  |((uint32_t)msg[i+j*4+2]<< 8)|((uint32_t)msg[i+j*4+3]);
        for (int j = 16; j < 64; j++) {
            uint32_t s0 = rotr32(w[j-15],7)^rotr32(w[j-15],18)^(w[j-15]>>3);
            uint32_t s1 = rotr32(w[j-2],17)^rotr32(w[j-2],19) ^(w[j-2] >>10);
            w[j] = w[j-16]+s0+w[j-7]+s1;
        }
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for (int j = 0; j < 64; j++) {
            uint32_t S1   = rotr32(e,6)^rotr32(e,11)^rotr32(e,25);
            uint32_t ch   = (e&f)^(~e&g);
            uint32_t tmp1 = hh+S1+ch+SHA256_K[j]+w[j];
            uint32_t S0   = rotr32(a,2)^rotr32(a,13)^rotr32(a,22);
            uint32_t maj  = (a&b)^(a&c)^(b&c);
            uint32_t tmp2 = S0+maj;
            hh=g; g=f; f=e; e=d+tmp1; d=c; c=b; b=a; a=tmp1+tmp2;
        }
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=hh;
    }
    std::vector<uint8_t> digest(32);
    for (int i = 0; i < 8; i++) {
        digest[i*4]  =(h[i]>>24)&0xFF; digest[i*4+1]=(h[i]>>16)&0xFF;
        digest[i*4+2]=(h[i]>> 8)&0xFF; digest[i*4+3]= h[i]     &0xFF;
    }
    return digest;
}

static std::string sha256_hex_str(const std::string& input) {
    auto d = sha256_raw(input);
    std::ostringstream oss;
    for (auto b : d) oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    return oss.str();
}

// ── HMAC-SHA256 ──────────────────────────────────────
static std::string hmac_sha256_hex(const std::string& key, const std::string& data) {
    std::string k = key;
    if (k.size() > 64) { auto d = sha256_raw(k); k.assign(d.begin(), d.end()); }
    k.resize(64, '\0');
    std::string ipad(64,'\0'), opad(64,'\0');
    for (int i = 0; i < 64; i++) { ipad[i]=k[i]^char(0x36); opad[i]=k[i]^char(0x5c); }
    auto inner = sha256_raw(ipad + data);
    std::string inner_str(inner.begin(), inner.end());
    return sha256_hex_str(opad + inner_str);
}

// ── URL encode/decode ─────────────────────────────────
static std::string url_encode(const std::string& s) {
    std::ostringstream oss;
    for (unsigned char c : s) {
        if (isalnum(c)||c=='-'||c=='_'||c=='.'||c=='~') oss << c;
        else oss << '%' << std::uppercase << std::hex
                 << std::setw(2) << std::setfill('0') << (int)c;
    }
    return oss.str();
}

static void reg_xi_kan() {
    auto& ku = WanWuKu::instance();

    // 编码 data → Base64 字符串
    ku.reg("习坎卦", "编码", WX::Huo, [](const std::vector<ClpsValue>& a) {
        return ClpsValue(kan_b64_encode(a.empty() ? "" : to_str(a[0])));
    });

    // 解码 b64str → 原始字符串
    ku.reg("习坎卦", "解码", WX::Huo, [](const std::vector<ClpsValue>& a) {
        return ClpsValue(kan_b64_decode(a.empty() ? "" : to_str(a[0])));
    });

    // 散列 data → SHA-256 十六进制摘要
    ku.reg("习坎卦", "散列", WX::Huo, [](const std::vector<ClpsValue>& a) {
        return ClpsValue(sha256_hex_str(a.empty() ? "" : to_str(a[0])));
    });

    // 签名 key data → HMAC-SHA256 十六进制
    ku.reg("习坎卦", "签名", WX::Huo, [](const std::vector<ClpsValue>& a) {
        std::string key  = a.size() >= 1 ? to_str(a[0]) : "";
        std::string data = a.size() >= 2 ? to_str(a[1]) : "";
        return ClpsValue(hmac_sha256_hex(key, data));
    });

    // 网址编 url → percent-encoded
    ku.reg("习坎卦", "网址编", WX::Huo, [](const std::vector<ClpsValue>& a) {
        return ClpsValue(url_encode(a.empty() ? "" : to_str(a[0])));
    });

    // 网址解 encoded → decoded
    ku.reg("习坎卦", "网址解", WX::Huo, [](const std::vector<ClpsValue>& a) {
        return ClpsValue(url_decode(a.empty() ? "" : to_str(a[0])));
    });
}

void init_mod_jiami() {
    reg_ze_tian_guai();
    reg_xi_kan();
}

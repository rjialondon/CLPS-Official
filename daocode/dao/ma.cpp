#include "ma.hpp"
#include <stdexcept>
#include <cstring>

// ── 小工具：LE 整数读写 ────────────────────────────────────────────────────
static void push_u16(std::vector<uint8_t>& v, uint16_t x) {
    v.push_back(x & 0xFF);
    v.push_back((x >> 8) & 0xFF);
}
static void push_u32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(x & 0xFF);
    v.push_back((x >> 8) & 0xFF);
    v.push_back((x >> 16) & 0xFF);
    v.push_back((x >> 24) & 0xFF);
}
static void push_i64(std::vector<uint8_t>& v, int64_t x) {
    uint64_t u; memcpy(&u, &x, 8);
    for (int i = 0; i < 8; ++i) v.push_back((u >> (8*i)) & 0xFF);
}
static void push_f64(std::vector<uint8_t>& v, double x) {
    uint64_t u; memcpy(&u, &x, 8);
    for (int i = 0; i < 8; ++i) v.push_back((u >> (8*i)) & 0xFF);
}
static void push_str(std::vector<uint8_t>& v, const std::string& s) {
    push_u16(v, (uint16_t)s.size());
    for (char c : s) v.push_back((uint8_t)c);
}

static uint16_t read_u16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static uint32_t read_u32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
static int64_t read_i64(const uint8_t* p) {
    uint64_t u = 0;
    for (int i = 0; i < 8; ++i) u |= ((uint64_t)p[i] << (8*i));
    int64_t v; memcpy(&v, &u, 8); return v;
}
static double read_f64(const uint8_t* p) {
    uint64_t u = 0;
    for (int i = 0; i < 8; ++i) u |= ((uint64_t)p[i] << (8*i));
    double v; memcpy(&v, &u, 8); return v;
}
static std::string read_str(const uint8_t* p, size_t& consumed) {
    uint16_t len = read_u16(p);
    consumed = 2 + len;
    return std::string((const char*)(p+2), len);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  序列化
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
std::vector<uint8_t> ClpsByteCode::serialize() const {
    std::vector<uint8_t> out;

    // 文件头
    out.push_back('C'); out.push_back('L');
    out.push_back('P'); out.push_back('S');
    out.push_back(0x01); // version
    out.push_back(0x00); // reserved

    // 常量池
    push_u16(out, (uint16_t)pool.size());
    for (auto& e : pool) {
        out.push_back((uint8_t)e.type);
        switch (e.type) {
            case PoolType::Name:
            case PoolType::StrLit:
            case PoolType::VarRef:
                push_str(out, e.str); break;
            case PoolType::Int64:
                push_i64(out, e.ival); break;
            case PoolType::Float64:
                push_f64(out, e.dval); break;
            case PoolType::Wx:
                out.push_back(e.wx); break;
        }
    }

    // 字节码
    push_u32(out, (uint32_t)code.size());
    out.insert(out.end(), code.begin(), code.end());
    return out;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  反序列化
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
ClpsByteCode ClpsByteCode::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 10)
        throw std::runtime_error("字节码文件过短");
    if (data[0]!='C'||data[1]!='L'||data[2]!='P'||data[3]!='S')
        throw std::runtime_error("字节码文件魔数错误（非 CLPS 格式）");

    ClpsByteCode bc;
    size_t pos = 6; // 跳过 magic + version + reserved

    // 常量池
    uint16_t pool_n = read_u16(data.data() + pos); pos += 2;
    bc.pool.reserve(pool_n);
    for (uint16_t i = 0; i < pool_n; ++i) {
        PoolEntry e;
        e.type = (PoolType)data[pos++];
        switch (e.type) {
            case PoolType::Name:
            case PoolType::StrLit:
            case PoolType::VarRef: {
                size_t c = 0;
                e.str = read_str(data.data() + pos, c);
                pos += c; break;
            }
            case PoolType::Int64:
                e.ival = read_i64(data.data() + pos); pos += 8; break;
            case PoolType::Float64:
                e.dval = read_f64(data.data() + pos); pos += 8; break;
            case PoolType::Wx:
                e.wx = data[pos++]; break;
        }
        bc.pool.push_back(e);
    }

    // 字节码
    uint32_t code_n = read_u32(data.data() + pos); pos += 4;
    bc.code.assign(data.begin() + pos, data.begin() + pos + code_n);
    return bc;
}

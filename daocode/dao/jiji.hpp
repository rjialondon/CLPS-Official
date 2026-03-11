#pragma once
/*
 * clps_codegen.hpp — 道·码 代码生成器（.dao → CLPS 字节码）
 *
 * 结构与 ClpsParser 镜像：同样的扁平坍缩法遍历 token 流，
 * 但不执行语义，而是向字节码缓冲区写入操作码。
 *
 * 两遍处理：
 *   1. gen_* 系列方法写入操作码，用 backpatch 处理 GEN/LOOP 的跳转距离
 *   2. compile() 返回完整 ClpsByteCode（含常量池）
 */

#include "yao.hpp"
#include "ma.hpp"
#include <vector>
#include <string>
#include <map>
#include <optional>
#include <stdexcept>

// 代码生成错误
class ClpsCodegenError : public std::runtime_error {
public:
    int line;
    ClpsCodegenError(const std::string& msg, int ln = 0)
        : std::runtime_error(msg), line(ln) {}
};

class ClpsCodegen {
public:
    explicit ClpsCodegen(std::vector<Token> tokens);

    // 编译：返回字节码产物
    ClpsByteCode compile();

private:
    std::vector<Token> toks_;
    size_t pos_ = 0;

    // 常量池（带去重）
    std::vector<PoolEntry> pool_;
    std::map<std::pair<PoolType,std::string>, uint16_t> str_map_;
    std::map<int64_t,  uint16_t> int_map_;
    std::map<double,   uint16_t> flt_map_;
    std::map<uint8_t,  uint16_t> wx_map_;

    // 字节码缓冲区
    std::vector<uint8_t> code_;

    // 蒙卦：记录上次发射 LINE 的行号（-1=未发射）
    int last_emitted_line_ = -1;

    // ── 常量池辅助 ──────────────────────────────────────────────────
    uint16_t add_name   (const std::string& s);   // 变量名/模块名/函数名
    uint16_t add_str_lit(const std::string& s);   // 字符串字面量
    uint16_t add_var_ref(const std::string& s);   // 变量引用
    uint16_t add_int    (int64_t v);
    uint16_t add_float  (double v);
    uint16_t add_wx     (uint8_t wx);

    // ── 字节码写入辅助 ───────────────────────────────────────────────
    void emit_u8 (uint8_t v)  { code_.push_back(v); }
    void emit_u16(uint16_t v) { code_.push_back(v&0xFF); code_.push_back((v>>8)&0xFF); }
    void emit_u32(uint32_t v) {
        code_.push_back(v&0xFF); code_.push_back((v>>8)&0xFF);
        code_.push_back((v>>16)&0xFF); code_.push_back((v>>24)&0xFF);
    }
    // 在 pos 处回填 u16
    void patch_u16(size_t pos, uint16_t v) {
        code_[pos]   = v & 0xFF;
        code_[pos+1] = (v >> 8) & 0xFF;
    }
    // 在 pos 处回填 u32
    void patch_u32(size_t pos, uint32_t v) {
        code_[pos]   = v & 0xFF;       code_[pos+1] = (v>>8) & 0xFF;
        code_[pos+2] = (v>>16) & 0xFF; code_[pos+3] = (v>>24) & 0xFF;
    }

    // ── Token 操作 ───────────────────────────────────────────────────
    const Token& peek()  const { return toks_[pos_]; }
    bool check(TK t)     const { return peek().type == t; }
    const Token& consume() {
        const Token& t = toks_[pos_];
        if (pos_+1 < toks_.size()) ++pos_;
        return t;
    }
    const Token& expect(TK t);

    // ── 值操作数编码（返回池索引）────────────────────────────────────
    uint16_t gen_value_operand(size_t loop_depth); // STR_LIT|INT|FLOAT|VAR_REF|$i
    uint8_t  gen_cmp_op();                          // == != > < >= <=
    uint8_t  gen_arith_op();                        // + - * /
    std::optional<uint8_t> try_gen_wx();            // 五行（可选）

    // ── 各语句生成 ───────────────────────────────────────────────────
    void gen_qian   (size_t loop_depth);
    void gen_zhen   (size_t loop_depth);
    void gen_kan    (size_t loop_depth);
    void gen_dui    ();
    void gen_kun    (size_t loop_depth);
    void gen_li     ();
    void gen_xun    ();
    void gen_bian   ();
    void gen_fu     (size_t loop_depth);
    void gen_jiejie (size_t loop_depth);
    void gen_gen    (size_t loop_depth);
    void gen_song   (size_t loop_depth);
    void gen_jiaren ();
    void gen_shi     ();   // 师·分兵
    void gen_he      ();   // 师·合师
    void gen_heng    (size_t loop_depth);  // 恒·只读常量
    void gen_jian    (size_t loop_depth);  // 蹇·错误恢复
    void gen_xiao_xu (size_t loop_depth);  // 小畜·列表积累
    void gen_yi      (size_t loop_depth);  // 益(42)·子程序定义/调用
    void gen_one     (size_t loop_depth);
};

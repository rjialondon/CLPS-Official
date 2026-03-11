#include "jiji.hpp"

ClpsCodegen::ClpsCodegen(std::vector<Token> tokens)
    : toks_(std::move(tokens)) {}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  常量池
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

static uint16_t add_str_entry(
    std::vector<PoolEntry>& pool,
    std::map<std::pair<PoolType,std::string>, uint16_t>& map,
    PoolType type, const std::string& s)
{
    auto key = std::make_pair(type, s);
    auto it = map.find(key);
    if (it != map.end()) return it->second;
    if (pool.size() >= 0xFFFD)
        throw ClpsCodegenError("常量池溢出（超过 65533 条）");
    uint16_t idx = (uint16_t)pool.size();
    pool.push_back({type, s});
    map[key] = idx;
    return idx;
}

uint16_t ClpsCodegen::add_name   (const std::string& s) { return add_str_entry(pool_, str_map_, PoolType::Name,    s); }
uint16_t ClpsCodegen::add_str_lit(const std::string& s) { return add_str_entry(pool_, str_map_, PoolType::StrLit,  s); }
uint16_t ClpsCodegen::add_var_ref(const std::string& s) { return add_str_entry(pool_, str_map_, PoolType::VarRef,  s); }

uint16_t ClpsCodegen::add_int(int64_t v) {
    auto it = int_map_.find(v);
    if (it != int_map_.end()) return it->second;
    uint16_t idx = (uint16_t)pool_.size();
    pool_.push_back({PoolType::Int64, "", v});
    int_map_[v] = idx;
    return idx;
}

uint16_t ClpsCodegen::add_float(double v) {
    auto it = flt_map_.find(v);
    if (it != flt_map_.end()) return it->second;
    uint16_t idx = (uint16_t)pool_.size();
    pool_.push_back({PoolType::Float64, "", 0, v});
    flt_map_[v] = idx;
    return idx;
}

uint16_t ClpsCodegen::add_wx(uint8_t wx) {
    auto it = wx_map_.find(wx);
    if (it != wx_map_.end()) return it->second;
    uint16_t idx = (uint16_t)pool_.size();
    pool_.push_back({PoolType::Wx, "", 0, 0.0, wx});
    wx_map_[wx] = idx;
    return idx;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Token 操作
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

const Token& ClpsCodegen::expect(TK t) {
    if (peek().type != t)
        throw ClpsCodegenError(
            std::string("代码生成：期望 ") + tk_name(t) +
            "，遇到 " + tk_name(peek().type) +
            " \"" + peek().text + "\"", peek().line);
    return consume();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  值操作数 → 池索引
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

uint16_t ClpsCodegen::gen_value_operand(size_t /*loop_depth*/) {
    const Token& t = peek();
    switch (t.type) {
        case TK::DOLLAR_I: consume(); return POOL_DOLLAR_I;
        case TK::STR:      consume(); return add_str_lit(t.text);
        case TK::INT:      consume(); return add_int(t.ival);
        case TK::FLOAT:    consume(); return add_float(t.dval);
        case TK::IDENT:    consume(); return add_var_ref(t.text);
        default:
            throw ClpsCodegenError(
                std::string("代码生成：期望值，遇到 ") + tk_name(t.type), t.line);
    }
}

uint8_t ClpsCodegen::gen_cmp_op() {
    const Token& t = peek();
    switch (t.type) {
        case TK::EQ:  consume(); return CMP_EQ;
        case TK::NEQ: consume(); return CMP_NEQ;
        case TK::GT:  consume(); return CMP_GT;
        case TK::LT:  consume(); return CMP_LT;
        case TK::GTE: consume(); return CMP_GTE;
        case TK::LTE: consume(); return CMP_LTE;
        default:
            throw ClpsCodegenError(
                std::string("代码生成：期望比较运算符，遇到 ") + tk_name(t.type), t.line);
    }
}

uint8_t ClpsCodegen::gen_arith_op() {
    const Token& t = peek();
    switch (t.type) {
        case TK::PLUS:    consume(); return AOP_ADD;
        case TK::MINUS:   consume(); return AOP_SUB;
        case TK::STAR:    consume(); return AOP_MUL;
        case TK::SLASH:   consume(); return AOP_DIV;
        case TK::PERCENT: consume(); return AOP_MOD;
        default:
            throw ClpsCodegenError(
                std::string("代码生成：期望算术运算符，遇到 ") + tk_name(t.type), t.line);
    }
}

std::optional<uint8_t> ClpsCodegen::try_gen_wx() {
    switch (peek().type) {
        case TK::JIN:  consume(); return 0; // Jin
        case TK::SHUI: consume(); return 1; // Shui
        case TK::MU:   consume(); return 2; // Mu
        case TK::HUO:  consume(); return 3; // Huo
        case TK::TU:   consume(); return 4; // Tu
        default: return std::nullopt;
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  各卦代码生成
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// 乾：QIAN [key:u16] [val:u16]
//      QIAN_WX [key:u16] [val:u16] [wx:u8]
void ClpsCodegen::gen_qian(size_t loop_depth) {
    consume(); // 乾
    uint16_t key = add_name(expect(TK::STR).text);
    uint16_t val = gen_value_operand(loop_depth);
    auto wx = try_gen_wx();
    if (wx) {
        emit_u8((uint8_t)Op::QIAN_WX);
        emit_u16(key); emit_u16(val); emit_u8(*wx);
    } else {
        emit_u8((uint8_t)Op::QIAN);
        emit_u16(key); emit_u16(val);
    }
}

// 震：ZHEN_OP [op:u8][key:u16][val:u16]
//      ZHEN_CALL [res:u16][mod:u16][fn:u16][argc:u8][args...]
//      ZHEN_SFX  [mod:u16][fn:u16][argc:u8][args...]
void ClpsCodegen::gen_zhen(size_t loop_depth) {
    consume(); // 震

    // 纯副作用库调用：震 【模块】 函数 [args]
    if (check(TK::MODULE)) {
        uint16_t mod = add_name(consume().text);
        uint16_t fn  = add_name(consume().text); // 语境坍缩：多义词在此位置坍缩为函数名
        std::vector<uint16_t> args;
        while (!check(TK::END) && !check(TK::RBRACE)) {
            TK t = peek().type;
            if (t==TK::JIEJIE||t==TK::QIAN||t==TK::ZHEN||t==TK::KAN||
                t==TK::DUI||t==TK::KUN||t==TK::LI||t==TK::XUN||
                t==TK::GEN||t==TK::BIAN||t==TK::FU||t==TK::SONG||
                t==TK::JIAREN||t==TK::HENG||t==TK::JIAN||t==TK::XIAO_XU||
                t==TK::YI||t==TK::SHI||t==TK::HE) break;
            args.push_back(gen_value_operand(loop_depth));
        }
        if (args.size() > 255)
            throw ClpsCodegenError("震·库调用：参数过多（上限255）", toks_[pos_-1].line);
        emit_u8((uint8_t)Op::ZHEN_SFX);
        emit_u16(mod); emit_u16(fn);
        emit_u8((uint8_t)args.size());
        for (auto a : args) emit_u16(a);
        return;
    }

    // 需要 STR key
    uint16_t key = add_name(expect(TK::STR).text);

    // 有返回值的库调用：震 "key" 【模块】 函数 [args]
    if (check(TK::MODULE)) {
        uint16_t mod = add_name(consume().text);
        uint16_t fn  = add_name(consume().text); // 语境坍缩：多义词在此位置坍缩为函数名
        std::vector<uint16_t> args;
        while (!check(TK::END) && !check(TK::RBRACE)) {
            TK t = peek().type;
            if (t==TK::JIEJIE||t==TK::QIAN||t==TK::ZHEN||t==TK::KAN||
                t==TK::DUI||t==TK::KUN||t==TK::LI||t==TK::XUN||
                t==TK::GEN||t==TK::BIAN||t==TK::FU||t==TK::SONG||
                t==TK::JIAREN||t==TK::HENG||t==TK::JIAN||t==TK::XIAO_XU||
                t==TK::YI||t==TK::SHI||t==TK::HE) break;
            args.push_back(gen_value_operand(loop_depth));
        }
        if (args.size() > 255)
            throw ClpsCodegenError("震·库调用：参数过多（上限255）", toks_[pos_-1].line);
        emit_u8((uint8_t)Op::ZHEN_CALL);
        emit_u16(key); emit_u16(mod); emit_u16(fn);
        emit_u8((uint8_t)args.size());
        for (auto a : args) emit_u16(a);
        return;
    }

    // 算术：震 "key" op value
    uint8_t op = gen_arith_op();
    uint16_t val = gen_value_operand(loop_depth);
    emit_u8((uint8_t)Op::ZHEN_OP);
    emit_u8(op); emit_u16(key); emit_u16(val);
}

// 坎：KAN_OP [op:u8][key:u16][val:u16]
void ClpsCodegen::gen_kan(size_t loop_depth) {
    consume(); // 坎
    uint16_t key = add_name(expect(TK::STR).text);
    uint8_t  op  = gen_arith_op();
    uint16_t val = gen_value_operand(loop_depth);
    emit_u8((uint8_t)Op::KAN_OP);
    emit_u8(op); emit_u16(key); emit_u16(val);
}

// 兑：DUI [key:u16]
void ClpsCodegen::gen_dui() {
    consume(); // 兑
    uint16_t key = add_name(expect(TK::STR).text);
    emit_u8((uint8_t)Op::DUI); emit_u16(key);
}

// 坤：KUN / KUN_LIST
//   坤              — 清空作用域
//   坤 "列表名" N  — 截断列表到前 N 项
void ClpsCodegen::gen_kun(size_t loop_depth) {
    consume(); // 坤
    if (check(TK::STR)) {
        uint16_t key = add_name(expect(TK::STR).text);
        uint16_t cnt = gen_value_operand(loop_depth);
        emit_u8((uint8_t)Op::KUN_LIST);
        emit_u16(key); emit_u16(cnt);
    } else {
        emit_u8((uint8_t)Op::KUN);
    }
}

// 离：LI [key:u16][scope:u16][src:u16]
void ClpsCodegen::gen_li() {
    consume(); // 离
    uint16_t key   = add_name(expect(TK::STR).text);
    uint16_t scope = add_name(expect(TK::IDENT).text);
    uint16_t src   = add_name(expect(TK::STR).text);
    emit_u8((uint8_t)Op::LI);
    emit_u16(key); emit_u16(scope); emit_u16(src);
}

// 巽：XUN [pat:u16]
void ClpsCodegen::gen_xun() {
    consume(); // 巽
    uint16_t pat = POOL_NONE;
    if (check(TK::STR))      pat = add_str_lit(consume().text);
    else if (check(TK::WILDCARD)) pat = add_str_lit(consume().text);
    else if (check(TK::IDENT))    pat = add_str_lit(consume().text);
    emit_u8((uint8_t)Op::XUN); emit_u16(pat);
}

// 变：BIAN [key:u16][wx:u8]
void ClpsCodegen::gen_bian() {
    consume(); // 变
    uint16_t key = add_name(expect(TK::STR).text);
    // parse_wx
    uint8_t wx = 4; // 默认土
    switch (peek().type) {
        case TK::JIN:  consume(); wx=0; break;
        case TK::SHUI: consume(); wx=1; break;
        case TK::MU:   consume(); wx=2; break;
        case TK::HUO:  consume(); wx=3; break;
        case TK::TU:   consume(); wx=4; break;
        default: break;
    }
    emit_u8((uint8_t)Op::BIAN); emit_u16(key); emit_u8(wx);
}

// 复：LOOP_BEG [ctype:u8][cnt:u16][body_sz:u32] ... body ... LOOP_END
void ClpsCodegen::gen_fu(size_t loop_depth) {
    consume(); // 复

    // 循环次数
    uint8_t  ctype;
    uint16_t cnt_val;
    if (check(TK::INT)) {
        ctype   = LOOP_LIT;
        cnt_val = (uint16_t)consume().ival;
    } else if (check(TK::IDENT)) {
        ctype   = LOOP_VAR;
        cnt_val = add_var_ref(consume().text);
    } else {
        throw ClpsCodegenError("复卦：期望循环次数", peek().line);
    }

    expect(TK::LBRACE);

    // 写 LOOP_BEG，body_sz 先填 0，之后回填
    emit_u8((uint8_t)Op::LOOP_BEG);
    emit_u8(ctype); emit_u16(cnt_val);
    size_t patch_pos = code_.size();
    emit_u32(0); // body_sz 占位

    size_t body_start = code_.size();

    // 生成循环体（loop_depth+1，使 $i 有效）
    while (!check(TK::RBRACE) && !check(TK::END)) {
        gen_one(loop_depth + 1);
    }
    expect(TK::RBRACE);

    emit_u8((uint8_t)Op::LOOP_END);

    // 回填 body_sz（含 LOOP_END 指令）
    uint32_t body_sz = (uint32_t)(code_.size() - body_start);
    patch_u32(patch_pos, body_sz);
}

// 结界：SCOPE_PUSH [name:u16] ... body ... SCOPE_POP
void ClpsCodegen::gen_jiejie(size_t loop_depth) {
    consume(); // 结界
    uint16_t name = add_name(expect(TK::STR).text);
    expect(TK::LBRACE);

    emit_u8((uint8_t)Op::SCOPE_PUSH); emit_u16(name);

    while (!check(TK::RBRACE) && !check(TK::END)) {
        gen_one(loop_depth);
    }
    expect(TK::RBRACE);

    emit_u8((uint8_t)Op::SCOPE_POP);
}

// 艮：GEN_CMP [cmp:u8][key:u16][val:u16][skip:u16] ... if_block ...
//              [GEN_ELSE [else_sz:u16] ... else_block ...]（可选）
//
// skip = 条件为假时跳过的字节数
//   无否块：skip = len(if_block)
//   有否块：skip = len(if_block) + 3（含 GEN_ELSE 头：1+2）
//
// GEN_ELSE 由 VM 在条件为真时执行，跳过 else_block
void ClpsCodegen::gen_gen(size_t loop_depth) {
    consume(); // 艮
    uint16_t key = add_name(expect(TK::STR).text);
    uint8_t  cmp = gen_cmp_op();
    uint16_t val = gen_value_operand(loop_depth);
    expect(TK::LBRACE);

    // GEN_CMP 指令头（7 字节）
    emit_u8((uint8_t)Op::GEN_CMP);
    emit_u8(cmp); emit_u16(key); emit_u16(val);
    size_t patch_pos = code_.size();
    emit_u16(0); // skip 占位

    size_t if_block_start = code_.size();

    while (!check(TK::RBRACE) && !check(TK::END)) {
        gen_one(loop_depth);
    }
    expect(TK::RBRACE);

    // 否块（可选）
    if (check(TK::POU)) {
        consume(); // 否
        expect(TK::LBRACE);

        // skip = len(if_block) + 3（GEN_ELSE 头：opcode 1 + else_sz 2）
        size_t if_raw = code_.size() - if_block_start;
        if (if_raw + 3 > 0xFFFF)
            throw ClpsCodegenError("艰·条件块过大（超过 64KB 字节码）", toks_[pos_-1].line);
        uint16_t if_sz = (uint16_t)if_raw;
        patch_u16(patch_pos, if_sz + 3);

        // GEN_ELSE [else_sz 占位]
        emit_u8((uint8_t)Op::GEN_ELSE);
        size_t else_sz_patch = code_.size();
        emit_u16(0);

        size_t else_block_start = code_.size();
        while (!check(TK::RBRACE) && !check(TK::END)) {
            gen_one(loop_depth);
        }
        expect(TK::RBRACE);

        size_t else_raw = code_.size() - else_block_start;
        if (else_raw > 0xFFFF)
            throw ClpsCodegenError("艰·否块过大（超过 64KB 字节码）", toks_[pos_-1].line);
        uint16_t else_sz = (uint16_t)else_raw;
        patch_u16(else_sz_patch, else_sz);
    } else {
        // 无否块：原有行为
        size_t skip_raw = code_.size() - if_block_start;
        if (skip_raw > 0xFFFF)
            throw ClpsCodegenError("艰·条件块过大（超过 64KB 字节码）", toks_[pos_-1].line);
        uint16_t skip = (uint16_t)skip_raw;
        patch_u16(patch_pos, skip);
    }
}

// 讼：SONG_CMP [cmp:u8][key:u16][val:u16]
void ClpsCodegen::gen_song(size_t loop_depth) {
    consume(); // 讼
    uint16_t key = add_name(expect(TK::STR).text);
    uint8_t  cmp = gen_cmp_op();
    uint16_t val = gen_value_operand(loop_depth);
    emit_u8((uint8_t)Op::SONG_CMP);
    emit_u8(cmp); emit_u16(key); emit_u16(val);
}

// 家人：JIAREN [name:u16][path:u16]
void ClpsCodegen::gen_jiaren() {
    consume(); // 家人
    uint16_t name = add_str_lit(expect(TK::STR).text);
    uint16_t path = add_str_lit(expect(TK::STR).text);
    emit_u8((uint8_t)Op::JIAREN);
    emit_u16(name); emit_u16(path);
}

// 恒：HENG [key:u16][val:u16]
//      HENG_WX [key:u16][val:u16][wx:u8]
void ClpsCodegen::gen_heng(size_t loop_depth) {
    consume(); // 恒
    uint16_t key = add_name(expect(TK::STR).text);
    uint16_t val = gen_value_operand(loop_depth);
    auto wx = try_gen_wx();
    if (wx) {
        emit_u8((uint8_t)Op::HENG_WX);
        emit_u16(key); emit_u16(val); emit_u8(*wx);
    } else {
        emit_u8((uint8_t)Op::HENG);
        emit_u16(key); emit_u16(val);
    }
}

// 蹇：JIAN_BEG [try_sz:u32][catch_key:u16][catch_sz:u32]
//     ...try body (try_sz bytes)...
//     ...catch body (catch_sz bytes, 0 if no 化 block)...
void ClpsCodegen::gen_jian(size_t loop_depth) {
    consume(); // 蹇
    expect(TK::LBRACE);

    emit_u8((uint8_t)Op::JIAN_BEG);
    size_t try_sz_patch    = code_.size(); emit_u32(0);         // try_sz 占位
    size_t catch_key_patch = code_.size(); emit_u16(POOL_NONE); // catch_key 占位
    size_t catch_sz_patch  = code_.size(); emit_u32(0);         // catch_sz 占位

    size_t try_body_start = code_.size();
    while (!check(TK::RBRACE) && !check(TK::END)) {
        gen_one(loop_depth);
    }
    expect(TK::RBRACE);
    patch_u32(try_sz_patch, (uint32_t)(code_.size() - try_body_start));

    // 化块（可选）
    uint16_t catch_key = POOL_NONE;
    if (check(TK::HUA)) {
        consume(); // 化
        catch_key = add_name(expect(TK::STR).text);
        expect(TK::LBRACE);
        size_t catch_body_start = code_.size();
        while (!check(TK::RBRACE) && !check(TK::END)) {
            gen_one(loop_depth);
        }
        expect(TK::RBRACE);
        patch_u32(catch_sz_patch, (uint32_t)(code_.size() - catch_body_start));
    }
    // 回填 catch_key（u16）
    code_[catch_key_patch]   = catch_key & 0xFF;
    code_[catch_key_patch+1] = (catch_key >> 8) & 0xFF;
}

// 小畜：XIAO_XU [key:u16][val:u16]  (val=POOL_NONE → 辟空列)
void ClpsCodegen::gen_xiao_xu(size_t loop_depth) {
    consume(); // 小畜
    uint16_t key = add_name(expect(TK::STR).text);
    uint16_t val = POOL_NONE;
    TK t = peek().type;
    if (t == TK::STR || t == TK::INT || t == TK::FLOAT ||
        t == TK::DOLLAR_I || t == TK::IDENT) {
        val = gen_value_operand(loop_depth);
    }
    emit_u8((uint8_t)Op::XIAO_XU);
    emit_u16(key); emit_u16(val);
}

// 师·分兵：SHI_BEI [name:u16][path:u16]
void ClpsCodegen::gen_shi() {
    consume(); // 师
    uint16_t name = add_str_lit(expect(TK::STR).text);
    uint16_t path = add_str_lit(expect(TK::STR).text);
    emit_u8((uint8_t)Op::SHI_BEI);
    emit_u16(name); emit_u16(path);
}

// 师·合师：SHI_HE
void ClpsCodegen::gen_he() {
    consume(); // 合
    emit_u8((uint8_t)Op::SHI_HE);
}

// ── 益(42)：子程序定义/调用 ─────────────────────────────────────────────
//
// 定义（无参）：益 "name" { body }
//   → YI_DEF [name:u16][nparams:u8=0][body_sz:u32] + body + YI_RET
//
// 定义（有参）：益 "name" "p1" "p2" { body }
//   → YI_DEF [name:u16][nparams:u8=2][p0:u16][p1:u16][body_sz:u32] + body + YI_RET
//
// 调用（无参）：益 "name"
//   → YI_CALL [name:u16][nargs:u8=0]
//
// 调用（有参）：益 "name" val1 val2
//   → QIAN [p0] [val0]; QIAN [p1] [val1]; ...; YI_CALL [name:u16][nargs:u8=N]
//   （参数绑定由 codegen 展开为 QIAN，VM 无需知道参数细节）
//
// 注：YI_CALL 带 [nargs] 字段为调试用，VM 实际只靠 YI_DEF 的 params 表绑定。
void ClpsCodegen::gen_yi(size_t loop_depth) {
    consume(); // 益
    uint16_t name_idx = add_name(expect(TK::STR).text);
    std::string name_str = pool_[name_idx].str;

    if (check(TK::LBRACE)) {
        // ── 定义（无参）──
        consume(); // {
        // YI_DEF header（body_sz 占位）
        emit_u8((uint8_t)Op::YI_DEF);
        emit_u16(name_idx);
        emit_u8(0);           // nparams = 0
        size_t patch_pos = code_.size();
        emit_u32(0);          // body_sz 占位

        size_t body_start = code_.size();
        while (!check(TK::RBRACE) && !check(TK::END)) gen_one(loop_depth);
        expect(TK::RBRACE);
        emit_u8((uint8_t)Op::YI_RET);

        uint32_t body_sz = (uint32_t)(code_.size() - body_start);
        patch_u32(patch_pos, body_sz);
        return;
    }

    // ── 可能是有参定义或调用 ──
    // 先试探性消费连续 STR token，若后接 LBRACE → 有参定义；否则 → 调用
    std::vector<uint16_t> str_pool_idxs;
    std::vector<std::string> str_texts;
    while (check(TK::STR)) {
        const std::string& s = peek().text;
        str_pool_idxs.push_back(add_str_lit(s));
        str_texts.push_back(s);
        consume();
    }

    if (check(TK::LBRACE)) {
        // ── 定义（有参）——str_texts 是参数名列表 ──
        consume(); // {
        if (str_texts.size() > 255)
            throw ClpsCodegenError("益·子程序定义：参数过多（上限255）", toks_[pos_-1].line);
        uint8_t nparams = (uint8_t)str_texts.size();

        emit_u8((uint8_t)Op::YI_DEF);
        emit_u16(name_idx);
        emit_u8(nparams);
        // 参数名加入常量池（作为 Name 类型）
        std::vector<uint16_t> param_idxs;
        for (auto& pname : str_texts) param_idxs.push_back(add_name(pname));
        for (auto pi : param_idxs) emit_u16(pi);

        size_t patch_pos = code_.size();
        emit_u32(0); // body_sz 占位

        size_t body_start = code_.size();
        while (!check(TK::RBRACE) && !check(TK::END)) gen_one(loop_depth);
        expect(TK::RBRACE);
        emit_u8((uint8_t)Op::YI_RET);

        uint32_t body_sz = (uint32_t)(code_.size() - body_start);
        patch_u32(patch_pos, body_sz);
        return;
    }

    // ── 调用 ——先处理已消费的 STR 字面量作为参数值，然后继续收集其余参数 ──
    // 策略：在 YI_CALL 前发射 QIAN 绑定每个参数（参数名在 YI_DEF 时已注册在 VM 的 defs_ 表）
    // 这里 codegen 不知道参数名，所以发射 QIAN 时 key 用特殊占位符：
    // 实际由 VM 在 YI_CALL 时根据 defs_[name].params[i] 逐一绑定 — 直接在 YI_CALL 中编码 arg vals
    //
    // 格式：YI_CALL [name:u16][nargs:u8][v0:u16]...[vN:u16]
    // VM 读取 nargs 个 val pool 索引，按 defs_[name].params[i] 绑定到当前作用域
    std::vector<uint16_t> arg_vals;
    // 已消费的 STR 字面量
    for (auto idx : str_pool_idxs) arg_vals.push_back(idx);
    // 继续收集剩余参数（整数/浮点/标识符/$i）
    while (!check(TK::END) && !check(TK::RBRACE)) {
        TK t = peek().type;
        if (t == TK::INT || t == TK::FLOAT || t == TK::IDENT || t == TK::DOLLAR_I) {
            arg_vals.push_back(gen_value_operand(loop_depth));
        } else break;
    }

    emit_u8((uint8_t)Op::YI_CALL);
    emit_u16(name_idx);
    emit_u8((uint8_t)arg_vals.size());
    for (auto v : arg_vals) emit_u16(v);
}

// ── 分发 ──────────────────────────────────────────────────────────────────
void ClpsCodegen::gen_one(size_t loop_depth) {
    // 蒙卦：每条语句前发射行号标记（仅行号变化时）
    int cur_line = peek().line;
    if (cur_line != last_emitted_line_) {
        emit_u8((uint8_t)Op::LINE);
        emit_u16((uint16_t)cur_line);
        last_emitted_line_ = cur_line;
    }

    switch (peek().type) {
        case TK::JIEJIE: gen_jiejie(loop_depth); break;
        case TK::QIAN:   gen_qian  (loop_depth); break;
        case TK::ZHEN:   gen_zhen  (loop_depth); break;
        case TK::KAN:    gen_kan   (loop_depth); break;
        case TK::DUI:    gen_dui   ();           break;
        case TK::KUN:    gen_kun   (loop_depth);  break;
        case TK::LI:     gen_li    ();           break;
        case TK::XUN:    gen_xun   ();           break;
        case TK::BIAN:   gen_bian  ();           break;
        case TK::FU:     gen_fu    (loop_depth); break;
        case TK::GEN:    gen_gen   (loop_depth); break;
        case TK::SONG:   gen_song  (loop_depth); break;
        case TK::JIAREN:  gen_jiaren ();           break;
        case TK::SHI:     gen_shi    ();          break;
        case TK::HE:      gen_he     ();          break;
        case TK::HENG:    gen_heng   (loop_depth); break;
        case TK::JIAN:    gen_jian   (loop_depth); break;
        case TK::XIAO_XU: gen_xiao_xu(loop_depth); break;
        case TK::YI:      gen_yi     (loop_depth); break;
        default:
            throw ClpsCodegenError(
                std::string("代码生成：意外的词元 ") + tk_name(peek().type) +
                " \"" + peek().text + "\"", peek().line);
    }
}

// ── 入口 ──────────────────────────────────────────────────────────────────
ClpsByteCode ClpsCodegen::compile() {
    while (!check(TK::END)) {
        gen_one(0);
    }
    ClpsByteCode bc;
    bc.pool = pool_;
    bc.code = code_;
    return bc;
}

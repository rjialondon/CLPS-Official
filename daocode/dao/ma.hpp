#pragma once
/*
 * clps_bytecode.hpp — 道·码 字节码格式定义
 *
 * 操作码原则：高 3 位 = 八卦三爻（底爻=MSB）
 *   乾=111=0xE_  兑=110=0xC_  离=101=0xA_  震=100=0x8_
 *   巽=011=0x6_  坎=010=0x4_  艮=001=0x2_  坤=000=0x0_
 *
 * 坤(000)范围 0x01–0x1F 用作编译器扩展指令（作用域/循环/控制）
 *
 * 文件格式 (.clps)：
 *   [magic:4]  "CLPS"
 *   [ver:1]    0x01
 *   [rsv:1]    0x00
 *   [pool_n:2] 常量池条目数（LE）
 *   [pool...]  常量池
 *   [code_n:4] 字节码长度（LE）
 *   [code...]  字节码
 */

#include <cstdint>
#include <string>
#include <vector>

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  常量池条目类型
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
enum class PoolType : uint8_t {
    Name    = 0,  // 变量名 / 模块名 / 函数名（字符串，作键使用）
    StrLit  = 1,  // 字符串字面量（值）
    VarRef  = 2,  // 变量引用（运行时从作用域读取）
    Int64   = 3,  // 64位整数字面量
    Float64 = 4,  // 64位浮点字面量
    Wx      = 5,  // 五行类型（0=金 1=水 2=木 3=火 4=土）
};

// 特殊池索引
static constexpr uint16_t POOL_DOLLAR_I = 0xFFFF; // $i 循环索引
static constexpr uint16_t POOL_NONE     = 0xFFFE; // 无值（可选操作数）

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  操作码
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
enum class Op : uint8_t {
    // ── 坤范围：编译器扩展（000_XXXXX）──────────────────
    KUN       = 0x00,  // 坤·归元（释放当前作用域）          无操作数
    SCOPE_PUSH= 0x01,  // 作用域压入                        [name:u16]
    SCOPE_POP = 0x02,  // 作用域弹出                        无
    LOOP_BEG  = 0x03,  // 复·循环开始  [ctype:u8][cnt:u16][body_sz:u32]
    LOOP_END  = 0x04,  // 复·循环结束                       无
    BIAN      = 0x05,  // 变·爻变      [key:u16][wx:u8]
    SONG_CMP  = 0x06,  // 讼·断言      [cmp:u8][key:u16][val:u16]
    JIAREN    = 0x07,  // 家人·加载    [name:u16][path:u16]
    LINE      = 0x08,  // 蒙·行号标记  [line:u16]  （调试用，不影响语义）
    SHI_BEI   = 0x09,  // 师·分兵      [name:u16][path:u16]  异步启动子任务
    SHI_HE    = 0x0A,  // 师·合师      无操作数，等待所有子任务完成
    HENG      = 0x0B,  // 恒·只读      [key:u16][val:u16]
    HENG_WX   = 0x0C,  // 恒·只读+五行 [key:u16][val:u16][wx:u8]
    JIAN_BEG  = 0x0D,  // 蹇·错误块    [try_sz:u32][catch_key:u16][catch_sz:u32] + try body + catch body
    XIAO_XU   = 0x0E,  // 小畜·积累    [key:u16][val:u16]  (val=POOL_NONE→辟空列)
    KUN_LIST  = 0x12,  // 坤·归列      [key:u16][count:u16]  截断列表到前 count 项

    // ── 益(42)·子程序 ─────────────────────────────────
    // YI_DEF: 定义子程序，跳过 body（回填法，同 LOOP_BEG）
    //   [name:u16][nparams:u8][p0:u16]...[body_sz:u32]  +  body  +  YI_RET
    // YI_CALL: 调用子程序（call stack 保存返回地址）
    //   [name:u16]（参数绑定由调用方提前通过 QIAN 完成）
    // YI_RET: 子程序返回（弹出 call stack，恢复 ip）
    YI_DEF    = 0x0F,
    YI_CALL   = 0x10,
    YI_RET    = 0x11,

    // ── 艮（001_00000）: 条件门控 ──────────────────────
    GEN_CMP   = 0x20,  // 艮           [cmp:u8][key:u16][val:u16][skip:u16]
    GEN_ELSE  = 0x21,  // 否           [else_sz:u16]  — if 块末跳过 else 块

    // ── 坎（010_00000）: 安全运算 ──────────────────────
    KAN_OP    = 0x40,  // 坎           [op:u8][key:u16][val:u16]

    // ── 巽（011_00000）: 遍历 ──────────────────────────
    XUN       = 0x60,  // 巽           [pat:u16] (POOL_NONE=无模式)

    // ── 震（100_XXXXX）: 运算/库调用 ───────────────────
    ZHEN_OP   = 0x80,  // 震·算术      [op:u8][key:u16][val:u16]
    ZHEN_CALL = 0x84,  // 震·库(有返回)[res:u16][mod:u16][fn:u16][argc:u8][args:u16...]
    ZHEN_SFX  = 0x85,  // 震·库(副作用)[mod:u16][fn:u16][argc:u8][args:u16...]

    // ── 离（101_00000）: 跨界引用 ──────────────────────
    LI        = 0xA0,  // 离           [key:u16][scope:u16][src:u16]

    // ── 兑（110_00000）: 输出 ──────────────────────────
    DUI       = 0xC0,  // 兑           [key:u16]

    // ── 乾（111_XXXXX）: 存入 ──────────────────────────
    QIAN      = 0xE0,  // 乾           [key:u16][val:u16]
    QIAN_WX   = 0xE1,  // 乾·显式五行  [key:u16][val:u16][wx:u8]
};

// 算术操作符编码（用于 ZHEN_OP/KAN_OP 的 op:u8 字段）
static constexpr uint8_t AOP_ADD = '+';
static constexpr uint8_t AOP_SUB = '-';
static constexpr uint8_t AOP_MUL = '*';
static constexpr uint8_t AOP_DIV = '/';
static constexpr uint8_t AOP_MOD = '%';

// 比较操作符编码（用于 GEN_CMP/SONG_CMP 的 cmp:u8 字段）
static constexpr uint8_t CMP_EQ  = 0;
static constexpr uint8_t CMP_NEQ = 1;
static constexpr uint8_t CMP_GT  = 2;
static constexpr uint8_t CMP_LT  = 3;
static constexpr uint8_t CMP_GTE = 4;
static constexpr uint8_t CMP_LTE = 5;

// LOOP_BEG 的 count type
static constexpr uint8_t LOOP_LIT = 0; // count 是字面量
static constexpr uint8_t LOOP_VAR = 1; // count 是变量名池索引

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  常量池条目（内存结构）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
struct PoolEntry {
    PoolType type;
    std::string str;    // Name / StrLit / VarRef
    int64_t  ival = 0;  // Int64
    double   dval = 0;  // Float64
    uint8_t  wx   = 0;  // Wx
};

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  编译产物
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
struct ClpsByteCode {
    std::vector<PoolEntry>  pool;
    std::vector<uint8_t>    code;

    // 序列化为字节流（写入文件）
    std::vector<uint8_t> serialize() const;

    // 从字节流加载
    static ClpsByteCode deserialize(const std::vector<uint8_t>& data);
};

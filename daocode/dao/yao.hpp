#pragma once
/*
 * clps_token.hpp — 道·码 词元（Token）定义
 *
 * 八卦映射（底爻=MSB）：
 *   乾=111=2.90V  兑=110=2.50V  离=101=2.10V  震=100=1.70V
 *   巽=011=1.30V  坎=010=0.90V  艮=001=0.50V  坤=000=0.00V
 */

#include <string>
#include <cstdint>

// ── 词元类型 ──────────────────────────────────────────────────────────────
enum class TK : uint8_t {
    // 八卦关键字（操作码）
    JIEJIE,  // 结界  — 作用域
    QIAN,    // 乾    — 存入资源（111）
    ZHEN,    // 震    — 运算，克制则抛异常（100）
    KAN,     // 坎    — 安全运算，克制则静默（010）
    DUI,     // 兑    — 读取/输出（110）
    KUN,     // 坤    — 归元/销毁（000）
    LI,      // 离    — 跨界引用（101）
    XUN,     // 巽    — 遍历（011）
    GEN,     // 艮    — 锁（001）
    BIAN,    // 变    — 爻变（改五行）
    FU,      // 复    — 循环
    SONG,    // 讼    — 断言/判断（讼卦·天与水违行）
    JIAREN,  // 家人  — 模块加载（家人卦·风自火出）
    SHI,     // 师    — 异步分兵（师卦·地中有水）
    HE,      // 合    — 合师等待（合师·众归）
    XIAO_XU, // 小畜  — 列表积累（小畜卦·密云不雨）
    HENG,    // 恒    — 只读常量（恒卦·雷风恒·32）
    JIAN,    // 蹇    — 错误恢复 try 块（蹇卦·水山蹇·39）
    HUA,     // 化    — 错误恢复 catch 块（配合蹇卦）
    YI,      // 益    — 用户自定义子程序（益卦·风雷益·42）
    POU,     // 否    — 艮的 else 分支（否卦·天地否·12；天地不交·封闭路径）
    // 注：艮 (GEN) 已在上方声明，用于条件分支（止·门控）

    // 五行关键字
    JIN,     // 金
    SHUI,    // 水
    MU,      // 木
    HUO,     // 火
    TU,      // 土

    // 字面量
    STR,       // "文字"
    INT,       // 42
    FLOAT,     // 3.14
    DOLLAR_I,  // $i  — 循环索引

    // 算术运算符
    PLUS,    // +
    MINUS,   // -
    STAR,    // *
    SLASH,   // /
    PERCENT, // %  取余（模运算）

    // 比较运算符
    EQ,      // ==
    NEQ,     // !=
    GT,      // >
    LT,      // <
    GTE,     // >=
    LTE,     // <=

    // 结构
    LBRACE,  // {
    RBRACE,  // }

    // 万物库模块名：【大衍之数】→ text="大衍之数"
    MODULE,

    // 通配符 / 标识符
    WILDCARD,  // *前缀 / *词* / 后缀*
    IDENT,     // 其他标识符（结界名、key 名等）

    // 结束
    END,
};

// ── Token 结构体 ──────────────────────────────────────────────────────────
struct Token {
    TK          type  = TK::END;
    std::string text;          // 原始文本（key名、字符串内容等）
    int64_t     ival  = 0;
    double      dval  = 0.0;
    int         line  = 1;
};

// ── 辅助：类型名（调试用）────────────────────────────────────────────────
inline const char* tk_name(TK t) {
    switch (t) {
        case TK::JIEJIE:   return "结界";
        case TK::QIAN:     return "乾";
        case TK::ZHEN:     return "震";
        case TK::KAN:      return "坎";
        case TK::DUI:      return "兑";
        case TK::KUN:      return "坤";
        case TK::LI:       return "离";
        case TK::XUN:      return "巽";
        case TK::GEN:      return "艮";
        case TK::BIAN:     return "变";
        case TK::FU:       return "复";
        case TK::SONG:     return "讼";
        case TK::JIAREN:   return "家人";
        case TK::SHI:      return "师";
        case TK::HE:       return "合";
        case TK::XIAO_XU:  return "小畜";
        case TK::HENG:     return "恒";
        case TK::JIAN:     return "蹇";
        case TK::HUA:      return "化";
        case TK::YI:       return "益";
        case TK::POU:      return "否";
        case TK::JIN:      return "金";
        case TK::SHUI:     return "水";
        case TK::MU:       return "木";
        case TK::HUO:      return "火";
        case TK::TU:       return "土";
        case TK::STR:      return "STR";
        case TK::INT:      return "INT";
        case TK::FLOAT:    return "FLOAT";
        case TK::DOLLAR_I: return "$i";
        case TK::PLUS:     return "+";
        case TK::MINUS:    return "-";
        case TK::STAR:     return "*";
        case TK::SLASH:    return "/";
        case TK::PERCENT:  return "%";
        case TK::EQ:       return "==";
        case TK::NEQ:      return "!=";
        case TK::GT:       return ">";
        case TK::LT:       return "<";
        case TK::GTE:      return ">=";
        case TK::LTE:      return "<=";
        case TK::LBRACE:   return "{";
        case TK::RBRACE:   return "}";
        case TK::MODULE:   return "MODULE";
        case TK::WILDCARD: return "WILDCARD";
        case TK::IDENT:    return "IDENT";
        case TK::END:      return "END";
        default:           return "?";
    }
}

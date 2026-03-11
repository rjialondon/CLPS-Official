// tokdump.cpp — 词元转储工具（T10b对比验证用）
// 用法: tokdump <文件.dao>
// 输出格式: TYPE|text|line （与 tun.dao 输出格式一致）
#include "dao/tun.hpp"
#include "dao/yao.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

static const char* tk_enum_name(TK t) {
    switch (t) {
        case TK::JIEJIE:   return "JIEJIE";
        case TK::QIAN:     return "QIAN";
        case TK::ZHEN:     return "ZHEN";
        case TK::KAN:      return "KAN";
        case TK::DUI:      return "DUI";
        case TK::KUN:      return "KUN";
        case TK::LI:       return "LI";
        case TK::XUN:      return "XUN";
        case TK::GEN:      return "GEN";
        case TK::BIAN:     return "BIAN";
        case TK::FU:       return "FU";
        case TK::SONG:     return "SONG";
        case TK::JIAREN:   return "JIAREN";
        case TK::SHI:      return "SHI";
        case TK::HE:       return "HE";
        case TK::XIAO_XU:  return "XIAO_XU";
        case TK::HENG:     return "HENG";
        case TK::JIAN:     return "JIAN";
        case TK::HUA:      return "HUA";
        case TK::YI:       return "YI";
        case TK::POU:      return "POU";
        case TK::JIN:      return "JIN";
        case TK::SHUI:     return "SHUI";
        case TK::MU:       return "MU";
        case TK::HUO:      return "HUO_WX";
        case TK::TU:       return "TU_WX";
        case TK::STR:      return "STR";
        case TK::INT:      return "INT";
        case TK::FLOAT:    return "FLOAT";
        case TK::DOLLAR_I: return "DOLLAR_I";
        case TK::PLUS:     return "PLUS";
        case TK::MINUS:    return "MINUS";
        case TK::STAR:     return "STAR";
        case TK::SLASH:    return "SLASH";
        case TK::PERCENT:  return "PERCENT";
        case TK::EQ:       return "EQ";
        case TK::NEQ:      return "NEQ";
        case TK::GT:       return "GT";
        case TK::LT:       return "LT";
        case TK::GTE:      return "GTE";
        case TK::LTE:      return "LTE";
        case TK::LBRACE:   return "LBRACE";
        case TK::RBRACE:   return "RBRACE";
        case TK::MODULE:   return "MODULE";
        case TK::WILDCARD: return "WILDCARD";
        case TK::IDENT:    return "IDENT";
        case TK::END:      return "END";
        default:           return "?";
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "用法: tokdump <文件.dao>\n";
        return 1;
    }
    std::ifstream f(argv[1]);
    if (!f) {
        std::cerr << "无法打开: " << argv[1] << "\n";
        return 1;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    ClpsLexer lex(ss.str());
    auto tokens = lex.tokenize();
    for (auto& t : tokens) {
        std::cout << tk_enum_name(t.type) << "|" << t.text << "|" << t.line << "\n";
    }
    return 0;
}

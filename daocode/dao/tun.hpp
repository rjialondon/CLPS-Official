#pragma once
/*
 * clps_lexer.hpp — 道·码 词法分析器
 *
 * 输入：UTF-8 编码的 .dao 源文件字符串
 * 输出：Token 序列
 *
 * 特性：
 * - 识别八卦关键字（中文多字节匹配）
 * - 识别五行关键字
 * - 字符串字面量（双引号）
 * - 整数 / 浮点数
 * - 注释（# 到行尾）
 * - $i 循环索引
 */

#include "yao.hpp"
#include <vector>
#include <string>
#include <stdexcept>

class ClpsLexError : public std::runtime_error {
public:
    int line;
    ClpsLexError(const std::string& msg, int ln)
        : std::runtime_error(msg), line(ln) {}
};

class ClpsLexer {
public:
    explicit ClpsLexer(std::string src);
    std::vector<Token> tokenize();

private:
    std::string src_;
    size_t      pos_ = 0;
    int         line_ = 1;

    bool at_end() const { return pos_ >= src_.size(); }
    char peek()   const { return at_end() ? '\0' : src_[pos_]; }
    char peek2()  const { return (pos_+1 < src_.size()) ? src_[pos_+1] : '\0'; }
    char advance()      { char c = src_[pos_++]; if (c=='\n') ++line_; return c; }

    // 从当前 pos_ 开始，src_ 是否以 utf8 字符串 s 开头
    bool starts_with(const char* s) const;

    void skip_whitespace_and_comments();
    Token read_string();
    Token read_number();
    Token read_ident_or_keyword();
    Token make(TK t, std::string text = "") {
        return Token{t, std::move(text), 0, 0.0, line_};
    }
};

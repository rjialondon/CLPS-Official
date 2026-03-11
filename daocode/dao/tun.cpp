#include "tun.hpp"
#include "cihai.hpp"
#include <cstring>
#include <cctype>
#include <stdexcept>

// ── 关键字表（UTF-8 字符串 → TK）────────────────────────────────────────
struct KW { const char* utf8; TK tk; };

static const KW KEYWORDS[] = {
    // 多字关键字先匹配（避免首字单独被识别成IDENT）
    { "结界", TK::JIEJIE  },
    { "家人", TK::JIAREN  },
    { "小畜", TK::XIAO_XU },
    { "否",   TK::POU     },  // 否卦(12)·天地不交·艮的 else 分支
    // 师卦
    { "师",   TK::SHI   },
    { "合",   TK::HE    },
    // 单字八卦
    { "乾",   TK::QIAN  },
    { "震",   TK::ZHEN  },
    { "坎",   TK::KAN   },
    { "兑",   TK::DUI   },
    { "坤",   TK::KUN   },
    { "离",   TK::LI    },
    { "巽",   TK::XUN   },
    { "艮",   TK::GEN   },
    { "变",   TK::BIAN  },
    { "复",   TK::FU    },
    { "讼",   TK::SONG  },
    // 恒卦(32)/蹇卦(39)
    { "恒",   TK::HENG  },
    { "蹇",   TK::JIAN  },
    { "化",   TK::HUA   },
    { "益",   TK::YI    },  // 益卦(42)：用户自定义子程序
    // 五行
    { "金",   TK::JIN   },
    { "水",   TK::SHUI  },
    { "木",   TK::MU    },
    { "火",   TK::HUO   },
    { "土",   TK::TU    },
    { nullptr, TK::END  },
};

// ── 构造 ─────────────────────────────────────────────────────────────────
ClpsLexer::ClpsLexer(std::string src) : src_(std::move(src)) {}

// ── starts_with：逐字节比较 ───────────────────────────────────────────────
bool ClpsLexer::starts_with(const char* s) const {
    size_t len = strlen(s);
    if (pos_ + len > src_.size()) return false;
    return memcmp(src_.data() + pos_, s, len) == 0;
}

// ── 跳过空白与注释 ────────────────────────────────────────────────────────
void ClpsLexer::skip_whitespace_and_comments() {
    while (!at_end()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '#') {
            // 注释：跳到行尾
            while (!at_end() && peek() != '\n') advance();
        } else {
            break;
        }
    }
}

// ── 读取字符串字面量："..." ───────────────────────────────────────────────
Token ClpsLexer::read_string() {
    advance(); // 跳过开头 "
    std::string val;
    while (!at_end() && peek() != '"') {
        char c = advance();
        if (c == '\\') {
            char esc = advance();
            switch (esc) {
                case 'n':  val += '\n'; break;
                case 'r':  val += '\r'; break;
                case 't':  val += '\t'; break;
                case '"':  val += '"';  break;
                case '\\': val += '\\'; break;
                default:   val += esc;
            }
        } else {
            val += c;
        }
    }
    if (at_end()) throw ClpsLexError("字符串未闭合", line_);
    advance(); // 跳过结尾 "
    return Token{TK::STR, val, 0, 0.0, line_};
}

// ── 读取数字字面量 ────────────────────────────────────────────────────────
Token ClpsLexer::read_number() {
    std::string s;
    bool is_float = false;
    bool is_neg = false;

    if (peek() == '-') { is_neg = true; s += advance(); }

    while (!at_end() && std::isdigit((unsigned char)peek())) s += advance();
    if (!at_end() && peek() == '.') {
        is_float = true;
        s += advance();
        while (!at_end() && std::isdigit((unsigned char)peek())) s += advance();
    }

    if (is_float) {
        double v = std::stod(s);
        return Token{TK::FLOAT, s, 0, v, line_};
    } else {
        int64_t v = std::stoll(s);
        return Token{TK::INT, s, v, 0.0, line_};
    }
}

// ── UTF-8 字符宽度（字节数）──────────────────────────────────────────────
static size_t utf8_char_len(unsigned char c) {
    if      ((c & 0x80) == 0x00) return 1;
    else if ((c & 0xE0) == 0xC0) return 2;
    else if ((c & 0xF0) == 0xE0) return 3;
    else if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

// ── 读取标识符 / 关键字 ───────────────────────────────────────────────────
Token ClpsLexer::read_ident_or_keyword() {
    // 先尝试双字关键字，再尝试单字关键字（按表顺序）
    for (const KW* kw = KEYWORDS; kw->utf8 != nullptr; ++kw) {
        if (starts_with(kw->utf8)) {
            size_t len = strlen(kw->utf8);
            std::string text(src_.data() + pos_, len);
            pos_ += len;
            return Token{kw->tk, text, 0, 0.0, line_};
        }
    }

    // 通配符模式（含 * 的 key 模式，如 账*  *户  *词*）
    // 以 * 开头，或含 *
    // 这里只采集连续的非空白非{}"# 字符
    std::string id;
    while (!at_end()) {
        unsigned char c = (unsigned char)peek();
        // 停止字符
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
            c == '{' || c == '}' || c == '"'  || c == '#')
            break;
        size_t w = utf8_char_len(c);
        for (size_t i = 0; i < w && !at_end(); ++i)
            id += src_[pos_++];
    }

    if (id.empty())
        throw ClpsLexError(std::string("无法识别的字符: ") + peek(), line_);

    // 含 * 的视为通配符模式
    if (id.find('*') != std::string::npos)
        return Token{TK::WILDCARD, id, 0, 0.0, line_};

    // 辞海·近义词折叠（词法层，零运行时开销）
    // 中文近义词 / 英文词 → 规范 TK，原文保留在 text 中
    TK syn = cihai_lookup(id);
    if (syn != TK::END)
        return Token{syn, id, 0, 0.0, line_};

    return Token{TK::IDENT, id, 0, 0.0, line_};
}

// ── 主分词循环 ────────────────────────────────────────────────────────────
std::vector<Token> ClpsLexer::tokenize() {
    std::vector<Token> tokens;

    while (true) {
        skip_whitespace_and_comments();
        if (at_end()) break;

        int cur_line = line_;
        char c = peek();

        // ASCII 单字符
        if (c == '"') {
            tokens.push_back(read_string());
            continue;
        }
        if (c == '{') { advance(); tokens.push_back(make(TK::LBRACE, "{")); continue; }
        if (c == '}') { advance(); tokens.push_back(make(TK::RBRACE, "}")); continue; }
        if (c == '+') { advance(); tokens.push_back(make(TK::PLUS,   "+")); continue; }
        if (c == '-') {
            // 负数：- 后紧跟数字
            if (std::isdigit((unsigned char)peek2())) {
                tokens.push_back(read_number());
            } else {
                advance();
                tokens.push_back(make(TK::MINUS, "-"));
            }
            continue;
        }
        if (c == '/') { advance(); tokens.push_back(make(TK::SLASH,   "/")); continue; }
        if (c == '%') { advance(); tokens.push_back(make(TK::PERCENT,"%")); continue; }

        // 比较运算符
        if (c == '=') {
            advance();
            if (!at_end() && peek() == '=') { advance(); tokens.push_back(make(TK::EQ, "==")); }
            else throw ClpsLexError("孤立的 '=' 不合法，请使用 '=='", line_);
            continue;
        }
        if (c == '!') {
            advance();
            if (!at_end() && peek() == '=') { advance(); tokens.push_back(make(TK::NEQ, "!=")); }
            else throw ClpsLexError("孤立的 '!' 不合法，请使用 '!='", line_);
            continue;
        }
        if (c == '>') {
            advance();
            if (!at_end() && peek() == '=') { advance(); tokens.push_back(make(TK::GTE, ">=")); }
            else tokens.push_back(make(TK::GT, ">"));
            continue;
        }
        if (c == '<') {
            advance();
            if (!at_end() && peek() == '=') { advance(); tokens.push_back(make(TK::LTE, "<=")); }
            else tokens.push_back(make(TK::LT, "<"));
            continue;
        }

        // * 单独出现（不和标识符连在一起）→ STAR
        if (c == '*') {
            // 看下一个字符是否是空白/结构符 → 单独 *
            char n = peek2();
            if (n == ' ' || n == '\t' || n == '\n' || n == '\r' ||
                n == '{' || n == '"' || n == '\0') {
                advance();
                tokens.push_back(make(TK::STAR, "*"));
            } else {
                // 通配符模式，如 *词* *后缀
                tokens.push_back(read_ident_or_keyword());
            }
            continue;
        }

        // 万物库模块名：【...】→ MODULE token
        // 【 = E3 80 90, 】= E3 80 91
        if ((unsigned char)c == 0xE3 &&
            pos_ + 2 < src_.size() &&
            (unsigned char)src_[pos_+1] == 0x80 &&
            (unsigned char)src_[pos_+2] == 0x90) {
            pos_ += 3; // 跳过 【
            std::string name;
            // 读到 】(E3 80 91) 为止
            while (!at_end()) {
                if ((unsigned char)src_[pos_] == 0xE3 &&
                    pos_ + 2 < src_.size() &&
                    (unsigned char)src_[pos_+1] == 0x80 &&
                    (unsigned char)src_[pos_+2] == 0x91) {
                    pos_ += 3; // 跳过 】
                    break;
                }
                name += src_[pos_++];
            }
            tokens.push_back(Token{TK::MODULE, name, 0, 0.0, cur_line});
            continue;
        }

        // $i
        if (c == '$' && peek2() == 'i') {
            advance(); advance();
            tokens.push_back(make(TK::DOLLAR_I, "$i"));
            continue;
        }

        // 数字
        if (std::isdigit((unsigned char)c)) {
            tokens.push_back(read_number());
            continue;
        }

        // 多字节 UTF-8（中文关键字/标识符）或其他
        tokens.push_back(read_ident_or_keyword());
    }

    tokens.push_back(make(TK::END, ""));
    return tokens;
}

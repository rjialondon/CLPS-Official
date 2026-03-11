#pragma once
/*
 * clps_parser.hpp — 道·码 扁平坍缩法解析/执行器
 *
 * 原理（PDF15·红方）：
 *   不建树、不深递归。遇 { 压帧，内卦成型（遇 }）立即坍缩执行，
 *   结果直达外卦，外卦再收拢……如此迭代，永远保持扁平态。
 *
 * 执行模型：
 *   每个 Frame 对应一个 结界 或 复 的作用域。
 *   Frame 内顺序执行 Statement，Statement 直接调用 C++ VM。
 */

#include "yao.hpp"
#include "wanwu.hpp"
#include "../taiji/wuxing.hpp"
#include "../taiji/jiejie.hpp"
#include <vector>
#include <memory>
#include <string>
#include <functional>
#include <stdexcept>
#include <unordered_map>
#include <future>

// ── 解析执行错误 ──────────────────────────────────────────────────────────
class ClpsParseError : public std::runtime_error {
public:
    int line;
    ClpsParseError(const std::string& msg, int ln = 0)
        : std::runtime_error(msg), line(ln) {}
};

// ── 输出回调（默认 stdout，可重定向）────────────────────────────────────
using ClpsOutputFn = std::function<void(const std::string&)>;

// ── 解析+执行器 ───────────────────────────────────────────────────────────
class ClpsParser {
public:
    explicit ClpsParser(std::vector<Token> tokens,
                        ClpsOutputFn output_fn = nullptr);

    // 顶层执行（扁平坍缩法入口）
    void run();

    // REPL 专用：注入新词元，在当前持久作用域内执行
    void exec_snippet(const std::vector<Token>& new_tokens);

    // 顶层 结界 — 由外部注入（用于模块系统）
    std::shared_ptr<TaiJiJieJie> root_jj;

private:
    std::vector<Token> toks_;
    size_t             pos_  = 0;
    ClpsOutputFn       out_;

    // 符号表栈（结界层级）
    std::vector<std::shared_ptr<TaiJiJieJie>> scope_stack_;

    // 已加载的家人模块（名称 → 根结界）
    std::unordered_map<std::string, std::shared_ptr<TaiJiJieJie>> modules_;

    // 师卦：异步任务（解释器模式）
    struct ShiParserTask {
        std::future<std::shared_ptr<TaiJiJieJie>> fut;
        std::shared_ptr<std::vector<std::string>> buf;
    };
    std::unordered_map<std::string, ShiParserTask> pending_shi_;

    // 益卦(42)：用户自定义子程序（token 范围表）
    // key → {params, body_start, body_end}
    //   params:     参数名列表（空则无参，按序与调用时传值对应）
    //   body_start: { 后第一个 token 位置
    //   body_end:   } 的 token 位置（不含）
    struct YiDef {
        std::vector<std::string> params;
        size_t body_start;
        size_t body_end;
    };
    std::unordered_map<std::string, YiDef> yi_defs_;

    // 当前作用域
    std::shared_ptr<TaiJiJieJie>& cur() { return scope_stack_.back(); }

    // Token 操作
    const Token& peek()  const { return toks_[pos_]; }
    const Token& peek2() const {
        return (pos_+1 < toks_.size()) ? toks_[pos_+1] : toks_.back();
    }
    const Token& consume();                 // 无条件前进
    const Token& expect(TK t);             // 消费并检查类型
    bool check(TK t) const { return peek().type == t; }

    // 解析辅助
    ClpsValue     parse_value(size_t loop_i = 0);   // STR|INT|FLOAT|IDENT|$i
    WX            parse_wx();                         // 金水木火土
    char          parse_op();                         // +-*/
    std::optional<WX> try_parse_wx();
    TK            parse_cmp_op();                     // == != > < >= <=

    // 比较辅助（静态，用于 艮/讼）
    static bool   compare_clps(const ClpsValue& lhs, const ClpsValue& rhs, TK op);
    static std::string cmp_op_str(TK op);

    // 语句解析（扁平坍缩：遇 { 入栈，遇 } 出栈）
    void exec_statements(std::shared_ptr<TaiJiJieJie> jj, size_t loop_i = 0);
    void exec_one(size_t loop_i);

    // 各卦执行
    void exec_qian(size_t loop_i);
    void exec_zhen(size_t loop_i);
    void exec_zhen_module(const std::string& result_key, size_t loop_i);
    void exec_kan (size_t loop_i);
    void exec_dui (size_t loop_i);
    void exec_kun (size_t loop_i);
    void exec_li  (size_t loop_i);
    void exec_xun (size_t loop_i);
    void exec_bian();
    void exec_fu  (size_t loop_i);
    void exec_jiejie(size_t loop_i);
    void exec_gen   (size_t loop_i);  // 艮：条件分支（止·门控）
    void exec_song  (size_t loop_i);  // 讼：断言（诉讼·判断）
    void exec_jiaren(size_t loop_i);  // 家人：模块加载（外部.dao → 独立结界）
    void exec_shi    (size_t loop_i);  // 师：异步分兵
    void exec_he     (size_t loop_i);  // 合：合师等待
    void exec_xiao_xu(size_t loop_i);  // 小畜：列表积累
    void exec_heng   (size_t loop_i);  // 恒(32)：只读常量声明
    void exec_jian   (size_t loop_i);  // 蹇(39)：错误恢复 try/catch
    void exec_yi     (size_t loop_i);  // 益(42)：用户自定义子程序 定义/调用

    // 输出
    void emit(const std::string& s);
};

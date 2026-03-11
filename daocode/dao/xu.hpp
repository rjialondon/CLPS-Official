#pragma once
/*
 * clps_checker.hpp — 道·码 静态五行检查器（需卦）
 *
 * 需卦象意：水在天上，雨尚未降，等待时机，提前预判。
 * 在代码执行前扫描，编译期检测五行克制冲突，
 * 优于运行时崩溃，优于无辜的沉默失败。
 *
 * 设计原则：
 *   - 震 (100) + 克制 → ERROR（运行会抛出，必须报）
 *   - 坎 (010) + 克制 → WARNING（运行静默，但开发者应知晓）
 *   - 变 之后重新推断（五行可变）
 *   - 结界/复 各自维护作用域符号表
 *   - 未知五行（引用外部变量等）→ 保守放行
 */

#include "yao.hpp"
#include "../taiji/wuxing.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <optional>

// ── 诊断条目 ─────────────────────────────────────────────────────────────
struct ClpsDiag {
    enum class Level { Error, Warning, Info };
    Level       level;
    int         line;
    std::string msg;

    bool is_error()   const { return level == Level::Error;   }
    bool is_warning() const { return level == Level::Warning; }
};

// ── 静态五行检查器 ────────────────────────────────────────────────────────
class ClpsChecker {
public:
    explicit ClpsChecker(const std::vector<Token>& tokens);

    // 执行检查，返回诊断列表
    // 若含 Error 级别，不应继续执行
    std::vector<ClpsDiag> check();

private:
    const std::vector<Token>& toks_;
    size_t pos_ = 0;

    // 作用域：key → 五行
    struct Scope {
        std::string name;
        std::unordered_map<std::string, WX> vars;
    };
    std::vector<Scope> scopes_;
    std::vector<ClpsDiag> diags_;

    // Token 操作（只读，不修改状态，只前进 pos_）
    const Token& peek()  const { return toks_[pos_]; }
    bool check_tk(TK t)  const { return peek().type == t; }
    const Token& consume();
    bool try_consume(TK t);

    // 五行推断
    std::optional<WX> infer_wx_of_value();  // 从当前token推断操作数五行
    std::optional<WX> lookup_var_wx(const std::string& key) const;
    void set_var_wx(const std::string& key, WX wx);

    // 跳过一个 { ... } 块（含嵌套）
    void skip_block();

    // 各语句检查
    void check_block();              // { statement* }
    void check_stmt();
    void check_qian();
    void check_zhen();
    void check_kan();
    void check_dui();
    void check_kun();
    void check_li();
    void check_xun();
    void check_bian();
    void check_fu();
    void check_jiejie();
    void check_gen();     // 艮：条件分支（消费语法结构，检查块体）
    void check_song();    // 讼：断言（消费语法结构）
    void check_jiaren();  // 家人：模块加载（消费语法，跨文件五行未知→保守放行）
    void check_shi();      // 师：异步分兵（消费 STR STR，保守放行）
    void check_he();       // 合：合师（消费无操作数）
    void check_xiao_xu();  // 小畜：列表积累（消费 STR [值]，保守放行）
    void check_heng();     // 恒(32)：只读常量声明（语法同乾）
    void check_jian();     // 蹇(39)：错误恢复（try/catch 块）
    void check_yi();       // 益(42)：用户自定义子程序（定义/调用）

    // 诊断辅助
    void error  (int line, const std::string& msg);
    void warning(int line, const std::string& msg);
    void info   (int line, const std::string& msg);
};

// ── 打印诊断（工具函数）──────────────────────────────────────────────────
void print_diags(const std::vector<ClpsDiag>& diags,
                 const std::string& filename = "");
bool diags_has_error(const std::vector<ClpsDiag>& diags);

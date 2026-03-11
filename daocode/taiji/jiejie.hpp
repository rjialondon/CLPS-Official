#pragma once
/*
 * taiji_jiejie.hpp  — 太极结界（纯C++，零Python依赖）
 */

#include "wuxing.hpp"
#include <optional>
#include <vector>
#include <unordered_set>

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Batch 操作类型（batch_lifecycle 参数）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

enum class BatchOperandKind { Int, Float, LoopVar };

struct BatchOperand {
    BatchOperandKind kind = BatchOperandKind::Int;
    int64_t ival = 0;
    double  fval = 0.0;

    double resolve(size_t i) const {
        switch (kind) {
            case BatchOperandKind::Float:   return fval;
            case BatchOperandKind::LoopVar: return static_cast<double>(i);
            default:                        return static_cast<double>(ival);
        }
    }
    WX wx() const { return WX::Jin; }  // 数值操作数均为金
};

enum class BatchOpKind { Qian, Zhen, Dui };

struct BatchOp {
    BatchOpKind  kind    = BatchOpKind::Dui;
    std::string  key;
    BatchOperand val;      // Qian 用
    WX           xing = WX::Jin;  // Qian 用
    char         op_char = '+';   // Zhen 用
    BatchOperand operand;  // Zhen 用
};

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  TaiJiJieJie
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

class TaiJiJieJie {
public:
    std::string  name;
    int64_t      level;
    ClpsMap      resources;
    bool         alive;
    std::unordered_map<std::string, WX>  meta;

    // 恒卦(32)：只读key集合
    std::unordered_set<std::string> readonly_keys;

    // 父引用（optional，避免循环引用问题）
    std::shared_ptr<TaiJiJieJie> parent;

    // 离卦引用：key → (源结界, 源key)
    std::unordered_map<std::string,
        std::pair<std::shared_ptr<TaiJiJieJie>, std::string>> refs;

    explicit TaiJiJieJie(std::string name, int64_t level = 0,
                         std::shared_ptr<TaiJiJieJie> parent = nullptr);

    // ── 八卦操作 ─────────────────────────────────────
    void      qian(const std::string& key, ClpsValue val,
                   std::optional<WX> xing = {});
    ClpsValue zhen(const std::string& key, char op, ClpsValue operand);
    ClpsValue dui (const std::string& key) const;
    void      kun ();
    void      kun_list(const std::string& key, size_t count);

    std::shared_ptr<TaiJiJieJie> qi_child(const std::string& child_name);

    ClpsMap xun(std::optional<std::string> pattern = {}) const;

    std::pair<bool, ClpsValue> kan(const std::string& key, char op,
                                   ClpsValue operand);

    void li  (const std::string& key,
              std::shared_ptr<TaiJiJieJie> src_jj,
              const std::string& src_key);

    void bian(const std::string& key, WX new_xing);

    // 小畜：辟出列表或向列表追加值
    void xiao_xu(const std::string& key, std::optional<ClpsValue> val = {});

    // 恒(32)：声明只读常量（存入 + 标记 readonly）
    void heng(const std::string& key, ClpsValue val,
              std::optional<WX> xing = {});

    // ── 批处理（热路径，全程无Python对象）────────────
    void batch_lifecycle  (size_t n, const std::vector<BatchOp>& ops);
    void chain_grow_shrink(size_t depth);
};

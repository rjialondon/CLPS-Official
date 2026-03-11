#pragma once
/*
 * clps_value.hpp  — CLPS 原生值类型
 * 零 Python/pybind11 依赖，可直接嵌入任何 C++ 程序
 */

#include <variant>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <stdexcept>
#include <cstdint>

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  WX enum — 五行
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

enum class WX : uint8_t { Jin=0, Shui=1, Mu=2, Huo=3, Tu=4 };

inline WX wx_from_str(const std::string& s) {
    if (s == "金") return WX::Jin;
    if (s == "水") return WX::Shui;
    if (s == "木") return WX::Mu;
    if (s == "火") return WX::Huo;
    return WX::Tu;
}
inline std::string wx_to_str(WX w) {
    switch (w) {
        case WX::Jin:  return "金";
        case WX::Shui: return "水";
        case WX::Mu:   return "木";
        case WX::Huo:  return "火";
        default:       return "土";
    }
}
inline bool wx_can_sheng(WX a, WX b) {
    return (a==WX::Jin  && b==WX::Shui) || (a==WX::Shui && b==WX::Mu)  ||
           (a==WX::Mu   && b==WX::Huo)  || (a==WX::Huo  && b==WX::Tu)  ||
           (a==WX::Tu   && b==WX::Jin);
}
inline bool wx_is_ke(WX a, WX b) {
    return (a==WX::Jin  && b==WX::Mu)   || (a==WX::Mu   && b==WX::Tu)  ||
           (a==WX::Tu   && b==WX::Shui) || (a==WX::Shui && b==WX::Huo) ||
           (a==WX::Huo  && b==WX::Jin);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  CLPS 异常体系
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// 基础 CLPS 运行时错误（Python 层映射为 RuntimeError）
class ClpsError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

// 五行克制错误（Python 层映射为 TypeError）
class ClpsKezError : public ClpsError {
    using ClpsError::ClpsError;
};

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  ClpsValue — 递归值类型（前向声明）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

struct ClpsValue;
using ClpsList = std::vector<ClpsValue>;
using ClpsMap  = std::unordered_map<std::string, ClpsValue>;

struct ClpsValue {
    using Data = std::variant<
        std::monostate,                    // None
        int64_t,                           // 金·整数
        double,                            // 金·浮点
        std::string,                       // 火·字符串
        std::shared_ptr<ClpsList>,         // 木·列表
        std::shared_ptr<ClpsMap>           // 土·字典
    >;
    Data data;

    // 构造
    ClpsValue()                                 : data(std::monostate{}) {}
    explicit ClpsValue(int64_t v)               : data(v) {}
    explicit ClpsValue(double v)                : data(v) {}
    explicit ClpsValue(std::string s)           : data(std::move(s)) {}
    explicit ClpsValue(std::shared_ptr<ClpsList> v) : data(std::move(v)) {}
    explicit ClpsValue(std::shared_ptr<ClpsMap>  v) : data(std::move(v)) {}

    // 五行检测（零动态分配）
    WX wx() const;

    bool is_none()   const { return std::holds_alternative<std::monostate>(data); }
    bool is_truthy() const;

    // 算术运算（供 zhen/kan 调用）
    ClpsValue apply_op(char op, const ClpsValue& rhs) const;

    // 调试
    std::string repr() const;
};

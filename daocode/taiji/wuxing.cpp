#include "wuxing.hpp"
#include <sstream>
#include <cmath>

// std::visit helper
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

// ── wx() ──────────────────────────────────────────────────────
WX ClpsValue::wx() const {
    return std::visit(overloaded{
        [](std::monostate)                          { return WX::Tu;  },
        [](int64_t)                                 { return WX::Jin; },  // 金：精确、刚强、不可分割
        [](double)                                  { return WX::Shui; }, // 水：流动、连续、近似——浮点本性
        [](const std::string&)                      { return WX::Huo; },
        [](const std::shared_ptr<ClpsList>&)        { return WX::Mu;  },
        [](const std::shared_ptr<ClpsMap>&)         { return WX::Tu;  },
    }, data);
}

// ── is_truthy() ───────────────────────────────────────────────
bool ClpsValue::is_truthy() const {
    return std::visit(overloaded{
        [](std::monostate)                          { return false; },
        [](int64_t v)                               { return v != 0; },
        [](double v)                                { return v != 0.0; },
        [](const std::string& s)                    { return !s.empty(); },
        [](const std::shared_ptr<ClpsList>& v)      { return v && !v->empty(); },
        [](const std::shared_ptr<ClpsMap>& v)       { return v && !v->empty(); },
    }, data);
}

// ── apply_op() ────────────────────────────────────────────────
ClpsValue ClpsValue::apply_op(char op, const ClpsValue& rhs) const {
    // 字符串拼接（仅 +）
    if (std::holds_alternative<std::string>(data) &&
        std::holds_alternative<std::string>(rhs.data) && op == '+') {
        return ClpsValue(std::get<std::string>(data) + std::get<std::string>(rhs.data));
    }

    // int × int（避免 / 丢精度时也转 double）
    bool lhs_int = std::holds_alternative<int64_t>(data);
    bool rhs_int = std::holds_alternative<int64_t>(rhs.data);

    if (lhs_int && rhs_int && op != '/') {
        int64_t l = std::get<int64_t>(data);
        int64_t r = std::get<int64_t>(rhs.data);
        switch (op) {
            case '+': return ClpsValue(l + r);
            case '-': return ClpsValue(l - r);
            case '*': return ClpsValue(l * r);
            case '%': return (r != 0) ? ClpsValue(l % r) : ClpsValue(int64_t(0));
            default:  return *this;
        }
    }

    // 通用浮点路径
    auto to_dbl = [](const ClpsValue& v) -> double {
        if (auto* i = std::get_if<int64_t>(&v.data)) return static_cast<double>(*i);
        if (auto* d = std::get_if<double>(&v.data))  return *d;
        throw ClpsError("非数值类型不支持此运算");
    };
    double l = to_dbl(*this), r = to_dbl(rhs);
    switch (op) {
        case '+': return ClpsValue(l + r);
        case '-': return ClpsValue(l - r);
        case '*': return ClpsValue(l * r);
        case '/': return (r != 0.0) ? ClpsValue(l / r) : ClpsValue(int64_t(0));
        case '%': return (r != 0.0) ? ClpsValue(std::fmod(l, r)) : ClpsValue(int64_t(0));
        default:  return *this;
    }
}

// ── repr() ────────────────────────────────────────────────────
std::string ClpsValue::repr() const {
    return std::visit(overloaded{
        [](std::monostate)                 { return std::string("None"); },
        [](int64_t v)                      { return std::to_string(v); },
        [](double v) {
            std::ostringstream oss; oss << v;  // 自动去尾零：3.14 而非 3.140000
            return oss.str();
        },
        [](const std::string& s)           { return "\"" + s + "\""; },
        [](const std::shared_ptr<ClpsList>&) { return std::string("[...]"); },
        [](const std::shared_ptr<ClpsMap>&)  { return std::string("{...}"); },
    }, data);
}

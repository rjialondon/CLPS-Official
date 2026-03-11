#pragma once
// mod_helpers.hpp — 万物库内部共享工具函数
// （仅供 modules/ 下各 mod_*.cpp 使用，不对外暴露）
#include "../wanwu.hpp"
#include <string>
#include <variant>
#include <cstdint>
#include <memory>

inline int64_t to_int(const ClpsValue& v, int64_t def = 0) {
    if (auto* p = std::get_if<int64_t>(&v.data)) return *p;
    if (auto* p = std::get_if<double>(&v.data))  return static_cast<int64_t>(*p);
    return def;
}

inline std::string to_str(const ClpsValue& v) {
    if (auto* p = std::get_if<std::string>(&v.data)) return *p;
    if (auto* p = std::get_if<int64_t>(&v.data)) return std::to_string(*p);
    if (auto* p = std::get_if<double>(&v.data))  return std::to_string(*p);
    return "";
}

inline double to_double(const ClpsValue& v, double def = 0.0) {
    if (auto* p = std::get_if<int64_t>(&v.data)) return static_cast<double>(*p);
    if (auto* p = std::get_if<double>(&v.data))  return *p;
    return def;
}

// 巽卦辅助：从 ClpsValue 中提取 ClpsList（一源统三用，重巽以申命）
inline std::shared_ptr<ClpsList> xun_get_list(const ClpsValue& v) {
    auto lst = std::make_shared<ClpsList>();
    if (auto* l = std::get_if<std::shared_ptr<ClpsList>>(&v.data)) {
        *lst = **l;
    }
    return lst;
}

// 坎卦辅助：URL 百分号解码（习坎卦·网址解 / 雷水解卦共用，归宗于此）
inline std::string url_decode(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            std::string h2 = s.substr(i + 1, 2);
            out.push_back(static_cast<char>(strtol(h2.c_str(), nullptr, 16)));
            i += 2;
        } else if (s[i] == '+') {
            out.push_back(' ');
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

// mod_shuxue.cpp — 道·码万物库子模块
#include "../wanwu.hpp"
#include "mod_helpers.hpp"
#include <random>
#include <cmath>
#include <sstream>
#include <iomanip>

static void reg_da_yan() {
    auto& ku = WanWuKu::instance();

    // 推演变数 — 随机整数
    // 无参数：0 ~ 2^31-1
    // 一个参数 N：0 ~ N-1
    ku.reg("大衍之数", "推演变数", WX::Jin, [](const std::vector<ClpsValue>& args) {
        static std::mt19937_64 rng(std::random_device{}());
        if (args.empty()) {
            return ClpsValue(static_cast<int64_t>(rng() & 0x7FFFFFFF));
        }
        int64_t n = to_int(args[0], 100);
        if (n <= 0) n = 100;
        std::uniform_int_distribution<int64_t> dist(0, n - 1);
        return ClpsValue(dist(rng));
    });

    // 掩藏天机 — FNV-1a 32位哈希，返回8位十六进制字符串
    // 卦义：大过 = 超越常规 = 将明文炼化为密符
    ku.reg("大衍之数", "掩藏天机", WX::Huo, [](const std::vector<ClpsValue>& args) {
        std::string text = args.empty() ? "" : to_str(args[0]);
        // FNV-1a 32bit
        uint32_t hash = 2166136261u;
        for (unsigned char c : text) {
            hash ^= c;
            hash *= 16777619u;
        }
        std::ostringstream oss;
        oss << std::hex << std::setw(8) << std::setfill('0') << hash;
        return ClpsValue(oss.str());
    });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【四时革卦】— 革卦(49)，时间 + 休眠
//  卦义：泽中有火，革；君子以治历明时
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

static void reg_shan_lei_yi() {
    auto& ku = WanWuKu::instance();

    // 辅助：取第一个参数为 double
    auto arg0 = [](const std::vector<ClpsValue>& a, double def=0.0) -> double {
        if (a.empty()) return def;
        if (auto* p = std::get_if<double>(&a[0].data))  return *p;
        if (auto* p = std::get_if<int64_t>(&a[0].data)) return static_cast<double>(*p);
        return def;
    };
    auto arg1 = [](const std::vector<ClpsValue>& a, double def=0.0) -> double {
        if (a.size() < 2) return def;
        if (auto* p = std::get_if<double>(&a[1].data))  return *p;
        if (auto* p = std::get_if<int64_t>(&a[1].data)) return static_cast<double>(*p);
        return def;
    };

    ku.reg("山雷颐卦", "正弦",  WX::Shui, [arg0](const std::vector<ClpsValue>& a) {
        return ClpsValue(std::sin(arg0(a))); });
    ku.reg("山雷颐卦", "余弦",  WX::Shui, [arg0](const std::vector<ClpsValue>& a) {
        return ClpsValue(std::cos(arg0(a))); });
    ku.reg("山雷颐卦", "开方",  WX::Shui, [arg0](const std::vector<ClpsValue>& a) {
        return ClpsValue(std::sqrt(arg0(a))); });
    ku.reg("山雷颐卦", "幂次",  WX::Shui, [arg0,arg1](const std::vector<ClpsValue>& a) {
        return ClpsValue(std::pow(arg0(a), arg1(a,1.0))); });
    ku.reg("山雷颐卦", "对数",  WX::Shui, [arg0](const std::vector<ClpsValue>& a) {
        double x = arg0(a); return ClpsValue(x > 0 ? std::log(x) : 0.0); });
    ku.reg("山雷颐卦", "绝对值",WX::Shui, [arg0](const std::vector<ClpsValue>& a) {
        return ClpsValue(std::abs(arg0(a))); });
    ku.reg("山雷颐卦", "上取整",WX::Jin,  [arg0](const std::vector<ClpsValue>& a) {
        return ClpsValue(static_cast<int64_t>(std::ceil(arg0(a)))); });
    ku.reg("山雷颐卦", "下取整",WX::Jin,  [arg0](const std::vector<ClpsValue>& a) {
        return ClpsValue(static_cast<int64_t>(std::floor(arg0(a)))); });
    ku.reg("山雷颐卦", "四舍五入",WX::Jin, [arg0](const std::vector<ClpsValue>& a) {
        return ClpsValue(static_cast<int64_t>(std::round(arg0(a)))); });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【火地晋卦】— 晋(35)，文件系统
//  卦义：明出地上，晋；君子以自昭明德
//  函数：存在/大小/列目录/建目录/删除
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void init_mod_shuxue() {
    reg_da_yan();
    reg_shan_lei_yi();
}

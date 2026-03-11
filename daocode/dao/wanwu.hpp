#pragma once
/*
 * clps_module.hpp — 万物库注册表
 *
 * 六十四卦命名规则（来自易经.md §三）：
 *   【大衍之数】 = 随机数、哈希  (大过卦 28)
 *   【四时革卦】 = 时间、休眠    (革卦 49)
 *   【厚土艮卦】 = 文件读写      (艮卦 52)
 *   【厚土井卦】 = 标准输入      (井卦 48)
 *
 * 调用语法：
 *   震 "结果"  【模块名】  函数名  [参数...]   — 有返回值
 *   震         【模块名】  函数名  [参数...]   — 纯副作用
 */

#include "../taiji/wuxing.hpp"
#include <string>
#include <unordered_map>
#include <functional>
#include <stdexcept>
#include <vector>

// ── 模块函数签名 ──────────────────────────────────────────────────────────
using ModuleFn = std::function<ClpsValue(const std::vector<ClpsValue>&)>;

// ── 函数返回值的五行类型（供静态检查器推断）────────────────────────────────
//
//  太极生两仪：
//    阳路 fnptr — 非捕获 lambda 隐式转函数指针，调用约 3~5 ns
//    阴路 fn    — 有捕获 lambda / std::function，保持坤卦承载性
//  太极判断在编译期（if constexpr），运行期零额外分支开销。
//
struct ModuleFnMeta {
    using FnPtr = ClpsValue(*)(const std::vector<ClpsValue>&);

    FnPtr    fnptr     = nullptr;  // 阳路：函数指针（无捕获）
    ModuleFn fn;                   // 阴路：std::function（有捕获）
    WX       return_wx = WX::Tu;  // 返回值五行（WX::Tu = 无/未知）

    // 太极调用：阳先于阴
    ClpsValue call(const std::vector<ClpsValue>& args) const {
        return fnptr ? fnptr(args) : fn(args);
    }
};

// ── 单个六十四卦模块 ──────────────────────────────────────────────────────
struct ClpsModule {
    std::string name;   // e.g. "大衍之数"
    std::unordered_map<std::string, ModuleFnMeta> funcs;
};

// ── 万物库注册表（单例）─────────────────────────────────────────────────────
class WanWuKu {
public:
    static WanWuKu& instance() {
        static WanWuKu inst;
        return inst;
    }

    // 注册模块函数（太极两仪：编译期判断阴阳路径）
    template<typename F>
    void reg(const std::string& mod_name,
             const std::string& fn_name,
             WX return_wx,
             F&& f) {
        ModuleFnMeta meta;
        meta.return_wx = return_wx;
        if constexpr (std::is_convertible_v<F, ModuleFnMeta::FnPtr>) {
            meta.fnptr = static_cast<ModuleFnMeta::FnPtr>(f);  // 阳路
        } else {
            meta.fn = std::forward<F>(f);                       // 阴路
        }
        mods_[mod_name].name = mod_name;
        mods_[mod_name].funcs[fn_name] = std::move(meta);
    }

    // 调用（太极判断由 ModuleFnMeta::call 承担）
    ClpsValue call(const std::string& mod_name,
                   const std::string& fn_name,
                   const std::vector<ClpsValue>& args) {
        auto mit = mods_.find(mod_name);
        if (mit == mods_.end())
            throw ClpsError("万物库：未知模块【" + mod_name + "】");
        auto fit = mit->second.funcs.find(fn_name);
        if (fit == mit->second.funcs.end())
            throw ClpsError("万物库【" + mod_name + "】：未知函数「" + fn_name + "」");
        return fit->second.call(args);
    }

    // 查询返回值五行（供静态检查器用）
    std::optional<WX> return_wx(const std::string& mod_name,
                                const std::string& fn_name) const {
        auto mit = mods_.find(mod_name);
        if (mit == mods_.end()) return std::nullopt;
        auto fit = mit->second.funcs.find(fn_name);
        if (fit == mit->second.funcs.end()) return std::nullopt;
        return fit->second.return_wx;
    }

    bool has_module(const std::string& name) const {
        return mods_.count(name) > 0;
    }

private:
    WanWuKu() = default;
    std::unordered_map<std::string, ClpsModule> mods_;
};

// ── 启动时注册四卦（在 wan_wu_ku.cpp 中实现）────────────────────────────
void wan_wu_ku_init();
// 在 wan_wu_ku_init() 之前调用，将用户参数注入万物库
void wan_wu_ku_set_args(const std::vector<std::string>& args);

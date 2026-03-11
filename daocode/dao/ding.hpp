#pragma once
/*
 * clps_vm.hpp — 道·码 字节码虚拟机
 *
 * 执行 ClpsByteCode，复用 TaiJiJieJie 作为运行时结界。
 * 扁平坍缩法在字节码层的体现：
 *   - GEN_CMP：条件为假时指令指针直接跳过 skip 字节
 *   - LOOP_BEG/LOOP_END：维护循环栈，LOOP_END 回跳
 *   - SCOPE_PUSH/SCOPE_POP：作用域栈压弹
 */

#include "ma.hpp"
#include "../taiji/jiejie.hpp"
#include "../taiji/wuxing.hpp"
#include <vector>
#include <memory>
#include <functional>
#include <string>
#include <future>
#include <mutex>

using ClpsOutputFn = std::function<void(const std::string&)>;

class ClpsVM {
public:
    explicit ClpsVM(const ClpsByteCode& bc,
                    ClpsOutputFn out = nullptr);
    void run();
    void step(); // 执行一条指令（JIAN_BEG 内部递归使用）

private:
    const ClpsByteCode& bc_;
    ClpsOutputFn        out_;

    // 作用域栈
    std::vector<std::shared_ptr<TaiJiJieJie>> scope_stack_;

    // 已加载的家人模块（同步）
    std::unordered_map<std::string, std::shared_ptr<TaiJiJieJie>> modules_;

    // 师卦：异步子任务
    struct ShiTask {
        std::future<std::shared_ptr<TaiJiJieJie>> fut;
    };
    std::unordered_map<std::string, ShiTask> pending_tasks_;
    std::unordered_map<std::string, std::shared_ptr<std::vector<std::string>>> pending_bufs_;

    // 循环栈帧
    struct LoopFrame {
        size_t   ip_body_start; // 循环体起始 ip（LOOP_BEG 之后）
        uint32_t remaining;     // 剩余迭代次数
        uint64_t iter_index;    // 当前 $i
    };
    std::vector<LoopFrame> loop_stack_;

    // 益卦(42)：子程序定义表（由 YI_DEF 填充）
    struct YiVmDef {
        std::vector<std::string> params;  // 参数名列表（常量池 Name 字符串）
        size_t body_start;                // body 第一条指令的 ip
        size_t body_end;                  // YI_RET 的 ip（不含）
    };
    std::unordered_map<std::string, YiVmDef> yi_defs_;

    // 益卦调用栈（保存返回地址）
    std::vector<size_t> yi_call_stack_;

    // 指令指针
    size_t ip_ = 0;

    // 蒙卦：当前源码行号（由 LINE 操作码更新）
    uint32_t cur_line_ = 0;

    // ── 辅助 ────────────────────────────────────────────────────────
    std::shared_ptr<TaiJiJieJie>& cur() { return scope_stack_.back(); }
    void emit(const std::string& s);

    uint8_t  read_u8 () { return bc_.code[ip_++]; }
    uint16_t read_u16() {
        uint16_t v = (uint16_t)bc_.code[ip_] | ((uint16_t)bc_.code[ip_+1]<<8);
        ip_ += 2; return v;
    }
    uint32_t read_u32() {
        uint32_t v = (uint32_t)bc_.code[ip_]
            | ((uint32_t)bc_.code[ip_+1]<<8)
            | ((uint32_t)bc_.code[ip_+2]<<16)
            | ((uint32_t)bc_.code[ip_+3]<<24);
        ip_ += 4; return v;
    }

    // 将池条目解析为 ClpsValue
    ClpsValue resolve_pool(uint16_t idx) const;

    // 蒙卦：抛出运行时错误（行号由 run() 外层统一添加）
    [[noreturn]] void vm_throw(const std::string& msg) const {
        throw ClpsError(msg);
    }

    // 比较两个 ClpsValue
    static bool compare_vals(const ClpsValue& lhs, const ClpsValue& rhs, uint8_t cmp_op);
};

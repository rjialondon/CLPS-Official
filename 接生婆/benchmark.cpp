/*
 * benchmark.cpp — 道·码 CLPS 性能对决
 *
 * 六项测试：
 *   T1  太极生灭    — 10万子结界 创建/运算/归元
 *   T2  五行类型引擎 — 50万随机值 类型检测 + 相生/相克判定
 *   T3  艮需并发锁  — 8线程争抢共享结界，每线程累加5万次
 *   T4  辞海宏展开  — 注册10万词汇，逐个查表调用
 *   T5  分形深度    — 嵌套1000层结界，链式生长后逐层坍缩
 *   T6  综合业务    — 道德经片段×500，UTF-8逐字频率统计
 *
 * 编译：clang++ -std=c++17 -O2 -o benchmark benchmark.cpp
 *       taiji/jiejie.cpp taiji/wuxing.cpp -I. -framework CoreFoundation
 */

#include "taiji/wuxing.hpp"
#include "taiji/jiejie.hpp"
#include "dao/wanwu.hpp"

#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <mutex>
#include <vector>
#include <string>
#include <random>
#include <unordered_map>
#include <functional>
#include <numeric>
#include <sys/resource.h>

#ifdef __APPLE__
#include <mach/mach.h>
#endif

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  计时与内存工具
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

using Clock = std::chrono::steady_clock;
using TP    = Clock::time_point;

static TP    tnow()           { return Clock::now(); }
static double tsec(TP a, TP b){ return std::chrono::duration<double>(b - a).count(); }

// 当前进程物理内存占用（KB）
static long resident_kb() {
#ifdef __APPLE__
    mach_task_basic_info info{};
    mach_msg_type_number_t cnt = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info), &cnt) == KERN_SUCCESS)
        return static_cast<long>(info.resident_size / 1024);
#endif
    struct rusage ru{};
    getrusage(RUSAGE_SELF, &ru);
#ifdef __APPLE__
    return ru.ru_maxrss / 1024;
#else
    return ru.ru_maxrss;
#endif
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  结果收集
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

struct BenchResult {
    std::string name;
    double      secs;
    long        mem_before_kb;
    long        mem_after_kb;
    long        mem_delta_kb;
};

static std::vector<BenchResult> g_results;

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  T1  太极生灭（10万子结界）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

static BenchResult bench_taiji() {
    std::cout << "  运行 T1 太极生灭...\n" << std::flush;
    long mb = resident_kb();
    TP   t0 = tnow();

    TaiJiJieJie root("太极根", 0);
    volatile int64_t sink = 0;

    for (int i = 0; i < 100000; ++i) {
        auto child = root.qi_child("子" + std::to_string(i));
        child->qian("甲", ClpsValue(int64_t(i)),      WX::Jin);
        child->qian("乙", ClpsValue(int64_t(i * 3)),  WX::Jin);
        child->zhen("甲", '+', ClpsValue(int64_t(i))); // 甲 += i  → 2i
        child->zhen("甲", '*', ClpsValue(int64_t(2))); // 甲 *= 2  → 4i
        sink += std::get<int64_t>(child->dui("甲").data);
        child->kun();
    }
    (void)sink;

    double secs = tsec(t0, tnow());
    long   ma   = resident_kb();
    return {"T1  太极生灭（10万子结界）", secs, mb, ma, ma - mb};
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  T2  五行类型引擎（50万随机值）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

static BenchResult bench_wuxing() {
    std::cout << "  运行 T2 五行类型引擎...\n" << std::flush;
    long mb = resident_kb();
    TP   t0 = tnow();

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> type_d(0, 4);
    std::uniform_int_distribution<int> val_d(0, 99999);

    std::vector<ClpsValue> vals;
    vals.reserve(500000);
    for (int i = 0; i < 500000; ++i) {
        switch (type_d(rng)) {
            case 0: vals.emplace_back(ClpsValue(int64_t(val_d(rng)))); break;
            case 1: vals.emplace_back(ClpsValue(double(val_d(rng)) / 3.0)); break;
            case 2: vals.emplace_back(ClpsValue(std::string("道") + std::to_string(val_d(rng)))); break;
            case 3: vals.emplace_back(ClpsValue(std::make_shared<ClpsList>())); break;
            default: vals.emplace_back(ClpsValue{}); break;
        }
    }

    int64_t sheng = 0, ke = 0;
    for (size_t i = 0; i + 1 < vals.size(); i += 2) {
        WX a = vals[i].wx(), b = vals[i+1].wx();
        if (wx_can_sheng(a, b)) ++sheng;
        if (wx_is_ke   (a, b)) ++ke;
    }
    volatile int64_t vs = sheng, vk = ke; (void)vs; (void)vk;

    double secs = tsec(t0, tnow());
    long   ma   = resident_kb();
    return {"T2  五行类型引擎（50万随机值）", secs, mb, ma, ma - mb};
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  T3  艮需并发锁（8线程×5万次）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

static BenchResult bench_concurrent() {
    std::cout << "  运行 T3 艮需并发锁...\n" << std::flush;
    long mb = resident_kb();
    TP   t0 = tnow();

    TaiJiJieJie shared_jj("共享结界", 0);
    shared_jj.qian("计数", ClpsValue(int64_t(0)), WX::Jin);
    std::mutex mx;

    constexpr int THREADS = 8, ITERS = 50000;
    std::vector<std::thread> pool;
    pool.reserve(THREADS);
    for (int t = 0; t < THREADS; ++t) {
        pool.emplace_back([&mx, &shared_jj]() {
            for (int i = 0; i < ITERS; ++i) {
                std::lock_guard<std::mutex> lk(mx);
                shared_jj.zhen("计数", '+', ClpsValue(int64_t(1)));
            }
        });
    }
    for (auto& t : pool) t.join();

    volatile int64_t v = std::get<int64_t>(shared_jj.dui("计数").data);
    (void)v;  // expected: 400000

    double secs = tsec(t0, tnow());
    long   ma   = resident_kb();
    return {"T3  艮需并发锁（8线程×5万次）", secs, mb, ma, ma - mb};
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  T4  辞海宏展开（注册10万词+逐个调用）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

static BenchResult bench_cihai() {
    std::cout << "  运行 T4 辞海宏展开...\n" << std::flush;
    long mb = resident_kb();
    TP   t0 = tnow();

    // 模拟 WanWuKu 内部结构：string → ModuleFnMeta（含 std::function）
    // 注册 10 万个词条
    constexpr int N = 100000;
    std::unordered_map<std::string, ModuleFnMeta> table;
    table.reserve(N);

    for (int i = 0; i < N; ++i) {
        int64_t cap = int64_t(i);
        ModuleFnMeta meta;
        meta.fn         = [cap](const std::vector<ClpsValue>&) -> ClpsValue {
            return ClpsValue(cap * 2 + 1);   // 有捕获 → 阴路 std::function
        };
        meta.return_wx  = WX::Jin;
        table["词" + std::to_string(i)] = std::move(meta);
    }

    // 逐个查表调用（展开）
    int64_t sum = 0;
    for (int i = 0; i < N; ++i) {
        auto it = table.find("词" + std::to_string(i));
        if (it != table.end())
            sum += std::get<int64_t>(it->second.fn({}).data);
    }
    volatile int64_t vs = sum; (void)vs;

    double secs = tsec(t0, tnow());
    long   ma   = resident_kb();
    return {"T4  辞海宏展开（10万词注册+调用）", secs, mb, ma, ma - mb};
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  T5  分形深度（1000层嵌套结界）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

static BenchResult bench_fractal() {
    std::cout << "  运行 T5 分形深度...\n" << std::flush;
    long mb = resident_kb();
    TP   t0 = tnow();

    constexpr size_t DEPTH = 1000;

    // 生长：创建 1000 层链
    std::vector<std::shared_ptr<TaiJiJieJie>> chain;
    chain.reserve(DEPTH + 1);
    chain.push_back(std::make_shared<TaiJiJieJie>("根", int64_t(0)));

    for (size_t d = 0; d < DEPTH; ++d) {
        auto child = chain.back()->qi_child("层" + std::to_string(d));
        child->qian("深度", ClpsValue(int64_t(d)),        WX::Jin);
        child->qian("灵气", ClpsValue(int64_t(d * d)),    WX::Jin);
        child->qian("名",   ClpsValue(std::string("层") + std::to_string(d)), WX::Huo);
        chain.push_back(std::move(child));
    }

    // 读取最深层
    volatile int64_t depth_val =
        std::get<int64_t>(chain.back()->dui("深度").data);
    (void)depth_val;

    // 坍缩：从最深层往根逐层归元
    while (!chain.empty()) {
        chain.back()->kun();
        chain.pop_back();
    }

    double secs = tsec(t0, tnow());
    long   ma   = resident_kb();
    return {"T5  分形深度（1000层链式生灭）", secs, mb, ma, ma - mb};
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  T6  综合业务（UTF-8字频统计）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

static BenchResult bench_business() {
    std::cout << "  运行 T6 综合业务...\n" << std::flush;
    long mb = resident_kb();
    TP   t0 = tnow();

    // 构造文本：道德经片段 × 500
    const std::string base =
        "道可道非常道名可名非常名无名天地之始有名万物之母"
        "故常无欲以观其妙常有欲以观其徼此两者同出而异名"
        "同谓之玄玄之又玄众妙之门";
    std::string text;
    text.reserve(base.size() * 500);
    for (int i = 0; i < 500; ++i) text += base;

    // 用 TaiJiJieJie 作字频表（key = UTF-8字，value = 计数）
    TaiJiJieJie freq("字频表", 0);

    // UTF-8 逐字遍历
    size_t pos = 0;
    while (pos < text.size()) {
        unsigned char c = static_cast<unsigned char>(text[pos]);
        size_t clen = (c < 0x80) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
        if (pos + clen > text.size()) break;

        std::string ch = text.substr(pos, clen);
        ClpsValue existing = freq.dui(ch);
        if (existing.is_none()) {
            freq.qian(ch, ClpsValue(int64_t(1)), WX::Jin);
        } else {
            freq.zhen(ch, '+', ClpsValue(int64_t(1)));
        }
        pos += clen;
    }

    // 统计不重复字数
    auto all = freq.xun();
    volatile size_t unique = all.size(); (void)unique;

    // 找出最高频字
    std::string top_char;
    int64_t top_count = 0;
    for (auto& [ch, val] : all) {
        if (auto* p = std::get_if<int64_t>(&val.data)) {
            if (*p > top_count) { top_count = *p; top_char = ch; }
        }
    }
    std::cout << "    └─ 不重复字: " << unique
              << "  最高频: [" << top_char << "] ×" << top_count << "\n";

    double secs = tsec(t0, tnow());
    long   ma   = resident_kb();
    return {"T6  综合业务（道德经×500字频统计）", secs, mb, ma, ma - mb};
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  main — 运行六项测试，输出汇总表
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

int main() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║        道·码 CLPS  性能对决 Benchmark                   ║\n";
    std::cout << "║        clang++ -std=c++17 -O2                           ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n\n";

    g_results.push_back(bench_taiji());
    g_results.push_back(bench_wuxing());
    g_results.push_back(bench_concurrent());
    g_results.push_back(bench_cihai());
    g_results.push_back(bench_fractal());
    g_results.push_back(bench_business());

    // ── 汇总表 ──────────────────────────────────────────────────────────────
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                        性能汇总表                                   ║\n";
    std::cout << "╠═══════════════════════════════════╦══════════╦════════╦═════════════╣\n";
    std::cout << "║  测试项目                         ║  耗时(s) ║ 起(KB) ║ 峰值增量(KB)║\n";
    std::cout << "╠═══════════════════════════════════╬══════════╬════════╬═════════════╣\n";

    double total_secs = 0;
    for (auto& r : g_results) {
        total_secs += r.secs;
        // pad name to 33 chars
        std::string name = r.name;
        while (name.size() < 33) name += ' ';  // ASCII pad (CJK double-width handled below)

        std::cout << "║  " << std::left << std::setw(33) << r.name
                  << " ║ " << std::fixed << std::setprecision(4)
                  << std::right << std::setw(8) << r.secs
                  << " ║ " << std::setw(6) << r.mem_before_kb
                  << " ║ " << std::setw(11)
                  << (r.mem_delta_kb > 0 ? ("+" + std::to_string(r.mem_delta_kb))
                                         : std::to_string(r.mem_delta_kb))
                  << " ║\n";
    }

    std::cout << "╠═══════════════════════════════════╬══════════╬════════╬═════════════╣\n";
    std::cout << "║  合计                             ║ "
              << std::fixed << std::setprecision(4) << std::right << std::setw(8)
              << total_secs
              << " ║                        ║\n";
    std::cout << "╚═══════════════════════════════════╩══════════╩════════╩═════════════╝\n\n";

    return 0;
}

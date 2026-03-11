// mod_bingfa.cpp — 道·码万物库子模块
#include "../wanwu.hpp"
#include "mod_helpers.hpp"
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <deque>
#include <atomic>
#include <map>
#include <unordered_map>
#include <functional>
#include <sstream>
#include <iostream>
#include <iomanip>

// 命名门控（否卦·天地不交原语）
// available=true：门开，可以通过；available=false：门关，阻塞等待
struct PiGate {
    std::mutex              mx;
    std::condition_variable cv;
    bool                    available = true;
};

static std::mutex                                              g_pi_registry_mx;
static std::unordered_map<std::string, std::shared_ptr<PiGate>> g_pi_gates;

static std::shared_ptr<PiGate> pi_gate(const std::string& name) {
    std::lock_guard<std::mutex> lk(g_pi_registry_mx);
    auto it = g_pi_gates.find(name);
    if (it != g_pi_gates.end()) return it->second;
    auto g = std::make_shared<PiGate>();
    g_pi_gates[name] = g;
    return g;
}

static void reg_tian_di_pi() {
    auto& ku = WanWuKu::instance();

    // 候 N_ms — 阻塞等待 N 毫秒，返回实际耗时（毫秒）
    // 否·天地不交：程序在此停驻，什么都不通
    ku.reg("天地否卦", "候", WX::Jin, [](const std::vector<ClpsValue>& args) {
        int64_t ms = args.empty() ? 0 : to_int(args[0], 0);
        auto t0 = std::chrono::steady_clock::now();
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        auto t1 = std::chrono::steady_clock::now();
        int64_t elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        return ClpsValue(elapsed);
    });

    // 占 "名" — 获取命名门控（阻塞直到可用），返回 1
    // 否卦象：天在上，地在下，不交——甲方叩门，门关则等
    ku.reg("天地否卦", "占", WX::Jin, [](const std::vector<ClpsValue>& args) {
        std::string name = args.empty() ? "__pi_default__" : to_str(args[0]);
        auto g = pi_gate(name);
        std::unique_lock<std::mutex> lk(g->mx);
        g->cv.wait(lk, [&g]{ return g->available; });
        g->available = false;
        return ClpsValue(int64_t(1));
    });

    // 解 "名" — 释放命名门控，唤醒等待者，返回 1
    // 否极泰来：门开，天地重新交通
    ku.reg("天地否卦", "解", WX::Jin, [](const std::vector<ClpsValue>& args) {
        std::string name = args.empty() ? "__pi_default__" : to_str(args[0]);
        auto g = pi_gate(name);
        {
            std::lock_guard<std::mutex> lk(g->mx);
            g->available = true;
        }
        g->cv.notify_one();
        return ClpsValue(int64_t(1));
    });

    // 叩 "名" N_ms — 超时尝试获取，N毫秒内可用返回1，超时返回0
    // 否之叩门：敲门等候，过时不候——死锁自检
    ku.reg("天地否卦", "叩", WX::Jin, [](const std::vector<ClpsValue>& args) {
        std::string name = args.size() >= 1 ? to_str(args[0]) : "__pi_default__";
        int64_t ms = args.size() >= 2 ? to_int(args[1], 0) : 0;
        auto g = pi_gate(name);
        std::unique_lock<std::mutex> lk(g->mx);
        bool ok = g->cv.wait_for(lk, std::chrono::milliseconds(ms),
                                  [&g]{ return g->available; });
        if (ok) g->available = false;
        return ClpsValue(int64_t(ok ? 1 : 0));
    });
}
// 命名消息队列（同人·天与火同）
struct TongRenQueue {
    std::mutex              mx;
    std::condition_variable cv;
    std::deque<ClpsValue>   msgs;
};

static std::mutex                                                  g_tr_registry_mx;
static std::unordered_map<std::string, std::shared_ptr<TongRenQueue>> g_tr_queues;

static std::shared_ptr<TongRenQueue> tr_queue(const std::string& name) {
    std::lock_guard<std::mutex> lk(g_tr_registry_mx);
    auto it = g_tr_queues.find(name);
    if (it != g_tr_queues.end()) return it->second;
    auto q = std::make_shared<TongRenQueue>();
    g_tr_queues[name] = q;
    return q;
}

static void reg_tian_huo_tong_ren() {
    auto& ku = WanWuKu::instance();

    // 投 "队列" 值 — 入队（非阻塞），返回当前队列长度
    // 同人于野：把消息投入公共渠道，无论对方是否在听
    ku.reg("天火同人卦", "投", WX::Jin, [](const std::vector<ClpsValue>& args) {
        std::string name = args.size() >= 1 ? to_str(args[0]) : "__tr_default__";
        ClpsValue   val  = args.size() >= 2 ? args[1] : ClpsValue{};
        auto q = tr_queue(name);
        int64_t sz;
        {
            std::lock_guard<std::mutex> lk(q->mx);
            q->msgs.push_back(std::move(val));
            sz = static_cast<int64_t>(q->msgs.size());
        }
        q->cv.notify_one();
        return ClpsValue(sz);
    });

    // 取 "队列" — 阻塞出队，直到有消息到来
    // 利涉大川：等待消息如等待渡江，终将有渡
    ku.reg("天火同人卦", "取", WX::Huo, [](const std::vector<ClpsValue>& args) {
        std::string name = args.empty() ? "__tr_default__" : to_str(args[0]);
        auto q = tr_queue(name);
        std::unique_lock<std::mutex> lk(q->mx);
        q->cv.wait(lk, [&q]{ return !q->msgs.empty(); });
        ClpsValue val = std::move(q->msgs.front());
        q->msgs.pop_front();
        return val;
    });

    // 试 "队列" N_ms — 超时尝试出队，有消息返回消息，超时返回空串
    // 君子以类族辨物：不死等，有则取，无则识其空
    ku.reg("天火同人卦", "试", WX::Huo, [](const std::vector<ClpsValue>& args) {
        std::string name = args.size() >= 1 ? to_str(args[0]) : "__tr_default__";
        int64_t ms = args.size() >= 2 ? to_int(args[1], 0) : 0;
        auto q = tr_queue(name);
        std::unique_lock<std::mutex> lk(q->mx);
        bool ok = q->cv.wait_for(lk, std::chrono::milliseconds(ms),
                                  [&q]{ return !q->msgs.empty(); });
        if (!ok) return ClpsValue(std::string(""));
        ClpsValue val = std::move(q->msgs.front());
        q->msgs.pop_front();
        return val;
    });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【火天大有卦】— 大有(14)，批量聚合运算
//  卦义：火在天上，大有。君子以遏恶扬善，顺天休命。
//  实现：总（求和）/ 极大（最大值）/ 极小（最小值）/ 均值（算术平均）/ 积（连乘）
//  原则：小畜积累数据，大有运算整体——密云不雨积蓄，火在天上普照
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// 辅助：从 ClpsValue 取数值（double），非数值返回 nullopt
static std::optional<double> to_num(const ClpsValue& v) {
    if (auto* p = std::get_if<int64_t>(&v.data)) return static_cast<double>(*p);
    if (auto* p = std::get_if<double>  (&v.data)) return *p;
    return std::nullopt;
}

struct QuotaPool {
    std::mutex mx;
    int64_t    remaining;
    int64_t    capacity;
};

static std::mutex g_quota_registry_mx;
static std::unordered_map<std::string, std::shared_ptr<QuotaPool>> g_quotas;

static std::shared_ptr<QuotaPool> quota_pool(const std::string& name, int64_t cap = -1) {
    std::lock_guard<std::mutex> lk(g_quota_registry_mx);
    auto it = g_quotas.find(name);
    if (it != g_quotas.end()) return it->second;
    auto q = std::make_shared<QuotaPool>();
    q->capacity  = cap > 0 ? cap : 0;
    q->remaining = q->capacity;
    g_quotas[name] = q;
    return q;
}

static void reg_di_shan_qian() {
    auto& ku = WanWuKu::instance();

    // 限额 "名" N — 建立容量为 N 的配额池，返回容量
    // 地中有山：确立上限，山高地有顶
    ku.reg("地山谦卦", "限额", WX::Jin, [](const std::vector<ClpsValue>& args) {
        std::string name = args.size() >= 1 ? to_str(args[0]) : "__quota_default__";
        int64_t cap = args.size() >= 2 ? to_int(args[1], 0) : 0;
        std::lock_guard<std::mutex> lk(g_quota_registry_mx);
        auto q = std::make_shared<QuotaPool>();
        q->capacity  = cap;
        q->remaining = cap;
        g_quotas[name] = q;     // 重建（覆盖旧池）
        return ClpsValue(cap);
    });

    // 申请 "名" N — 申请 N 单位；足够返回 1，不足返回 0（非阻塞）
    // 谦不强求：君子量力而行，不足则退而不争
    ku.reg("地山谦卦", "申请", WX::Jin, [](const std::vector<ClpsValue>& args) {
        std::string name = args.size() >= 1 ? to_str(args[0]) : "__quota_default__";
        int64_t req = args.size() >= 2 ? to_int(args[1], 1) : 1;
        auto q = quota_pool(name);
        std::lock_guard<std::mutex> lk(q->mx);
        if (q->remaining >= req) {
            q->remaining -= req;
            return ClpsValue(int64_t(1));
        }
        return ClpsValue(int64_t(0));
    });

    // 余量 "名" — 返回当前剩余配额
    // 称物平施：先知余量，才能平施
    ku.reg("地山谦卦", "余量", WX::Jin, [](const std::vector<ClpsValue>& args) {
        std::string name = args.empty() ? "__quota_default__" : to_str(args[0]);
        auto q = quota_pool(name);
        std::lock_guard<std::mutex> lk(q->mx);
        return ClpsValue(q->remaining);
    });

    // 归还 "名" N — 归还 N 单位（不超过容量），返回归还后余量
    // 裒多益寡：有余则归，使不足者得益
    ku.reg("地山谦卦", "归还", WX::Jin, [](const std::vector<ClpsValue>& args) {
        std::string name = args.size() >= 1 ? to_str(args[0]) : "__quota_default__";
        int64_t ret = args.size() >= 2 ? to_int(args[1], 0) : 0;
        auto q = quota_pool(name);
        std::lock_guard<std::mutex> lk(q->mx);
        q->remaining = std::min(q->remaining + ret, q->capacity);
        return ClpsValue(q->remaining);
    });
}
struct CircuitBreaker {
    std::mutex mx;
    int64_t  fail_count  = 0;
    int64_t  threshold   = 3;   // 连续失败N次→断路
    bool     open        = false;
    std::chrono::steady_clock::time_point open_since;
    int64_t  reset_ms    = 5000; // 5秒后尝试半开
};

static std::mutex g_cb_registry_mx;
static std::unordered_map<std::string, std::shared_ptr<CircuitBreaker>> g_circuit_breakers;

static std::shared_ptr<CircuitBreaker> get_or_create_cb(const std::string& name,
                                                         int64_t threshold = 3,
                                                         int64_t reset_ms = 5000) {
    std::lock_guard<std::mutex> lg(g_cb_registry_mx);
    auto it = g_circuit_breakers.find(name);
    if (it != g_circuit_breakers.end()) return it->second;
    auto cb = std::make_shared<CircuitBreaker>();
    cb->threshold = threshold;
    cb->reset_ms  = reset_ms;
    g_circuit_breakers[name] = cb;
    return cb;
}

static void reg_ze_shui_kun() {
    auto& ku = WanWuKu::instance();

    // 等待 N ms（与否卦·候类似，但属困域）
    ku.reg("泽水困卦", "等待", WX::Jin, [](const std::vector<ClpsValue>& a) {
        int64_t ms = a.empty() ? 0 : to_int(a[0]);
        if (ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        }
        return ClpsValue(ms);
    });

    // 建熔断 "名" [阈值=3] [重置毫秒=5000] → 阈值
    ku.reg("泽水困卦", "建熔断", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string name = a.empty() ? "default" : to_str(a[0]);
        int64_t thr  = a.size() >= 2 ? to_int(a[1]) : 3;
        int64_t rst  = a.size() >= 3 ? to_int(a[2]) : 5000;
        auto cb = get_or_create_cb(name, thr, rst);
        return ClpsValue(cb->threshold);
    });

    // 熔断状态 "名" → 0=正常，1=断路
    ku.reg("泽水困卦", "断路否", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string name = a.empty() ? "default" : to_str(a[0]);
        std::lock_guard<std::mutex> lg(g_cb_registry_mx);
        auto it = g_circuit_breakers.find(name);
        if (it == g_circuit_breakers.end()) return ClpsValue(int64_t(0));
        auto& cb = *it->second;
        std::lock_guard<std::mutex> lk(cb.mx);
        if (!cb.open) return ClpsValue(int64_t(0));
        // 检查是否过了重置时间
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - cb.open_since).count();
        if (elapsed >= cb.reset_ms) {
            cb.open = false;
            cb.fail_count = 0;
            return ClpsValue(int64_t(0)); // 半开→放行
        }
        return ClpsValue(int64_t(1)); // 仍断路
    });

    // 记失败 "名" → 当前失败次数（达阈值则断路）
    ku.reg("泽水困卦", "记失败", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string name = a.empty() ? "default" : to_str(a[0]);
        auto cb = get_or_create_cb(name);
        std::lock_guard<std::mutex> lk(cb->mx);
        cb->fail_count++;
        if (cb->fail_count >= cb->threshold && !cb->open) {
            cb->open = true;
            cb->open_since = std::chrono::steady_clock::now();
        }
        return ClpsValue(cb->fail_count);
    });

    // 记成功 "名" → 清零失败计数
    ku.reg("泽水困卦", "记成功", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string name = a.empty() ? "default" : to_str(a[0]);
        auto cb = get_or_create_cb(name);
        std::lock_guard<std::mutex> lk(cb->mx);
        cb->fail_count = 0;
        cb->open = false;
        return ClpsValue(int64_t(0));
    });
}
struct YuCache {
    std::mutex mx;
    std::unordered_map<std::string, ClpsValue> store;
    int64_t hits   = 0;
    int64_t misses = 0;
};

static std::mutex g_yu_registry_mx;
static std::unordered_map<std::string, std::shared_ptr<YuCache>> g_yu_caches;

static std::shared_ptr<YuCache> get_or_create_yu(const std::string& name) {
    std::lock_guard<std::mutex> lg(g_yu_registry_mx);
    auto it = g_yu_caches.find(name);
    if (it != g_yu_caches.end()) return it->second;
    auto c = std::make_shared<YuCache>();
    g_yu_caches[name] = c;
    return c;
}

static void reg_lei_di_yu() {
    auto& ku = WanWuKu::instance();

    // 缓存存 "缓存名" "键" 值 → 值
    ku.reg("雷地豫卦", "缓存存", WX::Tu, [](const std::vector<ClpsValue>& a) {
        std::string cache = a.size() >= 1 ? to_str(a[0]) : "default";
        std::string key   = a.size() >= 2 ? to_str(a[1]) : "";
        ClpsValue   val   = a.size() >= 3 ? a[2] : ClpsValue{};
        auto c = get_or_create_yu(cache);
        std::lock_guard<std::mutex> lk(c->mx);
        c->store[key] = val;
        return val;
    });

    // 缓存取 "缓存名" "键" → 值（未命中返回 None）
    ku.reg("雷地豫卦", "缓存取", WX::Tu, [](const std::vector<ClpsValue>& a) {
        std::string cache = a.size() >= 1 ? to_str(a[0]) : "default";
        std::string key   = a.size() >= 2 ? to_str(a[1]) : "";
        auto c = get_or_create_yu(cache);
        std::lock_guard<std::mutex> lk(c->mx);
        auto it = c->store.find(key);
        if (it != c->store.end()) { c->hits++; return it->second; }
        c->misses++;
        return ClpsValue{};
    });

    // 缓存删 "缓存名" "键" → 1=删除成功, 0=不存在
    ku.reg("雷地豫卦", "缓存删", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string cache = a.size() >= 1 ? to_str(a[0]) : "default";
        std::string key   = a.size() >= 2 ? to_str(a[1]) : "";
        auto c = get_or_create_yu(cache);
        std::lock_guard<std::mutex> lk(c->mx);
        return ClpsValue(static_cast<int64_t>(c->store.erase(key) > 0 ? 1 : 0));
    });

    // 缓存大小 "缓存名" → 项数
    ku.reg("雷地豫卦", "缓存大小", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string cache = a.empty() ? "default" : to_str(a[0]);
        auto c = get_or_create_yu(cache);
        std::lock_guard<std::mutex> lk(c->mx);
        return ClpsValue(static_cast<int64_t>(c->store.size()));
    });

    // 命中率 "缓存名" → 0.0~1.0
    ku.reg("雷地豫卦", "命中率", WX::Shui, [](const std::vector<ClpsValue>& a) {
        std::string cache = a.empty() ? "default" : to_str(a[0]);
        auto c = get_or_create_yu(cache);
        std::lock_guard<std::mutex> lk(c->mx);
        int64_t total = c->hits + c->misses;
        if (total == 0) return ClpsValue(0.0);
        return ClpsValue(static_cast<double>(c->hits) / static_cast<double>(total));
    });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【泽雷随卦】— 随(17)，事件总线（发布/订阅）
//  卦义：泽中有雷，随。君子以向晦入宴息。
//  随物应机，生产者发射，消费者按类型订阅
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

struct SuiBus {
    std::mutex mx;
    std::condition_variable cv;
    // event_type → FIFO queue of values
    std::unordered_map<std::string, std::deque<ClpsValue>> channels;
};

static std::mutex g_sui_registry_mx;
static std::unordered_map<std::string, std::shared_ptr<SuiBus>> g_sui_buses;

static std::shared_ptr<SuiBus> get_or_create_sui(const std::string& name) {
    std::lock_guard<std::mutex> lg(g_sui_registry_mx);
    auto it = g_sui_buses.find(name);
    if (it != g_sui_buses.end()) return it->second;
    auto b = std::make_shared<SuiBus>();
    g_sui_buses[name] = b;
    return b;
}

static void reg_ze_lei_sui() {
    auto& ku = WanWuKu::instance();

    // 建总线 "名" → 总线名（确保存在）
    ku.reg("泽雷随卦", "建总线", WX::Huo, [](const std::vector<ClpsValue>& a) {
        std::string name = a.empty() ? "default" : to_str(a[0]);
        get_or_create_sui(name);
        return ClpsValue(name);
    });

    // 发 "总线" "类型" value → 积压数（发布事件）
    ku.reg("泽雷随卦", "发", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string bus  = a.size() >= 1 ? to_str(a[0]) : "default";
        std::string type = a.size() >= 2 ? to_str(a[1]) : "event";
        ClpsValue   val  = a.size() >= 3 ? a[2] : ClpsValue{};
        auto b = get_or_create_sui(bus);
        std::lock_guard<std::mutex> lk(b->mx);
        b->channels[type].push_back(val);
        int64_t sz = static_cast<int64_t>(b->channels[type].size());
        b->cv.notify_all();
        return ClpsValue(sz);
    });

    // 收 "总线" "类型" → 值（阻塞等待，FIFO）
    ku.reg("泽雷随卦", "收", WX::Tu, [](const std::vector<ClpsValue>& a) {
        std::string bus  = a.size() >= 1 ? to_str(a[0]) : "default";
        std::string type = a.size() >= 2 ? to_str(a[1]) : "event";
        auto b = get_or_create_sui(bus);
        std::unique_lock<std::mutex> lk(b->mx);
        b->cv.wait(lk, [&]{ return !b->channels[type].empty(); });
        ClpsValue v = b->channels[type].front();
        b->channels[type].pop_front();
        return v;
    });

    // 试收 "总线" "类型" → 值或 None（非阻塞）
    ku.reg("泽雷随卦", "试收", WX::Tu, [](const std::vector<ClpsValue>& a) {
        std::string bus  = a.size() >= 1 ? to_str(a[0]) : "default";
        std::string type = a.size() >= 2 ? to_str(a[1]) : "event";
        auto b = get_or_create_sui(bus);
        std::lock_guard<std::mutex> lk(b->mx);
        auto it = b->channels.find(type);
        if (it == b->channels.end() || it->second.empty()) return ClpsValue{};
        ClpsValue v = it->second.front();
        it->second.pop_front();
        return v;
    });

    // 积压 "总线" "类型" → 队列长度
    ku.reg("泽雷随卦", "积压", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string bus  = a.size() >= 1 ? to_str(a[0]) : "default";
        std::string type = a.size() >= 2 ? to_str(a[1]) : "event";
        auto b = get_or_create_sui(bus);
        std::lock_guard<std::mutex> lk(b->mx);
        auto it = b->channels.find(type);
        if (it == b->channels.end()) return ClpsValue(int64_t(0));
        return ClpsValue(static_cast<int64_t>(it->second.size()));
    });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【火山旅卦】— 旅(56)，HTTP 客户端
//  卦义：山上有火，旅。君子以明慎用刑而不留狱。
//  旅行于网络，问道于远方；HTTP/1.0，无外部依赖
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

struct GuiMeiPromise {
    enum class State : uint8_t { Pending, Fulfilled, Rejected };
    State state = State::Pending;
    ClpsValue value;
    std::string error;
    std::mutex mx;
    std::condition_variable cv;
};
static std::unordered_map<std::string, std::shared_ptr<GuiMeiPromise>> g_guimei;
static std::mutex g_guimei_mx;

static std::shared_ptr<GuiMeiPromise> get_promise(const std::string& name) {
    std::lock_guard<std::mutex> lk(g_guimei_mx);
    auto it = g_guimei.find(name);
    if (it == g_guimei.end())
        throw ClpsError("雷泽归妹：Promise \"" + name + "\" 不存在，请先「许诺」");
    return it->second;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
static void reg_lei_ze_gui_mei() {
    auto& ku = WanWuKu::instance();

    // 许诺 name → void
    ku.reg("雷泽归妹卦", "许诺", WX::Tu, [](const std::vector<ClpsValue>& a) {
        std::string name = a.empty() ? "默认" : to_str(a[0]);
        auto p = std::make_shared<GuiMeiPromise>();
        std::lock_guard<std::mutex> lk(g_guimei_mx);
        g_guimei[name] = p;
        return ClpsValue{};
    });

    // 履约 name value → void
    ku.reg("雷泽归妹卦", "履约", WX::Tu, [](const std::vector<ClpsValue>& a) {
        std::string name = a.empty() ? "默认" : to_str(a[0]);
        ClpsValue val = a.size() >= 2 ? a[1] : ClpsValue{};
        auto p = get_promise(name);
        {
            std::lock_guard<std::mutex> lk(p->mx);
            if (p->state != GuiMeiPromise::State::Pending) return ClpsValue{};
            p->value = std::move(val);
            p->state = GuiMeiPromise::State::Fulfilled;
        }
        p->cv.notify_all();
        return ClpsValue{};
    });

    // 失信 name msg → void
    ku.reg("雷泽归妹卦", "失信", WX::Huo, [](const std::vector<ClpsValue>& a) {
        std::string name = a.empty() ? "默认" : to_str(a[0]);
        std::string msg  = a.size() >= 2 ? to_str(a[1]) : "Promise 被拒绝";
        auto p = get_promise(name);
        {
            std::lock_guard<std::mutex> lk(p->mx);
            if (p->state != GuiMeiPromise::State::Pending) return ClpsValue{};
            p->error = msg;
            p->state = GuiMeiPromise::State::Rejected;
        }
        p->cv.notify_all();
        return ClpsValue{};
    });

    // 候约 name [timeout_ms] → value (None on timeout)
    ku.reg("雷泽归妹卦", "候约", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string name   = a.empty() ? "默认" : to_str(a[0]);
        int64_t timeout_ms = a.size() >= 2 ? to_int(a[1], -1) : -1;
        auto p = get_promise(name);
        std::unique_lock<std::mutex> lk(p->mx);
        auto pred = [&]{ return p->state != GuiMeiPromise::State::Pending; };
        if (timeout_ms > 0) {
            p->cv.wait_for(lk, std::chrono::milliseconds(timeout_ms), pred);
        } else {
            p->cv.wait(lk, pred);
        }
        if (p->state == GuiMeiPromise::State::Rejected)
            throw ClpsError("雷泽归妹：Promise \"" + name + "\" 已拒绝: " + p->error);
        if (p->state == GuiMeiPromise::State::Pending)
            return ClpsValue{}; // timeout → None
        return p->value;
    });

    // 查约 name → int (0=待 1=已兑 2=已拒)
    ku.reg("雷泽归妹卦", "查约", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string name = a.empty() ? "默认" : to_str(a[0]);
        auto p = get_promise(name);
        std::lock_guard<std::mutex> lk(p->mx);
        return ClpsValue(int64_t(static_cast<uint8_t>(p->state)));
    });

    // 清约 → void
    ku.reg("雷泽归妹卦", "清约", WX::Tu, [](const std::vector<ClpsValue>&) {
        std::lock_guard<std::mutex> lk(g_guimei_mx);
        g_guimei.clear();
        return ClpsValue{};
    });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

struct DiGengGuanKu {
    std::mutex mx;
    std::deque<std::string> logs;      // 带时间戳的日志行
    std::unordered_map<std::string,
        std::chrono::steady_clock::time_point> timers;
    int cnt_xin = 0;   // 信（info）
    int cnt_jing = 0;  // 警（warn）
    int cnt_wei  = 0;  // 危（error）
};
static DiGengGuanKu g_guan;

static std::string guan_timestamp() {
    using namespace std::chrono;
    auto now   = system_clock::now();
    auto t     = system_clock::to_time_t(now);
    auto ms    = duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000;
    std::tm tm_buf{};
    localtime_r(&t, &tm_buf);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03lld",
        tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, (long long)ms);
    return std::string(buf);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  临(19) 【地泽临卦】— SQLite 数据库
//  坤上兑下，泽在地中，持久沉积，俯临万数
//
//  开库 path  → void   打开（或创建）数据库文件
//  执行 sql   → int    执行 DDL/DML，返回受影响行数
//  查询 sql   → list   执行 SELECT，返回行列表（每行 tab 分隔字符串）
//  关库       → void   关闭数据库
//  事务       → void   BEGIN TRANSACTION
//  提交       → void   COMMIT
//  回滚       → void   ROLLBACK
static void reg_di_feng_guan() {
    auto& ku = WanWuKu::instance();

    // 观察 level msg → void（打印 + 存缓冲）
    ku.reg("地风观卦", "观察", WX::Tu, [](const std::vector<ClpsValue>& a) {
        std::string level = a.size() >= 1 ? to_str(a[0]) : "信";
        std::string msg   = a.size() >= 2 ? to_str(a[1]) : "";
        std::string ts    = guan_timestamp();
        std::string line  = "[" + ts + "] [" + level + "] " + msg;
        {
            std::lock_guard<std::mutex> lk(g_guan.mx);
            if      (level == "信") g_guan.cnt_xin++;
            else if (level == "警") g_guan.cnt_jing++;
            else if (level == "危") g_guan.cnt_wei++;
            g_guan.logs.push_back(line);
            if (g_guan.logs.size() > 10000) g_guan.logs.pop_front();
        }
        std::cout << line << "\n";
        return ClpsValue{};
    });

    // 立标 name → void（记录计时器起点）
    ku.reg("地风观卦", "立标", WX::Tu, [](const std::vector<ClpsValue>& a) {
        std::string name = a.empty() ? "默认" : to_str(a[0]);
        std::lock_guard<std::mutex> lk(g_guan.mx);
        g_guan.timers[name] = std::chrono::steady_clock::now();
        return ClpsValue{};
    });

    // 量时 name → int（毫秒数）
    ku.reg("地风观卦", "量时", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string name = a.empty() ? "默认" : to_str(a[0]);
        std::lock_guard<std::mutex> lk(g_guan.mx);
        auto it = g_guan.timers.find(name);
        if (it == g_guan.timers.end()) return ClpsValue(int64_t(-1));
        auto elapsed = std::chrono::steady_clock::now() - it->second;
        int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        return ClpsValue(ms);
    });

    // 统计 → string（各级条数）
    ku.reg("地风观卦", "统计", WX::Huo, [](const std::vector<ClpsValue>&) {
        std::lock_guard<std::mutex> lk(g_guan.mx);
        std::ostringstream oss;
        oss << "信:" << g_guan.cnt_xin
            << " 警:" << g_guan.cnt_jing
            << " 危:" << g_guan.cnt_wei
            << " 共:" << (g_guan.cnt_xin + g_guan.cnt_jing + g_guan.cnt_wei);
        return ClpsValue(oss.str());
    });

    // 清观 → void
    ku.reg("地风观卦", "清观", WX::Tu, [](const std::vector<ClpsValue>&) {
        std::lock_guard<std::mutex> lk(g_guan.mx);
        g_guan.logs.clear();
        g_guan.cnt_xin = g_guan.cnt_jing = g_guan.cnt_wei = 0;
        g_guan.timers.clear();
        return ClpsValue{};
    });

    // 回看 N → string（最近 N 条，换行分隔）
    ku.reg("地风观卦", "回看", WX::Huo, [](const std::vector<ClpsValue>& a) {
        int64_t n = a.empty() ? 10 : to_int(a[0], 10);
        std::lock_guard<std::mutex> lk(g_guan.mx);
        std::ostringstream oss;
        size_t total = g_guan.logs.size();
        size_t start = (int64_t)total > n ? total - (size_t)n : 0;
        for (size_t i = start; i < total; ++i) {
            if (i > start) oss << "\n";
            oss << g_guan.logs[i];
        }
        return ClpsValue(oss.str());
    });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【习坎卦】— 坎(29)，安全防护
//  卦义：习坎，有孚，维心亨，行有尚。
//  险中求通：Base64编解码 + SHA-256 + HMAC-SHA256 + URL编解码
//  纯C++零依赖，不链接 OpenSSL
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━


struct JieBucket {
    double capacity;
    double refill_per_sec;
    double tokens;
    std::chrono::steady_clock::time_point last_refill;
    std::mutex mx;

    JieBucket(double cap, double rps)
        : capacity(cap), refill_per_sec(rps), tokens(cap),
          last_refill(std::chrono::steady_clock::now()) {}

    // 补充令牌（调用前需持有 mx）
    void refill_locked() {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - last_refill).count();
        tokens = std::min(capacity, tokens + elapsed * refill_per_sec);
        last_refill = now;
    }

    // 非阻塞取令：成功返回剩余数，失败返回 -1
    int64_t try_consume() {
        std::lock_guard<std::mutex> lk(mx);
        refill_locked();
        if (tokens >= 1.0) { tokens -= 1.0; return (int64_t)tokens; }
        return -1;
    }

    // 阻塞取令（轮询）：成功返回剩余数，超时返回 -1
    int64_t wait_consume(int64_t timeout_ms) {
        auto deadline = std::chrono::steady_clock::now()
                      + std::chrono::milliseconds(timeout_ms);
        while (true) {
            {
                std::lock_guard<std::mutex> lk(mx);
                refill_locked();
                if (tokens >= 1.0) { tokens -= 1.0; return (int64_t)tokens; }
            }
            if (std::chrono::steady_clock::now() >= deadline) return -1;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    // 查询当前令牌数（不消耗）
    int64_t query() {
        std::lock_guard<std::mutex> lk(mx);
        refill_locked();
        return (int64_t)tokens;
    }
};

static std::mutex g_jie_mx;
static std::unordered_map<std::string, std::shared_ptr<JieBucket>> g_jie_buckets;

static std::shared_ptr<JieBucket> get_jie(const std::string& name) {
    std::lock_guard<std::mutex> lk(g_jie_mx);
    auto it = g_jie_buckets.find(name);
    return it != g_jie_buckets.end() ? it->second : nullptr;
}


static void reg_shui_ze_jie() {
    auto& ku = WanWuKu::instance();

    // 制桶 "name" capacity refill_per_sec → "name"
    ku.reg("水泽节卦", "制桶", WX::Tu, [](const std::vector<ClpsValue>& a) {
        std::string name = a.size() >= 1 ? to_str(a[0]) : "default";
        double cap = a.size() >= 2 ? to_double(a[1], 10.0) : 10.0;
        double rps = a.size() >= 3 ? to_double(a[2],  1.0) :  1.0;
        std::lock_guard<std::mutex> lk(g_jie_mx);
        g_jie_buckets[name] = std::make_shared<JieBucket>(cap, rps);
        return ClpsValue(name);
    });

    // 取令 "name" → 剩余令牌数，-1=桶空（非阻塞）
    ku.reg("水泽节卦", "取令", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string name = a.empty() ? "default" : to_str(a[0]);
        auto b = get_jie(name);
        if (!b) throw std::runtime_error("节卦：令牌桶不存在: " + name);
        return ClpsValue(b->try_consume());
    });

    // 候令 "name" [timeout_ms=1000] → 剩余令牌数，-1=超时（阻塞）
    ku.reg("水泽节卦", "候令", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string name    = a.empty() ? "default" : to_str(a[0]);
        int64_t     timeout = a.size() >= 2 ? to_int(a[1], 1000) : 1000;
        auto b = get_jie(name);
        if (!b) throw std::runtime_error("节卦：令牌桶不存在: " + name);
        return ClpsValue(b->wait_consume(timeout));
    });

    // 余令 "name" → 当前令牌数（不消耗）
    ku.reg("水泽节卦", "余令", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string name = a.empty() ? "default" : to_str(a[0]);
        auto b = get_jie(name);
        if (!b) throw std::runtime_error("节卦：令牌桶不存在: " + name);
        return ClpsValue(b->query());
    });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【风水涣卦】— 涣(59)，广播 PubSub
//  卦义：风行水上，涣。先王以享于帝立庙。
//  涣散凝聚：广发到所有订阅者，与随(17)工作队列有别
//  随=一对一消费；涣=一对多广播（每人各得一份）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

struct HuanInbox {
    std::string channel;
    std::string type_filter;   // "" = 接受所有类型
    std::deque<std::string> messages;
    std::mutex mx;
    std::condition_variable cv;
};

static std::mutex g_huan_mx;
// key = channel + ":" + sub_id
static std::unordered_map<std::string, std::shared_ptr<HuanInbox>> g_huan_subs;

static std::string huan_key(const std::string& ch, const std::string& id) {
    return ch + ":" + id;
}

static void reg_feng_shui_huan() {
    auto& ku = WanWuKu::instance();

    // 订阅 "channel" "type" "sub_id" → "sub_id"
    // type="" 订阅该频道所有类型
    ku.reg("风水涣卦", "订阅", WX::Huo, [](const std::vector<ClpsValue>& a) {
        std::string channel = a.size() >= 1 ? to_str(a[0]) : "default";
        std::string type    = a.size() >= 2 ? to_str(a[1]) : "";
        std::string sub_id  = a.size() >= 3 ? to_str(a[2]) : "sub0";
        auto inbox = std::make_shared<HuanInbox>();
        inbox->channel     = channel;
        inbox->type_filter = type;
        std::lock_guard<std::mutex> lk(g_huan_mx);
        g_huan_subs[huan_key(channel, sub_id)] = inbox;
        return ClpsValue(sub_id);
    });

    // 退订 "channel" "sub_id" → 1
    ku.reg("风水涣卦", "退订", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string channel = a.size() >= 1 ? to_str(a[0]) : "default";
        std::string sub_id  = a.size() >= 2 ? to_str(a[1]) : "sub0";
        std::lock_guard<std::mutex> lk(g_huan_mx);
        g_huan_subs.erase(huan_key(channel, sub_id));
        return ClpsValue(int64_t(1));
    });

    // 广发 "channel" "type" "msg" → 送达订阅者数
    ku.reg("风水涣卦", "广发", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string channel = a.size() >= 1 ? to_str(a[0]) : "default";
        std::string type    = a.size() >= 2 ? to_str(a[1]) : "event";
        std::string msg     = a.size() >= 3 ? to_str(a[2]) : "";
        // 收集所有匹配的 inbox（先解锁注册表，再分别加锁入队）
        std::vector<std::shared_ptr<HuanInbox>> targets;
        {
            std::lock_guard<std::mutex> lk(g_huan_mx);
            for (auto& [k, inbox] : g_huan_subs) {
                if (inbox->channel == channel &&
                    (inbox->type_filter.empty() || inbox->type_filter == type))
                    targets.push_back(inbox);
            }
        }
        for (auto& inbox : targets) {
            std::lock_guard<std::mutex> lk(inbox->mx);
            inbox->messages.push_back(msg);
            inbox->cv.notify_all();
        }
        return ClpsValue(static_cast<int64_t>(targets.size()));
    });

    // 候播 "channel" "type" "sub_id" [timeout_ms=-1] → msg 或 ""
    // timeout_ms=-1 → 永久阻塞
    ku.reg("风水涣卦", "候播", WX::Huo, [](const std::vector<ClpsValue>& a) {
        std::string channel = a.size() >= 1 ? to_str(a[0]) : "default";
        std::string sub_id  = a.size() >= 3 ? to_str(a[2]) : "sub0";
        int64_t     timeout = a.size() >= 4 ? to_int(a[3], -1) : -1;
        std::string key = huan_key(channel, sub_id);
        std::shared_ptr<HuanInbox> inbox;
        {
            std::lock_guard<std::mutex> lk(g_huan_mx);
            auto it = g_huan_subs.find(key);
            if (it == g_huan_subs.end())
                throw std::runtime_error("涣卦：订阅不存在: " + key);
            inbox = it->second;
        }
        std::unique_lock<std::mutex> lk(inbox->mx);
        if (timeout < 0) {
            inbox->cv.wait(lk, [&]{ return !inbox->messages.empty(); });
        } else {
            bool ok = inbox->cv.wait_for(lk,
                std::chrono::milliseconds(timeout),
                [&]{ return !inbox->messages.empty(); });
            if (!ok) return ClpsValue(std::string(""));
        }
        std::string msg = inbox->messages.front();
        inbox->messages.pop_front();
        return ClpsValue(msg);
    });

    // 积播 "channel" "sub_id" → 积压消息数
    ku.reg("风水涣卦", "积播", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string channel = a.size() >= 1 ? to_str(a[0]) : "default";
        std::string sub_id  = a.size() >= 2 ? to_str(a[1]) : "sub0";
        std::string key = huan_key(channel, sub_id);
        std::lock_guard<std::mutex> lk(g_huan_mx);
        auto it = g_huan_subs.find(key);
        if (it == g_huan_subs.end()) return ClpsValue(int64_t(0));
        std::lock_guard<std::mutex> lk2(it->second->mx);
        return ClpsValue(static_cast<int64_t>(it->second->messages.size()));
    });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【雷水解卦】— 解(40)，HTTP 请求参数解析
//  卦义：雷雨作，解。君子以赦过宥罪。
//  险难消解：解析 URL、查询字符串、请求头
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// 辅助：将 ClpsList 中的字符串 push 进结果

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

struct DaZhuangCounter {
    std::atomic<int64_t> val{0};
    explicit DaZhuangCounter(int64_t init = 0) : val(init) {}
};

static std::mutex g_dz_mx;
static std::unordered_map<std::string, std::shared_ptr<DaZhuangCounter>> g_dz_counters;

static std::shared_ptr<DaZhuangCounter> dz_get_or_create(const std::string& name, int64_t init = 0) {
    std::lock_guard<std::mutex> lk(g_dz_mx);
    auto& p = g_dz_counters[name];
    if (!p) p = std::make_shared<DaZhuangCounter>(init);
    return p;
}


static void reg_da_zhuang() {
    auto& ku = WanWuKu::instance();

    // 立 "name" [init] → init  创建命名计数器（可省略初值，默认0）
    ku.reg("雷天大壮卦", "立", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string name = a.size() >= 1 ? to_str(a[0]) : "default";
        int64_t init     = a.size() >= 2 ? to_int(a[1]) : 0;
        auto c = std::make_shared<DaZhuangCounter>(init);
        { std::lock_guard<std::mutex> lk(g_dz_mx); g_dz_counters[name] = c; }
        return ClpsValue(init);
    });

    // 增 "name" [n] → 新值  原子加（默认+1）
    ku.reg("雷天大壮卦", "增", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string name = a.size() >= 1 ? to_str(a[0]) : "default";
        int64_t n        = a.size() >= 2 ? to_int(a[1]) : 1;
        auto c = dz_get_or_create(name);
        return ClpsValue(c->val.fetch_add(n) + n);
    });

    // 减 "name" [n] → 新值  原子减（默认-1）
    ku.reg("雷天大壮卦", "减", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string name = a.size() >= 1 ? to_str(a[0]) : "default";
        int64_t n        = a.size() >= 2 ? to_int(a[1]) : 1;
        auto c = dz_get_or_create(name);
        return ClpsValue(c->val.fetch_sub(n) - n);
    });

    // 读 "name" → 当前值
    ku.reg("雷天大壮卦", "读", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string name = a.size() >= 1 ? to_str(a[0]) : "default";
        auto c = dz_get_or_create(name);
        return ClpsValue(c->val.load());
    });

    // 置 "name" val → val  原子设置
    ku.reg("雷天大壮卦", "置", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string name = a.size() >= 1 ? to_str(a[0]) : "default";
        int64_t val      = a.size() >= 2 ? to_int(a[1]) : 0;
        auto c = dz_get_or_create(name, val);
        c->val.store(val);
        return ClpsValue(val);
    });

    // 零 "name" → 旧值  重置为0
    ku.reg("雷天大壮卦", "零", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string name = a.size() >= 1 ? to_str(a[0]) : "default";
        auto c = dz_get_or_create(name);
        return ClpsValue(c->val.exchange(0));
    });

    // 换 "name" expected new → 1=交换成功 0=失败（CAS）
    ku.reg("雷天大壮卦", "换", WX::Jin, [](const std::vector<ClpsValue>& a) {
        std::string name = a.size() >= 1 ? to_str(a[0]) : "default";
        int64_t expected = a.size() >= 2 ? to_int(a[1]) : 0;
        int64_t newval   = a.size() >= 3 ? to_int(a[2]) : 0;
        auto c = dz_get_or_create(name, expected);
        return ClpsValue(int64_t(c->val.compare_exchange_strong(expected, newval) ? 1 : 0));
    });

    // 列 → ClpsList of "name=val"  枚举所有计数器
    ku.reg("雷天大壮卦", "列", WX::Mu, [](const std::vector<ClpsValue>&) {
        auto lst = std::make_shared<ClpsList>();
        std::lock_guard<std::mutex> lk(g_dz_mx);
        for (auto& [name, c] : g_dz_counters) {
            lst->push_back(ClpsValue(name + "=" + std::to_string(c->val.load())));
        }
        return ClpsValue(lst);
    });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  【水火既济卦】— 既济(63)，位运算
//  卦义：既济，亨小，利贞；初吉终乱。
//  既济=水火交融=0与1交错=位运算的本质
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━


void init_mod_bingfa() {
    reg_tian_di_pi();
    reg_tian_huo_tong_ren();
    reg_di_shan_qian();
    reg_ze_shui_kun();
    reg_lei_di_yu();
    reg_ze_lei_sui();
    reg_lei_ze_gui_mei();
    reg_di_feng_guan();
    reg_shui_ze_jie();
    reg_feng_shui_huan();
    reg_da_zhuang();
}

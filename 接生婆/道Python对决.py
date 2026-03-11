#!/usr/bin/env python3
"""
道·码 (CLPS) vs Python — 性能对决
锚点：易经.md — 以实测定优劣，以卦象知边界

测试维度：
  ① 顺序累加   — 复+震 (哈希表变量) vs Python for loop
  ② 列表聚合   — 小畜+大有·总 (C++ vector) vs Python list+sum
  ③ 并发任务   — 4师真OS线程 vs Python threading (GIL)
  ④ 消息队列   — 同人 投/取 vs Python queue.Queue
  ⑤ 启动开销   — 空.dao vs python -c pass（基准线）
"""

import subprocess, time, sys, os, threading, queue as pyqueue

BASE  = os.path.dirname(os.path.abspath(__file__))
CLPSD = os.path.join(BASE, "taiji_cpp/clpsd")
REPS  = 3   # 取最小值（排除抖动）

# ── 计时辅助 ─────────────────────────────────────────────────────────────────

def run_clps(dao_file, reps=REPS):
    best = float('inf')
    for _ in range(reps):
        t0 = time.perf_counter()
        r  = subprocess.run(
            [CLPSD, "--no-check", dao_file],
            capture_output=True, cwd=BASE)
        t1 = time.perf_counter()
        if r.returncode == 0:
            best = min(best, t1 - t0)
    return best

def run_py(fn, reps=REPS):
    best = float('inf')
    for _ in range(reps):
        t0 = time.perf_counter()
        fn()
        best = min(best, time.perf_counter() - t0)
    return best

def report(label, clps_t, py_t):
    ratio = clps_t / py_t if py_t > 1e-9 else float('inf')
    if   ratio < 0.75: verdict = f"✅ CLPS   {1/ratio:.1f}× 快"
    elif ratio > 1.33: verdict = f"🐍 Python {ratio:.1f}× 快"
    else:              verdict = "≈ 相当"
    bar_c = "█" * min(40, int(clps_t * 400))
    bar_p = "█" * min(40, int(py_t  * 400))
    print(f"\n  {label}")
    print(f"    CLPS   {clps_t*1000:7.1f} ms  {bar_c}")
    print(f"    Python {py_t*1000:7.1f} ms  {bar_p}")
    print(f"    → {verdict}")
    return ratio

# ── 测试实体 ──────────────────────────────────────────────────────────────────

N_LOOP   = 50000
N_LIST   = 10000
N_WORKER = 20000
N_MSGS   = 2000

# ⑤ 启动基准（空.dao）
def write_empty():
    p = os.path.join(BASE, "_bench_empty.dao")
    with open(p, "w") as f:
        f.write("# empty\n乾 \"x\" 0\n")
    return p

empty_dao = write_empty()

def py_pass(): pass

# ① 循环累加
def py_loop():
    s = 0
    for i in range(N_LOOP):
        s += i
    return s

# ② 列表构建+聚合
def py_list_sum():
    lst = list(range(1, N_LIST + 1))
    return sum(lst)

# ③ 并发（Python threading，GIL限制）
def py_concurrent():
    results = []
    lock = threading.Lock()
    def worker():
        s = 0
        for i in range(N_WORKER): s += i
        with lock: results.append(s)
    threads = [threading.Thread(target=worker) for _ in range(4)]
    for t in threads: t.start()
    for t in threads: t.join()
    return sum(results)

# ④ 消息队列
def py_queue():
    q = pyqueue.Queue()
    total_box = [0]
    def producer():
        for i in range(1, N_MSGS + 1): q.put(i)
    def consumer():
        t = 0
        for _ in range(N_MSGS): t += q.get()
        total_box[0] = t
    pt = threading.Thread(target=producer)
    ct = threading.Thread(target=consumer)
    pt.start(); ct.start()
    pt.join(); ct.join()
    return total_box[0]

# ── 主程序 ────────────────────────────────────────────────────────────────────

print("=" * 62)
print("  道·码 (CLPS) vs Python — 性能对决")
print("  以实测定优劣，以卦象知边界")
print("=" * 62)

ratios = {}

# ⑤ 启动开销
ct = run_clps(empty_dao)
pt = run_py(py_pass)
ratios["startup"] = report(f"⑤ 启动开销（空程序）", ct, pt)

# ①
ct = run_clps("bench_loop.dao")
pt = run_py(py_loop)
ratios["loop"] = report(f"① 顺序累加 复{N_LOOP}（哈希表变量）", ct, pt)

# ②
ct = run_clps("bench_list.dao")
pt = run_py(py_list_sum)
ratios["list"] = report(f"② 列表构建{N_LIST}项 + 大有·总", ct, pt)

# ③
ct = run_clps("bench_concurrent.dao")
pt = run_py(py_concurrent)
ratios["concurrent"] = report(f"③ 并发 4师×复{N_WORKER}（CLPS真线程 vs GIL）", ct, pt)

# ④
ct = run_clps("bench_queue.dao")
pt = run_py(py_queue)
ratios["queue"] = report(f"④ 消息队列 {N_MSGS}条（同人 vs queue.Queue）", ct, pt)

# ── 总结分析 ──────────────────────────────────────────────────────────────────
print("\n" + "=" * 62)
print("  分析：")
print(f"  • 启动开销   ≈ {ratios['startup']*1000:.0f}ms overhead（clpsd 启动+解析）")
print(f"  • 哈希表循环  Python {ratios['loop']:.1f}× 快 — 每步 unordered_map 查询成本高")
print(f"  • C++ 聚合   {'CLPS' if ratios['list']<1 else 'Python'} 赢 — vector 遍历 vs Python sum(list)")
print(f"  • 真线程并发  {'CLPS' if ratios['concurrent']<1 else 'Python'} 赢 — 无GIL vs Python threading")
print(f"  • 消息队列   {'相当' if 0.75<ratios['queue']<1.33 else ('CLPS' if ratios['queue']<1 else 'Python')+' 赢'}")
print()
print("  结论：")
print("  ✅ CLPS 胜场：批量聚合(C++)、真并发(无GIL)、同步原语(condvar)")
print("  🐍 Python 胜场：短脚本、字符串处理、生态库、逐步调试")
print("  ⚠️  CLPS 固有成本：进程启动+解析 ≈ 数十ms，不适合极短任务")
print("=" * 62)

os.remove(empty_dao)

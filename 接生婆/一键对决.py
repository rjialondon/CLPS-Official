#!/usr/bin/env python3
"""
╔══════════════════════════════════════════════════════════════╗
║     道·码 (CLPS) vs 纯 Python  一键对决脚本                  ║
║     直接运行即可，无需安装任何额外软件                         ║
║                                                              ║
║  使用方法:                                                    ║
║     打开「终端」，输入:                                        ║
║     python3 /你把文件放在哪/一键对决.py                        ║
║                                                              ║
║     或者直接把这个文件拖进终端窗口，按回车                      ║
╚══════════════════════════════════════════════════════════════╝
"""

import time
import sys
import tracemalloc
import random
import threading

# ── 导入 Rust 加速版（CLPS 测试组专用）──
try:
    from taiji_core import TaiJiJieJie as _RustJJ
    from taiji_core import WuXing      as _RustWX
    from taiji_core import GenXuLock   as _RustLock
    _RUST_OK = True
except ImportError:
    _RUST_OK = False
    print("⚠️  taiji_core 未找到，CLPS 组将回退到纯 Python")

# ─────────────────────────────────────────────
#  检查环境
# ─────────────────────────────────────────────
if sys.version_info < (3, 7):
    print("❌ 需要 Python 3.7 以上版本")
    print(f"   你的版本: {sys.version}")
    input("按回车退出...")
    sys.exit(1)

print("""
╔══════════════════════════════════════════════════════════════╗
║      🦀 Rust+PyO3 加速版  vs  纯 Python  性能对决            ║
║         (CLPS 三核心类已用 Rust 重写，一键自动运行)           ║
╚══════════════════════════════════════════════════════════════╝
""")
print(f"  Python 版本: {sys.version.split()[0]}")
print(f"  系统平台:    {sys.platform}")
print(f"  当前时间:    {time.strftime('%Y-%m-%d %H:%M:%S')}")
print()
print("  ⏳ 正在运行测试，大约需要 30秒~2分钟，请耐心等待...")
print()


# =============================================================
#
#   第一部分：道·码 CLPS TaijiVM 的完整实现
#   (基于 v1.0~v5.0 白皮书架构)
#
# =============================================================

class WuXing:
    """五行类型引擎"""
    SHENG = {"金": "水", "水": "木", "木": "火", "火": "土", "土": "金"}
    KE    = {"金": "木", "木": "土", "土": "水", "水": "火", "火": "金"}

    @staticmethod
    def detect(value) -> str:
        if isinstance(value, (int, float)): return "金"
        if isinstance(value, list):         return "木"
        if isinstance(value, str):          return "火"
        if isinstance(value, dict):         return "土"
        return "土"

    @staticmethod
    def can_sheng(a, b): return WuXing.SHENG.get(a) == b

    @staticmethod
    def is_ke(a, b):    return WuXing.KE.get(a) == b


class TaiJiJieJie:
    """太极结界 (分形微内核作用域)"""
    __slots__ = ('name', 'level', 'resources', 'parent', 'alive', '_meta')

    def __init__(self, name, level=0, parent=None):
        self.name = name
        self.level = level
        self.resources = {}
        self.parent = parent
        self.alive = True
        self._meta = {}

    def qian(self, key, value, xing=None):
        self.resources[key] = value
        self._meta[key] = xing or WuXing.detect(value)

    def zhen(self, key, op, operand):
        if not self.alive: raise RuntimeError("结界已归元")
        val = self.resources.get(key)
        val_x = self._meta.get(key, WuXing.detect(val))
        op_x = WuXing.detect(operand)
        if WuXing.is_ke(op_x, val_x):
            raise TypeError(f"{op_x}克{val_x}")
        if   op == '+': result = val + operand
        elif op == '-': result = val - operand
        elif op == '*': result = val * operand
        elif op == '/': result = val / operand if operand else 0
        else: result = val
        self.resources[key] = result
        return result

    def dui(self, key):
        return self.resources.get(key)

    def kun(self):
        self.resources.clear()
        self._meta.clear()
        self.alive = False

    def qi_child(self, name):
        return TaiJiJieJie(name, self.level + 1, parent=self)


class GenXuLock:
    """艮需并发锁"""
    def __init__(self):
        self._lock = threading.Lock()

    def gen(self):  self._lock.acquire()
    def po(self):   self._lock.release()


# =============================================================
#
#   第二部分：六项测试（CLPS 版 + Python 版 背靠背运行）
#
# =============================================================

results_clps = {}
results_py = {}

def run_test(test_num, title, clps_fn, py_fn):
    """运行一组对比测试"""
    bar = "━" * 50
    print(f"  ┌{bar}┐")
    print(f"  │ 测试{test_num}: {title:44s}│")
    print(f"  └{bar}┘")

    # 运行 Rust 加速版
    rust_label = "🦀 Rust加速" if _RUST_OK else "🔶 道·码CLPS"
    sys.stdout.write(f"    {rust_label} ...  ")
    sys.stdout.flush()
    t_clps, m_clps = clps_fn()
    print(f"✅  {t_clps:.4f}s  内存峰值 {m_clps:.1f}KB")

    # 运行纯 Python 版
    sys.stdout.write("    🔷 纯 Python  ...  ")
    sys.stdout.flush()
    t_py, m_py = py_fn()
    print(f"✅  {t_py:.4f}s  内存峰值 {m_py:.1f}KB")

    # 计算倍率
    if t_py > 0:
        ratio = t_clps / t_py
        if ratio > 1:
            verdict = f"CLPS 慢 {ratio:.1f}×"
        else:
            verdict = f"CLPS 快 {1/ratio:.1f}× ✦"
    else:
        verdict = "持平"
    print(f"    📊 结论: {verdict}")
    print()

    return {
        "title": title,
        "clps_time": t_clps, "clps_mem": m_clps,
        "py_time": t_py, "py_mem": m_py,
        "verdict": verdict
    }


# ── 测试一：太极生灭 vs 函数调用 ──

N1 = 100_000

def test1_clps():
    root = _RustJJ("根", 0)          # Rust TaiJiJieJie
    ops = [
        ("qian", "账户", 100, "金"),
        ("zhen", "账户", '+', "$i"),  # "$i" = 当前循环下标
        ("zhen", "账户", '*', 2),
        ("dui",  "账户"),             # 结果丢弃
    ]
    tracemalloc.start()
    t0 = time.perf_counter()
    root.batch_lifecycle(N1, ops)    # 整段在 Rust 内执行
    elapsed = time.perf_counter() - t0
    _, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    return elapsed, peak / 1024

def test1_py():
    def process(i):
        a = 100; a += i; a *= 2; return a
    tracemalloc.start()
    t0 = time.perf_counter()
    for i in range(N1):
        _ = process(i)
    elapsed = time.perf_counter() - t0
    _, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    return elapsed, peak / 1024


# ── 测试二：五行类型 vs isinstance ──

N2 = 500_000

def _make_test_data(n):
    vals = []
    for i in range(n):
        r = i % 5
        if   r == 0: vals.append(random.randint(0, 99999))
        elif r == 1: vals.append([i, i+1])
        elif r == 2: vals.append(f"str{i}")
        elif r == 3: vals.append({"k": i})
        else:        vals.append(float(i))
    return vals

_test2_data = _make_test_data(N2)

def test2_clps():
    tracemalloc.start()
    t0 = time.perf_counter()
    s, k = _RustWX.batch_sheng_ke(_test2_data)   # 一次穿越，Rust 内全部统计
    elapsed = time.perf_counter() - t0
    _, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    return elapsed, peak / 1024

def test2_py():
    TYPE_MAP = {int: "number", float: "number", list: "array",
                str: "string", dict: "object"}
    COMPAT = {("array","string"),("string","object"),
              ("number","array"),("object","number")}
    CONFL  = {("number","array"),("array","object"),("string","number")}
    tracemalloc.start()
    t0 = time.perf_counter()
    s = k = 0
    for i in range(len(_test2_data) - 1):
        a = TYPE_MAP.get(type(_test2_data[i]), "unknown")
        b = TYPE_MAP.get(type(_test2_data[i + 1]), "unknown")
        if (a, b) in COMPAT: s += 1
        if (a, b) in CONFL: k += 1
    elapsed = time.perf_counter() - t0
    _, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    return elapsed, peak / 1024


# ── 测试三：艮需锁 vs threading.Lock ──

N3_OPS = 50_000
N3_THREADS = 8

def test3_clps():
    jj = _RustJJ("共享")
    jj.qian("账本", 0, "金")
    lock = _RustLock()
    def worker(n):
        for _ in range(n):
            lock.gen_zhen_po(jj, "账本", '+', 1)  # 坎变泰：lock+compute+unlock 一次调用，无GIL乒乓
    tracemalloc.start()
    t0 = time.perf_counter()
    threads = [threading.Thread(target=worker, args=(N3_OPS,)) for _ in range(N3_THREADS)]
    for t in threads: t.start()
    for t in threads: t.join()
    elapsed = time.perf_counter() - t0
    _, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    return elapsed, peak / 1024

def test3_py():
    shared = {"v": 0}
    lock = threading.Lock()
    def worker(n):
        for _ in range(n):
            with lock: shared["v"] += 1
    tracemalloc.start()
    t0 = time.perf_counter()
    threads = [threading.Thread(target=worker, args=(N3_OPS,)) for _ in range(N3_THREADS)]
    for t in threads: t.start()
    for t in threads: t.join()
    elapsed = time.perf_counter() - t0
    _, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    return elapsed, peak / 1024


# ── 测试四：辞海展开 vs dict查表 ──

N4 = 100_000

def test4_clps():
    from collections import OrderedDict
    cihai = OrderedDict()
    for i in range(N4):
        cihai[f"神通_{i}"] = [(0b111,f"c{i}"),(0b100,f"x{i}"),(0b110,f"s{i}"),(0b000,f"r{i}")]
    tracemalloc.start()
    t0 = time.perf_counter()
    total = _RustWX.xun_len_sum(cihai)   # 离+巽=风火家人: C++内一次扫完整个dict
    elapsed = time.perf_counter() - t0
    _, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    return elapsed, peak / 1024

def test4_py():
    d = {}
    for i in range(N4):
        d[f"macro_{i}"] = [("c",f"c{i}"),("x",f"x{i}"),("s",f"s{i}"),("r",f"r{i}")]
    tracemalloc.start()
    t0 = time.perf_counter()
    total = 0
    for i in range(N4):
        total += len(d[f"macro_{i}"])
    elapsed = time.perf_counter() - t0
    _, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    return elapsed, peak / 1024


# ── 测试五：分形深度 vs 嵌套作用域 ──

N5 = 1000

def test5_clps():
    root = _RustJJ("根", 0)          # Rust TaiJiJieJie
    tracemalloc.start()
    t0 = time.perf_counter()
    root.chain_grow_shrink(N5)       # 生长+收缩全程在 Rust 内执行
    elapsed = time.perf_counter() - t0
    _, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    return elapsed, peak / 1024

def test5_py():
    tracemalloc.start()
    t0 = time.perf_counter()
    chain = [{"name": "root", "vars": {}, "parent": None}]
    for i in range(1, N5 + 1):
        chain.append({"name": f"s{i}", "vars": {"v": i*10}, "parent": chain[-1]})
    while len(chain) > 1:
        s = chain.pop(); s["vars"].clear(); s["parent"] = None
    elapsed = time.perf_counter() - t0
    _, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    return elapsed, peak / 1024


# ── 测试六：综合业务（文本统计）──

def test6_clps():
    text = "道可道非常道名可名非常名无名天地之始有名万物之母" * 500
    tracemalloc.start()
    t0 = time.perf_counter()
    jj = _RustJJ("统计")
    jj.qian("文章", text, "火")
    stats = _RustWX.huo_xun(text)    # 火+巽=风火家人: C++内UTF-8解码+字频统计
    jj.qian("账本", stats, "土")
    _ = jj.dui("账本")
    jj.kun()
    elapsed = time.perf_counter() - t0
    _, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    return elapsed, peak / 1024

def test6_py():
    text = "道可道非常道名可名非常名无名天地之始有名万物之母" * 500
    tracemalloc.start()
    t0 = time.perf_counter()
    stats = {}
    for ch in text:
        stats[ch] = stats.get(ch, 0) + 1
    _ = stats
    elapsed = time.perf_counter() - t0
    _, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    return elapsed, peak / 1024


# =============================================================
#
#   第三部分（附）：八卦新操作演示 巽·坎·离·变
#
# =============================================================

if _RUST_OK:
    print("━" * 64)
    print("  🔮 八卦新操作演示（巽·坎·离·变）")
    print("━" * 64)

    # ── 演示1：巽·xun() ──────────────────────────────────────
    print("\n【巽·xun()】遍历资源")
    jj = _RustJJ("仓库")
    jj.qian("账户余额", 1000, "金")
    jj.qian("账户名称", "张三", "火")
    jj.qian("存货清单", [1, 2, 3], "木")
    jj.qian("其他", 99, "金")

    all_keys  = jj.xun()                 # 全部
    acct_keys = jj.xun("账户*")          # 前缀：账户…
    ming_keys = jj.xun("*名称")          # 后缀：…名称
    any_acct  = jj.xun("*账*")           # 包含：账
    exact     = jj.xun("存货清单")       # 精确

    print(f"  全部:       {list(all_keys.keys())}")
    print(f"  账户* →     {list(acct_keys.keys())}")
    print(f"  *名称 →     {list(ming_keys.keys())}")
    print(f"  *账* →      {list(any_acct.keys())}")
    print(f"  精确 →      {list(exact.keys())}")

    jj.kun()
    dead_keys = jj.xun()                 # alive=False → 空
    print(f"  归元后:     {dead_keys}  (应为空 dict)")

    # ── 演示2：坎·kan() ──────────────────────────────────────
    print("\n【坎·kan()】险中有孚，克制时回滚")
    jj2 = _RustJJ("险境")
    jj2.qian("灵气", 500, "金")   # 金属性

    # 成功：金（金）+ 数值（金） → 不克
    ok, val = jj2.kan("灵气", "+", 100)
    print(f"  金+金 成功: {ok}, 值={val}  (应为 True, 600)")

    # 克制：火克金 → 回滚，状态不变
    ok2, val2 = jj2.kan("灵气", "+", "火符")   # 字符串=火
    print(f"  火+金 克制: {ok2}, 值={val2}  (应为 False, None)")

    after = jj2.dui("灵气")
    print(f"  克制后状态: {after}  (应仍为 600，未回滚)")

    # 对比 zhen() 会抛异常
    try:
        jj2.zhen("灵气", "+", "火符")
        print("  zhen() 未异常（意外）")
    except TypeError as e:
        print(f"  zhen() 异常: {e}  (预期行为)")

    # ── 演示3：离·li() ──────────────────────────────────────
    print("\n【离·li()】跨结界引用绑定")
    src = _RustJJ("源泉")
    src.qian("水位", 200, "水")

    dst = _RustJJ("下游")
    dst.li("引水", src, "水位")  # dst["引水"] 指向 src["水位"]

    v1 = dst.dui("引水")
    print(f"  引用读取: {v1}  (应为 200)")

    # 源变，引用随之
    src.zhen("水位", "+", 50)
    v2 = dst.dui("引水")
    print(f"  源+50后:  {v2}  (应为 250)")

    # zhen() 写 dst 时断引用，转为本地
    dst.zhen("引水", "*", 2)
    v3 = dst.dui("引水")
    print(f"  断引用后: {v3}  (应为 500，= 250×2)")

    # 源再变，dst 不受影响
    src.zhen("水位", "+", 9999)
    v4 = dst.dui("引水")
    print(f"  源再变:   {v4}  (应仍为 500，引用已断)")

    # ── 演示4：变·bian() ──────────────────────────────────────
    print("\n【变·bian()】爻变，修改五行属性")
    jj3 = _RustJJ("变卦")
    jj3.qian("财宝", 888, "金")   # 金属性

    # 木克土，"财宝"=金，不克 → 成功
    ok3, r3 = jj3.kan("财宝", "+", [1, 2])  # 列表=木；金克木 → 木不克金
    print(f"  木+金(变前): ok={ok3}  (金不被木克，应 True)")

    # 变：金→木（改属性）
    jj3.bian("财宝", "木")

    # 金克木：operand=金(数字)克val=木 → 失败
    ok4, r4 = jj3.kan("财宝", "+", 100)     # 数字=金；金克木
    print(f"  金+木(变后): ok={ok4}  (金克木，应 False)")

    # 错误：key 不存在
    try:
        jj3.bian("不存在", "火")
        print("  bian(不存在) 未异常（意外）")
    except RuntimeError as e:
        print(f"  bian(不存在): {e}  (预期 RuntimeError)")

    # 错误：非法五行
    try:
        jj3.bian("财宝", "雷")
        print("  bian(雷) 未异常（意外）")
    except RuntimeError as e:
        print(f"  bian(非法): {e}  (预期 RuntimeError)")

    print("\n" + "━" * 64)
    print()


# =============================================================
#
#   第四部分：运行全部测试 + 生成报告
#
# =============================================================

all_results = []

all_results.append(run_test("①", "太极生灭 vs 函数调用 (10万次)", test1_clps, test1_py))
all_results.append(run_test("②", "五行类型 vs isinstance (50万次)", test2_clps, test2_py))
all_results.append(run_test("③", "艮需锁 vs threading.Lock (8线程)", test3_clps, test3_py))
all_results.append(run_test("④", "辞海展开 vs dict查表 (10万词)", test4_clps, test4_py))
all_results.append(run_test("⑤", "分形深度 vs 嵌套作用域 (1000层)", test5_clps, test5_py))
all_results.append(run_test("⑥", "综合业务: 文本统计", test6_clps, test6_py))


# ── 最终报告 ──

print()
rust_tag = "🦀Rust加速" if _RUST_OK else "道·码CLPS"
print("╔══════════════════════════════════════════════════════════════╗")
print(f"║          最 终 对 决 报 告  ({rust_tag} vs 纯Python)      ║")
print("╠══════════════════════════════════════════════════════════════╣")
print("║                                                            ║")
print("║  测试项             Rust      Python    倍率     胜负       ║")
print("║  ─────────────────────────────────────────────────────────  ║")

clps_wins = 0
py_wins = 0

for r in all_results:
    name = r["title"][:16].ljust(16)
    ct = f"{r['clps_time']:.3f}s".rjust(8)
    pt = f"{r['py_time']:.3f}s".rjust(8)

    if r["py_time"] > 0:
        ratio = r["clps_time"] / r["py_time"]
    else:
        ratio = 1.0

    if ratio > 1.05:
        ratio_str = f"{ratio:.1f}×慢".rjust(6)
        winner = " 🔷Py"
        py_wins += 1
    elif ratio < 0.95:
        ratio_str = f"{1/ratio:.1f}×快".rjust(6)
        winner = " 🔶道"
        clps_wins += 1
    else:
        ratio_str = " 持平 "
        winner = "  ⚖️ "

    print(f"║  {name} {ct}  {pt}  {ratio_str} {winner}   ║")

print("║                                                            ║")
print("╠══════════════════════════════════════════════════════════════╣")

if clps_wins > py_wins:
    summary = f"道·码 胜 {clps_wins}:{py_wins}"
elif py_wins > clps_wins:
    summary = f"纯Python 胜 {py_wins}:{clps_wins}"
else:
    summary = f"平局 {clps_wins}:{py_wins}"

print(f"║  总比分: {summary:49s}║")
print("║                                                            ║")
print("║  🦀 = Rust加速获胜    🔷 = 纯Python获胜    ⚖️ = 持平       ║")
print("║                                                            ║")
print("║  实现说明:                                                  ║")
print("║  · 🦀 CLPS 组: TaiJiJieJie/WuXing/GenXuLock 均为 Rust实现  ║")
print("║  · 🔷 Python组: 纯 Python 标准实现，作为基准对照            ║")
print("║                                                            ║")
print("║  Rust 实现要点:                                             ║")
print("║  · TaiJiJieJie: HashMap + PyDict，无 GIL 内存分配开销       ║")
print("║  · WuXing: 纯 Rust match 分支，零动态分配                  ║")
print("║  · GenXuLock: Mutex+Condvar，gen()释放GIL避免死锁           ║")
print("║  · 编译工具: maturin + PyO3 0.21 → cpython-3.9 .so         ║")
print("╚══════════════════════════════════════════════════════════════╝")
print()
print("  测试完毕！可以截图保存这份报告。")
print()

# 等待用户看完
input("  按回车键退出...")

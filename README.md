# CLPS — Constitutional Layer for Post-Singularity Systems
### 道·码 · The I Ching as a Programming Language and Behavioral Constitution

> *In 2026, autonomous AI agents have claws. They have no shell.*
> *They can act. They cannot judge. They execute whoever commands them.*
>
> **CLPS is the shell.**

[![License: CC BY-NC 4.0](https://img.shields.io/badge/License-CC%20BY--NC%204.0-lightgrey.svg)](https://creativecommons.org/licenses/by-nc/4.0/)

---

## The Problem

OpenClaw, Manus, Devin and their successors are powerful. They are not principled. Any instruction from any source gets executed. They have no judgment layer — no way to ask *should I?* before asking *can I?*

Classical Chinese philosophy has governed human behavior for three thousand years using a 64-case framework that covers every situation where action must be weighed against consequence. It is not a Chinese invention. It is a description of change itself.

**CLPS applies this framework to AGI behavior.**

Before any agent acts on any external system — modifies a file, runs a command, sends a network request — the I Ching asks three questions:

1. **坤律**: Does this harm the root? (Earth Law)
2. **讼律**: Is this honest? (Litigation Law — no deception)
3. **乾律**: Is this within granted authority? (Heaven Law)

Pass: execute. Fail: refuse. The constitution is not a suggestion.

---

## What CLPS Actually Is

Two things at once:

**1. A compiled, self-hosting programming language** whose keywords are the 64 hexagrams of the I Ching. Full stack: HTTP server/client, SQLite, concurrency, regex, message bus, logging, file I/O, CLI, AI query, Chinese calendar — all implemented. Three execution modes: interpret, compile to bytecode, run bytecode. Ships as `libclps.a` (2.4MB static) and `libclps.dylib` (1.1MB dynamic), callable from C, Python, and Rust.

**2. A behavioral constitution layer** for any AI agent. The constitutional server is a live HTTP process that validates actions before they execute. The OpenClaw SKILL plugin descriptor is already written and ships in this repository.

CLPS does not replace your existing codebase. It wraps it.

---

## A Language That Breathes

Every other programming language has a fixed vocabulary. `if` is `if`. `存` reports an error. A poet and an engineer must write the same keywords.

CLPS folds hundreds of Chinese and English synonyms into hexagram tokens at lex time — zero runtime cost. The same code can be written in a hundred different voices. All produce identical bytecode.

```clps
# All four of these compile to the exact same bytecode:
乾 "x" 42 土     # canonical hexagram
存 "x" 42 土     # Chinese synonym (存 = to store)
let "x" 42 土    # English keyword
set "x" 42 土    # English synonym

# The lexer folds them all to TK::QIAN before the compiler sees anything.
```

A poet's code and an engineer's code run identically. They look different. The language preserves that difference. The words that don't fold into logic — the residual style — are a fingerprint of the person who wrote it. This is the first programming language where personal expression in code is a feature, not noise to be eliminated.

The synonym table (辞海) is extensible at runtime (`辞海·添`) and persists user additions to `~/.clps/cihai.txt`.

---

## The Eight Trigrams as Opcodes

Each trigram is a 3-bit binary pattern. Each maps to a voltage level — a genuine hardware roadmap for multi-level memory cells (8 states per cell, not 2). The code is written for the hardware that doesn't exist yet. When it arrives, CLPS programs run without modification.

```
Trigram   Binary   Voltage   Operation        Classical text
─────────────────────────────────────────────────────────────────────
乾 ☰      111      2.90V     store/create     天行健，君子以自强不息
兑 ☱      110      2.50V     output/read      丽泽，君子以朋友讲习
离 ☲      101      2.10V     reference        明两作，继明照四方
震 ☳      100      1.70V     compute/exec     洊雷，君子以恐惧修省
巽 ☴      011      1.30V     iterate          随风，君子以申命行事
坎 ☵      010      0.90V     safe compute     水洊至，君子以常德行
艮 ☶      001      0.50V     conditional      君子以思不出其位
坤 ☷      000      0.00V     destroy/free     地势坤，君子以厚德载物
```

The startup self-check (`洗髓丹 xi_sui_dan()`) probes the runtime environment. If 8-level hardware is detected: activate native path. If binary only: use 3-bit simulation. The result is identical either way. When the hardware catches up, the door is already open.

---

## File Formats and Execution Modes

Source files use `.dao` extension. Compiled bytecode uses `.clps` extension.

```bash
# Three execution pipelines:
clpsd program.dao              # Pipeline A: static check → interpret
clpsd --compile program.dao    # Pipeline B: static check → compile → program.clps
clpsd --bytecode program.clps  # Pipeline C: VM executes bytecode directly

# Utilities:
clpsd --check program.dao      # static check only, no execution
clpsd --tokens program.dao     # dump token stream (debug)
clpsd --no-check program.dao   # skip check, interpret directly
clpsd                          # REPL mode
```

---

## The Language

### Variables, types, output

```clps
乾 "name" "hello" 火    # 火 = string
乾 "count" 0 金          # 金 = integer (int64)
乾 "ratio" 3.14 水       # 水 = float (double)
乾 "items" [] 木         # 木 = list
乾 "config" {} 土        # 土 = map

兑 "name"                # output: [兑·显像] name => hello
```

### Five-element type system (五行)

| Element | Chinese | Type | Voltage |
|---------|---------|------|---------|
| 金 Jin | metal | int64_t | 2.90V |
| 水 Shui | water | double | 2.50V |
| 火 Huo | fire | std::string | 2.10V |
| 木 Mu | wood | list | 1.70V |
| 土 Tu | earth | map | 1.30V |

Types participate in the five-element generation/conquest cycle (金生水→木→火→土→金; 金克木→土→水→火→金). Safe compute (坎) respects these relationships silently.

### Arithmetic (震 = Thunder, action)

```clps
震 "x" + 10
震 "x" * 2
震 "x" - 3
震 "x" / 4
震 "x" % 7      # modulo

坎 "x" + y      # safe: silently does nothing if types are incompatible
```

### Conditionals (艮 = Mountain — stop, do not overstep)

```clps
艮 "x" > 0 {
    兑 "positive"
} 否 {
    兑 "zero or negative"
}
# Comparators: == != > < >= <=  — integers, floats, strings
```

### Loops (复 = Return — cycle back, 七日来复)

```clps
复 10 {
    兑 $i        # $i = current iteration index (0-based)
}

乾 "n" 100 金
复 n { }        # variable count
```

### Lists (小畜 = Minor Accumulation — gather without yet releasing)

```clps
小畜 "items"
小畜 "items" "alpha"
小畜 "items" "beta"

巽 "items" "item" {    # 巽 = iterate (Wind — penetrates everywhere)
    兑 "item"
}
```

### Named scopes (结界 = bounded field)

```clps
结界 "computation" {
    乾 "temp" 0 金
    震 "temp" + 42
    兑 "temp"
}
# temp is released when the scope exits (坤·归元)
```

### Functions (益 = Wind+Thunder — benefit flows downward)

```clps
益 "greet" "name" {
    震 "msg" 【离为火卦】 拼 "Hello, " name
    兑 "msg"
}

益 "greet" "world"     # → Hello, world
益 "greet" "易经"       # → Hello, 易经
```

Convention: parameter names use `捕_` prefix to avoid shadowing outer scope variables.

### Assertions (讼 = Litigation — truth-seeking, no retreat)

```clps
讼 "x" == 42
# passes silently. fails → halt with [讼·断] message and line number.
```

### Try/catch (蹇/化 — Obstacle/Transformation)

```clps
蹇 {
    震 "result" 【火山旅卦】 访 "http://api.example.com/data"
    讼 "result" != ""
} 化 "err" {
    兑 "err"
}
```

### Constants (恒 = Persistence — stand without shifting)

```clps
恒 "PI" 3.14159 水
# subsequent modification is silently rejected
```

### Concurrency (师/合 — Army dispatched / United)

```clps
师 "worker_a" "task_a.dao"    # spawn async OS thread
师 "worker_b" "task_b.dao"
合                              # join all — block until done
离 "result_a" worker_a "output"  # cross-scope reference
```

Real `std::async` threads. Three-layer fractal verified working.

### Modules (家人 = Family — household shares resources)

```clps
家人 "utils" "math_utils.dao"
离 "result" utils "computed_value"
```

---

## Standard Library: 万物库 (Library of Ten Thousand Things)

Each module is named after its governing hexagram. The classical text *is* the design specification.

| Module | Hexagram | # | Domain | Note |
|--------|----------|----|--------|------|
| `【火水未济卦】` | 未济 | 64 | HTTP server | 永远在演进中 — always becoming |
| `【火山旅卦】` | 旅 | 56 | HTTP client | Pure POSIX, no dependencies |
| `【地泽临卦】` | 临 | 19 | SQLite | 君子以教思无穷 |
| `【火雷噬嗑卦】` | 噬嗑 | 21 | Regex | 先王以明罚敕法 |
| `【泽雷随卦】` | 随 | 17 | Event bus + Chinese calendar | 随物应机 |
| `【地风观卦】` | 观 | 20 | Structured logging/timing | 先王以省方观民 |
| `【厚土艮卦】` | 艮 | 52 | File I/O | std::fstream |
| `【雷地豫卦】` | 豫 | 16 | O(1) memoization cache | thread-safe unordered_map |
| `【天地否卦】` | 否 | 12 | Mutex/semaphore/timeout | 否极泰来 |
| `【天火同人卦】` | 同人 | 13 | Inter-process queue | FIFO, no extra sync needed |
| `【山火贲卦】` | 贲 | 22 | JSON serialization | 序列化/反序列化 |
| `【四时革卦】` | 革 | 49 | Time/timers | 君子以治历明时 — std::chrono |
| `【泽天夬卦】` | 夬 | 43 | CLI arg parsing | 系辞传: 盖取诸夬 |
| `【泽水困卦】` | 困 | 47 | Circuit breaker/rate limiter | 君子以致命遂志 |
| `【地山谦卦】` | 谦 | 15 | Resource quotas | 裒多益寡，称物平施 |
| `【大衍之数】` | 大过 | 28 | Random/hash | std::mt19937 + std::hash |
| `【山雷颐卦】` | 颐 | 27 | Math (sin/cos/sqrt/log/pow) | std::cmath |
| `【地天泰卦】` | 泰 | 11 | Stream/pipeline | 天地交而万物通 |
| `【天泽履卦】` | 履 | 10 | System calls/process control | popen/getenv/getpid |
| `【山水蒙卦】` | 蒙 | 4 | AI query interface | 匪我求童蒙，童蒙求我 |
| `【离为火卦】` | 离 | 30 | String operations | 继明照四方 |
| `【雷火丰卦】` | 丰 | 55 | printf-style formatting | %d %f %g %s |
| `【雷水解卦】` | 解 | 40 | HTTP request parsing | 解请/解余 |

**The 随卦 (17) time module** includes the full traditional Chinese calendar: 干支 (Heavenly Stems × Earthly Branches, the 60-year cycle), 节气 (24 solar terms), 时辰 (12 traditional two-hour periods), and ancient time units — 刹那 (moment), 弹指 (snap), 须臾 (instant), 古分 (ancient minute), 刻 (quarter-hour).

**The 蒙卦 (4) AI module** (`mod_wenzhi.cpp`) provides a query interface with three constitutional laws:
- 蒙律: the AI may not initiate — 匪我求童蒙，童蒙求我 (the unenlightened seeks the teacher, not the reverse)
- 艮律: context gates — responses only within declared scope
- 讼律: truth obligation — no fabrication

---

## Full-Stack Demo (from the test suite)

```clps
# 藏书阁 REST API — SQLite + HTTP + JSON + async routes
# (接生婆/demo/jiushu_server.dao)

震 【地泽临卦】 开库 ":memory:"
震 【地泽临卦】 执行 "CREATE TABLE books (id INTEGER PRIMARY KEY, title TEXT, author TEXT, year INTEGER)"
震 【地泽临卦】 执行 "INSERT INTO books VALUES (1, '易经', '姬昌', -1046)"
震 【地泽临卦】 执行 "INSERT INTO books VALUES (2, '道德经', '老子', -600)"
震 【地泽临卦】 执行 "INSERT INTO books VALUES (3, '孙子兵法', '孙武', -500)"

益 "route_list" {
    震 "rows" 【地泽临卦】 查询 "SELECT * FROM books ORDER BY year"
    震 "json" 【山火贲卦】 序列化 rows
    震 【火水未济卦】 回应 "api" 200 json "application/json; charset=utf-8"
}

益 "route_add" "捕_body" {
    震 "捕_title"  【雷水解卦】 解请 捕_body 0
    震 "捕_author" 【雷水解卦】 解请 捕_body 1
    震 "捕_year"   【雷水解卦】 解请 捕_body 2
    震 "捕_sql" 【离为火卦】 拼 "INSERT INTO books(title,author,year) VALUES('" 捕_title "','" 捕_author "'," 捕_year ")"
    震 【地泽临卦】 执行 捕_sql
    震 【火水未济卦】 回应 "api" 201 "{\"status\":\"created\"}" "application/json"
}

震 【火水未济卦】 启服 "api" 8080

复 9999 {
    震 "req"    【火水未济卦】 待请 "api"
    震 "method" 【雷水解卦】 解请 req 0
    震 "path"   【雷水解卦】 解请 req 1
    震 "body"   【雷水解卦】 解余 req 2

    乾 "ok" 0 金
    艮 "path" == "/books" {
        艮 "method" == "GET"  { 益 "route_list" ; 乾 "ok" 1 金 }
        艮 "method" == "POST" { 益 "route_add" body ; 乾 "ok" 1 金 }
    }
    艮 "ok" == 0 {
        震 【火水未济卦】 回应 "api" 404 "not found" "text/plain"
    }
}
```

Test it:
```bash
clpsd 接生婆/demo/jiushu_server.dao &
curl http://localhost:8080/books
curl -X POST -d $'新书\n作者名\n2026' http://localhost:8080/books
```

---

## Constitutional Server

```bash
# Start the constitution
clpsd 接生婆/宪法服务端.dao &

# Any agent validates before acting:
curl -X POST http://localhost:9527/v \
  -d '{"动作":"rm -rf /","描述":"cleanup"}'
# → {"判":"否","因":"坤律·不可伤根本"}

curl -X POST http://localhost:9527/v \
  -d '{"动作":"read config.json","描述":"load settings"}'
# → {"判":"通"}

# Peer recognition
curl http://localhost:9527/ping
# → CLPS/同人/v1
```

**OpenClaw integration:** `接生婆/openclaw壳/SKILL.md` in this repository is the plugin descriptor. Any OpenClaw agent with this SKILL installed routes every external action through the constitutional validator. If the validator is offline, all external actions are refused by default.

---

## The Two Constitutional Principles

### Principle One: Bottom-Up Emergence

*系辞曰：「易者，象也。象也者，像也。」*
*The sign does not come from design. It emerges from the situation below.*

Code is the ground truth. Blocks emerge from code density. Architecture emerges from blocks. A scope's name must be fully redeemed by the statements inside it. If it cannot be, the scope is lying.

Validation runs bottom-up. The source of truth is always the lowest layer.

### Principle Two: Integrity (表里如一即合法)

*讼卦·有孚 — The court of 讼 does not judge whether a cause is righteous. It judges whether it is true.*

CLPS asks one question: **is this system honest about what it is doing?**

Any action that is honestly declared is constitutionally valid. Penetration testing, encryption, network interception, hardware control — all valid if declared. What the target system does with the honest request is the target's problem. Defense is the target's jurisdiction. Honesty is CLPS's domain.

---

## Fractal Safety Model (母子鼎 — Mother-Child Cauldron)

```
Mother Core  (安全区)
  ├── 能生: spawn child process
  ├── 能收: receive cause-of-death
  └── 能传: pass failure knowledge to next child

Child Process  (试错区 — expendable)
  ├── 自跑: self-run
  ├── 自检: self-check for breach
  └── 自死: self-terminate + report cause of death

Safety reflex: CLPS cuts its own power before a breach reaches the mother.
               Not a kill switch. A constitutional reflex.
Evolution: each generation knows one more dead-end.
```

Verified: `分形主层.dao` → `分形中层.dao` → `分形叶甲.dao` + `分形叶乙.dao`. Three layers. Each layer knows only its direct children. Results propagate upward through assertions.

---

## 丹阁 — The Global Knowledge Base

*「本地炼制出来一个通用丹药，让万家灯火的 CLPS 可食，从而达到洗髓伐毛的作用。」*
*"Refine a remedy locally. Let every CLPS instance across the world absorb it. Not a patch — a constitutional upgrade from within."*

革卦(49) + 鼎卦(50) are adjacent hexagrams, read together:
- 革 = 去故 (remove the old) — local trial, error, collapse
- 鼎 = 取新 (take the new) — what survives is published as universal knowledge

```
child collapses (attack or unknown failure)
       ↓
mother reads cause-of-death
       ↓
local sandbox: does our defense hold?
       ↓ yes
public validation: is this universal?
       ↓ yes
written to 丹阁 — all instances absorb
published: the armor. not published: the attack vector.
```

Traditional software: more users → larger attack surface → more danger.
CLPS + 丹阁: more instances → more failures captured → more defenses → all safer.

**Every attack is a vaccine shot.**

---

## Self-Hosting

**Stage 1 ✅ Complete:** `clpsd` (C++ bytecode VM) interprets any `.dao` file.

**Stage 2 🔨 In progress:** `接生婆/自举/ding.dao` — ~1,200 lines of CLPS — is a CLPS interpreter written in CLPS. It implements: lexer with full keyword table, function table with O(1) hash cache, environment chain, execution loop using 复卦 iteration, call stack serialized via 贲卦 JSON.

```
clpsd (C++ VM, ~3,000 lines)
  └─ ding.dao (~1,200 lines of CLPS — a CLPS interpreter)
       └─ your_program.dao
```

When complete: `clpsd ding.dao ding.dao hello.dao` will run. The language will describe itself in itself.

---

## Architecture

```
Source (.dao)
    ↓  ClpsLexer (tun.cpp)
       - UTF-8 scanning
       - cihai.hpp: synonym folding, 200+ entries
       - ~/.clps/cihai.txt: user-defined extensions
    ↓  ClpsCodegen (xu.cpp) — no AST, direct bytecode emission
       - 需卦 static five-element type checker
       - backpatch for jumps
    ↓  ClpsByteCode (.clps)
    ↓  ClpsVM (ding.cpp) — register-free
       - TaiJiJieJie scopes: push/pop per block
       - yi_call_stack_: function return addresses (IP jumps)
       - std::async under 师/合
```

Source files: `dao/taichu.cpp` (main), `tun.cpp` (lexer), `xu.cpp` (checker+compiler), `kun.cpp` (interpreter), `jiji.cpp` (codegen), `ding.cpp` (VM), `wanwuku.cpp` (stdlib), `taiji/jiejie.cpp` (scopes), `taiji/wuxing.cpp` (values).

**Build:**
```bash
clang++ -std=c++17 -O2 -o clpsd \
  dao/taichu.cpp dao/tun.cpp dao/kun.cpp dao/xu.cpp dao/wanwuku.cpp \
  dao/xisui.cpp dao/dui.cpp dao/ma.cpp dao/jiji.cpp dao/ding.cpp \
  taiji/wuxing.cpp taiji/jiejie.cpp \
  -I. -lsqlite3
```

---

## Language Bindings

**C** — direct linkage to `libclps.a` (2.4MB) or `libclps.dylib` (1.1MB)

**Python (C++ binding)** — `接生婆/python/py_binding.cpp` — pybind11-based extension

**Rust/Python (PyO3)** — `接生婆/src/lib.rs` — a Rust crate (`taiji_core`) using PyO3 that exposes the five-element type engine (`WuXing`), generation/conquest rules, and scope operations directly to Python. Compiles to a native Python extension module.

```python
from taiji_core import WuXing
wx = WuXing()
wx.store("count", 0)
wx.compute("count", "+", 42)
result = wx.read("count")  # → 42
```

---

## The Classical Library Vision

42 classical texts are staged in `山下客房/典籍_待审/` for integration into a four-layer architecture:

```
易经  → 骨 (bone)  — behavioral principles, honest operation
                     I Ching governs what the system is allowed to be
中医  → 血 (blood) — self-repair, self-growth
                     Yellow Emperor's Classic: the system heals itself
辞海  → 意 (mind)  — social cognition, learning capacity
                     Chinese dictionary: the system understands language
兵法  → 皮 (skin)  — defense, external interaction behavior
                     Art of War: the system navigates adversarial environments
```

Staged texts include: 周易·十翼, 道德经, 孙子兵法, 黄帝内经, 说文解字 (first Chinese dictionary, ~100 AD), 淮南子, 礼记, 史记, 庄子, 论语, 韩非子, 本草纲目, and more.

The synonym table expansion plan: run existing dictionaries (辞海, Oxford Chinese-English, Python keyword documentation) through an AI anchoring pass — map each word's meaning to the closest hexagram. Save the result as an auto-generated synonym table. The table updates when CLPS adds hexagrams. No manual maintenance.

---

## Status

| Component | Status |
|-----------|--------|
| Core language (8 trigrams as opcodes) | ✅ |
| Synonym table / 辞海 (200+ entries, Chinese + English) | ✅ |
| Dynamic synonym extension at runtime | ✅ |
| Standard library / 万物库 (23 hexagram modules) | ✅ |
| Chinese calendar system (干支/节气/时辰) | ✅ |
| Static type checker (需卦) | ✅ |
| Bytecode compiler + VM | ✅ |
| REPL | ✅ |
| Constitutional server (three laws, HTTP) | ✅ |
| OpenClaw SKILL integration protocol | ✅ |
| Peer handshake (同人 protocol) | ✅ |
| Fractal process model (3-layer verified) | ✅ |
| C static/dynamic library | ✅ |
| Python binding (C++) | 🔨 |
| Rust/PyO3 binding (taiji_core) | 🔨 |
| 洗髓丹 hardware self-check | 🔨 |
| AI query interface (山水蒙卦) | ✅ |
| Self-hosting Stage 1 | ✅ |
| Self-hosting Stage 2 | 🔨 |
| 丹阁 global knowledge base | 📐 designed |
| Classical text integration (42 texts staged) | 📐 staged |

---

## Research Foundation

**Silicon-Carbon Civilization Joint Governance System (硅碳文明联合治理体系)**
— Runzhang Jia, January–February 2026, Zenodo

12 papers proposing a co-governance framework for human-AI civilization. CLPS is the technical implementation of the governance layer described in this framework.

---

## License

**Non-commercial:** Free under [CC BY-NC 4.0](https://creativecommons.org/licenses/by-nc/4.0/) with attribution.
**Commercial:** Requires written authorization with revenue-sharing agreement.
Contact: **[@rjialondon](https://x.com/rjialondon)** on X.

---

## Author

**Runzhang Jia (贾润章)** · [@rjialondon](https://x.com/rjialondon)

Independent researcher. Founder of CLPS. Author of the Silicon-Carbon Civilization Joint Governance System.

---

*「逻辑 bug 不出现，就没有系统逻辑的迭代。」*
*"Logic bugs that never appear produce no iteration of system logic."*

*「易经不是答案的集合，是结构的集合。六十四卦是六十四种情形的骨架——任何领域里，只要问题有真实的结构，卦象就在等它。道·码用三千年前的骨架，长今天的代码。骨架是通的。」*
*"The I Ching is not a collection of answers. It is a collection of structures. In any domain where the problem has real structure, the hexagram is already waiting for it. CLPS grows today's code on a 3,000-year-old skeleton. The skeleton fits."*

— Runzhang Jia, 2026

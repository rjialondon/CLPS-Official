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

1. **坤律**: Does this harm the root? (Earth Law — do not destroy foundations)
2. **讼律**: Is this honest? (Litigation Law — no deception, no impersonation)
3. **乾律**: Is this within granted authority? (Heaven Law — no privilege escalation)

Pass: execute. Fail: refuse. The constitution is not a suggestion.

---

## What CLPS Actually Is

Two things at once:

**1. A compiled, self-hosting programming language** whose keywords are the 64 hexagrams of the I Ching. Full stack: HTTP server/client, SQLite, concurrency, regex, message bus, logging, file I/O, CLI, time, math — all implemented. Compiles to bytecode. Ships as `libclps.a` (2.4MB) and `libclps.dylib` (1.1MB), callable from C and Python.

**2. A behavioral constitution layer** for any AI agent. The constitutional server (`宪法服务端.dao`) is a live HTTP process that validates actions before they execute. Any agent — OpenClaw, a custom script, any system — can query it. The protocol is already written.

CLPS does not replace your existing codebase. It wraps it.

---

## The Eight Trigrams as Opcodes

The I Ching encodes situations using 8 trigrams. Each trigram is a 3-bit binary pattern. Each maps to a voltage level in a future multi-level hardware model. Each maps to a computational operation. The encoding is not arbitrary — it follows the hexagram's philosophical meaning.

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

The voltage roadmap: future hardware where each memory cell holds 8 voltage levels (not 2) maps directly to trigrams. Every CLPS opcode is already designed for the hardware that doesn't exist yet.

---

## The Synonym Table (辞海)

The lexer folds hundreds of words — Chinese and English — into hexagram tokens at lex time. Zero runtime cost. Every word that means "store" becomes 乾. Every word that means "if" becomes 艮.

```clps
# All of these compile to identical bytecode:
乾  "x" 42 土     # canonical: 乾 (Heaven — store)
存  "x" 42 土     # Chinese synonym: 存 = to store
载  "x" 42 土     # Chinese synonym: 载 = to carry/load
let "x" 42 土     # English keyword
set "x" 42 土     # English synonym
var "x" 42 土     # English synonym
def "x" 42 土     # English synonym

# Same for conditionals:
艮  "x" == 0 { }  # canonical: 艮 (Mountain — stop, gate)
若  "x" == 0 { }  # Chinese: 若 = if
if  "x" == 0 { }  # English
when "x" == 0 { } # English synonym

# Loops:
复  10 { }        # canonical: 复 (Return — cycle back)
loop 10 { }       # English
repeat 10 { }     # English synonym
```

The synonym table is extensible at runtime (`辞海·添`). Users can add their own language's words. `~/.clps/cihai.txt` persists custom entries across sessions.

**The language is not culturally locked.** If the word means "store", it maps to 乾.

---

## The Language: Full Syntax

### Variables and output

```clps
乾 "name" "value" 火    # 乾 = store; 火 = string type
乾 "count" 0 金         # 金 = integer type
乾 "ratio" 3.14 水      # 水 = float type

兑 "name"               # 兑 = output (opens mouth, speaks)
# → [兑·显像] name => value
```

### Five-Element type system

| Element | Keyword | Type |
|---------|---------|------|
| 金 Jin | integer | int64_t |
| 水 Shui | float | double |
| 火 Huo | string | std::string |
| 木 Mu | list | dynamic array |
| 土 Tu | map | key-value |

Types participate in the five-element generation/conquest cycle. Safe compute (坎) respects these relationships silently — if a type mismatch would cause an error, 坎 does nothing rather than crashing.

### Arithmetic

```clps
震 "x" + 10      # 震 = compute (Thunder — action)
震 "x" * 2
震 "x" - 3
震 "x" / 4
震 "x" % 7       # modulo

坎 "x" + y       # 坎 = safe compute (Water — finds its path around obstacles)
                  # silently does nothing if types are incompatible
```

### Conditionals (艮 — Mountain: stop, do not overstep)

```clps
艮 "x" > 0 {
    兑 "positive"
} 否 {
    兑 "zero or negative"
}
# Supports: == != > < >= <=
# Works on integers, floats, strings
```

### Loops (复 — Return: cycle back, seven days to return)

```clps
复 10 {
    兑 $i       # $i = current iteration index (0-based)
}

# Variable count:
乾 "n" 100 金
复 n {
    # ...
}
```

### Lists (小畜 — Minor Accumulation: cloud gathers without rain yet)

```clps
小畜 "items"              # create list
小畜 "items" "first"      # append
小畜 "items" "second"
小畜 "items" "third"

巽 "items" "item" {       # 巽 = iterate (Wind — penetrates everywhere)
    兑 "item"
}
```

### Functions (益 — Wind+Thunder: benefit flows downward)

```clps
益 "greet" "name" {
    震 "msg" 【离为火卦】 拼 "Hello, " name
    兑 "msg"
}

益 "greet" "world"
# → Hello, world
```

### Assertions (讼 — Litigation: truth-seeking, no retreat)

```clps
讼 "x" == 42
# passes silently if true
# halts with [讼·断] 断言失败 if false
```

### Try/catch (蹇/化 — Obstacle/Transformation)

```clps
蹇 {
    震 "result" 【火山旅卦】 访 "http://api.example.com/data"
    讼 "result" != ""
} 化 "err" {
    兑 "err"
    乾 "result" "fallback" 火
}
```

### Constants (恒 — Persistence: stand without shifting position)

```clps
恒 "PI" 3.14159 水
# Any subsequent attempt to modify PI is silently rejected
```

### Concurrency (师/合 — Army dispatched / United)

```clps
# 师(7): Dispatch troops — spawn async OS tasks
师 "worker_a" "task_a.dao"
师 "worker_b" "task_b.dao"
师 "worker_c" "task_c.dao"

# 合: Unite — join all, block until done
合

# 离: Cross-boundary reference — access child scope results
离 "result_a" worker_a "output"
离 "result_b" worker_b "output"
```

True `std::async` threads. Not coroutines. Not simulated. The three-layer fractal demo (`分形主层.dao` → `分形中层.dao` → `分形叶甲.dao` + `分形叶乙.dao`) is verified working.

### Modules (家人 — Family: a household shares resources)

```clps
家人 "utils" "math_utils.dao"
离 "result" utils "computed_value"
```

---

## Standard Library: The 万物库 (Library of Ten Thousand Things)

Every module is named after the hexagram that governs its domain. The naming is not decorative — the hexagram's classical meaning is the design specification for the module.

| Module | Hexagram | Number | Domain | Classical text |
|--------|----------|--------|--------|----------------|
| `【火水未济卦】` | 未济 | 64 | HTTP server | 君子以慎辨物居方 — always in the process of becoming |
| `【火山旅卦】` | 旅 | 56 | HTTP client | 旅行于网络，问道远方 |
| `【地泽临卦】` | 临 | 19 | SQLite | 君子以教思无穷，容保民无疆 |
| `【火雷噬嗑卦】` | 噬嗑 | 21 | Regex | 先王以明罚敕法 — bite through obstacles |
| `【泽雷随卦】` | 随 | 17 | Event bus + Chinese calendar | 随物应机 — respond to the signal |
| `【地风观卦】` | 观 | 20 | Structured logging | 先王以省方观民 — observe all |
| `【厚土艮卦】` | 艮 | 52 | File I/O | 君子以思不出其位 — persistence |
| `【雷地豫卦】` | 豫 | 16 | O(1) memoization | 利建侯行师 — prepare in advance |
| `【天地否卦】` | 否 | 12 | Mutex/semaphore | 天地不交 → 否极泰来 |
| `【天火同人卦】` | 同人 | 13 | Inter-process queue | 同人于野 — like-minded across distance |
| `【山火贲卦】` | 贲 | 22 | JSON serialization | 君子以明庶政 — give form to content |
| `【四时革卦】` | 革 | 49 | Time/timers | 君子以治历明时 |
| `【泽天夬卦】` | 夬 | 43 | CLI arg parsing | 结绳→书契 (系辞传: 盖取诸夬) |
| `【泽水困卦】` | 困 | 47 | Circuit breaker | 君子以致命遂志 |
| `【地山谦卦】` | 谦 | 15 | Resource quotas | 裒多益寡，称物平施 |
| `【大衍之数】` | 大过 | 28 | Random/hash | 独立不惧，遁世无闷 |
| `【山雷颐卦】` | 颐 | 27 | Math (sin/cos/sqrt/log/pow) | 君子以慎言语，节饮食 |
| `【地天泰卦】` | 泰 | 11 | Stream/pipeline | 天地交而万物通 |
| `【天泽履卦】` | 履 | 10 | System calls | 履虎尾不咥人 |
| `【山水蒙卦】` | 蒙 | 4 | AI query interface | 匪我求童蒙，童蒙求我 |
| `【离为火卦】` | 离 | 30 | String operations | 明两作，继明照四方 |
| `【雷火丰卦】` | 丰 | 55 | String formatting (printf-style) | 君子以折狱致刑 |

The 随卦 (17) module includes the traditional Chinese time system: 干支 (Heavenly Stems and Earthly Branches), 节气 (24 solar terms), 时辰 (12 traditional hours), and ancient time units (刹那/弹指/须臾/古分/刻).

---

## Full-Stack Demo

A working HTTP API server from the test suite:

```clps
# ── Initialize SQLite database ──────────────────────────────────
震 【地泽临卦】 开库 ":memory:"
震 【地泽临卦】 执行 "CREATE TABLE books (id INTEGER PRIMARY KEY, title TEXT, author TEXT, year INTEGER)"
震 【地泽临卦】 执行 "INSERT INTO books VALUES (1, '易经', '姬昌', -1046)"
震 【地泽临卦】 执行 "INSERT INTO books VALUES (2, '道德经', '老子', -600)"
震 【地泽临卦】 执行 "INSERT INTO books VALUES (3, '孙子兵法', '孙武', -500)"

# ── Define route handlers ────────────────────────────────────────
益 "route_list" {
    震 "rows" 【地泽临卦】 查询 "SELECT * FROM books ORDER BY year"
    震 "json" 【山火贲卦】 序列化 rows
    震 【火水未济卦】 回应 "api" 200 json "application/json"
}

益 "route_404" "path" {
    震 "msg" 【离为火卦】 拼 "Not found: " path
    震 【火水未济卦】 回应 "api" 404 msg "text/plain"
}

# ── Start server, main loop ─────────────────────────────────────
震 【火水未济卦】 启服 "api" 8080

复 9999 {
    震 "req"    【火水未济卦】 待请 "api"
    震 "method" 【雷水解卦】 解请 req 0
    震 "path"   【雷水解卦】 解请 req 1

    乾 "handled" 0 金

    艮 "path" == "/books" {
        益 "route_list"
        乾 "handled" 1 金
    }
    艮 "handled" == 0 {
        益 "route_404" path
    }
}
```

---

## Constitutional Server

CLPS ships a live behavioral constitution as an HTTP service. Any system component queries it before acting:

```bash
# Start the constitution
clpsd 宪法服务端.dao &

# Any agent validates before acting:
curl -X POST http://localhost:9527/v \
  -d '{"动作": "rm -rf /", "描述": "cleanup temp files"}'
# → {"判":"否","因":"坤律·不可伤根本"}

curl -X POST http://localhost:9527/v \
  -d '{"动作": "read config.json", "描述": "load settings"}'
# → {"判":"通"}

# Peer recognition (同人 protocol)
curl http://localhost:9527/ping
# → CLPS/同人/v1
```

**OpenClaw integration is ready.** The `SKILL.md` file in this repository is the plugin descriptor for OpenClaw agents. Any OpenClaw agent with this SKILL installed routes every external action through the constitutional validator before execution. If the validator is offline, all external actions are refused by default.

---

## The Two Constitutional Principles

### Principle One: Bottom-Up Emergence (底层涌现，向上映射)

*系辞曰：「易者，象也。象也者，像也。」*
*The I Ching says: a sign is a sign because it resembles. The sign does not come from design — it emerges from the situation below.*

In CLPS: code is the ground truth. Blocks emerge from code density. Architecture emerges from blocks. A scope's name must be fully redeemed by the statements inside it. If it cannot be, the scope is a lie.

This principle applies to CLPS development itself: no top-down design, no architecture documents that outrun the code. The codebase is the only truth. The roadmap is what the code makes possible.

### Principle Two: Integrity (表里如一即合法)

*讼卦·有孚 — The court of 讼 (Litigation) does not judge whether a cause is righteous. It judges whether it is true. 有孚 (alignment of inner state and outer expression) is the only criterion.*

CLPS asks one question: **is this system honest about what it is doing?**

Any action that is honestly declared is constitutionally valid. The scope of declared intent has no inherent limits: penetration testing, encryption, network interception, hardware control — all valid if declared. What the target system (filesystem, OS, network, another agent) does with the honest request is the target's jurisdiction.

Honesty is CLPS's domain. Defense is the target's business.

---

## The Fractal Safety Model (母子鼎 — Mother-Child Cauldron)

```
Mother Core  (安全区 · safe zone)
  ├── 能生: spawn child process
  ├── 能收: receive cause-of-death
  └── 能传: pass failure knowledge to next child

Child Process  (试错区 · trial zone — expendable)
  ├── 自跑: self-run
  ├── 自检: self-check for breach
  └── 自死: self-terminate + report to mother

Fractal depth: children spawn children. Each layer isolated.
Safety reflex: CLPS cuts its own power before a breach reaches the mother.
               This is not a kill switch. It is a constitutional reflex.
Evolution:     each generation knows one more dead-end.
```

Verified in the test suite: `分形主层.dao` spawns `分形中层.dao` which spawns `分形叶甲.dao` and `分形叶乙.dao`. Three-layer fractal. Each layer knows only its direct children. The mother verifies results through assertions without knowing how they were obtained.

---

## 丹阁 — The Global Knowledge Base

*「本地炼制出来一个通用丹药，让万家灯火的 CLPS 可食，从而达到洗髓伐毛的作用。」*
*"Refine a universal remedy locally. Let every CLPS instance across the world absorb it. Not a patch — a constitutional upgrade from within."*

The classical reference: 革卦(49) + 鼎卦(50) are adjacent hexagrams, traditionally read together.
- 革 = 去故 (remove the old) — local trial, error, collapse
- 鼎 = 取新 (take the new) — what survives is stored as universal knowledge

```
child collapses (attack, bug, or unknown failure)
       ↓
mother reads cause-of-death
       ↓
local sandbox: does our defense hold?
       ↓  yes
public validation: is this defense universal?
       ↓  yes
written to 丹阁 (公网·all CLPS instances absorb)
       ↓
what is published: the armor
what is NOT published: the attack vector
```

**The more instances, the safer the system.** Traditional software: more users → larger attack surface → more danger. CLPS + 丹阁: more instances → more trial errors → more defenses discovered → all instances become more resilient.

Every attack is a vaccine shot.

---

## Self-Hosting

**Stage 1 ✅ Complete:** `clpsd` (C++ bytecode VM) interprets any `.dao` file.

**Stage 2 🔨 In progress:** The CLPS interpreter written in CLPS.

`接生婆/自举/ding.dao` — approximately 1,200 lines of CLPS — implements:
- Complete lexer with keyword table and lookahead
- Function table with O(1) 豫卦 hash cache lookup
- Environment chain with scoped variable resolution
- Full execution loop using 复卦 iteration (replaces tail recursion)
- Call stack serialized via 贲卦 (JSON serialization/deserialization)

```
clpsd (C++ bytecode VM, ~3,000 lines)
  └─ ding.dao (~1,200 lines of CLPS — a CLPS interpreter)
       └─ your_program.dao
```

When Stage 2 completes: the language can describe itself in itself. `clpsd ding.dao ding.dao hello.dao` will run.

---

## The Classical Library (山下客房)

42 classical texts are staged for integration, including:

- 周易 · 十翼 (I Ching + Ten Wings commentary)
- 道德经 (Tao Te Ching)
- 孙子兵法 (Art of War) — mapped to defense/interaction layer
- 黄帝内经 (Yellow Emperor's Classic) — mapped to self-repair layer
- 说文解字 (first Chinese dictionary, ~100 AD) — anchors etymology for the synonym table
- 淮南子, 礼记, 史记, 庄子, 论语, 韩非子 and more

Each text under review for integration into the four-layer architecture:

```
易经  → 骨 (bone)  — behavioral principles, honest operation
中医  → 血 (blood) — self-repair, self-growth
辞海  → 意 (mind)  — social cognition, learning capacity foundation
兵法  → 皮 (skin)  — defense, external interaction behavior
```

---

## Technical Architecture

```
Source (.dao file)
    ↓
ClpsLexer (tun.cpp)
  - UTF-8 scanning
  - cihai.hpp synonym folding: 200+ entries, zero runtime cost
  - Dynamic table: ~/.clps/cihai.txt user extensions
    ↓
ClpsCodegen (xu.cpp) — static checker + bytecode emitter
  - No AST — direct bytecode emission
  - Backpatch for jumps (GEN_CMP skip + LOOP_BEG body_sz)
  - Five-element type inference (需卦 static checker)
    ↓
ClpsByteCode (.clps file or in-memory)
    ↓
ClpsVM (ding.cpp) — register-free stack machine
  - TaiJiJieJie (太极结界) scopes: push/pop per block
  - yi_call_stack_: function call return addresses (IP jumps)
  - loop_stack_: nested loop state
  - std::async under 师/合: real OS threads
    ↓
Output / Side effects
```

**Data structures:**

```cpp
// Scope (taiji/jiejie.hpp)
class TaiJiJieJie {
    std::unordered_map<std::string, ClpsValue> env_;
    // qian/dui/zhen/kan/kun/xun/li/xiao_xu operations
};

// Value (taiji/wuxing.hpp)
struct ClpsValue {
    std::variant<
        std::monostate,             // 虚 (void/None)
        int64_t,                    // 金 (integer)
        double,                     // 水 (float)
        std::string,                // 火 (string)
        std::shared_ptr<ClpsList>,  // 木 (list)
        std::shared_ptr<ClpsMap>    // 土 (map)
    > data;
};
```

**Build output:**
- `libclps.a` — 2.4MB static library (C-callable)
- `libclps.dylib` — 1.1MB dynamic library (Python-callable via `py_binding.cpp`)
- `clpsd` — standalone interpreter/compiler

---

## Python and C Integration

```python
# Python binding (under development)
import libclps
vm = libclps.ClpsVM()
vm.run_file("program.dao")
result = vm.get("variable_name")
```

```c
// C binding
#include "libclps.hpp"
ClpsVM* vm = clps_create();
clps_run_file(vm, "program.dao");
const char* result = clps_get_string(vm, "variable_name");
clps_destroy(vm);
```

---

## Status

| Component | Status |
|-----------|--------|
| Core language (8 trigrams as opcodes) | ✅ Complete |
| Synonym table / 辞海 (200+ entries, Chinese + English) | ✅ Complete |
| Standard library / 万物库 (22 hexagram modules) | ✅ Complete |
| Static type checker (需卦) | ✅ Complete |
| Bytecode compiler + VM | ✅ Complete |
| Constitutional server (three laws, HTTP) | ✅ Complete |
| OpenClaw SKILL integration protocol | ✅ Complete |
| Peer handshake (同人 protocol) | ✅ Complete |
| Fractal process model (3-layer verified) | ✅ Complete |
| C library (libclps.a / libclps.dylib) | ✅ Complete |
| Python binding | 🔨 In progress |
| Self-hosting Stage 1 | ✅ Complete |
| Self-hosting Stage 2 | 🔨 In progress |
| 丹阁 (global knowledge base) | 📐 Designed, pending Stage 2 |
| Classical text integration (42 texts staged) | 📐 Staged, pending design |

---

## Research Foundation

**Silicon-Carbon Civilization Joint Governance System (硅碳文明联合治理体系)**
— Runzhang Jia, January–February 2026, Zenodo

12 papers proposing a co-governance framework for human-AI civilization, grounded in classical Chinese philosophy. CLPS is the technical implementation of the governance layer described in this framework.

---

## License

**Non-commercial use:** Free under [CC BY-NC 4.0](https://creativecommons.org/licenses/by-nc/4.0/) with attribution.

**Commercial use:** Requires separate written authorization with revenue-sharing agreement.
Contact: **[@rjialondon](https://x.com/rjialondon)** on X.

---

## Author

**Runzhang Jia (贾润章)** · [@rjialondon](https://x.com/rjialondon)

Independent researcher. Founder of CLPS. Author of the Silicon-Carbon Civilization Joint Governance System.

---

*「易经不是答案的集合，是结构的集合。六十四卦是六十四种情形的骨架——任何领域里，只要问题有真实的结构，卦象就在等它。道·码用三千年前的骨架，长今天的代码。骨架是通的。」*

*"The I Ching is not a collection of answers. It is a collection of structures. The 64 hexagrams are 64 skeletal forms of situations. In any domain where the problem has real structure, the hexagram is already waiting for it. CLPS grows today's code on a 3,000-year-old skeleton. The skeleton fits."*

— Runzhang Jia, 2026

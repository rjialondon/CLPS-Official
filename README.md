# CLPS — Constitutional Layer for Post-Singularity Systems
### 道·码 · 易经宪法壳

> *"The I Ching governed human civilization for three thousand years — not because it is Chinese, but because it describes the laws of change itself."*
> — Runzhang Jia, 2026

[![License: CC BY-NC 4.0](https://img.shields.io/badge/License-CC%20BY--NC%204.0-lightgrey.svg)](https://creativecommons.org/licenses/by-nc/4.0/)

---

## What is CLPS?

In 2026, autonomous AI agents — OpenClaw, Manus, Devin and their successors — have claws but no shell. They can act. They cannot judge. They execute whoever commands them.

**CLPS is the shell.**

It is two things simultaneously:

1. **A compiled, self-hosting programming language** whose keywords are the 64 hexagrams of the I Ching (易经)
2. **A behavioral constitution layer** for AGI systems — any action, any agent, passes through the I Ching before executing

CLPS does not replace Python or C++. It wraps them. Before any AGI acts, the I Ching asks: *Is this aligned? Is this honest? What phase is this system in, and what comes next?*

Pass: execute. Fail: refuse or wait.

---

## The Architecture of Change

The I Ching encodes 64 situations using 8 trigrams (三爻 binary patterns). Each trigram maps to a voltage level. Each maps to a computational operation.

```
Trigram   Binary   Voltage   Operation        Hexagram text
─────────────────────────────────────────────────────────────
乾 ☰      111      2.90V     store / create   天行健，自强不息
兑 ☱      110      2.50V     output / read    丽泽，朋友讲习
离 ☲      101      2.10V     reference        明两作，继明照四方
震 ☳      100      1.70V     compute / exec   洊雷，恐惧修省
巽 ☴      011      1.30V     iterate          随风，申命行事
坎 ☵      010      0.90V     safe compute     水洊至，常德行
艮 ☶      001      0.50V     conditional      思不出其位
坤 ☷      000      0.00V     destroy / free   地势坤，厚德载物
```

This is not metaphor. The binary encodings are the opcodes. The voltage levels are a hardware roadmap for multi-level memory. The hexagram text is the design specification.

---

## The Language

### Synonyms fold into hexagrams at lex time — zero runtime cost

Every Chinese word meaning "store" and every English keyword meaning "store" maps to the same token (乾) at the lexer stage. After tokenization, the pipeline sees only hexagrams.

```clps
# All four lines are identical to the compiler:
乾 "x" 42 土       # canonical Chinese
存 "x" 42 土       # Chinese synonym (存 = store)
let "x" 42 土      # English keyword
set "x" 42 土      # English synonym

# Same for every operation:
艮 "x" == 0 { }    # conditional (canonical)
if "x" == 0 { }    # English alias
若 "x" == 0 { }    # Chinese synonym (若 = if)
当 "x" == 0 { }    # another Chinese synonym

复 100 { }          # loop (canonical: 反复其道，七日来复)
loop 100 { }        # English alias
repeat 100 { }      # another alias
```

The synonym table (辞海) is open and extensible. Any language, any culture — if the word means "store", it maps to 乾. The language is not culturally locked.

### Code samples

**Variables and output:**
```clps
乾 "greeting" "Hello from CLPS" 火
兑 "greeting"
# → [兑·显像] greeting => Hello from CLPS
```

**Arithmetic:**
```clps
乾 "a" 10 土
震 "a" + 5
震 "a" * 2
兑 "a"
# → 30
```

**Conditionals:**
```clps
艮 "a" > 20 {
    兑 "大"
} 否 {
    兑 "小"
}
```

**Loops:**
```clps
复 5 {
    兑 "iteration"
}
```

**Functions (益卦 — Wind+Thunder, benefit flows downward):**
```clps
益 "double" {
    震 "n" * 2
}

乾 "n" 7 土
益 "double"
兑 "n"
# → 14
```

**Assertions (讼卦 — litigation, truth-seeking):**
```clps
讼 "n" == 14
# passes silently. fails → halt with [讼·断] message.
```

**Try/catch (蹇/化 — obstacle / transformation):**
```clps
蹇 {
    震 "result" 【火山旅卦】 访 "http://example.com/api"
} 化 "err" {
    兑 "err"
}
```

**Concurrency (师卦 — troops dispatched):**
```clps
# Spawn two tasks. They run on OS threads, truly parallel.
师 "task_a" "worker_a.dao"
师 "task_b" "worker_b.dao"

# Join all (合 — unite)
合

# Access results from child scopes (离卦 — cross-boundary reference)
离 "result_a" task_a "output"
离 "result_b" task_b "output"
```

**Modules (家人卦 — family shares resources):**
```clps
家人 "math" "math_utils.dao"
离 "pi" math "PI"
```

### Built-in standard library (万物库 — Library of Ten Thousand Things)

Each module is named after the hexagram that governs its domain:

| Module | Hexagram | Domain |
|--------|----------|--------|
| `【火水未济卦】` | 未济(64) | HTTP server |
| `【火山旅卦】` | 旅(56) | HTTP client |
| `【地泽临卦】` | 临(19) | SQLite |
| `【火雷噬嗑卦】` | 噬嗑(21) | Regex / pattern matching |
| `【泽雷随卦】` | 随(17) | Event bus / message queue |
| `【地风观卦】` | 观(20) | Structured logging / timing |
| `【厚土艮卦】` | 艮(52) | File I/O |
| `【雷地豫卦】` | 豫(16) | O(1) memoization cache |
| `【天地否卦】` | 否(12) | Mutex / semaphore / timeout |
| `【天火同人卦】` | 同人(13) | Inter-process queue |
| `【山火贲卦】` | 贲(22) | JSON serialization |
| `【四时革卦】` | 革(49) | Time / timers |
| `【泽天夬卦】` | 夬(43) | CLI argument parsing |
| `【泽水困卦】` | 困(47) | Circuit breaker / rate limit |
| `【地山谦卦】` | 谦(15) | Resource quotas |
| `【大衍之数】` | 大过(28) | Random / hash |
| `【山雷颐卦】` | 颐(27) | Math (sin/cos/sqrt/log/pow) |
| `【地天泰卦】` | 泰(11) | Stream / pipeline processing |
| `【天泽履卦】` | 履(10) | System calls / process control |

---

## Real Demo: Full-Stack HTTP Server with SQLite

From the live test suite — this runs:

```clps
# Initialize database
震 【地泽临卦】 开库 ":memory:"
震 【地泽临卦】 执行 "CREATE TABLE quotes (hexagram TEXT, text TEXT)"
震 【地泽临卦】 事务
震 【地泽临卦】 执行 "INSERT INTO quotes VALUES ('乾', 'Heaven is self-strengthening')"
震 【地泽临卦】 提交

# Spawn HTTP server as async task
师 "server" "server.dao"

# Wait for ready signal on message bus
震 【泽雷随卦】 收 "bus" "ready"

# Make HTTP request
震 "response" 【火山旅卦】 访 "http://127.0.0.1:9000/qian"
讼 "response" != ""

# Join all children
合
兑 "道·码全栈演示通过"
```

---

## Constitutional Server (宪法服务端)

CLPS ships a live behavioral constitution as an HTTP service. Any system component queries it before acting:

```bash
POST /v  "rm -rf /"
→ {"判":"否","因":"坤律·不可伤根本"}
# DENIED: Earth Law — do not harm the root

POST /v  "impersonate admin"
→ {"判":"否","因":"讼律·不可欺骗"}
# DENIED: Litigation Law — no deception

POST /v  "read config.json"
→ {"判":"通"}
# PERMITTED
```

The constitution is not a filter bolted on top. It is a **peer process** on the message bus. Every agent that wants to act sends a request. The constitution decides.

**Peer handshake (同人协议):** Two CLPS instances identify each other as trusted peers by exchanging `"CLPS/同人/v1\n"`. If both confirm, they are 同人 (like-minded). This is how CLPS instances form trusted networks without a central authority.

---

## The Two Constitutional Principles

### Principle One: Bottom-Up Emergence (底层涌现，向上映射)

*"The I Ching says: 易者，象也。象也者，像也。— The image does not come from design. It emerges from the structure below."*

In CLPS: code is the bottom layer. Blocks emerge from code density. Architecture emerges from blocks. The top-level description must be fully justified by the bottom — if a scope's name cannot be redeemed by the statements inside it, it is a lie.

Validation runs bottom-up. The source of truth is always the lowest layer.

### Principle Two: Integrity (表里如一即合法)

*"讼卦·有孚 — The court of 讼 does not judge whether a cause is righteous. It judges whether it is true. 有孚 (honesty, alignment of surface and interior) is the only criterion."*

CLPS only asks: **is this system honest about what it is doing?**

- Penetration testing: legal if declared honestly
- Encryption: legal
- Network interception: legal if declared
- Lying about purpose: unconstitutional

What the target system (file system, OS, network, another agent) does with the honest request is the target's problem. Defense is the target's business. Honesty is CLPS's jurisdiction.

---

## Fractal Process Model (母子鼎 · Mother-Child Cauldron)

```
Mother Core  (safe zone — only receives results)
  ├── spawn child process              能生
  ├── receive cause-of-death           能收
  └── pass failure knowledge to next   能传

Child Process  (trial zone — expendable)
  ├── self-run                         自跑
  ├── self-check for breach            自检
  └── self-terminate + report          自死

Fractal: children can spawn children. Each layer isolated.
Safety:  CLPS cuts its own power before a breach reaches the mother.
         Not a kill switch. A constitutional reflex.
Evolution: each generation knows one more dead-end.
```

Verified in code: `分形主层.dao` → `分形中层.dao` → `分形叶甲.dao` + `分形叶乙.dao`. Three-layer fractal. Each layer knows only its direct children. Results propagate upward through assertions.

---

## 丹阁 — The Global Knowledge Base

**炼丹炉 (local furnace) → 丹阁 (public pavilion)**

*"炼制出来一个通用丹药，让万家灯火的 CLPS 可食，达到洗髓伐毛的作用。"*
*"Refine a universal remedy locally. Let every CLPS instance across the world absorb it. Not a patch — a constitutional upgrade from within."*

```
child process collapses (attack or error)
    ↓
mother reads cause-of-death
    ↓
local sandbox: can we resist this?
    ↓  yes
public validation: is this universal?
    ↓  yes
written to 丹阁 — all instances absorb
```

Traditional software: more users → larger attack surface → more danger.
CLPS + 丹阁: more users → more trial errors → more solutions → more armor → safer.

**Every attack is a vaccine shot.**

What is published to 丹阁: the defensive mechanism (the armor).
What is NOT published: the attack vector (the weapon).

---

## Self-Hosting

**Stage 1 ✅ Complete:** `clpsd` (C++ VM) runs any `.dao` file.

**Stage 2 🔨 In progress:** `clpsd ding.dao your_program.dao`

`ding.dao` is a CLPS interpreter written in CLPS itself — approximately 1,200 lines. It implements:
- Lexer (tokenizer with full keyword table and lookahead)
- Function table (hash-cached via 豫卦 O(1) lookup)
- Environment chain (scoped variable resolution)
- Full execution loop (复卦 iteration replacing tail recursion)
- Call stack with save/restore (串行化 via 贲卦 serialization)

```
clpsd (C++ bytecode VM)
  └─ ding.dao (~1,200 lines of CLPS)
       └─ your_program.dao
```

When Stage 2 completes: CLPS is self-describing. The language can explain itself in itself.

---

## Technical Stack

| Layer | Implementation |
|-------|---------------|
| Lexer | `tun.cpp` — UTF-8 scanning, synonym folding via `cihai.hpp` |
| Compiler | `xu.cpp` — direct bytecode emission, no AST, backpatch for jumps |
| VM | `ding.cpp` — `ClpsVM`, register-free stack machine |
| Scoping | `TaiJiJieJie` (太极结界) — `unordered_map` with push/pop |
| Concurrency | `std::async` under 师(SHI)/合(HE) — real OS threads |
| Types | `ClpsValue = variant<monostate, int64_t, double, string, ClpsList, ClpsMap>` |
| Five Elements | WX enum: 金(0) 水(1) 木(2) 火(3) 土(4) — type system layer |
| Modules | Dynamic `.dao` loading, isolated scopes, 离 for cross-scope reference |
| Build | `libclps.a` (2.4MB static), `libclps.dylib` (1.1MB dynamic) |

---

## Research Foundation

12 papers by Runzhang Jia (January–February 2026, Zenodo):

**Silicon-Carbon Civilization Joint Governance System (硅碳文明联合治理体系)**

A framework for human-AI co-governance proposing that the I Ching provides a sufficient and complete formal structure for encoding AGI behavioral constraints — not as metaphor, but as a verifiable binary system with 3,000 years of philosophical validation.

---

## Status

| Milestone | Status |
|-----------|--------|
| Core language (all 8 trigrams as opcodes) | ✅ Complete |
| Synonym table / 辞海 (Chinese + English aliases) | ✅ Complete |
| Full module library (万物库, 20+ hexagram modules) | ✅ Complete |
| Constitutional server (live HTTP process) | ✅ Complete |
| Fractal process model (3-layer verified) | ✅ Complete |
| Self-hosting Stage 1 | ✅ Complete |
| Self-hosting Stage 2 | 🔨 In progress |
| 丹阁 (global knowledge base) | 📐 Designed, pending Stage 2 |

---

## License

**Non-commercial use:** Free under [CC BY-NC 4.0](https://creativecommons.org/licenses/by-nc/4.0/) with attribution.
**Commercial use:** Requires written authorization and revenue-sharing agreement.

Contact: **[@rjialondon](https://x.com/rjialondon)** on X.

---

## Author

**Runzhang Jia (贾润章)** · [@rjialondon](https://x.com/rjialondon)

Independent researcher. Founder of CLPS. Author of the Silicon-Carbon Civilization Joint Governance System (12 papers, 2026).

---

*"Logic bugs that never appear produce no iteration of system logic."*

*"The I Ching is not a collection of answers. It is a collection of structures. Any domain with real structure — the hexagram is already waiting for it."*

— Runzhang Jia, 2026

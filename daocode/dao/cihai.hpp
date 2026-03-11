#pragma once
/*
 * cihai.hpp — 辞海·近义词折叠表
 *
 * 词法层折叠：中文近义词 + 英文词 → 同一 TK 枚举
 * 零运行时开销：折叠仅发生在 tokenize() 阶段，后续流水线无感。
 * 歧义词由语境消歧（xu.cpp）处理，此表只收录单义词。
 *
 * 定案：只收中文和英文。
 *
 * 双层结构：
 *   静态层 — cihai_table()：编译时固化，零开销
 *   动态层 — cihai_dynamic_table()：运行时加载，支持用户自定义
 * 加载路径：~/.clps/cihai.txt（启动时自动读取）
 * 文件格式：每行 "族名 词"，# 为注释
 *           族名：乾 兑 震 坎 离 巽 艮 坤 复 讼 恒 蹇 化 益 变
 *                 结界 家人 师 合 小畜 金 水 木 火 土
 */

#include "yao.hpp"
#include <unordered_map>
#include <string>
#include <fstream>
#include <sstream>
#include <mutex>
#include <cstdlib>

// ── 辞海表（静态，首次调用时初始化）────────────────────────────────────
inline const std::unordered_map<std::string, TK>& cihai_table() {
    static const std::unordered_map<std::string, TK> table = {

        // ─────────────────────────────────────────────────────────────────
        // 乾族 (QIAN) — 存入 / 创建
        // 乾=111=2.90V：天行健，君子以自强不息。最高电压，最强存储。
        // ─────────────────────────────────────────────────────────────────
        { "存",     TK::QIAN }, { "载",    TK::QIAN },
        { "置",     TK::QIAN }, { "设",    TK::QIAN },
        { "建",     TK::QIAN },
        { "let",    TK::QIAN }, { "var",   TK::QIAN },
        { "val",    TK::QIAN }, { "set",   TK::QIAN },
        { "store",  TK::QIAN }, { "put",   TK::QIAN },
        { "def",    TK::QIAN }, { "define",TK::QIAN },

        // ─────────────────────────────────────────────────────────────────
        // 震族 (ZHEN) — 运算 / 执行
        // 震=100=1.70V：洊雷，君子以恐惧修省。
        // ─────────────────────────────────────────────────────────────────
        { "算",     TK::ZHEN }, { "运",    TK::ZHEN },
        { "calc",   TK::ZHEN }, { "compute",TK::ZHEN },
        { "exec",   TK::ZHEN }, { "run",   TK::ZHEN },

        // ─────────────────────────────────────────────────────────────────
        // 坎族 (KAN) — 安全运算（克制则静默）
        // 坎=010=0.90V：水洊至，君子以常德行习教事。
        // ─────────────────────────────────────────────────────────────────
        { "试",     TK::KAN  }, { "测",    TK::KAN  },
        { "safe",   TK::KAN  }, { "try_op",TK::KAN  },

        // ─────────────────────────────────────────────────────────────────
        // 兑族 (DUI) — 读取 / 输出
        // 兑=110=2.50V：丽泽，君子以朋友讲习。
        // ─────────────────────────────────────────────────────────────────
        { "读",     TK::DUI  }, { "取",    TK::DUI  },
        { "出",     TK::DUI  }, { "显",    TK::DUI  },
        { "印",     TK::DUI  },
        { "get",    TK::DUI  }, { "read",  TK::DUI  },
        { "print",  TK::DUI  }, { "show",  TK::DUI  },
        { "out",    TK::DUI  }, { "display",TK::DUI },
        { "fetch",  TK::DUI  },

        // ─────────────────────────────────────────────────────────────────
        // 坤族 (KUN) — 归元 / 销毁
        // 坤=000=0.00V：地势坤，君子以厚德载物。归零承载。
        // ─────────────────────────────────────────────────────────────────
        { "删",     TK::KUN  }, { "清",    TK::KUN  },
        { "消",     TK::KUN  }, { "终",    TK::KUN  },
        { "del",    TK::KUN  }, { "delete",TK::KUN  },
        { "drop",   TK::KUN  }, { "clear", TK::KUN  },
        { "destroy",TK::KUN  }, { "done",  TK::KUN  },

        // ─────────────────────────────────────────────────────────────────
        // 离族 (LI) — 跨界引用
        // 离=101=2.10V：明两作，君子以继明照四方。
        // ─────────────────────────────────────────────────────────────────
        { "引",     TK::LI   }, { "借",    TK::LI   },
        { "链",     TK::LI   },
        { "ref",    TK::LI   }, { "link",  TK::LI   },
        { "from",   TK::LI   }, { "bind",  TK::LI   },

        // ─────────────────────────────────────────────────────────────────
        // 巽族 (XUN) — 遍历
        // 巽=011=1.30V：随风，君子以申命行事。
        // ─────────────────────────────────────────────────────────────────
        { "遍",     TK::XUN  }, { "游",    TK::XUN  },
        { "历",     TK::XUN  },
        { "for",    TK::XUN  }, { "each",  TK::XUN  },
        { "scan",   TK::XUN  }, { "iter",  TK::XUN  },

        // ─────────────────────────────────────────────────────────────────
        // 艮族 (GEN) — 条件门控
        // 艮=001=0.50V：君子以思不出其位。止而不越。
        // ─────────────────────────────────────────────────────────────────
        { "若",     TK::GEN  }, { "如",    TK::GEN  },
        { "当",     TK::GEN  },
        { "if",     TK::GEN  }, { "when",  TK::GEN  },
        { "cond",   TK::GEN  }, { "check", TK::GEN  },

        // ─────────────────────────────────────────────────────────────────
        // 复族 (FU) — 循环
        // 复卦：反复其道。
        // ─────────────────────────────────────────────────────────────────
        { "再",     TK::FU   }, { "重",    TK::FU   },
        { "循",     TK::FU   },
        { "repeat", TK::FU   }, { "loop",  TK::FU   },
        { "times",  TK::FU   },

        // ─────────────────────────────────────────────────────────────────
        // 讼族 (SONG) — 断言
        // 讼卦·天与水违行：有孚窒惕，中吉，终凶。
        // ─────────────────────────────────────────────────────────────────
        { "断",     TK::SONG }, { "验",    TK::SONG },
        { "证",     TK::SONG },
        { "assert", TK::SONG }, { "ensure",TK::SONG },
        { "verify", TK::SONG }, { "must",  TK::SONG },

        // ─────────────────────────────────────────────────────────────────
        // 恒族 (HENG) — 只读常量（恒卦·雷风恒·32）
        // ─────────────────────────────────────────────────────────────────
        { "固",     TK::HENG }, { "常",    TK::HENG },
        { "const",  TK::HENG }, { "final", TK::HENG },
        { "readonly",TK::HENG},

        // ─────────────────────────────────────────────────────────────────
        // 蹇族 (JIAN) — 错误恢复 try 块（蹇卦·水山蹇·39）
        // ─────────────────────────────────────────────────────────────────
        { "险",     TK::JIAN }, { "难",    TK::JIAN },
        { "try",    TK::JIAN }, { "attempt",TK::JIAN },

        // ─────────────────────────────────────────────────────────────────
        // 化族 (HUA) — 错误处理 catch 块
        // ─────────────────────────────────────────────────────────────────
        { "解",     TK::HUA  }, { "救",    TK::HUA  },
        { "catch",  TK::HUA  }, { "handle",TK::HUA  },
        { "rescue", TK::HUA  },

        // ─────────────────────────────────────────────────────────────────
        // 益族 (YI) — 用户自定义子程序（益卦·风雷益·42）
        // 卦义：损上益下，利有攸往。把功能封装成可复用的"益"传递给调用者。
        // ─────────────────────────────────────────────────────────────────
        { "程",     TK::YI   }, { "式",    TK::YI   },
        { "func",   TK::YI   }, { "fn",    TK::YI   },
        { "proc",   TK::YI   }, { "sub",   TK::YI   },
        { "call",   TK::YI   }, { "invoke",TK::YI   },

        // ─────────────────────────────────────────────────────────────────
        // 变族 (BIAN) — 爻变 / 类型转换
        // ─────────────────────────────────────────────────────────────────
        { "改",     TK::BIAN }, { "换",    TK::BIAN },
        { "转",     TK::BIAN },
        { "change", TK::BIAN }, { "cast",  TK::BIAN },
        { "mutate", TK::BIAN }, { "to",    TK::BIAN },

        // ─────────────────────────────────────────────────────────────────
        // 结界族 (JIEJIE) — 作用域
        // ─────────────────────────────────────────────────────────────────
        { "域",     TK::JIEJIE }, { "界",    TK::JIEJIE },
        { "scope",  TK::JIEJIE }, { "block", TK::JIEJIE },
        { "zone",   TK::JIEJIE }, { "ns",    TK::JIEJIE },

        // ─────────────────────────────────────────────────────────────────
        // 家人族 (JIAREN) — 模块加载（家人卦·风自火出·37）
        // ─────────────────────────────────────────────────────────────────
        { "import",  TK::JIAREN }, { "load",   TK::JIAREN },
        { "include", TK::JIAREN }, { "require",TK::JIAREN },

        // ─────────────────────────────────────────────────────────────────
        // 师族 (SHI) — 异步分兵（师卦·地中有水·7）
        // ─────────────────────────────────────────────────────────────────
        { "async",  TK::SHI  }, { "spawn", TK::SHI  },
        { "go",     TK::SHI  },

        // ─────────────────────────────────────────────────────────────────
        // 合族 (HE) — 等待合师
        // ─────────────────────────────────────────────────────────────────
        { "await",  TK::HE   }, { "join",  TK::HE   },
        { "wait",   TK::HE   }, { "sync",  TK::HE   },

        // ─────────────────────────────────────────────────────────────────
        // 小畜族 (XIAO_XU) — 列表积累（小畜卦·风天小畜·9）
        // ─────────────────────────────────────────────────────────────────
        { "push",    TK::XIAO_XU }, { "append", TK::XIAO_XU },
        { "collect", TK::XIAO_XU }, { "add",    TK::XIAO_XU },

        // ─────────────────────────────────────────────────────────────────
        // 五行族 — 类型注解（英文程序员友好）
        // ─────────────────────────────────────────────────────────────────
        // 金 (JIN) — 整数：精确刚强
        { "int",     TK::JIN  }, { "integer",TK::JIN  },
        { "num",     TK::JIN  }, { "number", TK::JIN  },

        // 水 (SHUI) — 浮点：流动连续近似
        { "float",   TK::SHUI }, { "double", TK::SHUI },
        { "decimal", TK::SHUI },

        // 火 (HUO) — 字符串：光热传播
        { "str",     TK::HUO  }, { "string", TK::HUO  },
        { "text",    TK::HUO  }, { "char",   TK::HUO  },

        // 木 (MU) — 列表：生发成长
        { "list",    TK::MU   }, { "array",  TK::MU   },
        { "seq",     TK::MU   }, { "vec",    TK::MU   },

        // 土 (TU) — 字典 / None：厚德承载
        { "dict",    TK::TU   }, { "map",    TK::TU   },
        { "none",    TK::TU   }, { "null",   TK::TU   },
        { "nil",     TK::TU   }, { "obj",    TK::TU   },
        { "object",  TK::TU   },
    };
    return table;
}

// ── 族名 → TK（用于文件加载 / 辞海·添）────────────────────────────────
inline const std::unordered_map<std::string, TK>& cihai_clan_map() {
    static const std::unordered_map<std::string, TK> m = {
        {"乾",TK::QIAN},  {"兑",TK::DUI},   {"震",TK::ZHEN},  {"坎",TK::KAN},
        {"离",TK::LI},    {"巽",TK::XUN},   {"艮",TK::GEN},   {"坤",TK::KUN},
        {"复",TK::FU},    {"讼",TK::SONG},  {"恒",TK::HENG},  {"蹇",TK::JIAN},
        {"化",TK::HUA},   {"益",TK::YI},    {"变",TK::BIAN},
        {"结界",TK::JIEJIE}, {"家人",TK::JIAREN},
        {"师",TK::SHI},   {"合",TK::HE},    {"小畜",TK::XIAO_XU},
        {"金",TK::JIN},   {"水",TK::SHUI},  {"木",TK::MU},
        {"火",TK::HUO},   {"土",TK::TU},
    };
    return m;
}

// ── TK → 族名（用于辞海·查 输出）───────────────────────────────────────
inline std::string cihai_tk_to_clan(TK tk) {
    switch (tk) {
        case TK::QIAN:    return "乾";
        case TK::DUI:     return "兑";
        case TK::ZHEN:    return "震";
        case TK::KAN:     return "坎";
        case TK::LI:      return "离";
        case TK::XUN:     return "巽";
        case TK::GEN:     return "艮";
        case TK::KUN:     return "坤";
        case TK::FU:      return "复";
        case TK::SONG:    return "讼";
        case TK::HENG:    return "恒";
        case TK::JIAN:    return "蹇";
        case TK::HUA:     return "化";
        case TK::YI:      return "益";
        case TK::BIAN:    return "变";
        case TK::JIEJIE:  return "结界";
        case TK::JIAREN:  return "家人";
        case TK::SHI:     return "师";
        case TK::HE:      return "合";
        case TK::XIAO_XU: return "小畜";
        case TK::JIN:     return "金";
        case TK::SHUI:    return "水";
        case TK::MU:      return "木";
        case TK::HUO:     return "火";
        case TK::TU:      return "土";
        default:          return "未知";
    }
}

// ── 动态词表（运行时可写，艮律·守记）───────────────────────────────────
inline std::mutex& cihai_mutex() {
    static std::mutex m; return m;
}
inline std::unordered_map<std::string, TK>& cihai_dynamic_table() {
    static std::unordered_map<std::string, TK> table;
    return table;
}

// ── 默认文件路径：~/.clps/cihai.txt ────────────────────────────────────
inline std::string cihai_default_path() {
    const char* home = std::getenv("HOME");
    return std::string(home ? home : ".") + "/.clps/cihai.txt";
}

// ── 从文件加载到动态词表（启动时调用）──────────────────────────────────
inline void cihai_load_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) return;

    const auto& clan = cihai_clan_map();
    auto& dyn = cihai_dynamic_table();
    std::lock_guard<std::mutex> lk(cihai_mutex());

    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string clan_name, word;
        if (!(ss >> clan_name >> word)) continue;
        auto it = clan.find(clan_name);
        if (it != clan.end()) dyn[word] = it->second;
    }
}

// ── 添加一词到动态表并追加到文件（辞海·添 调用）────────────────────────
inline bool cihai_add_word(const std::string& clan_name, const std::string& word,
                           const std::string& path = "") {
    const auto& clan = cihai_clan_map();
    auto it = clan.find(clan_name);
    if (it == clan.end()) return false;

    {
        std::lock_guard<std::mutex> lk(cihai_mutex());
        cihai_dynamic_table()[word] = it->second;
    }

    // 追加到文件
    std::string fpath = path.empty() ? cihai_default_path() : path;
    // 确保目录存在
    {
        size_t sep = fpath.rfind('/');
        if (sep != std::string::npos) {
            std::string dir = fpath.substr(0, sep);
            // mkdir -p
            std::string cmd = "mkdir -p \"" + dir + "\"";
            std::system(cmd.c_str()); // NOLINT
        }
    }
    std::ofstream out(fpath, std::ios::app);
    if (out) out << clan_name << " " << word << "\n";
    return true;
}

// ── 近义词查找（静态 + 动态双层）────────────────────────────────────────
// 静态层优先，动态层兜底。返回 TK::END 表示未知。
inline TK cihai_lookup(const std::string& word) {
    // 静态层
    const auto& t = cihai_table();
    auto it = t.find(word);
    if (it != t.end()) return it->second;

    // 动态层（艮律·守记）
    std::lock_guard<std::mutex> lk(cihai_mutex());
    const auto& dyn = cihai_dynamic_table();
    auto dit = dyn.find(word);
    return (dit != dyn.end()) ? dit->second : TK::END;
}

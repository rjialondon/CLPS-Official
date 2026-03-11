/*
 * wanwuku.cpp — 万物库入口
 *
 * 仅负责统一调用各子模块的 init 函数。
 * 具体实现已拆分到 dao/modules/mod_*.cpp。
 *
 * 六十四卦分组：
 *   mod_wenzi   — 离(30)/丰(55)/噬嗑(21)      字符串/格式化/正则
 *   mod_shuxue  — 大衍(28)/颐(27)/渐(53)       随机/数学/序列
 *   mod_jijie   — 泰(11)/大有(14)/巽(57)       集合/管道/聚合
 *                 风雷益(42)/损(41)
 *   mod_wenjian — 艮(52)/井(48)/晋(35)/坤(2)   文件/IO/配置
 *   mod_geshi   — 贲(22)/既济(63)/中孚(61)     序列化/位运算/校验
 *                 鼎(50)/屯(3)                  格式转换/字节流
 *   mod_xitong  — 革(49)/履(10)/乾(1)/震(51)   时间/系统/子进程/定时器
 *   mod_wangluo — 比(8)/旅(56)/未济(64)/解(40)  TCP/HTTP
 *   mod_shujuku — 临(19)                        SQLite
 *   mod_bingfa  — 否(12)/同人(13)/谦(15)/困(47) 并发/限流/缓存/监控
 *                 豫(16)/随(17)/归妹(54)/节(60)
 *                 涣(59)/大壮(34)/观(20)
 *   mod_jiami   — 坎(29)/夬(43)                安全编码/CLI参数
 *   mod_wenzhi  — 山水蒙(4)                   问智·LLM接入（碳硅转译层）
 */

#include "wanwu.hpp"
#include "modules/mods.hpp"
#include "cihai.hpp"

// wan_wu_ku_set_args() 已移入 mod_jiami.cpp（与 g_clpsd_args 同住）

void wan_wu_ku_init() {
    // 屯卦·辞海动态词表：启动时加载用户自定义近义词
    cihai_load_file(cihai_default_path());
    init_mod_wenzi();
    init_mod_shuxue();
    init_mod_jijie();
    init_mod_wenjian();
    init_mod_geshi();
    init_mod_xitong();
    init_mod_wangluo();
    init_mod_shujuku();
    init_mod_bingfa();
    init_mod_jiami();
    init_mod_wenzhi();   // 山水蒙·问智（碳硅转译层）
}

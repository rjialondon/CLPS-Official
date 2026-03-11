#pragma once
// mods.hpp — 万物库分模块初始化函数声明

// wan_wu_ku_set_args() 实现在 mod_jiami.cpp（g_clpsd_args 也在那里）
// wan_wu_ku_init() 实现在 wanwuku.cpp

void init_mod_wenzi();    // 离/丰/噬嗑 — 字符串/格式化/正则
void init_mod_shuxue();   // 大衍/颐/渐 — 随机/数学/序列
void init_mod_jijie();    // 泰/大有/巽/风雷益/损 — 集合/管道/聚合
void init_mod_wenjian();  // 艮/井/晋/坤 — 文件/IO/配置
void init_mod_geshi();    // 贲/既济/中孚/鼎/屯 — 序列化/位运算/校验/格式转换
void init_mod_xitong();   // 革/履/乾/震 — 时间/系统/子进程/定时器
void init_mod_wangluo();  // 比/旅/未济/解 — TCP/HTTP
void init_mod_shujuku();  // 临 — SQLite
void init_mod_bingfa();   // 否/同人/谦/困/豫/随/归妹/节/涣/大壮/观 — 并发/限流/缓存/监控
void init_mod_jiami();    // 坎/夬 — 安全编码/CLI参数
void init_mod_wenzhi();   // 山水蒙 — 问智·LLM接入（碳硅转译层）

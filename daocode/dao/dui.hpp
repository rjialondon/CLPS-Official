#pragma once
/*
 * clps_repl.hpp — 道·码 交互式命令行（兑卦）
 *
 * 兑卦(58)象传：丽泽，兑；君子以朋友讲习。
 * 两泽相连，互相润泽。REPL 是语言的嘴——输入一行，立即回应。
 *
 * 设计：
 *   - 持久 ClpsParser 实例，root_jj 在整个会话内保持状态
 *   - 支持多行输入（{ 未闭合时继续读取，直到 } 配平）
 *   - 特殊命令：:q / :quit 退出，:clear 清空作用域，:wx 显示五行表
 */

#include "tun.hpp"
#include "xu.hpp"
#include "kun.hpp"
#include <string>

// 启动 REPL 主循环
void run_repl();

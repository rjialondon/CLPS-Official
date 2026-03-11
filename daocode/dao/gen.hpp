#pragma once
/*
 * gen.hpp — 艮卦·记忆内部桥接（☶☶）
 *
 * 艮，止也。时止则止，止于其所，其道光明。
 *
 * ClpsMemory 完整定义见 libclps.hpp。
 * 本文件仅供内部模块使用，用于注入记忆上下文。
 */

// 完整定义在 libclps.hpp
struct ClpsMemory;

// 执行前注入当前记忆实例（libclps.cpp 在 run_source() 前调用）
// nullptr = 无记忆模式（艮卦函数仍可调用，但读写报错）
void wan_wu_ku_set_memory(ClpsMemory* memory);

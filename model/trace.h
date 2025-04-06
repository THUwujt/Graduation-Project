#pragma once
#include "systemc.h"

namespace model
{
    // 声明全局追踪文件指针
    extern sc_core::sc_trace_file *g_trace;

    // 初始化追踪文件的函数（可选）
    void init_trace(const std::string &filename);
}
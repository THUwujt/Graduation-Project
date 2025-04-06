#include "trace.h"

namespace nsim
{
    // 定义全局追踪文件指针
    sc_core::sc_trace_file *g_trace = NULL;

    void init_trace(const std::string &filename)
    {
        if (!g_trace)
        {
            g_trace = sc_core::sc_create_vcd_trace_file(filename.c_str());
            if (!g_trace)
            {
                std::cerr << "Failed to create trace file!" << std::endl;
            }
        }
    }
}
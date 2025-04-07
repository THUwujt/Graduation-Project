// 接收2个bank的输入(data和valid)，然后选择其中valid的一个输出

#pragma once

#include <systemc.h>
#include <vector>
#include <iostream>
#include "trace.h"

using namespace std;

namespace model
{
    // 定义Bank模块
    // Bank模块模拟一个8x32x32的存储器
    // 输入命令：0-不操作，1-读取数据，2-换行，3-刷新
    // 地址格式：高3位为行地址，低5位为列地址
    class MUX : public sc_module
    {
    public:
        // 输入端口
        sc_in<bool> valid_in[2];        // 2个输入的valid信号
        sc_in<int> data_in[2][32];      // 2个输入的数据
        sc_in<int> target_column_in[2]; // 2个输入的列地址
        sc_out<int> data_out[32];       // 1个输出的数据
        sc_out<bool> valid_out;         // 1个输出的valid信号
        sc_out<int> target_column_out;  // 1个输出的列地址
        SC_HAS_PROCESS(MUX);
        MUX(sc_module_name name, int ID = 0, bool per2bank = true) : sc_module(name), id(ID), per2bank(per2bank)
        {
            SC_THREAD(process_mux);
        }
        void process_mux()
        {
            while (true)
            {
                // 选择有效的输入
                if (per2bank)
                {
                    if (valid_in[0].read())
                    {
                        for (int i = 0; i < 32; i++)
                        {
                            data_out[i].write(data_in[0][i].read());
                        }
                        valid_out.write(true); // 只要有一个valid信号为true，就输出valid信号
                        target_column_out.write(target_column_in[0].read());
                    }
                    else if (valid_in[1].read())
                    {
                        for (int i = 0; i < 32; i++)
                        {
                            data_out[i].write(data_in[1][i].read());
                        }
                        valid_out.write(true); // 只要有一个valid信号为true，就输出valid信号
                        target_column_out.write(target_column_in[1].read());
                    }
                    else
                    {
                        valid_out.write(false); // 如果没有有效输入，则输出无效信号
                    }
                    wait(tSYS_CK, SC_PS);
                }
                else
                {
                    // 2个都为有效的话第一个cycle取0的数据第二个cycle取1的数据
                    if (valid_in[0].read() && valid_in[1].read())
                    {
                        for (int i = 0; i < 32; i++)
                        {
                            data_out[i].write(data_in[0][i].read());
                        }
                        valid_out.write(true); // 只要有一个valid信号为true，就输出valid信号
                        target_column_out.write(target_column_in[0].read());
                        wait(tSYS_CK, SC_PS);
                        for (int i = 0; i < 32; i++)
                        {
                            data_out[i].write(data_in[1][i].read());
                        }
                        valid_out.write(true); // 只要有一个valid信号为true，就输出valid信号
                        target_column_out.write(target_column_in[1].read());
                        wait(tSYS_CK, SC_PS);
                    }
                    else if (valid_in[0].read())
                    {
                        for (int i = 0; i < 32; i++)
                        {
                            data_out[i].write(data_in[0][i].read());
                        }
                        valid_out.write(true); // 只要有一个valid信号为true，就输出valid信号
                        target_column_out.write(target_column_in[0].read());
                        wait(tSYS_CK, SC_PS);
                    }
                    else if (valid_in[1].read())
                    {
                        for (int i = 0; i < 32; i++)
                        {
                            data_out[i].write(data_in[1][i].read());
                        }
                        valid_out.write(true); // 只要有一个valid信号为true，就输出valid信号
                        target_column_out.write(target_column_in[1].read());
                        wait(tSYS_CK, SC_PS);
                    }
                    else
                    {
                        valid_out.write(false); // 如果没有有效输入，则输出无效信号
                        wait(tSYS_CK, SC_PS);
                    }
                }
            }
        }
        void trace()
        {
            if (g_trace) // 检查 g_trace 是否已初始化
            {
                // 使用id生成唯一的追踪信号名
                // 追踪 valid_out 信号
                string trace_name = "MUXed_bank_valid_out_" + to_string(id);
                sc_core::sc_trace(g_trace, valid_out, trace_name);
                // 追踪target_column_out
                trace_name = "MUXed_bank_target_column_out_" + to_string(id);
                sc_core::sc_trace(g_trace, target_column_out, trace_name);
                // trace target_column_in
                for (int i = 0; i < 2; i++)
                {
                    trace_name = "MUXed_bank_target_column_in_" + to_string(id) + "_" + to_string(i);
                    sc_core::sc_trace(g_trace, target_column_in[i], trace_name);
                }
            }
        }

    private:
        int id;
        bool per2bank;
    };
} // namespace model
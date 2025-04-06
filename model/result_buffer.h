#pragma once

#include <systemc.h>
#include <vector>
#include "trace.h"

using namespace sc_core;
using namespace std;

namespace model
{

    // 定义ResultBuffer模块
    class ResultBuffer : public sc_module
    {
    public:
        // 输入端口：32个int数据和32个valid信号
        sc_in<int> data_in[32];   // 32个int数据输入
        sc_in<bool> valid_in[32]; // 32个valid信号

        // 追踪所有的valid_in信号
        // sc_signal<bool> valid_in_signal[32];

        // 构造函数
        SC_CTOR(ResultBuffer)
        {
            // 初始化32个vector
            for (int i = 0; i < 32; i++)
            {
                result_buffers[i] = new vector<int>(); // 每个vector存储int数据
            }

            // 注册处理方法
            SC_THREAD(store_results);
        }

        // 析构函数
        ~ResultBuffer()
        {
            for (int i = 0; i < 32; i++)
            {
                delete result_buffers[i];
            }
        }
        // get the result in result buffers
        vector<int> get_result(int index)
        {
            if (index < 0 || index >= 32)
            {
                throw out_of_range("Index out of range");
            }
            return *result_buffers[index];
        }
        void trace()
        {
            if (g_trace)
            { // 检查 g_trace 是否已初始化
                for (int i = 0; i < 32; i++)
                {
                    sc_core::sc_trace(g_trace, valid_in[i], "result_buffer_valid_in_signal_" + to_string(i));
                }
            }
        }

    private:
        // 内部存储32个vector，每个vector存储int数据
        vector<int> *result_buffers[32];

        // 处理方法：根据valid信号将数据存储到对应的vector中
        void store_results()
        {
            while (true)
            {
                for (int i = 0; i < 32; i++)
                {
                    if (valid_in[i].read())
                    {
                        // 如果valid信号为true，则将数据存入对应的vector
                        result_buffers[i]->push_back(data_in[i].read());
                    }
                    // valid_in_signal[i].write(valid_in[i].read());
                }
                wait(tSYS_CK, SC_PS);
            }
        }
    };

} // namespace model

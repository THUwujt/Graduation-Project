#pragma once

#include <systemc.h>
#include <tlm>
#include <deque>
#include "trace.h"

using namespace sc_core;
using namespace tlm;
using namespace std;

namespace model
{
    // todo：输入输出端口忘了加valid了
    // 定义shift_reg模块
    class ShiftReg : public sc_module
    {
    public:
        // 输入端口：32个int数据
        sc_in<int> data_in[32]; // 输入数据
        sc_in<bool> valid_in;
        sc_out<int> data_out[32]; // 输出数据
        sc_out<bool> valid_out[32];

        // 追踪所有valid_out信号
        // sc_signal<bool> valid_out_signal[32]; // 追踪valid_out信号

        // 构造函数
        SC_CTOR(ShiftReg)
        {
            // 为每个FIFO实例化FIFO队列（长度分别为1到32）
            for (int i = 0; i < 32; i++)
            {
                // 使用FIFO长度i+1，FIFO的长度为1到31
                data_fifo[i] = new sc_fifo<int>(i + 1);   // ?不知道为什么这里会差2个周期，后续需要解释，当前是强行打的补丁
                valid_fifo[i] = new sc_fifo<bool>(i + 1); // ?也可以考虑给第一个加上1个shift reg这样就可以在没区别的前提下保持统一了
            }
            initialize();
            // 为每个输入数据绑定process
            SC_THREAD(shift_process);
        }
        void initialize()
        {
            // 所有fifo填满0，valid全部置零
            for (int i = 0; i < 32; i++)
            {
                for (int j = 0; j < i + 1; j++)
                {
                    data_fifo[i]->write(0);
                    valid_fifo[i]->write(false);
                }
            }
        }
        // 析构函数
        ~ShiftReg()
        {
            // 清理动态分配的FIFO
            for (int i = 0; i < 32; i++)
            {
                delete data_fifo[i];
                delete valid_fifo[i];
            }
        }
        void trace()
        {
            if (g_trace)
            { // 检查 g_trace 是否已初始化
                for (int i = 0; i < 32; i++)
                {
                    sc_core::sc_trace(g_trace, valid_out[i], "shift_reg_valid_out_signal_" + to_string(i));
                }
            }
        }

    private:
        // FIFO数组，每个FIFO长度不同
        sc_fifo<int> *data_fifo[32];
        sc_fifo<bool> *valid_fifo[32];

        // 处理过程：将输入数据移位到相应的FIFO，并将结果输出
        void shift_process()
        {
            while (true)
            {
                // 将输入数据依次写入对应的FIFO
                for (int i = 0; i < 32; i++)
                {
                    // 从FIFO中读取数据，并输出到对应的端口
                    data_out[i].write(data_fifo[i]->read());
                    bool tmp = valid_fifo[i]->read();
                    valid_out[i].write(tmp);
                    // valid_out_signal[i].write(tmp);
                    // 将每个输入数据写入对应长度的FIFO
                    data_fifo[i]->write(data_in[i].read());
                    valid_fifo[i]->write(valid_in.read());
                }
                wait(tSYS_CK, SC_PS);
            }
        }
    };

} // namespace model

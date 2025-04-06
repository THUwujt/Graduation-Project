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
    // 由于它和DRAM_shift_reg是同步前进的所以它的valid用DRAM_shift_reg的valid就行
    // 定义shift_reg模块
    class ColumnShiftReg : public sc_module
    {
    public:
        sc_in<int> column_in;
        sc_out<int> column_out[32];

        // 追踪所有valid_out信号
        // sc_signal<bool> valid_out_signal[32]; // 追踪valid_out信号

        // 构造函数
        SC_CTOR(ColumnShiftReg)
        {
            // 为每个FIFO实例化FIFO队列（长度分别为1到32）
            for (int i = 0; i < 32; i++)
            {
                // 使用FIFO长度i+1，FIFO的长度为1到31
                column_fifo[i] = new sc_fifo<int>(i + 1); // ?不知道为什么这里会差2个周期，后续需要解释，当前是强行打的补丁
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
                    column_fifo[i]->write(0);
                }
            }
        }
        // 析构函数
        ~ColumnShiftReg()
        {
            // 清理动态分配的FIFO
            for (int i = 0; i < 32; i++)
            {
                delete column_fifo[i];
            }
        }
        // void trace()
        // {
        //     if (g_trace)
        //     { // 检查 g_trace 是否已初始化
        //         for (int i = 0; i < 32; i++)
        //         {
        //             sc_core::sc_trace(g_trace, valid_out_signal[i], "shift_reg_valid_out_signal_" + to_string(i));
        //         }
        //     }
        // }

    private:
        // FIFO数组，每个FIFO长度不同
        sc_fifo<int> *column_fifo[32];

        // 处理过程：将输入数据移位到相应的FIFO，并将结果输出
        void shift_process()
        {
            while (true)
            {
                // 将输入数据依次写入对应的FIFO
                for (int i = 0; i < 32; i++)
                {
                    // 从FIFO中读取数据，并输出到对应的端口
                    column_out[i].write(column_fifo[i]->read());
                    // 将每个输入数据写入对应长度的FIFO
                    column_fifo[i]->write(column_in.read());
                }
                wait(tSYS_CK, SC_PS);
            }
        }
    };

} // namespace model

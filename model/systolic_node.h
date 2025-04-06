#pragma once

#include <systemc>
#include <tlm>
#include "trace.h"

using namespace sc_core;
using namespace tlm;
using namespace std;

namespace model
{
    class SystolicNode : public sc_module
    {
    public:
        // 输入端口
        sc_in<int> data_in_left;   // 来自左侧的输入数据
        sc_in<int> data_in_up;     // 来自上方的输入数据
        sc_in<bool> valid_in_left; // 左侧数据的valid信号
        sc_in<bool> valid_in_up;   // 上方数据的valid信号

        // 输出端口
        sc_out<int> data_out_right;   // 输出到右侧的数据
        sc_out<bool> valid_out_right; // 右侧数据的valid信号
        sc_out<int> data_out_down;    // 输出到下方的数据
        sc_out<bool> valid_out_down;  // 下方数据的valid信号

        // 追踪valid_in_left和valid_in_up
        // sc_signal<bool> valid_in_left_trace;
        // sc_signal<bool> valid_in_up_trace;

        SC_HAS_PROCESS(SystolicNode);
        // 构造函数
        SystolicNode(sc_module_name name, int r, int c) : sc_module(name), row(r), column(c)
        {
            // 默认初始化weight为1
            weight = 0;

            // 进程
            SC_THREAD(process);
        }

        void set_weight(int w)
        {
            weight = w;
        }
        int get_weight() const
        {
            return weight;
        }
        void trace()
        {
            if (g_trace) // 检查 g_trace 是否已初始化
            {
                // 使用row和column生成唯一的追踪信号名
                string trace_name_left = "node_valid_in_left_" + to_string(row) + "_" + to_string(column);
                string trace_name_up = "node_valid_in_up_" + to_string(row) + "_" + to_string(column);
                sc_core::sc_trace(g_trace, valid_in_left, trace_name_left);
                sc_core::sc_trace(g_trace, valid_in_up, trace_name_up);
                // trace weight
                string trace_name_weight = "node_weight_" + to_string(row) + "_" + to_string(column);
                sc_core::sc_trace(g_trace, weight, trace_name_weight);
                // trace data_in_left
                string trace_name_data_in_left = "node_data_in_left_" + to_string(row) + "_" + to_string(column);
                sc_core::sc_trace(g_trace, data_in_left, trace_name_data_in_left);
            }
        }

    private:
        int row;
        int column;
        int weight;
        // 处理过程
        void process()
        {
            while (true)
            {
                // 控制valid信号
                bool valid_left = valid_in_left.read();
                bool valid_up = valid_in_up.read();

                // 追踪valid信号
                // valid_in_left_trace.write(valid_left);
                // valid_in_up_trace.write(valid_up);

                // 计算逻辑
                if (valid_left && !valid_up)
                {
                    // 如果左侧valid，但上方不valid，做乘法并将结果传给下方
                    data_out_right.write(data_in_left.read());
                    valid_out_right.write(true);
                    data_out_down.write(data_in_left.read() * weight);
                    valid_out_down.write(true);
                }
                else if (valid_up && !valid_left)
                {
                    // 如果上方valid，左侧不valid，直接将上方数据传给下方
                    data_out_right.write(0);
                    valid_out_right.write(false);
                    data_out_down.write(data_in_up.read());
                    valid_out_down.write(true);
                }
                else if (valid_left && valid_up)
                {
                    // 如果上方和左侧都valid，进行计算
                    int result = (data_in_left.read() * weight) + data_in_up.read();
                    data_out_right.write(data_in_left.read());
                    valid_out_right.write(true);
                    data_out_down.write(result);
                    valid_out_down.write(true);
                }
                else
                {
                    // 如果都不valid，右侧和下方valid信号为false
                    data_out_right.write(0);
                    valid_out_right.write(false);
                    data_out_down.write(0);
                    valid_out_down.write(false);
                }
                wait(tSYS_CK, SC_PS);
            }
        }
    };

} // namespace model

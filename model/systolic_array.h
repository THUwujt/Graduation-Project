#pragma once

#include <systemc.h>
#include "systolic_node.h" // 包含SystolicNode的头文件

namespace model
{
    class SystolicArray : public sc_module
    {
    public:
        // 输入端口：32个int数据和valid信号
        sc_in<int> data_in[32];   // 32个输入数据端口（来自ShiftReg）
        sc_in<bool> valid_in[32]; // 32个valid信号

        // 输出端口：32个int数据和valid信号（最终计算结果输出）
        sc_out<int> data_out[32];   // 32个输出数据端口
        sc_out<bool> valid_out[32]; // 32个valid信号

        //**新增输入端口，用于控制weight的修改
        //**来自8个shift reg,每个对应4列脉动阵列
        sc_in<int> weight_in[8][32];     // 8个shift reg的输入数据
        sc_in<bool> weight_valid[8][32]; // 8个shift reg的valid信号
        sc_in<int> weight_column[8][32]; // 取值0~3，因为一个shift reg要管4列脉动阵列

        // **新增 sc_signal 变量** 用于替代直接赋值 0
        sc_signal<int> zero_int_signal;           // 代表0输入的信号
        sc_signal<bool> zero_bool_signal;         // 代表false输入的信号
        sc_signal<int> right_boundary_data[32];   // 右侧边界数据
        sc_signal<bool> right_boundary_valid[32]; // 右侧边界valid信号

        // **中间sc_signal作为连接的桥梁
        sc_signal<int> left_right_data[32][31];   // 节点间左右方向的数据信号
        sc_signal<bool> left_right_valid[32][31]; // 节点间左右方向的valid信号
        sc_signal<int> up_down_data[31][32];      // 节点间上下方向的数据信号
        sc_signal<bool> up_down_valid[31][32];    // 节点间上下方向的valid信号

        // 构造函数
        SC_CTOR(SystolicArray)
        {
            // 显式初始化 zero_int_signal 和 zero_bool_signal
            zero_int_signal.write(0);
            zero_bool_signal.write(false);
            for (int i = 0; i < 32; i++)
            {
                for (int j = 0; j < 32; j++)
                {
                    // 实例化节点时传入row和column
                    systolic_nodes[i][j] = new SystolicNode(("SystolicNode_" + to_string(i) + "_" + to_string(j)).c_str(), i, j);

                    // 连接左侧数据输入（第一列的节点连接data_in，其它列的节点连接左侧节点的输出）
                    if (j == 0)
                    {
                        systolic_nodes[i][j]->data_in_left(data_in[i]);
                        systolic_nodes[i][j]->valid_in_left(valid_in[i]);
                    }
                    else
                    {
                        systolic_nodes[i][j]->data_in_left.bind(left_right_data[i][j - 1]);
                        systolic_nodes[i][j]->valid_in_left.bind(left_right_valid[i][j - 1]);
                        systolic_nodes[i][j - 1]->data_out_right.bind(left_right_data[i][j - 1]);
                        systolic_nodes[i][j - 1]->valid_out_right.bind(left_right_valid[i][j - 1]);
                        // 连接最右侧的边界（防止悬空）
                        if (j == 31)
                        {
                            systolic_nodes[i][j]->data_out_right(right_boundary_data[i]);
                            systolic_nodes[i][j]->valid_out_right(right_boundary_valid[i]);
                        }
                    }

                    // 连接上方数据输入（第一行的节点data_in_up为空，其它行的节点连接上方节点的输出）
                    if (i == 0)
                    {
                        systolic_nodes[i][j]->data_in_up(zero_int_signal); // 上方无输入
                        systolic_nodes[i][j]->valid_in_up(zero_bool_signal);
                    }
                    else
                    {
                        systolic_nodes[i][j]->data_in_up.bind(up_down_data[i - 1][j]);
                        systolic_nodes[i][j]->valid_in_up.bind(up_down_valid[i - 1][j]);
                        systolic_nodes[i - 1][j]->data_out_down.bind(up_down_data[i - 1][j]);
                        systolic_nodes[i - 1][j]->valid_out_down.bind(up_down_valid[i - 1][j]);
                        if (i == 31) // 连接输出端口（最底部的节点连接到data_out，其它的向下传递）
                        {
                            systolic_nodes[i][j]->data_out_down(data_out[j]);
                            systolic_nodes[i][j]->valid_out_down(valid_out[j]);
                        }
                    }
                }
            }
            SC_THREAD(change_weight);
        }

        // 析构函数：释放所有SystolicNode的内存
        ~SystolicArray()
        {
            for (int i = 0; i < 32; i++)
            {
                for (int j = 0; j < 32; j++)
                {
                    delete systolic_nodes[i][j];
                }
            }
        }
        void trace()
        {
            if (g_trace)
            { // 检查 g_trace 是否已初始化
                for (int i = 0; i < 32; i++)
                {
                    for (int j = 0; j < 32; j++)
                    {
                        systolic_nodes[i][j]->trace();
                    }
                }
            }
        }

        void change_weight()
        {
            while (true)
            {
                wait(tSYS_CK, SC_PS); // 等待1个时钟周期
                // todo：根据新增的weight端口来决定给哪些node设置新的权重
                for (int i = 0; i < 8; i++)
                {
                    for (int j = 0; j < 32; j++)
                    {
                        if (weight_valid[i][j].read())
                        {
                            int column = weight_column[i][j].read() + i * 4; // 计算列索引
                            systolic_nodes[j][column]->set_weight(weight_in[i][j].read());
                        }
                    }
                }
            }
        }

    private:
        SystolicNode *systolic_nodes[32][32]; // 32x32的SystolicNode阵列
    };

} // namespace model

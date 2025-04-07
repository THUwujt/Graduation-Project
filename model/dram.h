#pragma once

#include <systemc.h>
#include <vector>
#include <iostream>
#include "trace.h"
#include <fstream>
#include <sstream>

using namespace std;

namespace model
{
    // 定义Bank模块
    // Bank模块模拟一个4x32x32的存储器
    // 输入命令：0-不操作，1-读取数据，2-换行，3-刷新
    // 地址格式：高3位为行地址，低5位为列地址
    class Bank : public sc_module
    {
    public:
        // 输入端口
        sc_in<int> cmd_in;  // 命令输入
        sc_in<int> addr_in; // 地址输入
        sc_in<int> target_column_in;
        sc_in<bool> valid_in;

        // 输出端口：32 个 int 数据
        sc_out<int> data_out[32];
        sc_out<bool> valid_out;
        sc_out<int> target_column_out;

        // 追踪valid_out信号

        Bank(sc_module_name name, int ID = 0) : sc_module(name), id(ID), current_row(0)
        {
            // 初始化 32x32x32 的三维存储器
            memory.resize(32, vector<vector<int>>(32, vector<int>(32, 0)));
            initialize();
            SC_THREAD(process_command);
        }

        void trace()
        {
            if (g_trace) // 检查 g_trace 是否已初始化
            {
                // 使用id生成唯一的追踪信号名
                // 追踪 valid_out 信号
                string trace_name = "bank" + to_string(id) + "_valid_out";
                sc_core::sc_trace(g_trace, valid_out, trace_name);
                // trace valid in
                string valid_in_trace_name = "bank" + to_string(id) + "_valid_in";
                sc_core::sc_trace(g_trace, valid_in, valid_in_trace_name);
                // trace target_column_out
                string target_column_trace_name = "bank" + to_string(id) + "_target_column_out";
                sc_core::sc_trace(g_trace, target_column_out, target_column_trace_name);
                // trace target_column_in
                string target_column_in_trace_name = "bank" + to_string(id) + "_target_column_in";
                sc_core::sc_trace(g_trace, target_column_in, target_column_in_trace_name);
            }
        }

        float get_utility_rate()
        {
            utility_rate = (float)used_cycle / (float)(sc_time_stamp().to_seconds() * 1e12 / tDRAM_CK);
            return utility_rate;
        }

    private:
        vector<vector<vector<int>>> memory; // 3D 存储器
        int current_row;                    // 记录当前选定的 row
        int id;
        int used_cycle = 0;     // 使用周期数
        float utility_rate = 0; // 带宽利用率
        bool per2bank = true;

        void initialize()
        {

            // 从文件加载矩阵数据
            std::ifstream file("matrix_vector_data.txt");
            if (!file.is_open())
            {
                std::cerr << "Error: Cannot open matrix_vector_data.txt file" << std::endl;
                return;
            }

            std::string line;
            int line_count = 0;
            int matrix_count = 0;
            int vector_count = 0;

            // 跳过前16个有效行（向量数据）
            while (vector_count < 16 && std::getline(file, line))
            {
                if (line.empty() || line[0] == '#')
                {
                    continue;
                }
                vector_count++;
            }
            if (per2bank)
            { // 如果id是奇数，跳过前面8个矩阵
                if (id % 2 == 1)
                {
                    while (matrix_count < 8 && std::getline(file, line))
                    {
                        if (line.empty() || line[0] == '#')
                        {
                            continue;
                        }
                        line_count++;
                        if (line_count == 32)
                        {
                            matrix_count++;
                            line_count = 0;
                        }
                    }
                }

                // 确定当前bank应该存储的矩阵范围
                int start_matrix = (id % 2 == 0) ? 0 : 8; // 偶数bank存储矩阵0-7，奇数bank存储矩阵8-15
                int matrix_offset = 0;                    // 当前处理的矩阵在范围内的偏移量

                // 确定当前bank应该存储的行范围
                int row_offset = (id / 2) * 4; // 每个bank存储4行数据，bank0存储0-3行，bank2存储4-7行，以此类推

                // 读取矩阵数据
                while (matrix_count < 16 && std::getline(file, line))
                {
                    // 跳过注释行
                    if (line.empty() || line[0] == '#')
                    {
                        continue;
                    }

                    // 检查是否已经处理完当前bank应该存储的所有矩阵
                    if (matrix_count >= start_matrix + 8)
                    {
                        break;
                    }

                    // 解析矩阵行数据
                    std::istringstream iss(line);
                    std::vector<int> row_data;
                    int value;

                    // 读取整行数据
                    while (iss >> value)
                    {
                        row_data.push_back(value);
                    }

                    // 如果行数据完整且当前行在当前bank的存储范围内
                    if (row_data.size() == 32 && line_count >= row_offset && line_count < row_offset + 4)
                    {
                        // 计算在memory中的位置
                        int row_in_bank = line_count - row_offset;         // 在bank中的行号
                        int matrix_in_range = matrix_count - start_matrix; // 在范围内的矩阵编号

                        // 存储整行数据到memory[0][row_in_bank + matrix_in_range * 4][:]
                        for (int col = 0; col < 32; ++col)
                        {
                            memory[0][row_in_bank + matrix_in_range * 4][col] = row_data[col];
                        }
                    }

                    line_count++;
                    if (line_count == 32)
                    {
                        matrix_count++;
                        line_count = 0;
                    }
                }
            }
            else
            {
                // 确定当前bank应该存储的行
                int row1, row2;
                if (id % 2 == 0)
                {
                    // 偶数bank存储第2*id行和第2*id+2行
                    row1 = 2 * id;
                    row2 = 2 * id + 2;
                }
                else
                {
                    // 奇数bank存储第2*id-1行和第2*id+1行
                    row1 = 2 * id - 1;
                    row2 = 2 * id + 1;
                }

                // 读取矩阵数据
                int matrix_count = 0;
                int line_count = 0;

                while (matrix_count < 16 && std::getline(file, line))
                {
                    // 跳过注释行
                    if (line.empty() || line[0] == '#')
                    {
                        continue;
                    }

                    // 解析矩阵行数据
                    std::istringstream iss(line);
                    std::vector<int> row_data;
                    int value;

                    // 读取整行数据
                    while (iss >> value)
                    {
                        row_data.push_back(value);
                    }

                    // 如果行数据完整且当前行在当前bank的存储范围内
                    if (row_data.size() == 32 && (line_count == row1 || line_count == row2))
                    {
                        // 计算在memory中的位置
                        int row_in_bank;
                        if (line_count == row1)
                        {
                            row_in_bank = matrix_count * 2; // 第一行存储在偶数位置
                        }
                        else
                        {
                            row_in_bank = matrix_count * 2 + 1; // 第二行存储在奇数位置
                        }

                        // 存储整行数据到memory[0][row_in_bank][:]
                        for (int col = 0; col < 32; ++col)
                        {
                            memory[0][row_in_bank][col] = row_data[col];
                        }
                    }

                    line_count++;
                    if (line_count == 32)
                    {
                        matrix_count++;
                        line_count = 0;
                    }
                }
            }

            file.close();
            // 初始化其余部分
            for (int i = 1; i < 4; ++i)
            {
                for (int j = 0; j < 32; ++j)
                {
                    for (int k = 0; k < 32; ++k)
                    {
                        memory[i][j][k] = memory[0][j][k] + i;
                    }
                }
            }
            // 把上面的数据重复7次就是剩下的初始化数据
            for (int i = 1; i < 8; ++i)
            {
                for (int r = 0; r < 4; ++r)
                {
                    for (int j = 0; j < 32; ++j)
                    {
                        for (int k = 0; k < 32; ++k)
                        {
                            memory[i * 4 + r][j][k] = memory[r][j][k];
                        }
                    }
                }
            }
        }

        void process_command()
        {
            while (true)
            {
                if (!valid_in.read())
                {
                    valid_out.write(false);
                    wait(tDRAM_CK, SC_PS);
                    continue;
                }
                int cmd = cmd_in.read();
                int addr = addr_in.read();
                int row = addr >> 5;      // 取高位作为 row
                int column = addr & 0x1F; // 取低 5 位作为 column (0-31)

                switch (cmd)
                {
                case 0:
                    // 命令 0：不做任何操作
                    for (int i = 0; i < 32; i++)
                    {
                        valid_out.write(false);
                    }
                    break;

                case 1:
                    // 读取数据命令
                    // 检查越界
                    if (row >= 32 || column >= 32)
                    {
                        for (int i = 0; i < 32; i++)
                        {
                            valid_out.write(false);
                        }
                        cerr << "Error: Row or column out of bounds!" << endl;
                        return;
                    }
                    //! 暂时先取消这个检查，后面需要的时候再恢复
                    // if (row != current_row)
                    // {
                    //     for (int i = 0; i < 32; i++)
                    //     {
                    //         valid_out.write(false);
                    //     }
                    //     cerr << "Error: Row address mismatch!" << endl;
                    //     return;
                    // }
                    for (int i = 0; i < 32; i++)
                    {
                        data_out[i].write(memory[row][column][i]);
                    }
                    valid_out.write(true);
                    used_cycle++;
                    target_column_out.write(target_column_in.read());
                    break;

                case 2:
                    // 换行命令
                    current_row = row;
                    for (int i = 0; i < 32; i++)
                    {
                        valid_out.write(false);
                    }
                    break;

                case 3:
                    // 刷新命令
                    current_row = -1;
                    for (int i = 0; i < 32; i++)
                    {
                        valid_out.write(false);
                    }
                    break;

                default:
                    for (int i = 0; i < 32; i++)
                    {
                        valid_out.write(false);
                    }
                    cerr << "Error: Invalid command!" << endl;
                    break;
                }
                wait(tDRAM_CK, SC_PS); // 等待 2.5 ns
            }
        }
    };
} // namespace model

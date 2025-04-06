#pragma once

#include <systemc.h>
#include <tlm.h>
#include <vector>
#include "trace.h"
#include <fstream>
#include <sstream>

using namespace sc_core;
using namespace std;

namespace model
{

    // 定义DataBuffer模块
    class DataBuffer : public sc_module
    {
    public:
        // 输入端口：发送信号
        sc_in<bool> send; // 触发发送数据的信号
        // 用于trace所有的valid_out
        // sc_signal<bool> valid_out_signal;

        // 输出端口：32个int数据和32个valid信号
        sc_out<int> data_out[32]; // 32个int数据输出
        sc_out<bool> valid_out;   // valid信号输出

        // 构造函数
        SC_CTOR(DataBuffer) : send("send")
        {
            // 初始化32个FIFO
            for (int i = 0; i < 32; i++)
            {
                data_fifos[i] = new sc_fifo<int>(64 * 8); // 每个FIFO的大小为64
            }
            // 默认初始化数据
            initialize_data();
            // 注册处理方法
            SC_THREAD(send_data);
        }

        // 初始化数据函数
        void initialize_data()
        {
            // 反复加载相同的16个vector32遍
            for (int i = 0; i < 32; i++)
            {
                // 从文件加载向量数据
                std::ifstream file("matrix_vector_data.txt");
                if (!file.is_open())
                {
                    std::cerr << "Error: Cannot open matrix_vector_data.txt file" << std::endl;
                    return;
                }

                std::string line;
                int line_count = 0;

                // 读取前16行（向量数据）
                while (line_count < 16 && std::getline(file, line))
                {
                    // 跳过注释行
                    if (line.empty() || line[0] == '#')
                    {
                        continue;
                    }

                    // 解析向量数据
                    std::istringstream iss(line);
                    int value;
                    int col = 0;

                    while (iss >> value && col < 32)
                    {
                        // 向每个FIFO中写入初始数据
                        data_fifos[col]->write(value);
                        col++;
                    }

                    line_count++;
                }

                file.close();
            }
        }

        // 析构函数：释放动态分配的FIFO内存
        ~DataBuffer()
        {
            for (int i = 0; i < 32; i++)
            {
                delete data_fifos[i];
            }
        }
        void trace()
        {
            if (g_trace)
            { // 检查 g_trace 是否已初始化
                sc_core::sc_trace(g_trace, valid_out, "data_buffer_valid_out_signal");
            }
        }

    private:
        // 内部存储32个FIFO队列
        sc_fifo<int> *data_fifos[32];

        // 处理方法：根据send信号的触发发送数据
        void send_data()
        {
            while (true)
            {
                // 当send信号为1时，发送数据
                if (send.read() == true)
                {
                    for (int i = 0; i < 32; i++)
                    {
                        if (data_fifos[i]->num_available() > 0)
                        {
                            int data = data_fifos[i]->read();
                            data_out[i].write(data); // 输出数据
                            valid_out.write(true);   // 对应的valid信号为true
                            // valid_out_signal.write(true); // 记录valid信号
                        }
                        else
                        {
                            valid_out.write(false); // 如果FIFO为空，valid信号为false
                            // valid_out_signal.write(false); // 记录valid信号
                        }
                    }
                }
                else
                {
                    for (int i = 0; i < 32; i++)
                    {
                        valid_out.write(false); // 如果send信号为0，所有valid信号为false
                        // valid_out_signal.write(false); // 记录valid信号
                    }
                }
                wait(tSYS_CK, SC_PS);
            }
        }
    };

} // namespace model

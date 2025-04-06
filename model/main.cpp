#include <systemc.h>
#include "TOP.h"
#include "trace.h"
#include <vector>
#include <fstream>
#include <sstream>
#include <numeric>
using namespace std;

int sc_main(int argc, char *argv[])
{
    // 初始化追踪文件
    model::init_trace("waveform"); // 创建 waveform.vcd
    // 实例化Top模块
    model::Top top("top");
    // 设置追踪
    top.trace(); // 调用 TOP 的 trace 函数，追踪所有信号，需要在sc_start之前调用
    // 启动仿真
    sc_start(8192, SC_NS); // 运行100ns仿真，期间Top模块的各个子模块将会工作

    // 输出存储在ResultBuffer中的数据
    // 暂存仿真结果：32个FIFO，每个深度16
    vector<vector<int>> sim_results(32); // 暂存32个FIFO的数据
    for (int i = 0; i < 32; i++)
    {
        sim_results[i] = top.get_result(i);
        // cout << "ResultBuffer[" << i << "]: ";
        // for (int j = 0; j < sim_results[i].size(); j++)
        // {
        //     cout << sim_results[i][j] << " ";
        // }
        // cout << endl;
    }
    // 输出平均利用率
    if (1) //!
    {
        float average_utility_rate = top.calculate_utility_rate();
        cout << "Average Utility Rate: " << average_utility_rate << endl;
    }
    if (1) // !turn off this for now
    {
        // 计算理论结果：16个向量与矩阵的乘法
        vector<vector<int>> source_vectors(16, vector<int>(32));                                  // 16个向量，每个长度为32
        vector<vector<vector<int>>> source_matrixs(16, vector<vector<int>>(32, vector<int>(32))); // 16个矩阵，每个大小为32x32
        vector<vector<int>> expected_results(64 * 8, vector<int>(32));                            // 16个结果向量，每个长度为32

        // 从文件加载向量和矩阵数据
        std::ifstream file("matrix_vector_data.txt");
        if (!file.is_open())
        {
            std::cerr << "Error: Cannot open matrix_vector_data.txt file" << std::endl;
            return -1;
        }

        std::string line;
        int line_count = 0;

        // 读取向量数据（前16行）
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
                source_vectors[line_count][col] = value;
                col++;
            }

            line_count++;
        }

        // 读取矩阵数据（接下来16*32行，每行32个数据）
        int matrix_count = 0;
        int row_in_matrix = 0;

        while (matrix_count < 16 && std::getline(file, line))
        {
            // 跳过注释行
            if (line.empty() || line[0] == '#')
            {
                continue;
            }

            // 解析矩阵行数据
            std::istringstream iss(line);
            int value;
            int col = 0;

            while (iss >> value && col < 32)
            {
                source_matrixs[matrix_count][row_in_matrix][col] = value;
                col++;
            }

            row_in_matrix++;
            if (row_in_matrix == 32)
            {
                matrix_count++;
                row_in_matrix = 0;
            }
        }

        file.close();

        // 计算理论结果
        for (int vec_idx = 0; vec_idx < 16; vec_idx++)
        {
            for (int i = 0; i < 32; i++)
            {
                expected_results[vec_idx][i] = 0;
                for (int j = 0; j < 32; j++)
                {
                    expected_results[vec_idx][i] += source_vectors[vec_idx][j] * source_matrixs[vec_idx][i][j];
                }
            }
        }
        // expected_results的其余部分直接根据已有的加出来
        for (int vec_idx = 16; vec_idx < 64; vec_idx++)
        {
            for (int i = 0; i < 32; i++)
            {
                expected_results[vec_idx][i] = expected_results[vec_idx % 16][i] + vec_idx / 16 * std::accumulate(source_vectors[vec_idx % 16].begin(), source_vectors[vec_idx % 16].end(), 0);
            }
        }
        // 剩下的结果和前面一样
        for (int vec_idx = 64; vec_idx < 64 * 8; vec_idx++)
        {
            for (int i = 0; i < 32; i++)
            {
                expected_results[vec_idx][i] = expected_results[vec_idx % 64][i];
            }
        }
        // 对比理论结果和仿真结果
        bool all_correct = true;
        for (int vec_idx = 64 * 7 + 56; vec_idx < 64 * 8; vec_idx++)
        {
            cout << "Checking Vector " << vec_idx << ": ";
            for (int i = 0; i < 32; i++)
            {
                if (sim_results[i][vec_idx] != expected_results[vec_idx][i])
                {
                    cout << "Mismatch at index " << i << ": expected " << expected_results[vec_idx][i]
                         << ", got " << sim_results[i][vec_idx]
                         << ", absolute error " << sim_results[i][vec_idx] - expected_results[vec_idx][i]
                         << endl;
                    all_correct = false;
                }
            }
            if (all_correct)
            {
                cout << "One result All match!" << endl;
            }
        }
        if (all_correct)
        {
            cout << "All results are correct!" << endl;
        }
        else
        {
            cout << "Some results are incorrect." << endl;
        }
    }
    if (model::g_trace)
    {
        sc_close_vcd_trace_file(model::g_trace);
        model::g_trace = NULL; // 重置为 NULL，避免重复关闭
    }

    return 0;
}

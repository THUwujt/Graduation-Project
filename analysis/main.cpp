#include <systemc>
#include "TOP.h" // 包含 TOP 模块的头文件
#include "trace.h"
#include <vector>
#include <xlsxwriter.h>

using namespace sc_core;
using namespace tlm;
using namespace std;
using namespace nsim;

int sc_main(int argc, char *argv[])
{
    std::vector<int> tFAW_values = {30, 35, 40, 45, 50};
    std::vector<int> tchange_row_values = {40, 60, 80};
    std::vector<int> tREFab_values = {120, 180, 240};
    std::vector<int> tREFpb_values = {40, 60, 80};
    std::vector<bool> all_bank_values = {true, false};
    std::vector<bool> per2bank_values = {true, false};
    std::vector<float> results;
    nsim::TOP top("top_module");
    init_trace("waveform"); // 创建 waveform.vcd
    top.trace();

    // Create a new Excel workbook
    std::string path = "/mnt/c/Users/lenovo/Desktop/"; // WSL 环境下的 Windows 桌面路径
    lxw_workbook *workbook = workbook_new((path + "simulation_results.xlsx").c_str());
    lxw_worksheet *worksheet = workbook_add_worksheet(workbook, NULL);

    // Add column headers
    worksheet_write_string(worksheet, 0, 0, "tCHANGE_ROW", NULL);
    worksheet_write_string(worksheet, 0, 1, "tREFab", NULL);
    worksheet_write_string(worksheet, 0, 2, "tREFpb", NULL);
    worksheet_write_string(worksheet, 0, 3, "tFAW", NULL);
    worksheet_write_string(worksheet, 0, 4, "all_bank", NULL);
    worksheet_write_string(worksheet, 0, 5, "per2bank", NULL);
    worksheet_write_string(worksheet, 0, 6, "Bandwidth_Utilization", NULL);
    int row = 1; // Row to start writing data
    for (auto tFAW : tFAW_values)
    {
        for (auto tchange_row : tchange_row_values)
        {
            for (auto tREFab : tREFab_values)
            {
                for (auto tREFpb : tREFpb_values)
                {
                    for (auto all_bank : all_bank_values)
                    {
                        for (auto per2bank : per2bank_values)
                        {
                            // 设置参数
                            nsim::tFAW = tFAW;
                            nsim::tCHANGE_ROW = tchange_row;
                            nsim::tREFab = tREFab;
                            nsim::tREFpb = tREFpb;
                            nsim::all_bank = all_bank;
                            nsim::per2bank = per2bank;
                            top.reset(); // 重置 TOP 模块

                            // 实例化 TOP 模块

                            // 设置仿真时间（例如 100us）
                            sc_time simulation_time(30000, SC_NS); // 10us

                            // 启动仿真
                            std::cout << "Starting simulation..." << std::endl;
                            sc_start(simulation_time);
                            std::cout << "Simulation finished at " << sc_time_stamp() << std::endl;
                            float utilization = top.get_bandwidth_utilization();

                            // 将仿真结果存储到结果容器中
                            results.push_back(utilization);

                            // Write the simulation parameters and result into Excel
                            worksheet_write_number(worksheet, row, 0, tchange_row, NULL);
                            worksheet_write_number(worksheet, row, 1, tREFab, NULL);
                            worksheet_write_number(worksheet, row, 2, tREFpb, NULL);
                            worksheet_write_number(worksheet, row, 3, tFAW, NULL);
                            worksheet_write_boolean(worksheet, row, 4, all_bank, NULL);
                            worksheet_write_boolean(worksheet, row, 5, per2bank, NULL);
                            worksheet_write_number(worksheet, row, 6, utilization, NULL);

                            row++; // Move to the next row

                            // delete top;
                        }
                    }
                }
            }
        }
    }
    // Close the workbook to save the file
    workbook_close(workbook);
    if (nsim::g_trace)
    {
        sc_close_vcd_trace_file(nsim::g_trace);
        nsim::g_trace = NULL; // 重置为 NULL，避免重复关闭
    }
    return 0;
}
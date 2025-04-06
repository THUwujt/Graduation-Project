#pragma once

#include <systemc.h>
#include "trace.h"
#include <vector>

using namespace sc_core;

namespace model
{
    const int tFAW = 40;                                          // ns
    const int tREFI = 3904;                                       // ns
    const int tREFab = 180;                                       // ns
    const int tCHANGE_ROW = 60;                                   // ns
    const int tUSE = 80;                                          // ns
    const int tDRAM_CK = 2500;                                    // ps DRAM clock is 400MHz
    const int tSYS_CK = 1250;                                     // ps systolic clock is 800MHz
    const int tRRD = 10;                                          // ns RRD time
    const int tCCD = 2500;                                        // ps Column 2 column delay
    const int twait = std::max(4 * tFAW - tUSE - tCHANGE_ROW, 0); // ns
    // 定义Control模块
    class Control : public sc_module
    {
    public:
        // 输出端口：控制DataBuffer的send信号
        sc_out<bool> send; // 控制DataBuffer发送数据的信号
        // sc_signal<bool> sending;

        // 16个bank的控制信号
        sc_out<int> dram_cmd[16];
        sc_out<int> dram_addr[16];
        sc_out<int> dram_target_column[16];
        sc_out<bool> dram_valid[16]; // 只有在发命令的那个cycle才是1
        // 构造函数
        SC_CTOR(Control) : send("send"), last_cmd(16, 2), last_cmd_time(16, sc_time(-10000, SC_NS)), last_addr(16, 0), last_target_column(16, 0), per2bank(false)
        {
            // 创建sc_thread，控制每8个时钟周期发出send信号
            SC_THREAD(control_send);
            SC_THREAD(dram_control);
        }

        void trace()
        {
            if (g_trace)
            { // 检查 g_trace 是否已初始化
                sc_core::sc_trace(g_trace, send, "control_send");
            }
        }

    private:
        // 每8个时钟周期发出一次send信号
        void control_send()
        {
            if (per2bank)
            {
                wait(7 * model::tSYS_CK, SC_PS); // 和weight对齐
                while (true)
                {
                    send.write(true); // 发出send信号
                    // sending.write(true);
                    wait(model::tSYS_CK, SC_PS); // 等待1个systolic cycle
                    send.write(false);           // 清除send信号
                    // sending.write(false);
                    wait(model::tSYS_CK * 7, SC_PS); // 保证发送信号后有足够的周期延迟
                }
            }
            else
            {
                // todo:这里对齐的cycle数还没确定，不一定是3个
                wait(3 * model::tSYS_CK, SC_PS); // 和weight对齐
                while (true)
                {
                    send.write(true); // 发出send信号
                    // sending.write(true);
                    wait(model::tSYS_CK, SC_PS); // 等待1个systolic cycle
                    send.write(false);           // 清除send信号
                    // sending.write(false);
                    wait(model::tSYS_CK * 3, SC_PS); // 保证发送信号后有足够的周期延迟
                }
            }
        }

        // 发送命令到指定组的4个bank
        void send_command(int dram_id, int cmd, int addr, int target_column)
        {
            dram_cmd[dram_id].write(cmd);
            dram_addr[dram_id].write(addr);
            dram_target_column[dram_id].write(target_column);
            dram_valid[dram_id].write(true);
            last_cmd[dram_id] = cmd;
            last_cmd_time[dram_id] = sc_time_stamp();
        }

        void dram_control()
        {
            if (per2bank)
            {
                // 启动过程 - 正确实现
                // 第0个bank启动后，每个tDRAM_CK发一个1命令，地址递增
                // 第4-7个cycle，第0和第2个bank每cycle发一个1命令
                // 以此类推直到bank14启动
                // 在此期间其他bank不发命令，所以valid信号为0

                // 初始化连续读取计数器
                std::vector<int> consecutive_reads(16, 0);
                std::vector<int> current_addr(16, 0); // 当前地址

                // 启动过程
                for (int phase = 0; phase < 8; phase++)
                {
                    // 每个phase有2个tDRAM_CK周期
                    for (int cycle = 0; cycle < 2; cycle++)
                    {
                        // 确定当前phase需要启动的bank
                        for (int bank = 0; bank <= phase * 2; bank += 2)
                        {
                            // 发送读取命令
                            send_command(bank, 1, current_addr[bank], last_target_column[bank]);
                            last_target_column[bank] = (last_target_column[bank] + 1) % 4;
                            current_addr[bank]++; // 每发一个读取命令下一次再发读取命令的时候地址+1
                            consecutive_reads[bank]++;
                        }
                        // 其他bank的valid信号为0
                        for (int i = 0; i < 16; i++)
                        {
                            if (i % 2 || i > phase * 2)
                            {
                                dram_valid[i].write(false);
                            }
                        }
                        wait(tDRAM_CK, SC_PS);
                    }
                }

                // 初始化其他状态变量

                sc_time last_row_change_time = sc_time(-10000, SC_NS); // 上次换行时间
                std::vector<int> last_active_bank(8, 0);               // 每组中最后活跃的bank (0或1)

                while (true)
                {
                    sc_time current_time = sc_time_stamp();
                    sc_time time_to_refresh = next_refresh_time - current_time;

                    // 处理每个bank的命令
                    for (int bank = 0; bank < 16; bank++)
                    {
                        sc_time elapsed_time = current_time - last_cmd_time[bank];

                        // 如果上一个命令是读取命令(cmd=1)
                        if (last_cmd[bank] == 1)
                        {
                            // 检查是否需要等待刷新
                            if (time_to_refresh <= sc_time(tUSE, SC_NS))
                            {
                                // 如果即将刷新，不发送新命令
                                dram_valid[bank].write(false);
                                continue;
                            }

                            // 检查是否已经连续发送了32个读取命令
                            if (consecutive_reads[bank] >= 32)
                            {
                                // 需要发送换行命令
                                if (check_faw_constraint(bank, current_time) &&
                                    check_rrd_constraint(bank, current_time, last_row_change_time))
                                {
                                    // 发送换行命令
                                    send_command(bank, 2, current_addr[bank], 0);

                                    // 更新换行时间记录
                                    last_row_change_time = current_time;
                                    // 记录当前组中最后活跃的bank
                                    last_active_bank[bank / 2] = bank % 2;
                                }
                                else
                                {
                                    // 不满足约束，不发送命令
                                    dram_valid[bank].write(false);
                                }
                            }
                            // 不到32次，可以发送下一个读取命令,且不需要关心配对bank的状态，因为配对bank应该等它而不是反过来
                            else
                            {
                                // 同一个bank的相邻2个读取命令之间有tCCD的延迟
                                if (last_cmd_time[bank] + sc_time(tCCD, SC_PS) <= sc_time_stamp())
                                {
                                    // 发送读取命令
                                    send_command(bank, 1, current_addr[bank], last_target_column[bank]);
                                    last_target_column[bank] = (last_target_column[bank] + 1) % 4;
                                    consecutive_reads[bank]++; // 增加连续读取计数
                                    current_addr[bank]++;      // 地址递增
                                }
                            }
                        }

                        // 如果上一个命令是换行命令(cmd=2)
                        else if (last_cmd[bank] == 2 && elapsed_time >= sc_time(tCHANGE_ROW, SC_NS))
                        {
                            // 检查是否需要等待刷新
                            if (time_to_refresh <= sc_time(tUSE, SC_NS))
                            {
                                // 如果即将刷新，不发送新命令
                                dram_valid[bank].write(false);
                                continue;
                            }

                            // 检查配对bank是否正在读取
                            int paired_bank = (bank % 2 == 0) ? bank + 1 : bank - 1;
                            if (last_cmd[paired_bank] == 1 && (last_cmd_time[paired_bank] == current_time || consecutive_reads[paired_bank] < 32))
                            {
                                // 配对bank正在读取，跳过
                                dram_valid[bank].write(false);
                                continue;
                            }

                            // 发送读取命令
                            send_command(bank, 1, current_addr[bank], last_target_column[bank]);
                            last_target_column[bank] = (last_target_column[bank] + 1) % 4;
                            consecutive_reads[bank] = 1; // 重置连续读取计数为1
                            current_addr[bank]++;        // 地址递增
                        }
                        // 如果上一个命令是刷新命令(cmd=3)
                        // ! 实际上现在的刷新后处理都不对，应该重新走一遍启动流程
                        else if (last_cmd[bank] == 3 && elapsed_time >= sc_time(tREFab, SC_NS))
                        {
                            // 检查是否需要等待刷新
                            if (time_to_refresh <= sc_time(tUSE, SC_NS))
                            {
                                // 如果即将刷新，不发送新命令
                                dram_valid[bank].write(false);
                                continue;
                            }

                            // 检查tFAW约束
                            if (check_faw_constraint(bank, current_time))
                            {
                                // 检查配对bank是否正在读取
                                int paired_bank = (bank % 2 == 0) ? bank + 1 : bank - 1;
                                if (last_cmd[paired_bank] == 1 && (last_cmd_time[paired_bank] == current_time || consecutive_reads[paired_bank] < 32))
                                {
                                    // 配对bank正在读取，跳过
                                    dram_valid[bank].write(false);
                                    continue;
                                }

                                // 刷新后，让另一个bank先启动
                                int group = bank / 2;
                                int other_bank = (last_active_bank[group] == 0) ? group * 2 + 1 : group * 2;

                                // 如果当前bank是应该先启动的那个
                                if (bank == other_bank)
                                {
                                    // 发送读取命令
                                    send_command(bank, 1, current_addr[bank], last_target_column[bank]);
                                    last_target_column[bank] = (last_target_column[bank] + 1) % 4;
                                    consecutive_reads[bank] = 1; // 重置连续读取计数为1
                                    current_addr[bank]++;        // 地址递增
                                }
                                else
                                {
                                    // 不是应该先启动的那个，跳过
                                    dram_valid[bank].write(false);
                                }
                            }
                            else
                            {
                                // 不满足约束，不发送命令
                                dram_valid[bank].write(false);
                            }
                        }
                    }

                    // 处理刷新
                    if (current_time >= next_refresh_time)
                    {
                        for (int bank = 0; bank < 16; bank++)
                        {
                            send_command(bank, 3, 0, 0);
                            consecutive_reads[bank] = 0; // 重置连续读取计数器
                        }
                        next_refresh_time += sc_time(model::tREFI, SC_NS);
                    }

                    // 更新valid信号
                    for (int i = 0; i < 16; i++)
                    {
                        if (last_cmd_time[i] != current_time)
                        {
                            dram_valid[i].write(false); // 凡是本cycle没有发命令的bank，valid都为0
                        }
                    }

                    wait(model::tDRAM_CK, SC_PS); // 保证发送信号后有一个周期延迟
                }
            }
            else
            {
                // 初始化连续读取计数器
                std::vector<int> consecutive_reads(16, 0);
                std::vector<int> current_addr(16, 0);     // 当前地址
                std::vector<int> row_change_count(16, 0); // 换行计数

                // 初始化last_target_column
                for (int i = 0; i < 16; i++)
                {
                    last_target_column[i] = i % 2;
                }

                // 启动过程 - 每个phase有2个tDRAM_CK周期
                for (int phase = 0; phase < 8; phase++)
                {
                    for (int cycle = 0; cycle < 2; cycle++)
                    {
                        // 确定当前phase需要启动的bank组
                        for (int group = 0; group <= phase; group++)
                        {
                            int bank1 = group * 2;
                            int bank2 = bank1 + 1;

                            // 发送读取命令到配对的bank
                            send_command(bank1, 1, current_addr[bank1], last_target_column[bank1]);
                            send_command(bank2, 1, current_addr[bank2], last_target_column[bank2]);

                            // 更新地址和列
                            last_target_column[bank1] = (last_target_column[bank1] + 2) % 4;
                            last_target_column[bank2] = (last_target_column[bank2] + 2) % 4;
                            current_addr[bank1]++;
                            current_addr[bank2]++;
                            consecutive_reads[bank1]++;
                            consecutive_reads[bank2]++;
                        }

                        // 其他bank的valid信号为0
                        for (int i = 0; i < 16; i++)
                        {
                            if (i > phase * 2 + 1)
                            {
                                dram_valid[i].write(false);
                            }
                        }
                        wait(model::tDRAM_CK, SC_PS);
                    }
                }

                while (true)
                {
                    sc_time current_time = sc_time_stamp();
                    sc_time time_to_refresh = next_refresh_time - current_time;

                    // 处理每个bank的命令
                    for (int bank = 0; bank < 16; bank++)
                    {
                        sc_time elapsed_time = current_time - last_cmd_time[bank];

                        // 如果上一个命令是读取命令(cmd=1)
                        if (last_cmd[bank] == 1)
                        {
                            // 检查是否需要等待刷新
                            if (time_to_refresh <= sc_time(tUSE, SC_NS))
                            {
                                // 如果即将刷新，不发送新命令
                                dram_valid[bank].write(false);
                                continue;
                            }

                            // 检查是否已经连续发送了32个读取命令
                            // 不需要标记换行，这个条件本身就是要换行的标记
                            if (consecutive_reads[bank] >= 32)
                            {
                                // 如果等待周期未结束
                                if (current_time - last_cmd_time[bank] < sc_time(twait, SC_NS))
                                {
                                    dram_valid[bank].write(false);
                                }
                                // 如果等待周期已结束
                                else if (current_time - last_cmd_time[bank] >= sc_time(twait, SC_NS))
                                {
                                    // 检查tFAW约束
                                    if (check_faw_constraint(bank, current_time))
                                    {
                                        // 发送换行命令
                                        send_command(bank, 2, current_addr[bank], 0);

                                        row_change_count[bank]++;

                                        // 重置状态
                                        consecutive_reads[bank] = 0;
                                    }
                                    else
                                    {
                                        // 不满足约束，不发送命令
                                        dram_valid[bank].write(false);
                                    }
                                }
                            }
                            // 不到32次，可以发送下一个读取命令
                            else
                            {
                                // 发送读取命令
                                send_command(bank, 1, current_addr[bank], last_target_column[bank]);
                                last_target_column[bank] = (last_target_column[bank] + 2) % 4;
                                consecutive_reads[bank]++; // 增加连续读取计数
                                current_addr[bank]++;      // 地址递增
                            }
                        }
                        // 如果上一个命令是换行命令(cmd=2)
                        else if (last_cmd[bank] == 2 && elapsed_time >= sc_time(model::tCHANGE_ROW, SC_NS))
                        {
                            // 检查是否需要等待刷新
                            if (time_to_refresh <= sc_time(model::tUSE, SC_NS))
                            {
                                // 如果即将刷新，不发送新命令
                                dram_valid[bank].write(false);
                                continue;
                            }

                            // 发送读取命令
                            send_command(bank, 1, current_addr[bank], last_target_column[bank]);
                            last_target_column[bank] = (last_target_column[bank] + 2) % 4;
                            consecutive_reads[bank] = 1; // 重置连续读取计数为1
                            current_addr[bank]++;        // 地址递增
                        }
                        // 如果上一个命令是刷新命令(cmd=3)
                        //! 实际上现在的刷新后处理都不对，应该重新走一遍启动流程
                        else if (last_cmd[bank] == 3 && elapsed_time >= sc_time(model::tREFab, SC_NS))
                        {
                            // 检查是否需要等待刷新
                            if (time_to_refresh <= sc_time(model::tUSE, SC_NS))
                            {
                                // 如果即将刷新，不发送新命令
                                dram_valid[bank].write(false);
                                continue;
                            }

                            // 检查tFAW约束
                            if (check_faw_constraint(bank, current_time))
                            {
                                // 发送读取命令
                                send_command(bank, 1, current_addr[bank], last_target_column[bank]);
                                last_target_column[bank] = (last_target_column[bank] + 2) % 4;
                                consecutive_reads[bank] = 1; // 重置连续读取计数为1
                                current_addr[bank]++;        // 地址递增
                            }
                            else
                            {
                                // 不满足约束，不发送命令
                                dram_valid[bank].write(false);
                            }
                        }
                    }

                    // 处理刷新
                    if (current_time >= next_refresh_time)
                    {
                        for (int bank = 0; bank < 16; bank++)
                        {
                            send_command(bank, 3, 0, 0);
                            consecutive_reads[bank] = 0; // 重置连续读取计数器
                        }
                        next_refresh_time += sc_time(model::tREFI, SC_NS);
                    }

                    // 更新valid信号
                    for (int i = 0; i < 16; i++)
                    {
                        if (last_cmd_time[i] != current_time)
                        {
                            dram_valid[i].write(false); // 凡是本cycle没有发命令的bank，valid都为0
                        }
                    }

                    wait(model::tDRAM_CK, SC_PS); // 保证发送信号后有一个周期延迟
                }
            }
        }

        // 检查tFAW约束
        bool check_faw_constraint(int dram_id, sc_time current_time)
        {
            int total = 0;
            for (int i = 0; i < 16; i++)
            {
                if (i == dram_id)
                    continue;
                sc_time last = last_cmd_time[i];
                if (last_cmd[i] == 2) // 换行命令需要检查tFAW约束
                {
                    if (current_time - last < sc_time(tFAW, SC_NS))
                    {
                        total++;
                    }
                }
                if (total == 4)
                {
                    return false; // 违反tFAW约束
                }
            }
            return true;
        }

        // 检查tRRD约束
        // 任意一个bank在换行时距离上一次发出换行命令(不论是哪一个bank)
        // 时间小于tRRD，则违反tRRD约束
        bool check_rrd_constraint(int dram_id, sc_time current_time, sc_time last_row_change_time)
        {
            // 检查最近是否有其他bank换行
            for (int i = 0; i < 16; i++)
            {
                if (i == dram_id)
                    continue;
                if (current_time - last_row_change_time < sc_time(tRRD, SC_NS))
                {
                    return false; // 违反tRRD约束
                }
            }
            return true;
        }
        bool per2bank; // 是否2个bank配对
        std::vector<int> last_cmd;
        std::vector<int> last_addr;
        std::vector<int> last_target_column;
        std::vector<sc_time> last_cmd_time;
        sc_time next_refresh_time = sc_time(tREFI - tREFab, SC_NS);
    };
} // namespace model

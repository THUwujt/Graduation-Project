#pragma once
#include <systemc>
#include <tlm>
#include <iostream>
#include <vector>
#include "bank.h"
#include "util.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "trace.h"

using namespace sc_core;
using namespace tlm;
using namespace std;

namespace nsim
{

    class TOP : public sc_module
    {
    public:
        SC_HAS_PROCESS(TOP);
        // 4 initiators in a vector
        std::vector<tlm_utils::simple_initiator_socket<TOP> *> initiator_sockets;

        TOP(const sc_module_name &name)
            : sc_module(name), last_change_row(0), num_change_row(0)
        {
            if (all_bank)
                next_refresh_time = sc_time(tREFI - tREFab, SC_NS);
            else
                next_refresh_time = sc_time(tREFIpb - tREFpb, SC_NS);
            for (int i = 0; i < 4; ++i)
            {
                // 4 initiator
                std::string socket_name = "initiator_socket_" + std::to_string(i);
                tlm_utils::simple_initiator_socket<TOP> *initiator_socket = new tlm_utils::simple_initiator_socket<TOP>(socket_name.c_str());
                std::string name = "BankGroup" + std::to_string(i);
                Bank *bank = new Bank(name.c_str(), i);
                bank_groups.push_back(bank);
                record_list.push_back(record(DRAMcommand::CHANGE_ROW, sc_time(0, SC_NS)));
                last_cmd_time.push_back(sc_time(0, SC_NS));
                initiator_socket->bind(bank->target_socket);
                initiator_sockets.push_back(initiator_socket);
            }
            SC_THREAD(run);
            SC_THREAD(calculate_bandwidth);
        }

        ~TOP()
        {
            for (auto bank : bank_groups)
            {
                delete bank;
            }
            for (auto socket : initiator_sockets)
            {
                delete socket;
            }
        }

        void run()
        {
            sc_time startup_delay(10, SC_NS);
            for (int i = 0; i < 4; ++i)
            {
                wait(startup_delay); // Sequential startup
                send_command(i, DRAMcommand::USE);
            }

            while (true)
            {
                sc_time current_time = sc_time_stamp();
                sc_time time_to_refresh = next_refresh_time - current_time;
                if (nsim::per2bank == false)
                {
                    num_change_row = 0;
                    for (int i = 0; i < 4; ++i)
                    {
                        if (record_list[i].get_cmd() == DRAMcommand::USE && current_time - last_cmd_time[i] >= sc_time(tUSE, SC_NS) || record_list[i].get_cmd() == DRAMcommand::REFRESH && current_time - last_cmd_time[i] >= sc_time(tREFab, SC_NS) || record_list[i].get_cmd() == DRAMcommand::REFRESHpb && current_time - last_cmd_time[i] >= sc_time(tREFpb, SC_NS))
                        {
                            if (time_to_refresh > sc_time(tUSE, SC_NS))
                            {
                                num_change_row++; // Wait for refresh
                            }
                        }
                    }
                    for (int i = 0; i < 4; ++i)
                    {
                        sc_time last_time = last_cmd_time[i];
                        sc_time elapsed = current_time - last_time;

                        // Check if current command is complete and next command can be issued
                        if (record_list[i].get_cmd() == DRAMcommand::USE && elapsed >= sc_time(tUSE, SC_NS))
                        {
                            // Check if refresh is imminent
                            if (time_to_refresh <= sc_time(tUSE, SC_NS))
                            {
                                continue; // Wait for refresh
                            }
                            // Check FAW constraint
                            if (check_faw_constraint(i, current_time) && (num_change_row == 1 || num_change_row > 1 && i == (last_change_row + 1) % 4))
                            {
                                send_command(i, DRAMcommand::CHANGE_ROW);
                                last_change_row = i;
                            }
                        }
                        else if (record_list[i].get_cmd() == DRAMcommand::CHANGE_ROW && elapsed >= sc_time(tCHANGE_ROW, SC_NS))
                        {
                            if (time_to_refresh <= sc_time(tUSE, SC_NS))
                            {
                                continue; // Wait for refresh
                            }
                            send_command(i, DRAMcommand::USE);
                        }
                        else if (record_list[i].get_cmd() == DRAMcommand::REFRESH && elapsed >= sc_time(tREFab, SC_NS))
                        {
                            // 刷新恢复后逐个给bank group换行，然后才能逐个USE
                            if (time_to_refresh <= sc_time(tUSE, SC_NS))
                            {
                                continue; // Wait for refresh
                            }
                            // Check FAW constraint
                            if (check_faw_constraint(i, current_time))
                            {
                                send_command(i, DRAMcommand::CHANGE_ROW);
                            }
                        }
                        else if (record_list[i].get_cmd() == DRAMcommand::REFRESHpb && elapsed >= sc_time(tREFpb, SC_NS))
                        {
                            // 刷新恢复后逐个给bank group换行，然后才能逐个USE
                            if (time_to_refresh <= sc_time(tUSE, SC_NS))
                            {
                                continue; // Wait for refresh
                            }
                            // Check FAW constraint
                            if (check_faw_constraint(i, current_time))
                            {
                                send_command(i, DRAMcommand::CHANGE_ROW);
                            }
                        }
                    }

                    // Handle refresh
                    if (!nsim::all_bank)
                    {
                        // If refresh time for this Bank group
                        if (current_time >= next_refresh_time && check_faw_constraint(this_refpb, current_time))
                        {
                            // Refresh for a specific Bank group (REFRESHpb)
                            send_command(this_refpb, DRAMcommand::REFRESHpb);
                            if (this_refpb < 3)
                                this_refpb++;
                            else
                                this_refpb = 0;
                            // Update next refresh time for this Bank group
                            next_refresh_time += sc_time(tREFIpb, SC_NS);
                        }
                    }
                    else
                    {
                        if (current_time >= next_refresh_time)
                        {
                            for (int i = 0; i < 4; ++i)
                            {
                                send_command(i, DRAMcommand::REFRESH);
                            }
                            // wait(tREFab, SC_NS);
                            next_refresh_time += sc_time(tREFI, SC_NS);
                        }
                    }
                    wait(tCK, SC_NS); // Minimum scheduling granularity}
                }
                else
                {
                    for (int i = 0; i < 4; i++)
                    {
                        sc_time last_time = last_cmd_time[i];
                        sc_time elapsed = current_time - last_time;

                        // Check if current command is complete and next command can be issued
                        if (record_list[i].get_cmd() == DRAMcommand::USE && elapsed >= sc_time(tUSE, SC_NS))
                        {
                            // Check if refresh is imminent
                            if (time_to_refresh <= sc_time(tUSE, SC_NS))
                            {
                                continue; // Wait for refresh
                            }
                            // Check FAW constraint
                            if (check_faw_constraint(i, current_time))
                            {
                                send_command(i, DRAMcommand::CHANGE_ROW);
                            }
                        }
                        else if (record_list[i].get_cmd() == DRAMcommand::CHANGE_ROW && elapsed >= sc_time(tCHANGE_ROW, SC_NS))
                        {
                            if (i % 2 && record_list[i - 1].get_cmd() != DRAMcommand::USE || !(i % 2) && record_list[i + 1].get_cmd() == DRAMcommand::USE && current_time - last_cmd_time[i + 1] >= sc_time(tUSE, SC_NS))
                            {
                                if (time_to_refresh <= sc_time(tUSE, SC_NS))
                                {
                                    continue; // Wait for refresh
                                }
                                send_command(i, DRAMcommand::USE);
                            }
                        }
                        else if (record_list[i].get_cmd() == DRAMcommand::REFRESH && elapsed >= sc_time(tREFab, SC_NS))
                        {
                            // 刷新恢复后逐个给bank group换行，然后才能逐个USE
                            if (time_to_refresh <= sc_time(tUSE, SC_NS))
                            {
                                continue; // Wait for refresh
                            }
                            // Check FAW constraint
                            if (check_faw_constraint(i, current_time))
                            {
                                send_command(i, DRAMcommand::CHANGE_ROW);
                            }
                        }
                        else if (record_list[i].get_cmd() == DRAMcommand::REFRESHpb && elapsed >= sc_time(tREFpb, SC_NS))
                        {
                            // 刷新恢复后逐个给bank group换行，然后才能逐个USE
                            if (time_to_refresh <= sc_time(tUSE, SC_NS))
                            {
                                continue; // Wait for refresh
                            }
                            // Check FAW constraint
                            if (check_faw_constraint(i, current_time))
                            {
                                send_command(i, DRAMcommand::CHANGE_ROW);
                            }
                        }
                    }

                    // Handle refresh
                    if (!nsim::all_bank)
                    {
                        // If refresh time for this Bank group
                        if (current_time >= next_refresh_time && check_faw_constraint(this_refpb, current_time))
                        {
                            // Refresh for a specific Bank group (REFRESHpb)
                            send_command(this_refpb, DRAMcommand::REFRESHpb);
                            if (this_refpb < 3)
                                this_refpb++;
                            else
                                this_refpb = 0;
                            // Update next refresh time for this Bank group
                            next_refresh_time += sc_time(tREFIpb, SC_NS);
                        }
                    }
                    else
                    {
                        if (current_time >= next_refresh_time)
                        {
                            for (int i = 0; i < 4; ++i)
                            {
                                send_command(i, DRAMcommand::REFRESH);
                            }
                            // wait(tREFab, SC_NS);
                            next_refresh_time += sc_time(tREFI, SC_NS);
                        }
                    }
                    wait(tCK, SC_NS); // Minimum scheduling granularity}
                }
            }
        }

        void calculate_bandwidth()
        {
            while (true)
            {
                wait(1000, SC_NS); // Update every 1us
                float total_use_rate = 0;
                for (auto bank : bank_groups)
                {
                    total_use_rate += bank->get_use_rate();
                }
                bandwidth_utilization = (total_use_rate / 4.0) * 100; // 16 banks total
                std::cout << "Bandwidth Utilization: " << bandwidth_utilization << "% at " << sc_time_stamp() << std::endl;
            }
        }
        void trace()
        {
            for (int i = 0; i < 4; ++i)
            {
                bank_groups[i]->trace();
            }
        }
        float get_bandwidth_utilization()
        {
            return bandwidth_utilization;
        }
        void reset()
        {
            for (int i = 0; i < 4; ++i)
            {
                bank_groups[i]->reset();
            }
            if (all_bank)
                next_refresh_time = sc_time_stamp() + sc_time(tREFI - tREFab, SC_NS);
            else
                next_refresh_time = sc_time_stamp() + sc_time(tREFIpb - tREFpb, SC_NS);

            // 修改record_list和last_cmd_time
            for (int i = 0; i < 4; ++i)
            {
                record_list[i] = record(DRAMcommand::CHANGE_ROW, sc_time_stamp());
                last_cmd_time[i] = sc_time_stamp();
            }
            // 打印新的参数
            std::cout << "tFAW=" << nsim::tFAW << std::endl;
            std::cout << "tCHANGE_ROW=" << nsim::tCHANGE_ROW << std::endl;
            std::cout << "tREFab=" << nsim::tREFab << std::endl;
            std::cout << "tREFpb=" << nsim::tREFpb << std::endl;
            std::cout << "tUSE=" << nsim::tUSE << std::endl;
            std::cout << "all_bank=" << nsim::all_bank << std::endl;
            std::cout << "per2bank=" << nsim::per2bank << std::endl;
        }

    private:
        vector<Bank *> bank_groups;
        bool all_bank; // All-bank refresh mode
        bool per2bank; // 2 bank groups covering each other
        int last_change_row;
        int num_change_row;
        vector<record> record_list;    // Command history per bank group
        vector<sc_time> last_cmd_time; // Last command time per bank group
        int this_refpb = 0;
        sc_time next_refresh_time;       // Time for next refresh
        float bandwidth_utilization = 0; // Percentage

        void send_command(int group_id, DRAMcommand cmd)
        {
            tlm_generic_payload payload;
            Command *command = new Command(cmd);
            payload.set_extension(command);
            sc_time delay = SC_ZERO_TIME;
            bank_groups[group_id]->b_transport(payload, delay);
            record_list[group_id] = record(cmd, sc_time_stamp());
            last_cmd_time[group_id] = sc_time_stamp();
            payload.release_extension(command);
        }

        bool check_faw_constraint(int group_id, sc_time current_time)
        {
            for (int i = 0; i < 4; ++i)
            {
                if (i == group_id)
                    continue;
                sc_time last = last_cmd_time[i];
                if (record_list[i].get_cmd() == DRAMcommand::CHANGE_ROW || record_list[i].get_cmd() == DRAMcommand::REFRESH || record_list[i].get_cmd() == DRAMcommand::REFRESHpb)
                {
                    if (current_time - last < sc_time(tFAW, SC_NS))
                    {
                        return false;
                    }
                }
            }
            return true;
        }
    };
}
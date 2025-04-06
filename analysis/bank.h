#pragma once
#include <systemc>
#include <tlm>
#include "tlm_utils/simple_target_socket.h"
#include "util.h"
#include "trace.h"

using namespace sc_core;
using namespace tlm;
using namespace std;

namespace nsim
{
    enum class State
    {
        WORKING,
        REFRESHING,
        CHANGING_ROW,
    };

    class Bank : public sc_module
    {
    public:
        SC_HAS_PROCESS(Bank);
        tlm_utils::simple_target_socket<Bank> target_socket;
        sc_signal<int> state_signal;
        sc_signal<bool> waiting;
        sc_signal<bool> refreshing;

        Bank(const sc_module_name &name, int id)
            : sc_module(name), group_id(id)
        {
            target_socket.register_b_transport(this, &Bank::b_transport);
            state_signal.write(static_cast<int>(state));
            SC_THREAD(count_use_rate);
        }

        void b_transport(tlm_generic_payload &payload, sc_time &delay)
        {
            Command *cmd;
            payload.get_extension(cmd);
            if (cmd)
            {
                DRAMcommand command = cmd->get_command();
                if (command == DRAMcommand::USE && state != State::WORKING)
                {
                    state = State::WORKING;
                    state_signal.write(static_cast<int>(state));
                }
                else if (command == DRAMcommand::CHANGE_ROW && state != State::CHANGING_ROW)
                {
                    state = State::CHANGING_ROW;
                    state_signal.write(static_cast<int>(state));
                }
                else if ((command == DRAMcommand::REFRESH || command == DRAMcommand::REFRESHpb) && state != State::REFRESHING)
                {
                    state = State::REFRESHING;
                    state_signal.write(static_cast<int>(state));
                }
                else
                {
                    SC_REPORT_ERROR("Bank", "Invalid state transition");
                }
            }
            else
            {
                SC_REPORT_ERROR("Bank", "No command extension found");
            }
        }

        void count_use_rate()
        {
            while (true)
            {
                wait(tCK, SC_NS); // Update every clock cycle
                cycle_elapsed++;
                if (state == State::WORKING)
                {
                    refreshing.write(false);
                    if (use_count < 32)
                    {
                        waiting.write(false);
                        use_cycle += 1;
                        use_count++;
                    }
                    else
                        waiting.write(true);
                }
                else
                {
                    waiting.write(false);
                    use_count = 0;
                    if (state == State::REFRESHING)
                    {
                        refreshing.write(true);
                    }
                    else
                    {
                        refreshing.write(false);
                    }
                }
                if (cycle_elapsed > 0)
                {
                    use_rate = static_cast<float>(use_cycle) / cycle_elapsed;
                }
                else
                    use_rate = 0;
            }
        }

        float get_use_rate()
        {
            return use_rate;
        }

        void trace()
        {
            if (g_trace)
            { // 检查 g_trace 是否已初始化
                // trace State
                sc_core::sc_trace(g_trace, state_signal, std::string(this->name()) + ".state");
                // trace waiting
                sc_core::sc_trace(g_trace, waiting, std::string(this->name()) + ".waiting");
                // trace refresh
                sc_core::sc_trace(g_trace, refreshing, std::string(this->name()) + ".refreshing");
            }
        }
        void reset()
        {
            use_cycle = 0;
            use_rate = 0;
            use_count = 0;
            state = State::CHANGING_ROW;
            state_signal.write(static_cast<int>(state));
            waiting.write(false);
            refreshing.write(false);
            cycle_elapsed = 0;
            // SC_REPORT_INFO("Bank", "Bank reset");
        }

    private:
        State state = State::CHANGING_ROW;
        int group_id;
        int use_cycle = 0;
        float use_rate = 0;
        int use_count = 0;
        int cycle_elapsed = 0;
    };
}
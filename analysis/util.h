#pragma once
#include <systemc>
#include <tlm>
#include <iostream>
#include <vector>
#include <string>

using namespace sc_core;
using namespace tlm;
using namespace std;

namespace nsim
{
    float tCK = 2.5;        // Clock cycle: 2.5ns
    int tCHANGE_ROW;        // Row change time: 60ns
    int tREFI = 3904;       // Refresh interval: 3904ns
    int tREFIpb = 3904 / 4; // Refresh interval: 976ns
    int tREFab;             // All-bank refresh time: 180ns
    int tREFpb;             // All-bank refresh time: 60ns
    int tUSE = 80;          // Row access duration: 80ns
    int tFAW;               // Four-activation window: 40ns
    bool all_bank;
    bool per2bank;
    enum DRAMcommand
    {
        USE,        // Access data in current row
        CHANGE_ROW, // Switch to a new row
        REFRESH,    // Refresh all banks in group
        REFRESHpb,
    };

    class Command : public tlm_extension<Command>
    {
    public:
        Command() : command(DRAMcommand::USE) {}
        Command(DRAMcommand cmd) : command(cmd) {}

        virtual tlm_extension_base *clone() const override
        {
            return new Command(*this);
        }
        virtual void copy_from(tlm_extension_base const &ext) override
        {
            const Command &cmd = dynamic_cast<Command const &>(ext);
            command = cmd.command;
        }

        DRAMcommand get_command() const
        {
            return command;
        }

    private:
        DRAMcommand command;
    };

    class record
    {
    public:
        DRAMcommand cmd;
        sc_time time;

        record(DRAMcommand cmd, sc_time time) : cmd(cmd), time(time) {}

        void print() const
        {
            std::cout << "Command: " << static_cast<int>(cmd) << ", Time: " << time << std::endl;
        }

        DRAMcommand get_cmd() const { return cmd; }
        sc_time get_time() const { return time; }
        void set_cmd(DRAMcommand cmd) { this->cmd = cmd; }
        void set_time(sc_time time) { this->time = time; }
    };
}
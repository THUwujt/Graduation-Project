#pragma once

#include <systemc.h>
#include "control.h"
#include "data_buffer.h"
#include "shift_reg.h"
#include "systolic_array.h"
#include "result_buffer.h"
#include "dram.h"
#include "MUX.h"
#include "col_shift_reg.h"

namespace model
{

    // 定义Top模块，将所有子模块连接起来
    class Top : public sc_module
    {
    public:
        // 子模块实例化
        Control *control;
        DataBuffer *data_buffer;
        ShiftReg *shift_reg;
        SystolicArray *systolic_array;
        ResultBuffer *result_buffer;

        Bank *bank[16];              // 16个Bank
        MUX *dram_mux[8];            // 每2个bank共用一个mux，MUX的输出接到DRAM_shift_reg
        ShiftReg *DRAM_shift_reg[8]; // 每2个bank共用一个MUX
        ColumnShiftReg *col_shift_reg[8];

        // 中间桥接信号
        sc_signal<bool> send_signal;               // 控制信号 (Control -> DataBuffer)
        sc_signal<int> data_signal[32];            // DataBuffer -> ShiftReg
        sc_signal<bool> valid_signal;              // DataBuffer -> ShiftReg
        sc_signal<int> systolic_data_signal[32];   // ShiftReg -> SystolicArray
        sc_signal<bool> systolic_valid_signal[32]; // ShiftReg -> SystolicArray
        sc_signal<int> result_data_signal[32];     // SystolicArray -> ResultBuffer
        sc_signal<bool> result_valid_signal[32];   // SystolicArray -> ResultBuffer

        sc_signal<int> dram_cmd_in[16];           // 从control发送给16个bank
        sc_signal<int> dram_addr_in[16];          // 从control发送给16个bank
        sc_signal<int> dram_target_column_in[16]; // 从control发送给16个bank
        sc_signal<bool> dram_valid_in[16];        // 从control发送给16个bank
        sc_signal<int> selected_column[8];        // MUX的输出，shift reg的输入

        sc_signal<int> dram_data_out[16][32];      // todo dram bank的输出，MUX的输入
        sc_signal<bool> dram_valid_out[16];        // todo加一个mux选择进入shift reg的数据
        sc_signal<int> dram_target_column_out[16]; // 从control发送给16个bank

        sc_signal<int> dram_data_shift[8][32]; // MUX的输出，DRAM_shift reg的输入
        sc_signal<bool> dram_valid_shift[8];

        // shift reg的输出，systolic array的weight输入
        sc_signal<int> weight_in_systolic[8][32];     // DRAM_shift_reg -> SystolicArray
        sc_signal<bool> weight_valid_systolic[8][32]; // DRAM_shift_reg -> SystolicArray
        sc_signal<int> weight_column_systolic[8][32]; // col_shift_reg -> SystolicArray

        sc_signal<bool> random_trace_signal; //**机动追踪，想看哪个信号都行

        // 构造函数
        SC_CTOR(Top)
        {
            // 实例化各个子模块
            control = new Control("control");
            data_buffer = new DataBuffer("data_buffer");
            shift_reg = new ShiftReg("shift_reg");
            systolic_array = new SystolicArray("systolic_array");
            result_buffer = new ResultBuffer("result_buffer");

            // 实例化8个dram_mux和shift_reg
            // bank0和1共用dram_mux0和shift_reg0，bank2和3共用dram_mux1和shift_reg1，以此类推
            for (int i = 0; i < 8; ++i)
            {
                dram_mux[i] = new MUX(("dram_mux" + to_string(i)).c_str(), i);
                DRAM_shift_reg[i] = new ShiftReg(("dram_shift_reg" + to_string(i)).c_str());
                col_shift_reg[i] = new ColumnShiftReg(("col_shift_reg" + to_string(i)).c_str());
            }

            for (int i = 0; i < 16; ++i)
            {
                bank[i] = new Bank(("bank" + to_string(i)).c_str(), i);
                // 连接Control和Bank
                control->dram_cmd[i].bind(dram_cmd_in[i]);
                control->dram_addr[i].bind(dram_addr_in[i]);
                control->dram_target_column[i].bind(dram_target_column_in[i]);
                control->dram_valid[i].bind(dram_valid_in[i]);
                bank[i]->cmd_in.bind(dram_cmd_in[i]);
                bank[i]->addr_in.bind(dram_addr_in[i]);
                bank[i]->target_column_in.bind(dram_target_column_in[i]);
                bank[i]->valid_in.bind(dram_valid_in[i]);
                bank[i]->valid_out.bind(dram_valid_out[i]);
                bank[i]->target_column_out.bind(dram_target_column_out[i]);
                // 连接Bank输出暂时悬空
                for (int j = 0; j < 32; j++)
                {
                    bank[i]->data_out[j].bind(dram_data_out[i][j]);
                }
            }

            // 连接dram_mux和bank，连接mux和shift_reg
            for (int i = 0; i < 8; ++i)
            {
                // bank[i * 2]和bank[i * 2 + 1]共用dram_mux[i]
                dram_mux[i]->valid_in[0].bind(dram_valid_out[i * 2]);
                dram_mux[i]->valid_in[1].bind(dram_valid_out[i * 2 + 1]);
                dram_mux[i]->target_column_in[0].bind(dram_target_column_out[i * 2]);
                dram_mux[i]->target_column_in[1].bind(dram_target_column_out[i * 2 + 1]);
                dram_mux[i]->valid_out.bind(dram_valid_shift[i]);
                dram_mux[i]->target_column_out.bind(selected_column[i]);

                for (int j = 0; j < 32; j++)
                {
                    dram_mux[i]->data_in[0][j].bind(dram_data_out[i * 2][j]);
                    dram_mux[i]->data_in[1][j].bind(dram_data_out[i * 2 + 1][j]);
                    dram_mux[i]->data_out[j].bind(dram_data_shift[i][j]);
                }
                // 连接dram_shift_reg和shift_reg
                DRAM_shift_reg[i]->valid_in.bind(dram_valid_shift[i]);
                for (int j = 0; j < 32; j++)
                {
                    DRAM_shift_reg[i]->data_in[j].bind(dram_data_shift[i][j]);
                    DRAM_shift_reg[i]->data_out[j].bind(weight_in_systolic[i][j]);
                    DRAM_shift_reg[i]->valid_out[j].bind(weight_valid_systolic[i][j]);
                }
                // 连接column_shift_reg和weight_column
                col_shift_reg[i]->column_in.bind(selected_column[i]);
                for (int j = 0; j < 32; j++)
                {
                    col_shift_reg[i]->column_out[j].bind(weight_column_systolic[i][j]);
                }

                // 连接shift_reg和systolic_array weight 输入
                for (int j = 0; j < 32; j++)
                {
                    systolic_array->weight_in[i][j].bind(weight_in_systolic[i][j]);
                    systolic_array->weight_valid[i][j].bind(weight_valid_systolic[i][j]);
                    systolic_array->weight_column[i][j].bind(weight_column_systolic[i][j]);
                }
            }

            // 连接模块端口
            // 连接 Control 和 DataBuffer
            control->send.bind(send_signal);
            data_buffer->send.bind(send_signal);
            data_buffer->valid_out.bind(valid_signal);
            shift_reg->valid_in.bind(valid_signal);
            SC_THREAD(run);

            for (int i = 0; i < 32; i++)
            {
                data_buffer->data_out[i].bind(data_signal[i]);
                shift_reg->data_in[i].bind(data_signal[i]);

                shift_reg->data_out[i].bind(systolic_data_signal[i]);
                shift_reg->valid_out[i].bind(systolic_valid_signal[i]);

                systolic_array->data_in[i].bind(systolic_data_signal[i]);
                systolic_array->valid_in[i].bind(systolic_valid_signal[i]);

                systolic_array->data_out[i].bind(result_data_signal[i]);
                systolic_array->valid_out[i].bind(result_valid_signal[i]);

                result_buffer->data_in[i].bind(result_data_signal[i]);
                result_buffer->valid_in[i].bind(result_valid_signal[i]);
            }
        }

        void run()
        {
            while (true)
            {
                random_trace_signal.write(systolic_valid_signal[0].read());
                wait(tSYS_CK, SC_PS);
            }
        }

        // get the result in result buffers
        vector<int> get_result(int index)
        {
            return result_buffer->get_result(index);
        }

        float calculate_utility_rate()
        {
            float average_utility_rate = 0.0;
            for (int i = 0; i < 16; ++i)
            {
                average_utility_rate += bank[i]->get_utility_rate();
                // 输出每个bank的利用率
                cout << "Bank " << i << " Utility Rate: " << bank[i]->get_utility_rate() << endl;
            }
            average_utility_rate /= 16.0;
            return average_utility_rate;
        }
        void trace()
        {
            if (g_trace)
            {
                control->trace();
                data_buffer->trace();
                shift_reg->trace();
                systolic_array->trace();
                result_buffer->trace();
                for (int i = 0; i < 8; i++)
                {
                    dram_mux[i]->trace();
                }
                for (int i = 0; i < 16; ++i)
                {
                    bank[i]->trace();
                }

                // 追踪random signal
                sc_core::sc_trace(g_trace, random_trace_signal, "random_trace_signal");
            }
        }
    };

} // namespace model

/* 046267 Computer Architecture - Winter 20/21 - HW #4 */

#include "core_api.h"
#include "sim_api.h"

#include <stdio.h>

int Cycles_Blocked = 0;    //calculate the number of cycles
int Cycles_Finegrained = 0;
int Insts_Blocked = 0;
int Insts_Finegrained = 0;

typedef struct {
    uint32_t ip;        //inst pointer
    bool halt;
    int delay_counter;  //for store and load inst
    int reg_file[REGS_COUNT];

} thread_info;

thread_info* thread_list;


//----------------------------------------------------------------------------------------------------------------------
//check if all the threads finished running
bool all_threads_halt(thread_info *thread_list){
    int num_of_threads = SIM_GetThreadsNum();
    for(int i=0;i<num_of_threads;i++){
        if(!(thread_list[i].halt)) {
            return false;
        }
    }
    return true;
}

void delay_dec(thread_info *thread_list){
    int num_of_threads = SIM_GetThreadsNum();
    for(int i=0;i<num_of_threads;i++){
        if((thread_list[i].delay_counter)>0) {
            thread_list[i].delay_counter--;
        }
    }
}

//return the next valid thread, if we return the cur_thread than all the thread have delay
int next_thread_BlockedMT(thread_info *thread_list,int cur_thread){
    int num_of_threads = SIM_GetThreadsNum();
    int i = (cur_thread)%num_of_threads;
    if(thread_list[i].halt == false && thread_list[i].delay_counter == 0){
        return i;
    }
    i=(i+1)%num_of_threads;
    while(i != cur_thread){
        if(thread_list[i].halt == false && thread_list[i].delay_counter == 0){
            Cycles_Blocked=Cycles_Blocked+SIM_GetSwitchCycles();
            for(int j=0;j<SIM_GetSwitchCycles();j++){
                delay_dec(thread_list);
            }
            return i;
        }
        i=(i+1)%num_of_threads;

    }
    return cur_thread;  //-1 mean idle for all threads

}

int next_thread_Finegrained(thread_info *thread_list,int cur_thread){
    int num_of_threads = SIM_GetThreadsNum();
    int i = (cur_thread+1)%num_of_threads;
    while(i != cur_thread){
        if(thread_list[i].halt == false && thread_list[i].delay_counter == 0){
            return i;
        }
        i=(i+1)%num_of_threads;

    }
    return cur_thread;  //-1 mean idle for all threads
}


//----------------------------------------------------------------------------------------------------------------------

//======================================================================================================================
void CORE_BlockedMT() {
    Insts_Blocked = 0;
    Cycles_Blocked = 0;
    thread_list = (thread_info*)malloc(SIM_GetThreadsNum() * sizeof(thread_info));
    for(int i=0;i<SIM_GetThreadsNum();i++){     //init the threads
        thread_info new_entry;
        new_entry.halt = false;
        new_entry.delay_counter = 0;
        new_entry.ip= 0;
        for(int j=0; j<REGS_COUNT;j++){         //Reg file init(to zeros)
            new_entry.reg_file[j] = 0;
        }
        thread_list[i] = new_entry;
    }
    //=-----------------------------------------------------------------------------------------------------------------
    int current_thread = 0;
    bool idle_flag = false;
    while(!all_threads_halt(thread_list)){
        //--------------------------------------------------------------------------------------------------------------
        if(!idle_flag) {
            Instruction cur_inst;
            SIM_MemInstRead(thread_list[current_thread].ip, &cur_inst, current_thread);
            int val;
            uint32_t address;
            switch (cur_inst.opcode) {
                case (CMD_NOP):
                    Insts_Blocked++;
                    thread_list[current_thread].ip = thread_list[current_thread].ip + 1;
                    break;

                case (CMD_ADD):
                    Insts_Blocked++;
                    thread_list[current_thread].reg_file[cur_inst.dst_index] =
                            thread_list[current_thread].reg_file[cur_inst.src1_index] +
                            thread_list[current_thread].reg_file[cur_inst.src2_index_imm];
                    thread_list[current_thread].ip = thread_list[current_thread].ip + 1;
                    break;

                case (CMD_SUB):
                    Insts_Blocked++;
                    thread_list[current_thread].reg_file[cur_inst.dst_index] =
                            thread_list[current_thread].reg_file[cur_inst.src1_index] -
                            thread_list[current_thread].reg_file[cur_inst.src2_index_imm];
                    thread_list[current_thread].ip = thread_list[current_thread].ip + 1;
                    break;

                case (CMD_ADDI):
                    Insts_Blocked++;
                    thread_list[current_thread].reg_file[cur_inst.dst_index] =
                            thread_list[current_thread].reg_file[cur_inst.src1_index] + cur_inst.src2_index_imm;
                    thread_list[current_thread].ip = thread_list[current_thread].ip + 1;
                    break;

                case (CMD_SUBI):
                    Insts_Blocked++;
                    thread_list[current_thread].reg_file[cur_inst.dst_index] =
                            thread_list[current_thread].reg_file[cur_inst.src1_index] - cur_inst.src2_index_imm;
                    thread_list[current_thread].ip = thread_list[current_thread].ip + 1;
                    break;

                case (CMD_LOAD):
                    Insts_Blocked++;
                    thread_list[current_thread].delay_counter = SIM_GetLoadLat() + 1;
                    if (cur_inst.isSrc2Imm) {
                        address = thread_list[current_thread].reg_file[cur_inst.dst_index] + cur_inst.src2_index_imm;
                    } else {
                        address = thread_list[current_thread].reg_file[cur_inst.dst_index] +
                                  thread_list[current_thread].reg_file[cur_inst.src2_index_imm];
                    }
                    SIM_MemDataRead(address, &val);
                    thread_list[current_thread].reg_file[cur_inst.dst_index] = val;
                    thread_list[current_thread].ip = thread_list[current_thread].ip + 1;
                    break;

                case (CMD_STORE):
                    Insts_Blocked++;
                    thread_list[current_thread].delay_counter = SIM_GetStoreLat() + 1;
                    val = thread_list[current_thread].reg_file[cur_inst.src1_index];
                    if (cur_inst.isSrc2Imm) {
                        address = thread_list[current_thread].reg_file[cur_inst.dst_index] + cur_inst.src2_index_imm;
                    } else {
                        address = thread_list[current_thread].reg_file[cur_inst.dst_index] +
                                  thread_list[current_thread].reg_file[cur_inst.src2_index_imm];
                    }
                    SIM_MemDataWrite(address, val);
                    thread_list[current_thread].ip = thread_list[current_thread].ip + 1;
                    break;

                case (CMD_HALT):
                    Insts_Blocked++;
                    thread_list[current_thread].halt = true;
                    thread_list[current_thread].ip = thread_list[current_thread].ip + 1;
                    //printf("%d", current_thread);
                    break;
            }
            //printf("\n");
        }

        //--------------------------------------------------------------------------------------------------------------
        Cycles_Blocked++;
        delay_dec(thread_list);

        //printf("cycle %d thread: %d thread_delay: %d opcode: %d \n",Cycles,current_thread,thread_list[current_thread].delay_counter);

        int next_thread = next_thread_BlockedMT(thread_list,current_thread);
        idle_flag = false;
        if((current_thread == next_thread && thread_list[current_thread].delay_counter>0) || (current_thread == next_thread && thread_list[current_thread].halt)){
            idle_flag = true;
        }
        current_thread=next_thread;
    }

}

void CORE_FinegrainedMT() {
    Insts_Finegrained = 0;
    Cycles_Finegrained = 0;
    thread_list = (thread_info*)malloc(SIM_GetThreadsNum() * sizeof(thread_info));
    for(int i=0;i<SIM_GetThreadsNum();i++){     //init the threads
        thread_info new_entry;
        new_entry.halt = false;
        new_entry.delay_counter = 0;
        new_entry.ip= 0;
        for(int j=0; j<REGS_COUNT;j++){         //Reg file init(to zeros)
            new_entry.reg_file[j] = 0;
        }
        thread_list[i] = new_entry;
    }
    //=-----------------------------------------------------------------------------------------------------------------
    int current_thread = 0;
    bool idle_flag = false;
    while(!all_threads_halt(thread_list)){
        //--------------------------------------------------------------------------------------------------------------
        //printf(" thread: %d thread_delay: %d\n",current_thread,thread_list[current_thread].delay_counter);
        if(!idle_flag) {
            Instruction cur_inst;
            SIM_MemInstRead(thread_list[current_thread].ip, &cur_inst, current_thread);
            int val;
            uint32_t address;
            switch (cur_inst.opcode) {
                case (CMD_NOP):
                    Insts_Finegrained++;
                    thread_list[current_thread].ip = thread_list[current_thread].ip + 1;
                    break;

                case (CMD_ADD):
                    Insts_Finegrained++;
                    thread_list[current_thread].reg_file[cur_inst.dst_index] =
                            thread_list[current_thread].reg_file[cur_inst.src1_index] +
                            thread_list[current_thread].reg_file[cur_inst.src2_index_imm];
                    thread_list[current_thread].ip = thread_list[current_thread].ip + 1;
                    break;

                case (CMD_SUB):
                    Insts_Finegrained++;
                    thread_list[current_thread].reg_file[cur_inst.dst_index] =
                            thread_list[current_thread].reg_file[cur_inst.src1_index] -
                            thread_list[current_thread].reg_file[cur_inst.src2_index_imm];
                    thread_list[current_thread].ip = thread_list[current_thread].ip + 1;
                    break;

                case (CMD_ADDI):
                    Insts_Finegrained++;
                    thread_list[current_thread].reg_file[cur_inst.dst_index] =
                            thread_list[current_thread].reg_file[cur_inst.src1_index] + cur_inst.src2_index_imm;
                    thread_list[current_thread].ip = thread_list[current_thread].ip + 1;
                    break;

                case (CMD_SUBI):
                    Insts_Finegrained++;
                    thread_list[current_thread].reg_file[cur_inst.dst_index] =
                            thread_list[current_thread].reg_file[cur_inst.src1_index] - cur_inst.src2_index_imm;
                    thread_list[current_thread].ip = thread_list[current_thread].ip + 1;
                    break;

                case (CMD_LOAD):
                    Insts_Finegrained++;
                    thread_list[current_thread].delay_counter = SIM_GetLoadLat() + 1;
                    if (cur_inst.isSrc2Imm) {
                        address = thread_list[current_thread].reg_file[cur_inst.dst_index] + cur_inst.src2_index_imm;
                    } else {
                        address = thread_list[current_thread].reg_file[cur_inst.dst_index] +
                                  thread_list[current_thread].reg_file[cur_inst.src2_index_imm];
                    }
                    SIM_MemDataRead(address, &val);
                    thread_list[current_thread].reg_file[cur_inst.dst_index] = val;
                    thread_list[current_thread].ip = thread_list[current_thread].ip + 1;
                    break;

                case (CMD_STORE):
                    Insts_Finegrained++;
                    thread_list[current_thread].delay_counter = SIM_GetStoreLat() + 1;
                    val = thread_list[current_thread].reg_file[cur_inst.src1_index];
                    if (cur_inst.isSrc2Imm) {
                        address = thread_list[current_thread].reg_file[cur_inst.dst_index] + cur_inst.src2_index_imm;
                    } else {
                        address = thread_list[current_thread].reg_file[cur_inst.dst_index] +
                                  thread_list[current_thread].reg_file[cur_inst.src2_index_imm];
                    }
                    SIM_MemDataWrite(address, val);
                    thread_list[current_thread].ip = thread_list[current_thread].ip + 1;
                    break;

                case (CMD_HALT):
                    Insts_Finegrained++;
                    thread_list[current_thread].halt = true;
                    thread_list[current_thread].ip = thread_list[current_thread].ip + 1;
                    //printf("%d", current_thread);
                    break;
            }
            //printf("\n");
        }
        //printf("cycle %d thread: %d thread_delay: %d opcode: %d \n",Cycles,current_thread,thread_list[current_thread].delay_counter);

        //--------------------------------------------------------------------------------------------------------------
        Cycles_Finegrained++;
        delay_dec(thread_list);

        //printf("cycle %d thread: %d thread_delay: %d opcode: %d \n",Cycles,current_thread,thread_list[current_thread].delay_counter);

        int next_thread = next_thread_Finegrained(thread_list,current_thread);
        idle_flag = false;
        if((current_thread == next_thread && thread_list[current_thread].delay_counter>0) || (current_thread == next_thread && thread_list[current_thread].halt)){
            idle_flag = true;
        }
        current_thread=next_thread;
    }
}

double CORE_BlockedMT_CPI(){
    if(Insts_Blocked != 0) {
        return (double)Cycles_Blocked/(double)Insts_Blocked;
    }
	return 0;
}

double CORE_FinegrainedMT_CPI(){
    if(Insts_Finegrained != 0) {
        return (double)Cycles_Finegrained/(double)Insts_Finegrained;
    }
    return 0;
}

void CORE_BlockedMT_CTX(tcontext* context, int threadid) {
    for(int i=0;i<REGS_COUNT;i++){
        context[threadid].reg[i] = thread_list[threadid].reg_file[i];
    }
}

void CORE_FinegrainedMT_CTX(tcontext* context, int threadid) {
    for(int i=0;i<REGS_COUNT;i++){
        context[threadid].reg[i] = thread_list[threadid].reg_file[i];
    }
}

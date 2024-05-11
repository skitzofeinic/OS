#ifndef SCHEDULER_PCB_H
#define SCHEDULER_PCB_H

#define N_IO_DEVICES (3)

struct student_pcb;
typedef struct student_pcb student_pcb;

typedef enum { INIT_STATE, READY_STATE, IO_STATE, DEFUNCT_STATE } proc_state;

typedef struct sim_pcb {
    double cpu_need, io_need[N_IO_DEVICES];
    double cpu_used, io_used[N_IO_DEVICES];
    double cpu_burst, io_burst[N_IO_DEVICES];
    double t_create, t_mem_alloc, t_cpu, t_io, t_end;
    struct sim_pcb *prev, *next, *prev_queue, *next_queue;
    student_pcb *stud_pcb, *in_queue;
    long mem_need, mem_base, proc_num, io_queue, io_cycles;
    proc_state state;
} sim_pcb;

#endif // SCHEDULER_PCB_H

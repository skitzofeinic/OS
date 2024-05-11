#include <stdio.h>
#include <stdlib.h>

#include "schedule.h"
#include "mem_alloc.h"

/* This file contains a bare bones skeleton for the scheduler function
   for the second assignment for the OSN course of the 2005 fall
   semester.
   Author:

   G.D. van Albada
   Section Computational Science
   Universiteit van Amsterdam

   Date:
   October 23, 2003
   Modified:
   April 14, 2020
*/

/* This variable will simulate the allocatable memory */

static long memory[MEM_SIZE];

/* The actual CPU scheduler is implemented here */

static void cpu_scheduler() { /* Insert the code for a MLFbQ scheduler here */ }

/* The high-level memory allocation scheduler is implemented here */

static void give_memory() {
    int index;
    student_pcb *proc;

    proc = new_proc;
    if (proc) {
        /* Search for new process(es) that should be given memory.
           Insert search code and criteria here. Remember you may
	   be able to allocate memory to multiple processes, so do not
	   stop early.

           Attempt to allocate memory as follows:
         */
        index = mem_get(proc->mem_need);
        if (index >= 0) {
            /* Allocation succeeded, now put in administration
             */
            proc->mem_base = index;

            /* You might want to move this process to the READY_STATE
              queue now
             */
        }
    }
}

/* Here we reclaim the memory of a process after it
  has finished */

static void reclaim_memory() {
    student_pcb *proc;

    proc = defunct_proc;
    while (proc) {
        /* Free your own administrative structure if it exists
         */
        if (proc->userdata) {
            free(proc->userdata);
        }
        /* Free the simulated allocated memory
         */
        mem_free(proc->mem_base);
        proc->mem_base = -1;

        /* Call the function that cleans up the simulated process
         */
        rm_process(&proc);

        /* See if there are more processes to be removed
         */
        proc = defunct_proc;
    }
}

/* You may want to have the last word... */

static void my_finale() { /* Your very own code goes here */ }

/* The main scheduling routine */

void schedule(event_type event) {
    static int first = 1;

    if (first) {
        mem_init(memory);
        finale = my_finale;
        first = 0;
        /* Add your own initialisation code here
         */
    }

    switch (event) {
    /* You may want to do this differently
     */
    case NEW_PROCESS_EVENT:
        give_memory();
        break;
    case TIME_EVENT:
    case IO_EVENT:
        cpu_scheduler();
        break;
    case READY_EVENT:
        break;
    case FINISH_EVENT:
        reclaim_memory();
        give_memory();
        cpu_scheduler();
        break;
    default:
        printf("I cannot handle event nr. %d\n", event);
        break;
    }
}

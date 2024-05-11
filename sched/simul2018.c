#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <argp.h>
#include <assert.h>
#include <stdbool.h>

#include <inttypes.h>
#include <locale.h>
#include <limits.h>
#include <sys/types.h>
#include <stdarg.h>
#include "schedule.h"
#include "mem_alloc.h"
#include "pcb.h"

/* The procedures in this file together implement a simple discrete-event
   simulator for process scheduling - except of course for the scheduler
   itself.

   In order to judge performance, some statistics must be collected.
   Which should be use-full?

   Per process: wait-time to memory allocation
                          to first service
                execution time
                turn-around time
        Look for average, maximum.
        For this we need per process:
                creation time
                t_mem
                t_run
                t_end
   Per device:  utilization of CPU
                            of memory
                            of IO-channels
        Time-averages only.
*/

student_pcb *new_proc, *ready_proc, *io_proc, *defunct_proc;
function *finale;
function *reset_stats;

#define N_SAMPLES (40960)

#define MAX_ERRORS (150)

#define MEM_MIN (512)

#define N_REQUESTS (4)

// Statistical data about process event times
static float t_mem_alloc[N_SAMPLES];
static float t_first_cpu[N_SAMPLES];
static float t_execution[N_SAMPLES];
static float t_turnaround[N_SAMPLES];

// Whether or not to collect some statistics (set to true by main)
static bool get_stats = false;

// Used to check whether processes haven't "disappeared"
static long num_terminated_processes;

// Used to terminate program when there are too many errors
static long n_errors_detected = 0;

// Number of statistical samples collected (used as array index)
static long n_samples = 0;

// Unique process ID counter
static long proc_num = 0;

// Data about queue lengths
static long current_defunct_queue_len = 0;
static long current_io_queue_len[N_IO_DEVICES] = {0, 0, 0};
static long current_cpu_queue_len = 0;
static long current_new_queue_len = 0;

static long max_defunct_queue_len = 0;
static long max_io_queue_len[N_IO_DEVICES] = {0, 0, 0};
static long max_cpu_queue_len = 0;
static long max_new_queue_len = 0;

static double avg_defunct_queue_len = 0;
static double avg_io_queue_len[N_IO_DEVICES] = {0, 0, 0};
static double avg_cpu_queue_len = 0;
static double avg_new_queue_len = 0;

// Process generation parameters
static double io_time_factor = 1.0; /* added 10-09-2003 */
static double mem_load = 1.0;       /* added 10-09-2003 */

static double mem_in_use = 0;

// Utilisation data
static double mem_util = 0;
static double io_util[N_IO_DEVICES] = {0, 0, 0};
static double cpu_util = 0;

static double eps = 1.0e-12;

// The processes which have control of the IO devices and the CPU
static struct sim_pcb *(current_io_processes[N_IO_DEVICES]) = {NULL, NULL,
                                                               NULL};
static struct sim_pcb *current_cpu_process = NULL;

// These pointers maintain a secondary list of all processes (aside from the one
// accessible to students) in order to check whether all processes are still in
// queues, etc. (See check_all function)
static struct sim_pcb *last = NULL;
static struct sim_pcb *first = NULL;

static double t_simulation_now = 0.0, t_step = 0.0;
static double t_start;
static double t_slice = 9.9e12;

static event_type cur_event;

/***********************************************************************
   The execution of a process is as follows.
   It starts with a CPU burst, next an IO burst on device 0, CPU, IO on device 1
   CPU, IO on device 2, CPU, IO on 0, etc, ending in a CPU burst.
   In this way NC CPU bursts occur, N0 IO burst on device 0, N1 on 1, N2 on 2.
   NC = 1 + N0 + N1 + N2.
   N0 + N1 + N2 is called IO_cycles in this program.
   In the version of the code that stood over from Nov. 98, the calculation
   of the burst lengths was performed in the following manner.
   The remaining CPU time for each was calculated and divided by IO_cycles + 1
   to obtain the CPU burst length.
   IO_cycles was reduced by 3 after an IO_burst on device 2. (But forced to be
at
         least 0).
   The length of an IO_burst was obtained by dividing the remaining IO_time per
        device by IO_cycles.
   The result was a slightly oscilatory behaviour of the CPU burst length with
        an increasing amplitude towards the end of the execution. The IO burst
        lengths increase rather drastically towards the end of the execution.

   The CPU load should be the arrival rate * average CPU need per job
   The IO load should be the arrival rate * average IO time per device per job
   The memory load is more complex: it depends on the resident time (which
        is at least the sum of the CPU and IO needs), the amount of memory
        per job, and the arrival rate, divided by the memory capacity.
        Only a lower bound can easily be given.

   The initialisation of the job parameters needs more care than we used
   originally.
   1. The average length of a CPU burst should be greater than the
      minimum timeslice (which was arbitrarily set to 1).
   2. The job execution times should be drawn from a reasonable random
      distribution.
   3. The length of the CPU burst should be modulated by a random amount.
   4. Same for the length of the IO burst.
   5. CPU load, memory load and IO load should be separately adjustable.

   Let's assume:
   CPU need is drawn from one of four randomly selected uniform
        distributions:
   2 to 6 units (avg 4)
   6 to 18 units (avg 12)
   18 to 54 units (avg 36)
   54 to 162 units (avg 108)

   Grand avg 40 units. (Actually, just draw from a uniform distribution and
   select one of four multipliers). Arrival rate of 0.025 leads to unity
   CPU load.

   IO_cycles - uniform random number from 3 (3) 21 (avg 12, or 4 per device).

   IO_burst[0] 3 always
   IO_burst[1] 1 -- 5 uniform, avg 3.
   IO_burst[2] 4 -- 16 uniform, avg (10),

   i.e. only device 2 can lead to IO-saturation. multiply by a time factor to
   allow scaling w.r.t. CPU load.
   Avg minimum execution time becomes 40 + 16 * 4 * IO_timefactor

   For an IO_timefactor of 1, that just happens to be 104, i.e. 2.6 times
   the CPU time. I.e. in that case, we should have 2.6 jobs in memory,
   on average. Make average memory request MEM_SIZE / 3 (= 10920) words.

**************************************************************************/

static sim_pcb request[N_REQUESTS] = {/* 1 */ {10,
                                               {5, 11, 15},
                                               0,
                                               {0, 0, 0},
                                               3,
                                               {3, 5, 3},
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               NULL,
                                               NULL,
                                               NULL,
                                               NULL,
                                               NULL,
                                               NULL,
                                               4096,
                                               -1,
                                               0,
                                               0,
                                               10,
                                               INIT_STATE},
                                      /* 2 */ {20,
                                               {35, 41, 55},
                                               0,
                                               {0, 0, 0},
                                               3,
                                               {3, 5, 3},
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               NULL,
                                               NULL,
                                               NULL,
                                               NULL,
                                               NULL,
                                               NULL,
                                               1024,
                                               -1,
                                               0,
                                               0,
                                               13,
                                               INIT_STATE},
                                      /* 3 */ {70,
                                               {15, 21, 15},
                                               0,
                                               {0, 0, 0},
                                               3,
                                               {3, 5, 3},
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               NULL,
                                               NULL,
                                               NULL,
                                               NULL,
                                               NULL,
                                               NULL,
                                               2048,
                                               -1,
                                               0,
                                               0,
                                               2,
                                               INIT_STATE},
                                      /* 4 */ {10,
                                               {5, 51, 15},
                                               0,
                                               {0, 0, 0},
                                               3,
                                               {3, 5, 3},
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               NULL,
                                               NULL,
                                               NULL,
                                               NULL,
                                               NULL,
                                               NULL,
                                               8192,
                                               -1,
                                               0,
                                               0,
                                               4,
                                               INIT_STATE}};

static double t_next_new,
    load_factor = 1.0,
    t_delay[N_REQUESTS] = {4, 27, 112,
                           17} /* avg = 40; strongly clustered arrivals */;

double sim_time() { return (t_simulation_now); }

void set_slice(double slice) {
    slice = (slice < 1.0) ? 1.0 : slice;
    t_slice = t_simulation_now + slice;
}


uint64_t PRNG_state = 100; /* The state must be seeded with a nonzero value. */

//Random generator taken from the wiki page of xorshift
uint64_t xorshift64star(void) {
    PRNG_state ^= PRNG_state >> 12; // a
    PRNG_state ^= PRNG_state << 25; // b
    PRNG_state ^= PRNG_state >> 27; // c
    return PRNG_state * UINT64_C(2685821657736338717);
}


//Random helpers
long genrand_int31(void)
{
    uint32_t num = xorshift64star();
    return (num>>1);
}

/* generates a random number on [0,1]-real-interval */
double genrand_real1(void)
{
    double temp =  (xorshift64star()>>32);
    double div = (1.0/((double)UINT32_MAX-1));
    return temp*div;
    /* divided by 2^32-1 */
}

static int mem_range() {
    return ((int)(5 * (mem_load * MEM_SIZE /
                       (1.25 * load_factor + 1.75 * io_time_factor) -
                       MEM_MIN)));
}

static void sluit_af() { printf("Einde programma\n"); }

static void my_init_sim() {}

static void my_reset_stats() {}

static void histogram(float *data, char *text) {
    long N, i, histo[66], j;
    double av = 0, sigma = 0, scaleh, scale, lim;
    float max, min, hi;
    char c;

    N = (n_samples < N_SAMPLES) ? n_samples : N_SAMPLES;

    printf("\nHistogram en statistieken van %s\n", text);
    printf("over de laatste %ld beeindigde processen\n", N);

    if (N < 2) {
        printf("Geen gegevens ...\n");
        return;
    }
    max = min = data[0];

    for (i = 0; i < N; i++) {
        av += data[i];
        sigma += data[i] * data[i];
        min = (data[i] < min) ? data[i] : min;
        max = (data[i] > max) ? data[i] : max;
    }
    av = av / (double)N;
    sigma = sigma / (double)N;
    sigma = sigma - av * av;
    sigma = N * sigma / ((double)N - 1);
    sigma = (sigma > 0) ? sqrt(sigma) : 0.0;

    scaleh = 65.9 / (max - min);

    for (i = 0; i < 65; i++) {
        histo[i] = 0;
    }
    for (i = 0; i < N; i++) {
        j = (data[i] - min) * scaleh;
        histo[j]++;
    }
    hi = 0.0;
    for (i = 0; i < 65; i++) {
        if (histo[i] > hi)
            hi = histo[i];
    }
    /*
       make histogram 18 lines high
     */
    scale = hi / 17.9;
    for (i = 17; i >= 0; i--) {
        lim = scale * i;
        if (0 == (i % 5)) {
            printf("%6.1f |", lim);
        } else {
            printf("       |");
        }
        for (j = 0; j < 65; j++) {
            c = (lim >= histo[j]) ? ' ' : '*';
            putchar(c);
        }
        printf("\n");
    }
    printf("%s\n%s\n  ", "       "
                         "|----|----|----|----|----|----|----|----|----|----|--"
                         "--|----|----|",
           "       |         |         |         |         |         |         "
           "|     ");
    for (i = 0; i < 7; i++) {
        printf("%6.0f    ", min + 10.0 * i / scaleh);
    }
    printf("\n                                           tijdseenheden\n");
    printf("\nGemiddelde waarde: %6.1f, spreiding: %6.2f\n", av, sigma);
    printf("Minimum waarde: %6.1f, maximum waarde: %6.1f\n", min, max);
    printf(
        "-----------------------------------------------------------------\n");
}

void print_statistics() {
    /*
       This routine will print the statistics gathered to this time
     */

    long mem_wait = 0, cpu_wait = 0, io_wait = 0, defunct_wait = 0, i;

    printf("Statistieken op tijdstip = %6.0f\n", t_simulation_now);
    printf("Opnemen statistieken gestart na 100 aangemaakte processen\n");
    printf("\top tijdstip %f\n", t_start);

    printf("Aantal gevolgde processen: %ld, aantal gereed: %ld\n",
           proc_num - 100, n_samples);

    mem_wait = queue_length(&new_proc);
    cpu_wait = queue_length(&ready_proc);
    io_wait = queue_length(&io_proc);
    defunct_wait = queue_length(&defunct_proc);

    printf("Aantal processen wachtend op geheugen: %ld\n", mem_wait);
    printf("Maximum was: %ld, gemiddelde was %f\n", max_new_queue_len,
           avg_new_queue_len / (t_simulation_now - t_start));
    printf("Gemiddeld gebruik geheugen: %6.0f woorden, utilisatie %6.4f\n",
           mem_util / (t_simulation_now - t_start),
           mem_util / ((t_simulation_now - t_start) * MEM_SIZE));
    printf("Aantal in de ready queue:              %ld\n", cpu_wait);
    printf("Maximum was: %ld, gemiddelde was %f\n", max_cpu_queue_len,
           avg_cpu_queue_len / (t_simulation_now - t_start));
    printf("\nGebruikte CPU-tijd: %6.0f, CPU utilisatie: %6.4f\n", cpu_util,
           cpu_util / (t_simulation_now - t_start));
    printf("Aantal in de I/O queue:                %ld\n", io_wait);
    for (i = 0; i < N_IO_DEVICES; i++) {
        printf("Maximum voor kanaal %ld was: %ld, gemiddelde %f\n", i,
               max_io_queue_len[i],
               avg_io_queue_len[i] / (t_simulation_now - t_start));
        printf("Gebruikte tijd op IO-kanaal %ld: %6.0f, utilisatie: %6.4f\n", i,
               io_util[i], io_util[i] / (t_simulation_now - t_start));
    }
    printf("Aantal wachtend op opruimen:           %ld\n", defunct_wait);
    printf("Maximum was: %ld, gemiddelde was %f\n", max_defunct_queue_len,
           avg_defunct_queue_len / (t_simulation_now - t_start));

    if (num_terminated_processes + mem_wait + cpu_wait + io_wait +
            defunct_wait !=
        proc_num) {
        printf("Er klopt iets niet met het totaal aantal processen,\n"
               "is een van de rijen misschien verstoord?\n");
        printf("Geteld: %ld, verwacht: %ld \n",
               num_terminated_processes + mem_wait + cpu_wait + io_wait +
                   defunct_wait,
               proc_num);
    }
    histogram(t_mem_alloc, "wachttijd op geheugentoewijzing");
    histogram(t_first_cpu, "wachttijd op eerste CPU cycle");
    histogram(t_execution, "executie-tijd vanaf geheugentoewijzing");
    histogram(t_turnaround, "totale verwerkingstijd");

    printf("\nEinde statistieken ----------\n\n");
    finale();
}

static void new_process(student_pcb **new_proc) {
    /*
       Select the next new process from the list of creatable processes
     */

    student_pcb *new_student_pcb;
    struct sim_pcb *new_sim_pcb;
    double cpu_factor;
    int i;

    static long next_request = 0;

    /*
       Create and initialize pcb structures
     */

    new_student_pcb = malloc(sizeof(student_pcb));
    new_sim_pcb = malloc(sizeof(struct sim_pcb));
    *new_sim_pcb = request[next_request];

    /* Generate CPU need */
    cpu_factor = 4.0;
    i = genrand_int31() % 32;
    i >>= 3;
    while (i--)
        cpu_factor *= 3.0;
    new_sim_pcb->cpu_need = cpu_factor * (0.5 + genrand_real1());

    new_sim_pcb->io_cycles = 2 * (1 + genrand_int31() % 10);
    new_sim_pcb->cpu_burst =
        (new_sim_pcb->cpu_need) / (1 + new_sim_pcb->io_cycles);
    new_sim_pcb->cpu_burst *= (0.8 + 0.4 * genrand_real1());

    new_student_pcb->sim_pcb = (void *)new_sim_pcb;
    i = 1 + (genrand_int31() % mem_range());
    new_student_pcb->mem_need = new_sim_pcb->mem_need =
        MEM_MIN + (genrand_int31() % i);
    if (new_student_pcb->mem_need > (3 * MEM_SIZE) / 4) {
        new_student_pcb->mem_need = new_sim_pcb->mem_need = (3 * MEM_SIZE) / 4;
    }
    new_student_pcb->mem_base = -1;
    new_sim_pcb->proc_num = proc_num++;
    new_student_pcb->userdata = NULL;
    new_sim_pcb->t_create = t_simulation_now;
    new_sim_pcb->stud_pcb = new_student_pcb;

    new_student_pcb->prev = NULL;
    new_student_pcb->next = NULL;

    /*
       Tie pcb structures into various queues
     */
    queue_append(new_proc, new_student_pcb);

    // TODO: Maybe make function for this
    if (first) {
        last->next = new_sim_pcb;
        new_sim_pcb->prev = last;
        last = new_sim_pcb;
    } else {
        first = last = new_sim_pcb;
        new_sim_pcb->prev = NULL;
    }
    new_sim_pcb->next = NULL;

    /*
       Determine when next new process is to arrive
     */

    next_request = genrand_int31() % N_REQUESTS;
    t_next_new = t_simulation_now + t_delay[next_request] / load_factor;
}

// Checks whether the queues still make sense
static void check_all() {
    sim_pcb *current;
    student_pcb *stud;
    long i;

    current = first;
    while (current) {
        stud = current->stud_pcb;
        if (current->mem_base != stud->mem_base) {
            if (current->mem_base <= 0) {
                /*
                   Memory appears to have been allocated...
                 */
                current->mem_base = stud->mem_base;
                current->t_mem_alloc = t_simulation_now;
                mem_in_use += current->mem_need;
            }
        }
        current->in_queue = NULL;
        current = current->next;
    }
    stud = new_proc;
    current_new_queue_len = queue_length(&new_proc);

    while (stud) {
        current = (sim_pcb *)stud->sim_pcb;
        current->in_queue = new_proc;
        stud = stud->next;
    }

    stud = ready_proc;
    current_cpu_queue_len = 0;
    while (stud) {
        current_cpu_queue_len++;
        current = (sim_pcb *)stud->sim_pcb;
        current->in_queue = ready_proc;
        if (current->state == INIT_STATE) {
            current->state = READY_STATE;
        }
        stud = stud->next;
    }
    stud = io_proc;
    for (i = 0; i < N_IO_DEVICES; i++) {
        current_io_queue_len[i] = 0;
    }
    while (stud) {
        current = (sim_pcb *)stud->sim_pcb;
        current->in_queue = io_proc;
        current_io_queue_len[current->io_queue]++;
        stud = stud->next;
    }
    stud = defunct_proc;
    current_defunct_queue_len = 0;
    while (stud) {
        current_defunct_queue_len++;
        current = (sim_pcb *)stud->sim_pcb;
        current->in_queue = defunct_proc;
    }
    current = first;
    while (current) {
        if (!(current->in_queue)) {
            n_errors_detected++;
            printf("Proces no. %ld bestaat nog, maar zit in geen enkele",
                   current->proc_num);
            printf(" queue\n");
            stud = current->stud_pcb;
            switch (current->state) {
            case INIT_STATE:
                printf("Het betreft een nieuw proces\n");
                queue_prepend(&new_proc, stud);
                break;
            case READY_STATE:
                printf("Het betreft een ready proces\n");
                queue_prepend(&ready_proc, stud);
                break;
            case IO_STATE:
                printf("Het proces is bezig met I/O\n");
                queue_prepend(&io_proc, stud);
                break;
            case DEFUNCT_STATE:
                printf("Het proces is defunct\n");
                queue_prepend(&defunct_proc, stud);
                break;
            default:
                printf("My fault....\n");
                print_statistics();
                exit(0);
            }
            current->in_queue = stud;
        }
        current = current->next;
    }
    stud = ready_proc;
    if (stud) {
        current = (sim_pcb *)stud->sim_pcb;
        current_cpu_process = current;
    } else {
        current_cpu_process = NULL;
    }
}

static void ready_process(student_pcb **ready_proc, student_pcb **io_proc) {

    /*
     * How do we indicate that a process is ready? Its state should be ready
     * - even though the process is still in the I/O queue
     */

    /* Now get all ready processes from the IO queues and move them
       to the end of the ready queue */

    student_pcb *current = *io_proc;
    while (current) {
        sim_pcb *current_sim = (sim_pcb *)current->sim_pcb;

        if (current_sim->state == READY_STATE) {
            current_sim->cpu_burst =
                (current_sim->cpu_need - current_sim->cpu_used) /
                (1 + current_sim->io_cycles);
            current_sim->cpu_burst *= (0.6 + 0.8 * genrand_real1());

            queue_remove(io_proc, current);
            queue_append(ready_proc, current);

            current_sim->in_queue = *ready_proc;
        }
        current = current->next;
    }
}

static void io_process(student_pcb **io_proc, student_pcb **ready_proc) {

    /*
     * How do we indicate that a process wants to do I/O? Simple - the only
     * process that could want to switch is the currently executing process.
     * If this procedure is called, that process wants to switch.
     */
    sim_pcb *current_sim_pcb = current_cpu_process;
    student_pcb *current = current_sim_pcb->stud_pcb;
    current_sim_pcb->state = IO_STATE;

    queue_remove(ready_proc, current);
    queue_append(io_proc, current);

    current_sim_pcb->in_queue = *io_proc;
}

static void finish_process(student_pcb **defunct_proc,
                           student_pcb **ready_proc) {

    /*
     * How do we indicate that a process wants to do quit? Simple - the only
     * process that could want to quit is the currently executing process. If
     * this procedure is called, that process wants to quit.
     */
    sim_pcb *current_sim = current_cpu_process;
    student_pcb *current = current_sim->stud_pcb;
    current_sim->state = DEFUNCT_STATE;

    queue_remove(ready_proc, current);
    queue_prepend(defunct_proc, current);

    current_sim->in_queue = *defunct_proc;
}

static void post_new() {

    /*
     * A new process was created and the student scheduler has been called.
     * The only plausible possible action would be the allocation of memory
     * to this process. Yet, we will first check the consistency of all pcbs
     * etc
     */

    check_all();
}

static void post_time() { check_all(); }

static void do_io() {

    /*
     * We should find out if there are any I/O devices free, and if so, if
     * there are processes waiting for that particular device
     */
    long i;
    sim_pcb *my;
    student_pcb *stud;

    for (i = 0; i < N_IO_DEVICES; i++) {
        /******************************************************************
           IO_burst[0] 3 always
           IO_burst[1] 1 -- 5 uniform, avg 3.
           IO_burst[2] 4 -- 16 uniform, avg (10),

           i.e. only device 2 can lead to IO-saturation. multiply by a time
        factor to
           allow scaling w.r.t. CPU load.
        *******************************************************************/
        if (current_io_processes[i] == NULL) {
            stud = io_proc;
            while (stud) {
                my = (sim_pcb *)stud->sim_pcb;
                if (my->io_queue == i) {
                    current_io_processes[i] = my;
                    my->t_io = t_simulation_now;
                    if (my->io_cycles < 1)
                        my->io_cycles = 1;
                    switch (i) {
                    case 0:
                        my->io_burst[i] = 3.0;
                        break;
                    case 1:
                        my->io_burst[i] = 1.0 + 4.0 * genrand_real1();
                        break;
                    case 2:
                        my->io_burst[i] = 4.0 + 12.0 * genrand_real1();
                        break;
                    }
                    my->io_burst[i] *= io_time_factor;
                    my->io_cycles -= 1;
                    break; /* while (stud) */
                }
                stud = stud->next;
            }
        }
    }
}

static void post_ready() {
    check_all();
    do_io();
}

static void post_io() {
    check_all();
    do_io();
}

static void post_finish() { check_all(); }

static event_type find_next_event() {
    /* The behaviour of the various processes generating events may differ.
       The NewProcess process will run at a fixed time - that is easy at least
       Each of the I/O processes will complete at a predictable moment, they
       will continue processing the associated task until finished.
       The CPU process will always process the task at the head of the
       ready-list (if it exists) until the next event. We will see again
       later */

    double t_next, t_event;
    sim_pcb *my, *next_proc;
    event_type next_event;
    long i;

    next_event = NEW_PROCESS_EVENT;
    t_next = t_next_new;
    next_proc = NULL;

    if (t_slice < t_next) {
        next_event = TIME_EVENT;
        t_next = t_slice;
        next_proc = current_cpu_process;
    }
    my = current_cpu_process;
    if (my) {
        t_event = t_simulation_now + my->cpu_burst;
        if (t_event < t_next) {
            next_proc = my;
            t_next = t_event;
            if (my->cpu_burst + my->cpu_used >= my->cpu_need) {
                next_event = FINISH_EVENT;
            } else {
                next_event = IO_EVENT;
            }
        }
    }
    for (i = 0; i < N_IO_DEVICES; i++) {
        my = current_io_processes[i];
        if (my) {
            t_event = my->io_burst[i] + my->t_io;
            if (t_event < t_next) {
                t_next = t_event;
                next_proc = my;
                next_event = READY_EVENT;
            }
        }
    }

    /*
     * Now we know which event will be next. Whatever the next event will be,
     * we must advance the task on the CPU in order to keep things
     * consistent. Also record some statistics.
     */

    t_step = t_next - t_simulation_now;
    if (current_cpu_process) {
        if (current_cpu_process->cpu_used == 0) {
            current_cpu_process->t_cpu = t_simulation_now;
        }
        if (get_stats) {
            cpu_util += t_step;
        }
        current_cpu_process->cpu_used += t_step;
        current_cpu_process->cpu_burst -= t_step;
        if (current_cpu_process->cpu_burst < eps) {
            current_cpu_process->cpu_burst = 0;
        }
    }

    /*
     * In case of a ready event, we will now mark the I/O device as empty
     * and the associated process as ready. Record some more statistics
     */

    if (get_stats) {
        for (i = 0; i < N_IO_DEVICES; i++) {
            if (current_io_processes[i])
                io_util[i] += t_step;
            if (max_io_queue_len[i] < current_io_queue_len[i])
                max_io_queue_len[i] = current_io_queue_len[i];
            avg_io_queue_len[i] += t_step * current_io_queue_len[i];
        }
        mem_util += t_step * mem_in_use;
        avg_new_queue_len += t_step * current_new_queue_len;
        avg_cpu_queue_len += t_step * current_cpu_queue_len;
        avg_defunct_queue_len += t_step * current_defunct_queue_len;
        if (max_new_queue_len < current_new_queue_len)
            max_new_queue_len = current_new_queue_len;
        if (max_cpu_queue_len < current_cpu_queue_len)
            max_cpu_queue_len = current_cpu_queue_len;
        if (max_defunct_queue_len < current_defunct_queue_len)
            max_defunct_queue_len = current_defunct_queue_len;
    }
    if (next_event == READY_EVENT) {
        i = next_proc->io_queue;
        current_io_processes[i] = NULL;
        next_proc->state = READY_STATE;
        next_proc->io_used[i] += next_proc->io_burst[i];
        next_proc->io_queue = (i + 1) % N_IO_DEVICES;
    }
    t_simulation_now = t_simulation_now + t_step;

    return (next_event);
}

long rm_process(student_pcb **process) {
    sim_pcb *my;
    student_pcb *stud;
    long pn;

    stud = *process;
    my = (sim_pcb *)stud->sim_pcb;
    pn = n_samples % N_SAMPLES;
    n_samples++;
    num_terminated_processes++;
    mem_in_use -= my->mem_need;

    // Collect statistics about this process
    t_mem_alloc[pn] = my->t_mem_alloc - my->t_create;
    t_first_cpu[pn] = my->t_cpu - my->t_create;
    t_execution[pn] = t_simulation_now - my->t_mem_alloc;
    t_turnaround[pn] = t_simulation_now - my->t_create;

    queue_remove(&defunct_proc, stud);
    free(stud);

    // TODO: Maybe make a function for this
    if (my->prev) {
        my->prev->next = my->next;
    } else {
        first = my->next;
    }
    if (my->next) {
        my->next->prev = my->prev;
    } else {
        last = my->prev;
    }

    free(my);

    return (0);
}
// Voor de argument parsing
struct arguments {
    float cpu;
    float io;
    float mem;
    long proc;
    long seed;
};

static int parse_opt(int key, char *arg, struct argp_state *state) {
    struct arguments *arguments = state->input;
    switch (key) {
    case 'c':
        arguments->cpu = strtof(arg, NULL);
        break;
    case 'i':
        arguments->io = strtof(arg, NULL);
        break;
    case 'm':
        arguments->mem = strtof(arg, NULL);
        break;
    case 'p':
        arguments->proc = strtol(arg, NULL, 10);
        break;
    case 's':
        arguments->seed = strtol(arg, NULL, 10);
        break;
    case ARGP_KEY_FINI:
        if (!(((0 < arguments->cpu) && (1.0 > arguments->cpu)) &&
              ((0 < arguments->io) && (1.0 > arguments->io)) &&
              ((0 < arguments->mem) && (1.0 > arguments->mem)) &&
              (arguments->proc > 0))) {
            argp_error(state, "Waardes buiten range\n");
        }
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }

    return 0;
}
int main(int argc, char *argv[]) {
    /* The simulation program had better be implemented as a good, oldfashioned
       discrete-event simulator

       At each event it should decide which of the systemprocessors should
       execute. These processors are:

       JES - the job-entry system
       CPU - the CPU
       IOn - the n identical IO systems.

       Upon completion these system processors will generate one of the
       five possible scheduler events:

       NEW_PROCESS_EVENT
       TIME_EVENT
       READY_EVENT
       IO_EVENT
       FINISH_EVENT

       This event will be passed to the scheduler, after which the
       scheduler can rearrange the process queues.

       Inspection of the process queues will indicate which system-event
       will occur next.

       The problem that we face here is that at every call to the scheduler,
       the current process may have been preempted.

       Preemption of the CPU implies an immediate CPU event, affecting the
       previously current process.

       In order to obtain a nice uniform structure, we will describe
       each system-processor with two functions - the event-executer.
       */

    long N_to_create = 7;
    long ranseed = 1579;

    struct argp_option options[] = {
        {0, 0, 0, 0, "Verplicht:", 1},
        {"cpu", 'c', "FLOAT (0-1)", 0, "CPU belasting", 0},
        {"io", 'i', "FLOAT (0-1)", 0, "IO belasting", 0},
        {"memory", 'm', "FLOAT (0-1)", 0, "geheugenbelasting belasting", 0},
        {"proc", 'p', "INT", 0, "aantal aan te maken processen", 0},
        {0, 0, 0, 0, "Optioneel:", -1},
        {"seed", 's', "INT", 0, "Seed voor de random generator", -1},
        {0, 0, 0, 0, 0, 0}};
    struct argp argp = {options, parse_opt, 0, 0, 0, 0, 0};

    struct arguments arguments;
    arguments.cpu = 0;
    arguments.io = 0;
    arguments.mem = 0;
    arguments.proc = 0;
    printf("Simulatie van geheugen-toewijzing en proces-scheduling\n");
    printf("Versie 2015-2016\n");
    argp_parse(&argp, argc, argv, 0, 0, &arguments);
    printf("CPU: %f\n", arguments.cpu);
    printf("io: %f\n", arguments.io);
    printf("mem: %f\n", arguments.mem);

    load_factor = arguments.cpu;

    io_time_factor = arguments.io;
    io_time_factor /= load_factor;

    mem_load = arguments.mem;

    N_to_create = arguments.proc;
    printf("Gelezen waarde: %ld\n", N_to_create);
    N_to_create = (N_to_create < 5)
                      ? 5
                      : ((N_to_create > N_SAMPLES) ? N_SAMPLES : N_to_create);
    printf("Gebruikte waarde: %ld\n", N_to_create);
    if(arguments.seed == 0){
        PRNG_state = ranseed;
    }else{
        PRNG_state = arguments.seed;
    }
    my_init_sim();

    new_proc = io_proc = ready_proc = defunct_proc = NULL;
    t_start = 0;

    finale = sluit_af;
    reset_stats = my_reset_stats;

    cur_event = NEW_PROCESS_EVENT;

    while (proc_num < 100) {
        switch (cur_event) {
        case NEW_PROCESS_EVENT:

            /*
             * We must create a new process and add it to the new process
             * queue, awaiting the allocation of memory
             */

            new_process(&new_proc);
            schedule(cur_event);

            /*
             * Find out what the student has done, and schedule the next
             * event accordingly. Report any illogical actions or errors
             */

            post_new();
            break;
        case TIME_EVENT:

            /*
             * A process has used up its time-slice. The scheduler can
             * re-arrange the ready queue if it wants. We need not do
             * anything, except reset the timer
             */

            set_slice(9.9e12);
            schedule(cur_event);
            post_time();
            break;
        case READY_EVENT:

            /*
             * A process has finished its I/O action. We move it from the I/O
             * queue to the ready queue and call the scheduler
             */

            ready_process(&ready_proc, &io_proc);
            schedule(cur_event);
            post_ready();
            break;
        case IO_EVENT:

            /*
             * The currently executing process starts I/O We move it from the
             * head of the ready queue to the io queue
             */

            io_process(&io_proc, &ready_proc);
            schedule(cur_event);
            post_io();
            break;
        case FINISH_EVENT:

            /*
             * The currently executing process has finished. We move it from
             * the ready queue to the defunct queue, awaiting reclamation of
             * its memory
             */

            finish_process(&defunct_proc, &ready_proc);
            schedule(cur_event);

            /*
             * After this operation the defunct queue should, once again, be
             * empty.
             */

            post_finish();
            break;
        default:
            /*
               This should never ever happen
             */

            printf("Het simulatie programma kent event nr %d niet\n",
                   cur_event);
            print_statistics();
            exit(0);
        }

        cur_event = find_next_event();
        if (n_errors_detected > MAX_ERRORS) {
            printf("\n*****************************************\n%s\n",
                   "Te veel fouten - programma wordt afgebroken");
            break;
        }
    }
    n_samples = 0;
    t_start = t_simulation_now;
    reset_stats();
    get_stats = true;
    while (proc_num < N_to_create + 100) {
        switch (cur_event) {
        case NEW_PROCESS_EVENT:

            /*
             * We must create a new process and add it to the new process
             * queue, awaiting the allocation of memory
             */

            new_process(&new_proc);
            schedule(cur_event);

            /*
             * Find out what the student has done, and schedule the next
             * event accordingly. Report any illogical actions or errors
             */

            post_new();
            break;
        case TIME_EVENT:

            /*
             * A process has used up its time-slice. The scheduler can
             * re-arrange the ready queue if it wants. We need not do
             * anything, except reset the timer
             */

            set_slice(9.9e12);
            schedule(cur_event);
            post_time();
            break;
        case READY_EVENT:

            /*
             * A process has finished its I/O action. We move it from the I/O
             * queue to the ready queue and call the scheduler
             */

            ready_process(&ready_proc, &io_proc);
            schedule(cur_event);
            post_ready();
            break;
        case IO_EVENT:

            /*
             * The currently executing process starts I/O We move it from the
             * head of the ready queue to the io queue
             */

            io_process(&io_proc, &ready_proc);
            schedule(cur_event);
            post_io();
            break;
        case FINISH_EVENT:

            /*
             * The currently executing process has finished. We move it from
             * the ready queue to the defunct queue, awaiting reclamation of
             * its memory
             */

            finish_process(&defunct_proc, &ready_proc);
            schedule(cur_event);

            /*
             * After this operation the defunct queue should, once again, be
             * empty.
             */

            post_finish();
            break;
        default:
            /*
               This should never ever happen
             */

            printf("Het simulatie programma kent event nr %d niet\n",
                   cur_event);
            print_statistics();
            exit(0);
        }

        cur_event = find_next_event();
        if (n_errors_detected > MAX_ERRORS) {
            printf("\n*****************************************\n%s\n",
                   "Te veel fouten - programma wordt afgebroken");
            break;
        }
    }
    print_statistics();
    return ((n_errors_detected > MAX_ERRORS));
}

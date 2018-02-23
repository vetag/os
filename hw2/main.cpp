/*
    Algorithms left to complete:
    -Round-Robin algo
    -3 following non-preemptive algos:
    --HPF,
    --SJF,
    --HPF-Aging

    Also:
    -cpu idle>2 quanta?
    -report

    If using GCC or compatible compiler,
    place all source and header files in the same directory and:

    g++ -std=c++11 -Wall -Wextra *.cpp

	good resource:
	https://www.cs.uic.edu/~jbell/CourseNotes/OperatingSystems/5_CPU_Scheduling.html
	definitions at top:
	https://www.geeksforgeeks.org/gate-notes-operating-system-process-scheduling/

	Notes (Jonathan):
	To avoid ambiguity, the job arrival times are rigged so no two jobs can arrive at the same time.
	This is done by shuffling array of bytes containing [1,100) exclusive,
	There are other ways to generate a random permutation span, some without extra space
	like lfsr's, but this seemed simplest. The space is small and the assignment focus is elsewhere.
	Also, one job(id 0: A) will always arrive at quantum 0 (why the shufbag does not have 0).

    ^Do something about cpu being idle for >2 quanta?
*/

#include "syms.h"

#include <stdlib.h>

#include <algorithm>
#include <random>
typedef std::minstd_rand Rng;
using std::fill;
using std::sort;

template<class CmpFunc, class InitFunc>
AlgoRet preemptive(const Job *job, int njobs, PerJobStats *stats, char *gantt);

#define SRT preemptive<SrtComp, QueueData>
#define HPF_PREEMPT preemptive<HpfComp, QueueData>
#define HPF_PREEMPT_AGE preemptive<HpfComp, AgeInit>

struct Sim
{
    const char* name;
    decltype(&fcfs) algo;//function ptr. a better name for decltype() would be typeof()
};

struct Sums
{
    int wait, response, turnaround;
};

//@TODO: fill this in
const Sim simulations[] =
{
    {"First Come First Serve", &fcfs},
    {"Shortest Job First (non-preemptive)", nullptr},//&sjf},
    {"Shortest Remaining Time (preemptive)", &SRT},
    {"Round Robin", nullptr},//&round_robin},
    {"Highest Priority (non-preemptive)", nullptr},//&hpf_non_preemptive},
    {"Highest Priority (preemptive)", &HPF_PREEMPT},//&hpf_preemptive}
    {"HPF-Aging (non-preemptive)", nullptr},//&hpf_preemptive}
    {"HPF-Aging (preemptive)", &HPF_PREEMPT_AGE}//&hpf_preemptive}
};

void writeln(const char* a, int n)
{
    fwrite(a, 1, n, stdout);
    putchar('\n');
}

Sums printJobLines(const Job* job, const PerJobStats* stats)
{
    Sums sums = {};
    puts("ID Arrival Burst Priority : Response Wait Turnaround");
    for (int i=0; i<NJOBS; ++i)
    {
        printf(" %c %7d %5d %8d : ", i+'A', job[i].arrival, job[i].burst, job[i].priority);

        if (stats[i].qend != 0)//if job was serviced
        {
            int const response   = stats[i].qbegin - job[i].arrival;
            int const turnaround = stats[i].qend - job[i].arrival;
            int const wait       = turnaround - job[i].burst;

            printf("%8d %4d %10d\n", response, wait, turnaround);
            sums.wait       += wait;
            sums.response   += response;
            sums.turnaround += turnaround;
        }
        else
            printf("%8s %4s %10s\n", "~", "~", "~");
    }
    putchar('\n');
    return sums;
}

double printAvg(int sum, int cnt, const char* str)
{
    double avg = sum/(double)cnt;
    printf("Average %s : %3d/%d = %6.3f\n", str, sum, cnt, avg);
    return avg;
}

//progname [ntests] [rng seed]
int main(int argc, char** argv)
{
    int ntests;
    unsigned initSeed;

    if (argc>1)
    {
        if ((ntests = atoi(argv[1])) <= 0)
            ntests = 1;
        if (argc==3)
        {
            if ((initSeed = strtoul(argv[2], NULL, 0)) == 0u)//detect hex from 0x if user want
                initSeed = INIT_SEED;
        }
    }
    else
    {
        ntests = 5;
        initSeed = INIT_SEED;
    }

    static_assert(QUANTA<256u, "");//can fit in u8
    enum{BagSize=QUANTA-1};
    unsigned char shufbag[BagSize];
    Job job[NJOBS];
    PerJobStats stats[NJOBS];
    char timechart[QUANTA + MAX_BURST];

    printf("Seed: 0x%X, Number of tests: %d\n", initSeed, ntests);

    for (const Sim sim : simulations)
    {
        //@TODO: remove this
        if (sim.algo==nullptr){ printf("TODO: %s\n", sim.name); continue; }

        for (int i=0; i<BagSize; ++i) shufbag[i] = i+1;
        Rng rng(initSeed);//each algo gets same data
        double aa_wait=0.0, aa_turnaround=0.0, aa_response=0.0, aa_thruput=0.0;
        printf("\n*** Testing algorithm: %s ***\n", sim.name);

        for (int testno=0; testno<ntests; ++testno)
        {
            std::shuffle(shufbag, shufbag+BagSize, rng);//rng by ref

            for (int arvtime=0, i=0; i<NJOBS; ++i)
            {
                job[i].arrival = arvtime;
                job[i].burst = MIN_BURST + (rng() % BURST_SPAN);
                job[i].priority = 1u + (rng() % 4u);//[1, 5u)
                arvtime = shufbag[i];
            }
            sort(job, job+NJOBS, [](Job a, Job b){return a.arrival < b.arrival;});
            //implicit //for (int i=0; i<NJOBS; ++i) job[i].id = i;

            fill(stats, stats+NJOBS, PerJobStats{});//zero to simplify logic of potentially unserviced jobs
            AlgoRet r = sim.algo(job, NJOBS, stats, timechart);

            printf("\nTest no: %d\n", testno);
            const Sums sums = printJobLines(job, stats);
            putchar('\n');

            aa_wait       += printAvg(sums.wait, r.jobsCompleted,       "wait      ");
            aa_response   += printAvg(sums.response, r.jobsCompleted,   "response  ");
            aa_turnaround += printAvg(sums.turnaround, r.jobsCompleted, "turnaround");

            double const thruput = double(r.jobsCompleted)/r.lastCompletionTime;
            aa_thruput += thruput;
            printf("Throughput: %d/%d = %f per single quantum\n", r.jobsCompleted, r.lastCompletionTime, thruput);

            puts("\nExecution chart:");
            for (char c='0'; c<='9'; ++c) printf("%c         ", c);
            puts("10        11        12");
            for (int i=0; i<13; ++i) fputs("0 2 4 6 8 ", stdout);
            putchar('\n');
            writeln(timechart, r.lastCompletionTime);
        }

        if (ntests!=1)
        {
            const double dnom = ntests;
            printf("\nAll %u tests for [%s] done, averages:\n", ntests, sim.name);
            printf("Wait      : %6.3f\n", aa_wait/dnom);
            printf("Response  : %6.3f\n", aa_response/dnom);
            printf("Turnaround: %6.3f\n", aa_turnaround/dnom);
            printf("Throughput: %6.3f per 100 quanta\n", aa_thruput*100.0/dnom);
        }
    }//for each algorithm

	return 0;
}

AlgoRet fcfs(const Job* job, int njobs, PerJobStats* stats, char* t)
{
    int j=0;
    int q=0;//elapsed quanta

    while (q<QUANTA)
    {
        if (q < job[j].arrival)
        {
            fill(t+q, t+job[j].arrival, '.');
            q = job[j].arrival;
        }

        int const comptime = q+job[j].burst;

        stats[j] = {q, comptime};

        fill(t+q, t+comptime, j+'A');
        q = comptime;
        if (++j==njobs)
            break;
    }

    return {j, q};//jobs completed, elapsed quanta
}

/*
    (Jonathan):
    Look at the end of this file for my original impl of SRT.
    Later I realized that SRT, HPF_preemptive
    and HPF_ preemptive aging are nearly identical, and created this template.

    SRT compares against remaining time
    HPF compares against priority
    HPF-Aging compares against priority too, but stores (initial_priority*5) + arrival time.

    So the only changes to the algorithms are the priority queue compare function
    and the init step taken inserting a job into the queue
*/
template<class Cmp, class I>
AlgoRet preemptive(const Job* job, int njobs, PerJobStats* stats, char* gantt)
{

    PriorityQueue<QueueData, Cmp> pque(njobs);

    int j=0;
    int q=0;//elapsed quanta
    //every job will be inserted, unlike fcfs
    while (j!=njobs)//q steps +1 each loop
    {
        //first: check if a new job arrives at this slice
        if (q == job[j].arrival)
        {
            pque.push(I::init(job[j], j));//init data based on heuristic for algo
            ++j;
            //goto more in loop
        }
        else if (pque.empty())
        {
            gantt[q++] = '.';
            continue;
        }
        //Decrementing will not invalidate heap invariant for SRT (incrementing might)
        //Grab "most important" job by the heuristic, it may be the one just pushed
        QueueData *const ptop = pque.ptr_top();
        unsigned const id = ptop->id;

        if (!ptop->bserved)//if not serviced set start time stats
        {
            stats[id].qbegin = q;
            ptop->bserved = true;
        }
        else if(--ptop->rem==0)//run for 1 time unit
        {
            stats[id].qend = q;
            pque.pop();
        }

        gantt[q++] = id+'A';
    }
    //all jobs have been inserted to queue,
    //those that were serviced but not completed need to finish,
    //those not are able to be serviced within QUANTA should be ignored,
    //decrementing njobs return value
    while (!pque.empty())
    {
        const QueueData topv = pque.top();
        pque.pop();

        if (!topv.bserved)
        {
            if (q<QUANTA)   stats[topv.id].qbegin = q;//more in loop
            else            { --j; continue; }//don't begin service >= QUANTA
        }

        unsigned const comptime = q + topv.rem;
        fill(gantt+q, gantt+comptime, topv.id+'A');
        stats[topv.id].qend = q = comptime;
    }

    return {j, q};//jobs completed, elapsed quanta
}

//my original SRT impl
#if 0
struct SrtData{short rem; unsigned char id; unsigned char bserved;};
struct SrtComp
{
    bool operator()(SrtData a, SrtData b) const { return a.rem>b.rem; }
};

/*
    j is index of next arriving job,
    all jobs put into queue, when q==arrival time

    if pque is empty, advance 1 slice idle
    else run job at pque top for 1 slice by decrementing remaining time,
    retains heap invariant

    after all jobs inserted, second loop
    finishes processing jobs hat were given time,
    and will start ones if q<QUANTA
*/
AlgoRet srt(const Job* job, int njobs, PerJobStats* stats, char* t)
{
    PriorityQueue<SrtData, SrtComp> pque(njobs);

    int j=0;
    int q=0;//elapsed quanta

    //everything will go in, but may not be serviced a single slice, example:
    //ID arrive burst
    // A     20    10
    // B     25    99
    // C     30    98
    //B wont be given a slice
    //C will be given a slice before 100 quanta,
    //and since started, will be finished (after 100 quanta)
    while (j!=njobs)
    {
        if (q == job[j].arrival)
        {
            pque.push({(short)job[j].burst, (unsigned char)j, false});
            ++j;
            //more in loop
        }
        else if (pque.empty())
        {
            t[q++] = '.';
            continue;
        }
        //will not invalidate heap invariant (incrementing would)
        SrtData *const ptop = const_cast<SrtData *>(&pque.top());
        unsigned const id = ptop->id;

        if (!ptop->bserved)//instead could memset .qbegin field of stats to sentinel value
        {
            stats[id].qbegin = q;
            ptop->bserved = true;
        }
        else if(--ptop->rem==0)//run for 1 time unit
        {
            stats[id].qend = q;
            pque.pop();
        }

        t[q++] = id+'A';
    }

    while (!pque.empty())
    {
        const SrtData topv = pque.top();
        pque.pop();

        if (!topv.bserved)
        {
            if (q<QUANTA)   stats[topv.id].qbegin = q;//more in loop
            else            { --j; continue; }//don't begin service >= QUANTA
        }

        unsigned const comptime = q + topv.rem;
        fill(t+q, t+comptime, topv.id+'A');
        stats[topv.id].qend = q = comptime;
    }

    return {j, q};//jobs completed, elapsed quanta
}
#endif



/*
Note how B is finished due to aging before
D even though D is initially more important

*** Testing algorithm: HPF-Aging (preemptive) ***

Test no: 0
ID Arrival Burst Priority : Response Wait Turnaround
 A       0     6        4 :        0    0          6
 B       8    16        4 :        0   13         29
 C      11    12        2 :        0    0         12
 D      35     9        2 :       13   13         22
 E      37     9        1 :        1    1         10
 F      44    12        4 :       31   31         43
 G      48    16        1 :       10   10         26
 H      59     5        4 :        ~    ~          ~
 I      63    16        3 :       25   26         42
 J      75    11        4 :        ~    ~          ~
 K      79     8        1 :        ~    ~          ~
 L      99    10        1 :        ~    ~          ~


Average wait       :  94/8 = 11.750
Average response   :  80/8 = 10.000
Average turnaround : 190/8 = 23.750
Throughput: 8/105 = 0.076190 per single quantum

Execution chart:
0         1         2         3         4         5         6         7         8         9         10        11        12
0 2 4 6 8 0 2 4 6 8 0 2 4 6 8 0 2 4 6 8 0 2 4 6 8 0 2 4 6 8 0 2 4 6 8 0 2 4 6 8 0 2 4 6 8 0 2 4 6 8 0 2 4 6 8 0 2 4 6 8 0 2 4 6 8
AAAAAAA.BBBCCCCCCCCCCCCCBBBBBBBBBBBBBBEEEEEEEEEEDDDDDDDDDDGGGGGGGGGGGGGGGGGFFFFFFFFFFFFFIIIIIIIIIIIIIIIII
*/











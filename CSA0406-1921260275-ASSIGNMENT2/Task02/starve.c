#include <stdio.h>
#include <stdlib.h>

/* Preemptive priority scheduling, 1 time unit per step.
   Job 0 : long, low priority (9), arrives at 0, burst 10.
   Jobs 1..20 : a stream of short high-priority jobs (priority 1), one every 2 units, burst 2.
   Ageing: every AGE units a job has waited, its priority number drops by 1 (min 1).
   AGE = 0 means no ageing.  All parameters are ASSUMED for the experiment. */
#define NJ 21
static int arrival[NJ], burst[NJ], basep[NJ], left[NJ], waited[NJ], startt[NJ], fin[NJ];

static void run(int age)
{
    for (int i = 0; i < NJ; i++) {
        arrival[i] = i == 0 ? 0 : (i - 1) * 2;
        burst[i] = i == 0 ? 10 : 2;
        basep[i] = i == 0 ? 9 : 1;
        left[i] = burst[i]; waited[i] = 0; startt[i] = -1; fin[i] = -1;
    }
    int t = 0, done = 0;
    while (done < NJ) {
        int cur = -1, curp = 0;
        for (int i = 0; i < NJ; i++) {
            if (arrival[i] > t || left[i] == 0) continue;
            int p = basep[i] - (age ? waited[i] / age : 0);
            if (p < 1) p = 1;
            if (cur < 0 || p < curp || (p == curp && arrival[i] < arrival[cur])) { cur = i; curp = p; }
        }
        if (cur < 0) { t++; continue; }
        for (int i = 0; i < NJ; i++)
            if (i != cur && arrival[i] <= t && left[i] > 0) waited[i]++;
        if (startt[cur] < 0) startt[cur] = t;
        t++;
        if (--left[cur] == 0) { fin[cur] = t; done++; }
    }
    double sw = 0;
    for (int i = 1; i < NJ; i++) sw += fin[i] - arrival[i] - burst[i];
    printf("ageing every %2d units: long job starts at t=%2d, finishes t=%2d, its WT=%2d | stream avg WT=%.2f\n",
           age, startt[0], fin[0], fin[0] - arrival[0] - burst[0], sw / (NJ - 1));
}

int main(void)
{
    printf("(age 0 = no ageing)\n");
    run(0); run(10); run(5); run(2);
    return 0;
}

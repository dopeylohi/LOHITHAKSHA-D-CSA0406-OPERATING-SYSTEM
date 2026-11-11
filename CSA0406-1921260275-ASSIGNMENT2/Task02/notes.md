# Q2 – CPU Scheduling (notes)
Labels: [M] measured/simulated by my programs, [C] calculated by hand from [M], [T] typical published knowledge.

## 1. Job set and assumptions
Jobs (from the TODO program): P1 arrives 0 burst 8; P2 arrives 1 burst 4; P3 arrives 2 burst 9; P4 arrives 3 burst 5 (abstract time units).
Priorities are NOT given in the question, so I ASSUMED P1=2, P2=3, P3=1, P4=4 (lower number = higher priority, non-preemptive).
Rules: SJF = non-preemptive shortest burst; SRTF = preemptive shortest remaining time; ties broken by arrival then index; in Round Robin a newly arrived process is queued before the preempted one. WT = TAT - burst; TAT = finish - arrival; RT = first start - arrival.

## 2. Gantt charts and averages (sched_output.txt) [M]
| Algorithm | Gantt chart |
|---|---|
| FCFS | P1(0-8) P2(8-12) P3(12-21) P4(21-26) |
| SJF | P1(0-8) P2(8-12) P4(12-17) P3(17-26) |
| SRTF | P1(0-1) P2(1-5) P4(5-10) P1(10-17) P3(17-26) |
| Priority | P1(0-8) P3(8-17) P2(17-21) P4(21-26) |
| Round Robin q=3 | P1(0-3) P2(3-6) P3(6-9) P4(9-12) P1(12-15) P2(15-16) P3(16-19) P4(19-21) P1(21-23) P3(23-26) |

| Algorithm | avg TAT | avg WT | avg RT | context switches |
|---|---|---|---|---|
| FCFS | 15.25 | 8.75 | 8.75 | 3 |
| SJF | 14.25 | 7.75 | 7.75 | 3 |
| SRTF | 13.00 | 6.50 | 4.25 | 4 |
| Priority (assumed) | 16.50 | 10.00 | 10.00 | 3 |
| Round Robin q=3 | 20.00 | 13.50 | 3.00 | 9 |

Hand check [C]: FCFS WT = 0+7+10+18 = 35, 35/4 = 8.75. SJF WT = 0+7+15+9 = 31, 31/4 = 7.75. SRTF WT = 9+0+15+2 = 26, 26/4 = 6.50. All three agree with the program. Context switches = number of Gantt segments - 1 [C].
Priority is worse than SJF here only because my assumed priorities favour the long job P3 over the short jobs.

## 3. Round Robin quantum study (quantum_study.txt) [M]
| Quantum | avg TAT | avg WT | avg RT | slices | context switches |
|---|---|---|---|---|---|
| 1 | 19.00 | 12.50 | 0.75 | 26 | 23 |
| 3 | 20.00 | 13.50 | 3.00 | 10 | 9 |
| 5 | 17.50 | 11.00 | 5.50 | 6 | 5 |
| 20 | 15.25 | 8.75 | 8.75 | 4 | 3 |
* Larger quantum: fewer context switches (23 -> 3) and lower waiting time on this job set, but slower first response (RT 0.75 -> 8.75).
* Quantum 20 is longer than every burst, so Round Robin becomes FCFS (same averages 15.25 / 8.75).
* Average WT is not strictly falling (12.50, 13.50, 11.00, 8.75): with only 4 jobs, where a burst ends relative to the quantum changes the result.
* Overhead [C]: fraction lost = c / (q + c). With c = 3.24 us (Q1, upper bound) and an assumed 1 ms quantum = 0.32 %; with an assumed 100 us quantum = 3.1 %.

## 4. Starvation and ageing (starve_output.txt) [M]
Experiment (parameters ASSUMED): one long low-priority job (burst 10, priority 9) at t=0 and a stream of 20 short high-priority jobs (burst 2, priority 1) arriving every 2 units. Preemptive priority scheduling. Ageing: priority number drops by 1 for every AGE units waited.
| Ageing step | long job starts | long job finishes | long job WT | stream avg WT |
|---|---|---|---|---|
| none | 40 | 50 | 40 | 0.00 |
| every 10 units | 40 | 50 | 40 | 0.00 |
| every 5 units | 40 | 50 | 40 | 0.00 |
| every 2 units | 16 | 26 | 16 | 6.00 |
* Starvation: without ageing the long job cannot run until the stream ends at t=40. SJF behaves the same way [C, by reasoning]: a 2-unit job always beats a 10-unit job, so a steady stream of short jobs blocks the long one.
* Ageing removes it by raising a waiting job's priority over time. It must be fast enough: ageing every 5 or 10 units would need 40 or 80 units to lift priority 9 to 1, so it changed nothing here.
* Price: with ageing every 2 units the stream jobs wait longer (average 0.00 -> 6.00).

## 5. How a multilevel feedback queue treats short and long jobs
A new job enters the top queue (quantum 2). A job that finishes inside its quantum (a short job) never leaves it. A job that uses its full quantum is demoted to the next queue (quantum 4, then 8), so long jobs sink to the lower queues. Higher queues are always served first. So short jobs get quick service without the scheduler knowing burst times, and long jobs get longer slices (fewer switches) but lower priority.

## 6. Alternative concept and complexity: MLFQ (mlfq_output.txt, pick_results.txt)
Workload (ASSUMED): 3 long jobs (burst 30) at t=0 and 12 short jobs (burst 2) at t = 2, 5, ..., 35. MLFQ quanta 2/4/8, slices are not interrupted.
| Policy | avg WT short | avg WT long | max WT long | dispatches |
|---|---|---|---|---|
| FCFS | 82.50 | 30.00 | 60 | 15 |
| SJF | 22.50 | 46.00 | 84 | 15 |
| Round Robin q=4 | 17.17 | 82.00 | 84 | 36 |
| MLFQ 2/4/8 | 1.83 | 76.00 | 84 | 27 |
* Short jobs wait 1.83 in MLFQ against 17.17 (Round Robin), 22.50 (SJF) and 82.50 (FCFS) [M]. Long jobs pay: 76.00 against 30.00 in FCFS.
* Total work is 3 x 30 + 12 x 2 = 114 units for every policy, so the processors are fully used under all of them [C]. The last long job ends at 114, so its WT is 114 - 30 = 84 in SJF, Round Robin and MLFQ [C].
Scheduling cost per decision (medians of 5 runs, pick_results.txt) [M]:
| ready jobs n | SJF (scan, O(n)) | MLFQ (queue head, O(1)) | ratio [C] |
|---|---|---|---|
| 10 | 15.5 ns | 10.7 ns | 1.4x |
| 100 | 116.4 ns | 10.7 ns | 10.9x |
| 1,000 | 1,089.7 ns | 10.6 ns | 103x |
| 10,000 | 10,656.7 ns | 11.0 ns | 969x |
SJF grows about linearly (about 1.07 ns per ready job [C]); MLFQ stays flat at about 11 ns. A heap would give O(log n) for SJF [T] (not measured).
* Starvation risk: in this finite workload the long jobs finish (max WT 84). If short jobs kept arriving forever, the bottom queue could starve like in section 4; the usual fix is a periodic priority boost back to the top queue [T] (not simulated).

## 7. Design decision: recommended policy
Recommendation: a 3-level multilevel feedback queue (quanta 2/4/8 units) with a periodic priority boost, for the video service.
* It fixes the complaint: short jobs wait 1.83 against 82.50 under FCFS, 22.50 under SJF and 17.17 under Round Robin [M].
* Processors stay fully used: all the policies are work-conserving and finish the same 114 units of work [C].
* It needs no advance knowledge of job lengths, unlike SJF and SRTF (SRTF has the best 4-job averages, WT 6.50, but needs the burst times and starves long jobs) [T].
* Decision cost stays about 11 ns whatever the queue length, against 10,657 ns for an SJF scan at 10,000 jobs [M].
* Trade-off: long jobs wait longer (76.00 against 30.00 for FCFS) - acceptable for 20-minute films. The boost prevents starvation.
* Plain Round Robin has the best response (q=1 RT 0.75) but the highest TAT (20.00 at q=3) and many switches; FCFS shows the convoy effect (short jobs wait 82.50).
Limits: the workload is small and assumed; real encoders have I/O and multiple CPUs, which were not simulated.

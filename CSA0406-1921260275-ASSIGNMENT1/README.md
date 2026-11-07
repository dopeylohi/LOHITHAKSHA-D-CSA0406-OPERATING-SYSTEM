# CSA04 Operating Systems - Assignment 1
# CSA04 Operating Systems - Assignment 1
## Evaluate and Justify the Operating-System Design of a Smart Hospital

| Field | Value |
|---|---|
| Name | Lohithaksha D |
| Register Number | 1921260275 |
| Department | Information Technology, SIMATS Engineering |
| Semester / Year | 2nd Semester, 1st Year, MS2, Sep-Dec 2026 (UG), Slot B |
| Faculty | Jayasakthi Velmurugan Kannaiyan |
| Submission date | 7/11/2026 |

## Environment
- Compiler: gcc 15.2.0 (Ubuntu 15.2.0-16ubuntu1)
- strace: 6.19
- Linux: Ubuntu 26.04.1 LTS (Resolute Raccoon), running on WSL2
- Compile command used for every program:
  `gcc -std=c11 -Wall -Wextra -pthread file.c -o file`
- Version evidence: `Setup/versions.txt` and `Setup/versions.png`

## Folder structure
| Folder | Contents |
|---|---|
| Setup | Environment versions, quick strace test |
| Task05 | `read_demo.c`, strace of read(), strace of cat (splice) |
| Task06 | `copy_file.c` (completed TODO 1), `bench_copy.c`, timing, boundary tests, strace counts |
| Task07 | `shell.c` (completed TODO 1), `shell_exp.c` (fflush experiment), strace of fork/exec/wait |
| Task10 | `proc_vs_thread.c`, timing results |

Tasks 1-4, 8, 9, 11 and 12 are written analysis tasks. They have no code and are in the Word report.
Compiled binaries and large `.dat` files are not committed (see `.gitignore`). Compile with the command above to rebuild them.

## How to run

### Task 5
```
cd Task05
echo "patient record 101" > rec.txt
gcc -std=c11 -Wall -Wextra -pthread read_demo.c -o read_demo
./read_demo
strace -e trace=openat,read,write,close ./read_demo
```

### Task 6
```
cd Task06
gcc -std=c11 -Wall -Wextra -pthread copy_file.c -o copy_file
./copy_file
strace -c -e trace=read,write ./copy_file
gcc -std=c11 -Wall -Wextra -pthread bench_copy.c -o bench_copy
head -c 10485760 /dev/urandom > big.dat
time ./bench_copy big.dat out.dat 4096
```

### Task 7
```
cd Task07
gcc -std=c11 -Wall -Wextra -pthread shell.c -o shell
./shell
gcc -std=c11 -Wall -Wextra -pthread shell_exp.c -o shell_exp
./shell_exp                  # terminal: no duplicates
./shell_exp > redirected.txt # file: duplicate lines
strace -f -e trace=clone,clone3,execve,wait4,exit_group,write ./shell
```

### Task 10
```
cd Task10
gcc -std=c11 -Wall -Wextra -pthread proc_vs_thread.c -o proc_vs_thread
./proc_vs_thread
time ./proc_vs_thread
```

## Task results

| Task | Result |
|---|---|
| 1 | Ranked requirement tables for each hospital environment; single general-purpose OS alternative evaluated (report). |
| 2 | OS type chosen per environment with rejected alternatives; SMP vs AMP; clustered fail-over (report). |
| 3 | Structures compared; microkernel for monitors, modular/hybrid for database server (report). |
| 4 | Service inclusion/exclusion table per environment (report). |
| 5 | `read_demo` strace shows openat -> fd 3, read(3, buf, 64) = 19, write(1, ..., 19) = 19, close(3). `cat` on this system uses splice() instead of read()/write(). |
| 6 | `copy_file` output matches expected: 10001 / 21 / 4 read() calls. strace -c: 10027 read and 10029 write calls (difference explained by loader read, fclose flushes and printf). 10 MB timing: 1-byte buffer 15.146 s (sys 14.088 s); 4096 bytes 0.020 s; 64 KB 0.017 s. Recommended backup buffer: 64 KB. Boundary tests: empty file gives 0 reads, missing file gives a clean open error. |
| 7 | Output matches expected five lines. With the second fflush removed and exit(3), the redirected file has 7 lines: "job 1 exit status 0" and "shell: running job 2" appear twice because the unflushed stdio buffer is copied by fork() and flushed by both processes. A terminal shows no duplicates because it is line-buffered. strace shows clone, four execve PATH attempts, wait4, exit_group and SIGCHLD. |
| 8 | CPU modes, interrupts, protection, polling vs interrupts (report). |
| 9 | Memory hierarchy, data placement, DMA vs interrupt-per-word for 500 MB image (report). |
| 10 | Mean time for 1,000 operations: pthread_create 0.096 s, fork 0.267 s, posix_spawn 0.543 s, fork+exec 0.658 s. Whole program: real 1.533 s, user 0.113 s, sys 0.523 s. strace -c counts per buffer size match n/b. |
| 11 | Final OS plan for all environments with limits, risks and the virtual machine alternative (report). |
| 12 | Design challenge: remote home monitoring and cloud records re-evaluated; design modified (report). |

## Commit history
One or more commits per task, in task order. See the repository commit log.

# Task01 - Linking, Loading and Address Binding (6 marks)

Setup: Ubuntu 26.04.1 on WSL2, gcc 15.2.0. Build flags: `-std=c11 -Wall -Wextra -O2` (see build.sh).
Labels: [M] measured, [C] calculated, [T] typical/textbook fact, [A] assumed.

## 1. What I built
- bankenc.c = toy "encryption library" (XOR with a key). Version 1 key 0x5A, version 2 (security fix) key 0x7C.
- app.c = the "bank application": encrypts the text ACC1234-BALANCE-5000, prints /proc/self/status and /proc/self/maps.
- app_static: library code compiled into the executable (`gcc -static app.c bankenc.c`).
- app_dyn: linked to libbankenc.so (`-L. -lbankenc -Wl,-rpath,'$ORIGIN'`). Library built with `-fPIC -shared`.
- dl_app.c: loads libbankenc.so only when asked, using dlopen() and dlsym().
- Real application scale: the question says 200 MB. My toy is KB-sized, so I compare ratios, not absolute MB.

## 2. Sizes and ldd (Table 1.1, Figure 1.1)
| File | Size in bytes [M] | text / data / bss from `size` [M] |
|---|---|---|
| app_static | 821680 | 703248 / 22824 / 22560 |
| app_dyn | 16528 | 3743 / 728 / 16 |
| libbankenc.so | 15224 | - |

- `ldd app_dyn` lists: linux-vdso.so.1, libbankenc.so, libc.so.6, /lib64/ld-linux-x86-64.so.2 [M].
- `ldd app_static` prints "not a dynamic executable" and exit code 1 [M]. It has no shared dependencies.
- [C] 821680 / 15224 = 53.97, so the static executable is about 54 times bigger than the library alone.
- [C] app_dyn + library = 16528 + 15224 = 31752 bytes; 821680 / 31752 = 25.9. The static file is about 26 times bigger.
- The static file is large because libc code that the program uses is copied into it. The dynamic one uses libc.so.6, already on the system.

## 3. Load addresses (Table 1.2, Figure 1.2)
Start address of each executable (r-xp) mapping from /proc/self/maps [M]:

| Item | static | dynamic run 1 | dynamic run 2 |
|---|---|---|---|
| program code | 0x401000 | 0x62e8a8a9c000 | 0x59963bb6e000 |
| libc.so.6 | (inside program) | 0x6ffee1a28000 | 0x75bfdf228000 |
| libbankenc.so | (inside program) | 0x6ffee1e03000 | 0x75bfdf54c000 |
| ld-linux (loader) | none | 0x6ffee1e12000 | 0x75bfdf55b000 |
| &main | 0x401800 | 0x62e8a8a9c1d0 | 0x59963bb6e1d0 |
| &bankenc_encrypt | 0x401cd0 | 0x6ffee1e03110 | 0x75bfdf54c110 |

- Static run: low address 0x401000, set when the program was linked.
- Dynamic runs: every base address changes between runs (ASLR), but the offset inside the page does not (0x1d0 for main, 0x110 for the function).
- dl_app, after dlopen(): the library appeared at 0x7adf477e4000 to 0x7adf477e9000 as 5 mappings (r--p, r-xp, r--p, r--p, rw-p), and dlsym() returned the function at 0x7adf477e5110 [M] (Figure 1.3). Code (r-xp) and data (rw-p) are separate segments with separate permissions.

## 4. Address binding
- Compile-time binding: the compiler/linker fixes absolute addresses. Evidence: app_static is a non-PIE executable linked to 0x401000, main = 0x401800 [M]. If that address were not free, the program would have to be recompiled.
- Load-time binding: the loader picks the base address when the program starts and relocates it. Evidence: &main differs between dynamic runs 1 and 2 but its offset in the page stays 0x1d0 [M]. The code was linked as relocatable (PIE / -fPIC).
- Execution-time binding: the address is not final until the instruction runs, so the process can move in memory. This needs hardware support (MMU, page tables). Evidence: dlsym() returned the address of bankenc_encrypt only while the program was running [M]. Every logical address of every process is also translated by the MMU at each access [T].

## 5. Linker and loader
- Linker (ld, called by gcc): combines object files and libraries, resolves symbols, and either copies library code in (static) or records "needs libbankenc.so" in the executable (dynamic) [T].
- Loader (kernel execve + ld-linux dynamic loader): maps the executable, finds and maps the needed .so files (ldd shows them), relocates, then jumps to main [T]. app_dyn was found its library because of `-Wl,-rpath,'$ORIGIN'`.

## 6. Position-independent code (PIC)
- PIC uses relative addresses (on x86-64, RIP-relative) and a table (GOT/PLT) for external symbols, so it contains no absolute addresses [T].
- Why it matters: the same library can be mapped at a different base address in every process, and its code pages need no patching, so they stay read-only and can be shared by all processes [T].
- Evidence: libbankenc.so was built with -fPIC and was mapped at different addresses in runs 1 and 2 [M]. Its code pages were shared (Table 1.4).

## 7. Start-up time and memory (Table 1.3, Figure 1.4 and 1.8)
Start-up: 5 rounds of 500 runs each (bash loop, output to /dev/null), microseconds per run.
| Version | Rounds [M] | Median [M] |
|---|---|---|
| app_static | 763, 715, 682, 705, 671 | 705 us |
| app_dyn | 1006, 904, 902, 917, 907 | 907 us |

- [C] 907 - 705 = 202 us extra; 202 / 705 = 28.7 percent slower to start. The loader has to find and map two libraries and relocate before main.
- The loop time includes bash fork and exec in both numbers, so only the difference is meaningful.

Memory (VmRSS, 5 runs each, "mem" mode):
| Version | Runs in kB [M] | Median [M] |
|---|---|---|
| app_static | 720, 720, 720, 720, 724 | 720 kB |
| app_dyn | 1640, 1644, 1640, 1640, 1640 | 1640 kB |

- [C] 1640 - 720 = 920 kB more resident memory for one dynamic process (ratio 2.28). The whole libc.so and ld.so are mapped, while the static file contains only the libc parts it needs.
- VmSize from the single full runs [M]: static 1044 kB, dynamic 2792 kB (both dynamic runs gave 2792).
- Note: in "full" mode (prints maps) VmRSS was higher (728 kB static, 1836 kB dynamic) because printing runs more code. The table above uses the same mode for both.

## 8. Library replacement without recompiling (Figure 1.6)
- app_dyn md5 before and after was identical: 0413a940674bf409e12b59295ca7ca19 [M]. The program was not recompiled or relinked.
- libbankenc.so md5 changed from ba3d57ca96223102d2a79fb9d38a0982 to 1a4635c0a03fddcc14f513d42b540de0 [M].
- Output changed from "1.0 (original, key 0x5A)", cipher 1b19196b... to "2.0 (security fix, key 0x7C)", cipher 3d3f3f4d... [M].
- app_static still printed version 1.0 [M]: a static program needs a rebuild and a new full executable sent to every branch.
- This is the bank's case: the fix to the encryption library reaches all programs by replacing only the library file (15224 bytes here [M]).

## 9. Two copies and shared pages (Table 1.4, Figure 1.5)
Two copies of each program run together; values from /proc/PID/smaps of the executable (r-xp) mappings, after `sync` (share2.txt) [M]. Pss = proportional set size: Rss divided by the number of processes sharing each page.
| Mapping | Rss | Pss | Shared_Clean | Shared_Dirty |
|---|---|---|---|---|
| app_static code (each copy) | 536 kB | 268 kB | 536 kB | 0 |
| app_dyn code (each copy) | 4 kB | 2 kB | 4 kB | 0 |
| libc.so.6 code (each copy) | 1248 kB | 52 kB | 1248 kB | 0 |
| libbankenc.so code (each copy) | 4 kB | 2 kB | 4 kB | 0 |

- All pages are Shared_Clean: both copies use the same physical pages of the file (page cache), none are private [M].
- [C] app_static: 536 / 268 = 2 sharers, so the two copies together use one 536 kB copy of the code.
- [C] libbankenc: 4 / 2 = 2 sharers (my two copies). libc: 1248 / 52 = 24, so about 24 processes on this system share the same libc pages (approximate, Pss is rounded).
- [C] Code memory charged to my two dynamic copies: 2 x (2 + 52 + 2) = 112 kB, against 2 x 268 = 536 kB for the static ones. The libc share is small only because other processes already use libc [A]; on a machine running only these two programs libc would count fully.
- First run, before `sync`: the pages showed as Shared_Dirty (share.txt), then Shared_Clean after sync. This is consistent with the files having just been written and still being dirty in the page cache; it does not change the sharing.
- Honest finding: two copies of the same static program also share their code pages. Dynamic linking saves memory mainly across different programs that all use the same library (libc here), and in the disk size of each program.

## 10. Missing shared library (Figure 1.7)
With libbankenc.so hidden [M]:
- app_dyn: the loader refused to start it: "error while loading shared libraries: libbankenc.so: cannot open shared object file", exit code 127. main() never ran, so the program could not print its own message.
- app_static: ran normally, exit code 0.
- dl_app enc: dlopen() returned NULL, program printed "ERROR: cannot load encryption library ... No such file or directory" and exited with its own code 2.
- dl_app none: ran normally, exit 0, "library mapped: no".
- In missing.txt the ERROR line appears above "mode: enc" only because stderr is not buffered and stdout is buffered when piped to tee.

## 11. Evaluation: static vs dynamic (Table 1.5)
| Criterion | Static | Dynamic (shared) |
|---|---|---|
| Executable size | 821680 B [M] | 16528 B + 15224 B library [M] |
| Start-up | 705 us [M] | 907 us, +28.7 percent [C] |
| Memory, one process | 720 kB RSS [M] | 1640 kB RSS [M] |
| Memory, many processes | each program has its own copy of the library code | one physical copy shared (libc about 24 sharers) [C] |
| Updating a library | rebuild and resend the whole executable | replace one .so, program unchanged [M] |
| Missing library | cannot happen | program refuses to start (exit 127) [M] |
| Risk | old vulnerable code stays inside until rebuilt | a wrong or incompatible .so version can break the program [T] |

## 12. Conclusion - which method for the bank
Use dynamic linking (shared library). Reasons:
1. The stated problem is security fixes to the encryption library: replacing one file fixed the behaviour without touching the program (Section 8), and one small file is sent to each branch instead of the full executable.
2. The cost is about 0.2 ms slower start-up [M] and more memory per single process [M]. For a long-running branch application started rarely, I judge this negligible [A].
3. The risks are controlled by (a) checking at install time that the library is present and at the right version, (b) testing the new library before roll-out, (c) refusing to run unencrypted if the library fails to load (never fall back to plain text).
Static linking would be better only for a small tool that must run on machines where installing libraries is impossible.

## 13. Alternative: load the library with dlopen() only when needed
Solution: do not link libbankenc.so at build time. At the first encryption request call dlopen("./libbankenc.so", RTLD_NOW), get the functions with dlsym(), keep the pointers. This is dl_app.c.
- Start-up time: the loader no longer maps this library at start, so start-up is shorter by that one library's cost. I did not measure this for dl_app, so I do not give a number. The first encryption request becomes slower, since it pays the loading cost then.
- Memory when encryption is not needed [M]: dl_app none medians 1756 kB (runs 1756, 1760, 1756, 1752, 1756) and "library mapped: no". With encryption, dl_app enc median 1844 kB (1844, 1848, 1844, 1844, 1848). [C] 1844 - 1756 = 88 kB not spent when it is not needed.
- Error handling: the program starts even if the library is missing, so it must check dlopen() for NULL, print dlerror(), check each dlsym() result, and choose what to do. For the bank the safe choice is to refuse the encrypted operation and report an error, not to send plain text. Measured: exit code 2 with a clear message, against exit 127 from the loader with the linked version [M].
- Complexity [T]: N = number of libraries, R = number of symbols to resolve. Load-time dynamic linking costs O(N + R) at start-up for every run, even when the library is never used; dlopen costs the same work but only if and when it is needed (and once, since the handle is kept). Calling the function is O(1) either way (one indirect call). Space: the library code is O(1) physical copies shared by all processes; a static executable carries O(size of library) in each program. The dlopen version adds a little code to check errors.

## 14. Limits of this evidence
- Toy library (15 KB), not 200 MB; one machine (WSL2), so absolute times include WSL2 process-start overhead.
- Memory numbers are VmRSS/Pss from /proc, only for the code mappings in Table 1.4; Pss for libc depends on what else is running.
- Start-up time of the dlopen version was not measured.

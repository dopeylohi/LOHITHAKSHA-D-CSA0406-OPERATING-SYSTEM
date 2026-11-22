#!/bin/bash
# Q4 calculations. [C] = calculated. Memory access m = 100 ns, page-fault service S = 8 ms = 8,000,000 ns (given in the question).
awk 'BEGIN{
  m=100; S=8000000;
  print "=== A. Effective access time EAT = (1-p)*m + p*S ===";
  split("0.001 0.00001 0.000001", P, " ");
  split("1 in 1,000|1 in 100,000|1 in 1,000,000", L, "|");
  for (i=1;i<=3;i++){ p=P[i]; e=(1-p)*m + p*S;
    printf "p = %s (%s): EAT = %.7f*%d + %.7f*%d = %.4f ns ; slow-down = %.2f times\n", p, L[i], 1-p, m, p, S, e, e/m }
  print "";
  print "=== B. Maximum fault rate for a 10% slow-down: EAT <= 1.1*m ===";
  pmax = (0.1*m)/(S-m);
  printf "(1-p)*100 + p*8000000 <= 110  ->  p <= 10/(8000000-100) = %.6e  (about 1 fault in %.0f accesses)\n", pmax, 1/pmax;
  print "";
  print "=== C. Frame allocation example [A]: m = 128 frames, three processes of 10, 40 and 150 pages ===";
  s[1]=10; s[2]=40; s[3]=150; S2=200; M=128;
  printf "equal allocation       : %.1f frames each (m/n)\n", M/3;
  for (i=1;i<=3;i++) printf "proportional, size %3d  : a = s/S*m = %d/%d*%d = %.1f frames\n", s[i], s[i], S2, M, s[i]/S2*M;
}'

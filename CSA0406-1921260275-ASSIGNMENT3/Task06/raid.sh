#!/bin/bash
# Q6 RAID calculations. [C] = calculated, [A] = assumed, [T] = typical published value.
awk 'BEGIN{
  n=6; d=4;
  print "=== A. Usable capacity, 6 disks of 4 TB (decimal TB, as sold) [C] ===";
  printf "RAID 0  : n*d         = %d*%d = %d TB   (no redundancy)\n", n, d, n*d;
  printf "RAID 1  : 3 mirrored pairs = 3*%d = %d TB as three separate volumes; one 6-way mirror = %d TB\n", d, 3*d, d;
  printf "RAID 5  : (n-1)*d     = %d*%d = %d TB\n", n-1, d, (n-1)*d;
  printf "RAID 6  : (n-2)*d     = %d*%d = %d TB\n", n-2, d, (n-2)*d;
  printf "RAID 10 : n/2*d       = %d*%d = %d TB\n", n/2, d, n/2*d;
  printf "Conversion: 1 TB = 10^12 B, 1 TiB = 2^40 B, so 4 TB = %.2f TiB; RAID 5 20 TB = %.2f TiB; RAID 6 16 TB = %.2f TiB; RAID 10 12 TB = %.2f TiB\n", 4e12/2^40, 20e12/2^40, 16e12/2^40, 12e12/2^40;
  print "";
  print "=== B. Probability of data loss given one disk has failed ===";
  afr=0.02; hours=24; q=afr*hours/8760;
  printf "Assumptions [A]: annual failure rate AFR = %.0f%% per disk (published HDD figures are about 1 to 3%% [T]); the array is exposed for %d h (replace + rebuild).\n", afr*100, hours;
  printf "q = chance that one given surviving disk fails during that window = AFR*h/8760 = %.0f%%*%d/8760 = %.4e\n", afr*100, hours, q;
  r1 = q;
  r5 = 1-(1-q)^5;
  r6 = 1-(1-q)^5 - 5*q*(1-q)^4;
  printf "RAID 1  (2-disk mirror): loss if the one partner fails          : q                          = %.3e\n", r1;
  printf "RAID 10 (3 mirrors)    : loss if the partner of the failed disk fails: q                    = %.3e\n", r1;
  printf "RAID 5  (6 disks)      : loss if any of the other 5 fails       : 1-(1-q)^5                  = %.3e  (about 5q = %.3e)\n", r5, 5*q;
  printf "RAID 6  (6 disks)      : loss if 2 or more of the other 5 fail  : 1-(1-q)^5-5q(1-q)^4       = %.3e  (about 10q^2 = %.3e)\n", r6, 10*q*q;
  print "";
  print "=== C. Probability of data loss in one year (first failure, then a second one inside the window) ===";
  f2 = 1-(1-afr)^2; f6 = 1-(1-afr)^6;
  printf "chance that at least one disk fails in a year: 2 disks %.4f, 6 disks %.4f\n", f2, f6;
  printf "RAID 1  (2 disks)  : %.4f * %.3e = %.2e per year (1 in %.0f)\n", f2, r1, f2*r1, 1/(f2*r1);
  printf "RAID 10 (6 disks)  : %.4f * %.3e = %.2e per year (1 in %.0f)\n", f6, r1, f6*r1, 1/(f6*r1);
  printf "RAID 5  (6 disks)  : %.4f * %.3e = %.2e per year (1 in %.0f)\n", f6, r5, f6*r5, 1/(f6*r5);
  printf "RAID 6  (6 disks)  : %.4f * %.3e = %.2e per year (1 in %.0f)\n", f6, r6, f6*r6, 1/(f6*r6);
  print "";
  print "=== D. Unrecoverable read error (URE) during a rebuild [T] ===";
  ure=1e-14;
  b5 = 5*4e12*8; b1 = 1*4e12*8;
  printf "Spec value [T]: 1 error per 10^14 bits read (typical consumer HDD datasheet; enterprise disks are often 10^-15)\n";
  printf "RAID 5 rebuild reads 5 disks x 4 TB = 20 TB = %.2e bits: expected errors = %.2f, P(at least one) = 1-exp(-%.2f) = %.1f%%\n", b5, b5*ure, b5*ure, 100*(1-exp(-b5*ure));
  printf "RAID 1/10 rebuild reads 1 disk x 4 TB = %.2e bits: expected errors = %.2f, P(at least one) = %.1f%%\n", b1, b1*ure, 100*(1-exp(-b1*ure));
  print "RAID 6 can still repair a read error during a single-disk rebuild, because it has a second parity.";
  print "";
  print "=== E. Small-write cost (I/O operations per block written) [T] ===";
  print "RAID 0: 1 write | RAID 1 and 10: 2 writes | RAID 5: 4 I/Os (read old data, read old parity, write data, write parity) | RAID 6: 6 I/Os (also read and write the second parity)";
}'

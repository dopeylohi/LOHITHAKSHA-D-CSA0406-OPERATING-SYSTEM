#!/usr/bin/env python3
"""B4.2 capacity and B4.3 cost estimate for the result portal.
[M] = measured by me in this assignment, [A] = assumption (edit the numbers below and re-run)."""
import math

# ---- [M] measured --------------------------------------------------------------
RPS_CONTAINER = 726.46   # B2: one container limited to --cpus 0.5 (same as the Kubernetes pod limit), ab -c 20, median of 5
RPS_VM = 793.16          # B3: one KVM VM (2 vCPU, 1 GiB), median of the 3 valid load runs
MEM_CONTAINER_MIB = 38   # B3: docker stats MEM USAGE of one container
MEM_VM_MIB = 553         # B3: host RSS of one QEMU VM process
CPU_PER_CONTAINER = 0.5  # CPU limit of one container / pod
VCPU_PER_VM = 2

# ---- [A] assumptions -----------------------------------------------------------
STUDENTS = 150_000
WINDOW_S = 2 * 3600          # two hours
REQ_PER_STUDENT = 10         # [A] page loads + API calls per student
PEAK_FACTOR = 10             # [A] peak load = 10 x the two-hour average (everyone arrives at the start)
MAX_UTIL = 0.7               # [A] plan to use at most 70 % of capacity
HEAVY = {"demo endpoint (as measured)": 1, "realistic page with database work": 20}  # [A] CPU cost relative to demo
PRICE_VCPU_HOUR = 0.05       # [A] USD per vCPU-hour, public cloud (check the provider's price list)
RESULT_DAYS_PER_YEAR = 2     # one per semester
BURST_HOURS = 3             # [A] hours of extra capacity rented per result day
BASELINE_VCPU = 8            # [A] always-needed CPUs for LMS, exams, portal on normal days
SERVER_CORES = 32            # [A] one own server
SERVER_PRICE = 6000          # [A] USD per server
SERVER_LIFE_YEARS = 3        # [A]
SERVER_RUNNING_FRACTION = 0.15  # [A] power, space, support per year as a fraction of price
EXISTING_SERVERS = 2         # [A] servers the university already owns (their purchase cost is already paid)

avg = STUDENTS * REQ_PER_STUDENT / WINDOW_S
peak = avg * PEAK_FACTOR
print(f"average load = {STUDENTS} x {REQ_PER_STUDENT} / {WINDOW_S} s = {avg:.1f} req/s")
print(f"peak load    = {avg:.1f} x {PEAK_FACTOR} = {peak:.1f} req/s")
for name, w in HEAVY.items():
    c_rps = RPS_CONTAINER / w * MAX_UTIL
    v_rps = RPS_VM / w * MAX_UTIL
    nc = math.ceil(peak / c_rps)
    nv = math.ceil(peak / v_rps)
    print(f"\n--- {name} (cost factor {w}) ---")
    print(f"usable per container = {RPS_CONTAINER} / {w} x {MAX_UTIL} = {c_rps:.1f} req/s -> containers = ceil({peak:.1f} / {c_rps:.1f}) = {nc}")
    print(f"usable per VM        = {RPS_VM} / {w} x {MAX_UTIL} = {v_rps:.1f} req/s -> VMs = ceil({peak:.1f} / {v_rps:.1f}) = {nv}")
    print(f"containers: {nc * CPU_PER_CONTAINER:.1f} CPUs, {nc * MEM_CONTAINER_MIB / 1024:.2f} GiB RAM")
    print(f"VMs:        {nv * VCPU_PER_VM} vCPUs, {nv * MEM_VM_MIB / 1024:.2f} GiB RAM")
    if w == max(HEAVY.values()):
        burst_cpu = nc * CPU_PER_CONTAINER
        run_cost = SERVER_PRICE * SERVER_RUNNING_FRACTION                       # per existing server per year
        new_server_cost = SERVER_PRICE * (1 / SERVER_LIFE_YEARS + SERVER_RUNNING_FRACTION)  # per NEW server per year
        usable_cores = SERVER_CORES * MAX_UTIL
        need_own_only = math.ceil((BASELINE_VCPU + burst_cpu) / usable_cores) + 1  # +1 spare server
        need_hybrid = math.ceil(BASELINE_VCPU / usable_cores) + 1
        extra = max(0, need_own_only - EXISTING_SERVERS)
        burst_cost = burst_cpu * BURST_HOURS * RESULT_DAYS_PER_YEAR * PRICE_VCPU_HOUR
        cloud_only = BASELINE_VCPU * 8760 * PRICE_VCPU_HOUR + burst_cost
        own_only = EXISTING_SERVERS * run_cost + extra * new_server_cost
        hybrid = EXISTING_SERVERS * run_cost + burst_cost
        print(f"\nCOST per year (realistic case, peak CPUs = {burst_cpu:.1f}) - own servers already exist ({EXISTING_SERVERS}):")
        print(f"own servers only: need {need_own_only} servers, must BUY {extra} more: {EXISTING_SERVERS} x ${run_cost:,.0f} + {extra} x ${new_server_cost:,.0f} = ${own_only:,.0f}")
        print(f"hybrid: baseline needs {need_hybrid} servers (have {EXISTING_SERVERS}, buy 0): {EXISTING_SERVERS} x ${run_cost:,.0f} + burst ${burst_cost:.2f} = ${hybrid:,.0f}")
        print(f"cloud only : baseline {BASELINE_VCPU} vCPU x 8760 h x ${PRICE_VCPU_HOUR} + burst ${burst_cost:.2f} = ${cloud_only:,.0f} (own servers switched off)")

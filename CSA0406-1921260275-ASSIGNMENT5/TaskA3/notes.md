# A3 - Evaluate virtualisation (5 marks)
Labels: [M] measured by me in Part B, [T] standard/published fact, [A] assumption.

## A3.1 Hypervisors (3)
 - Type-1 (bare-metal): runs directly on the hardware and hosts the VMs: KVM, VMware ESXi, Xen, Hyper-V. Best performance and isolation, used in data centres. KVM is a Linux kernel module that makes Linux itself the hypervisor (QEMU provides the virtual devices). [M] I ran a VM with KVM/QEMU: /dev/kvm was present on my machine.
 - Type-2 (hosted): a program running on a normal operating system: VirtualBox, VMware Workstation. Easy to install on a laptop, but every request passes through the host OS as well, so it is used for development and teaching, not production.
 - Full virtualisation: the guest OS runs unmodified; the hypervisor emulates a complete machine (hardware support makes it fast). Para-virtualisation: the guest knows it is virtual and calls the hypervisor directly (Xen PV, virtio drivers), which is faster for I/O. [M] my VM used virtio disk and network devices.
 - Hardware support: Intel VT-x and AMD-V let the CPU run guest code directly and trap only the privileged instructions; without them full virtualisation must use slow software translation. Extended page tables (EPT/NPT) do the same for memory.

## A3.2 Benefits and costs for the university (2)
Benefits: isolation (a crash or attack in one VM stays there), consolidation (many services on fewer servers), snapshots (roll back before a risky change), live migration (move a running VM to another host for maintenance).
Costs, with my measurements [M]:
 - Start-up: 14.18 s for one minimal VM (median of 5), against 0.77 s for a container and 0.21 s bare metal.
 - Memory: 553 MiB of host RAM for one small VM (guest configured 1024 MiB), against 38MiB for the container.
 - Disk: the VM image was 593M (+ a 610M overlay) against 49MB (compressed) for the container image.
 - Throughput: 793.16 req/s (median of the 3 valid runs, through QEMU user-mode networking) against 1649.55 bare metal.
 - Licences: KVM is free; ESXi and Windows guests need licences [T] (no prices looked up).
Verdict: use VMs where strong isolation or another OS matters (the database, exam systems); do not pay this cost for every stateless web instance.

## Alternative concept: type-2 (VirtualBox) instead of type-1 (KVM)
 - I only measured KVM, so everything below about VirtualBox is reasoning [T], not measurement.
 - CPU-bound work: both use VT-x, so CPU-bound code runs at close to native speed in both. I/O-bound work: a type-2 hypervisor sends disk and network I/O through the host OS drivers and file system, so it adds another layer and is usually slower than KVM with virtio.
 - Memory used by the host OS: a type-2 setup needs a full desktop-grade host OS (and its services) in addition to the VMs; KVM on a minimal Linux server keeps that small.
 - Suitability for production: type-2 is meant for desktops; it has no clustering, live migration or good management tools, and its availability depends on a general-purpose host. Type-1 is the right choice for the portal.
 - Complexity: memory for n VMs is O(n x M_guest) for both; a type-2 host adds a fixed extra O(M_host_os).
 Pseudocode (host memory for n VMs):
   ALGORITHM HostMemory(n, guestOsMemory, hostOsMemory, hypervisorType)
     total <- n x guestOsMemory
     IF hypervisorType = Type2 THEN total <- total + hostOsMemory END IF
     RETURN total

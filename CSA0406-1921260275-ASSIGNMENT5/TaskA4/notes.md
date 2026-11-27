# A4 - Evaluate containerisation (4 marks)
Labels: [M] measured by me in Part B, [T] standard/published fact, [A] assumption.

## A4.1 How containers work (2)
 - A container is an ordinary Linux process that the kernel shows a restricted view of (namespaces) and gives limited resources (cgroups). There is no guest kernel.
 - Namespaces: pid (own process numbers), net (own network interfaces and ports), mnt (own file system view), uts (own hostname), ipc, user, cgroup. [M] /health returned the container ID as the host name, so the container had its own hostname.
 - Control groups (cgroups): limit and account CPU, memory, process count and I/O. [M] --cpus 0.5 held the container at 49.69 % of a core; --memory 128m showed a limit of 128MiB in docker stats (usage 40.57MiB); a 200 MiB allocation under --memory 20m was killed (exit code 137).
 - Image layers: an image is a stack of read-only layers, one per Dockerfile instruction, combined with a union file system (overlayfs); a container adds one thin writable layer. Layers are cached and shared. [M] docker history showed base layers of 134.0MB (87.7 + 41.4 + 4.93 + 0.016), pip install 17.8MB and my code about 12 kB.

## A4.2 Containers versus virtual machines (2)
 Property          | Container [M]                          | VM [M]
 Start-up (median) | 0.77 s                                 | 14.18 s
 Memory            | 38MiB                                  | 553 MiB (host RSS of QEMU)
 Image / disk      | 49MB compressed, 201MB on disk         | 593M base image + 610M overlay
 Isolation         | shares the host kernel                 | own kernel, separated by the hypervisor
 Security          | a kernel flaw can affect all containers; reduce risk with a non-root user [M: whoami = portal], limited capabilities and seccomp [T] | smaller attack surface between guests [T]
Decision for the portal: run the stateless portal web tier in containers (fast start, cheap, scalable with Kubernetes); run the database in a VM or a managed database service (stronger isolation, stateful data), and keep the exam system separate from the public portal.

## Alternative concept: portal and database each in a VM instead of containers
 - Start-up: seconds for containers [M: 0.77 s] against about 14 s for my minimal VM [M] and minutes for a full server OS with a bigger image or cloud-init steps [A].
 - Memory per instance: each VM carries a whole guest OS [M: 553 MiB for one VM] while containers share the host kernel [M: 38MiB]. For n instances: VMs O(n x M_guest), containers O(M_kernel + n x M_app).
 - Isolation: VMs are stronger, because an attacker must break out of the hypervisor instead of the shared kernel.
 - Outcome: two VMs would work and be safer, but would cost about 14 times the memory per instance [C: 553 / 38 = 14.6] and be much slower to scale on result day.
 Pseudocode (memory for n instances):
   ALGORITHM MemoryNeeded(n, mode, guestOs, app, kernel)
     IF mode = VM THEN RETURN n x (guestOs + app) ELSE RETURN kernel + n x app END IF

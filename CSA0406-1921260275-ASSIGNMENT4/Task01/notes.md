# Task 1 – File-System Design Review (6 marks)

Labels: [C] calculated, [A] assumed, [M] measured. Calculations and the model tree come from `task01.sh` (output in `output.txt`).

## 1.1 Weaknesses of the proposal (2 marks)

| # | Weakness | Why it happens | Effect |
|---|----------|----------------|--------|
| 1 | Name clashes | One flat directory, names chosen freely; a directory cannot hold two entries with the same name | Two students saving `assignment.docx` overwrite each other |
| 2 | Slow search | Flat directory is scanned linearly, O(N) | See calculation below: about 300,000 comparisons on average |
| 3 | File growth | Contiguous allocation needs one unbroken run of blocks; the final size must be known in advance | A growing file must be copied elsewhere when the next block is taken; free space breaks into pieces (external fragmentation) |
| 4 | No protection or privacy | No access control, trust only | Anyone can read, change or delete anyone's files, by mistake or on purpose; marks and personal data are exposed |
| 5 | Backup and recovery | Everything is in one directory on one volume | One damaged directory loses the names of all 12,000 students; one student's file can only be restored by searching the whole backup; no per-department or incremental backup |
| 6 | No sharing | No groups, no ACLs, no links | Group projects are either open to all or impossible; people end up e-mailing copies |

### Search-cost calculation
Assumptions [A]: 12,000 students, 50 files each, 12 departments of equal size, 4 batches per department. Linear scan finds a name after N/2 comparisons on average.

- Flat directory [C]: N = 12,000 x 50 = 600,000 entries, so average = 600,000 / 2 = 300,000 comparisons.
- Improved tree [C]: 12/2 (department) + 4/2 (batch) + 250/2 (student) + 50/2 (file) = 6 + 2 + 125 + 25 = 158 comparisons.
- Ratio [C]: 300,000 / 158 = about 1,898 times fewer comparisons.

This assumes plain linear directories. Modern file systems (ext4 hashed directories) reduce a flat scan to about O(log N), but the clash, protection, backup and sharing problems remain.

## 1.2 Improved design (3 marks)

### Directory structure: tree, plus links to make it an acyclic graph
~~~
/students/<dept>/<batch>/<regno>/     private home, owner = student, mode 700
/staff/<dept>/<empid>/                private home, mode 700
/projects/<dept>/<project-id>/        group directory, group = project group, mode 2770
/courses/<code>/materials/            owner = faculty, mode 755 (students read only)
/courses/<code>/submit/               drop-box, mode 1733
~~~
Each member's home holds a symbolic link `P017 -> /projects/IT/P017`. Links point only to directories that are not ancestors of the link (and hard links to directories are not allowed), so the graph stays acyclic and there is only one copy of the project.

### Naming rules
- Home directory name = register number, which is unique, so name clashes between students cannot occur.
- File names: letters, digits, `.`, `_`, `-` only, maximum 255 bytes, no spaces.
- Names must be unique ignoring case (avoids clashes from Windows clients).
- Suggested pattern: `<course>_<assignment>_<regno>.<ext>`.

### File attributes
Owner (uid), group (gid), permission bits, ACL flag, type, size, created / modified / accessed times, link count, quota usage.

### Access control
| Place | Mode | Meaning |
|-------|------|---------|
| Student home | 700 `rwx------` | only the owner |
| Project directory | 2770 `rwxrws---` | owner and project group only; setgid so new files keep the group |
| Course materials | 755 | faculty write, everyone read |
| Submission drop-box | 1733 `rwx-wx-wt` | anyone can add a file, nobody can list it, sticky bit stops deleting another person's file |

Where bits are not enough (a faculty group needs read access to every submission), add an ACL entry for that group instead of widening the mode.

### Other changes
Indexed (inode-based) allocation instead of contiguous, so files grow in place. Per-department volumes or subtrees with quotas, and nightly incremental backup plus snapshots per subtree.

Evidence: `output.txt` shows the modes and links created on my machine. It is a model of the structure with modes and symlinks. It does not run 12,000 real users.

## 1.3 Design defence (1 mark)
Word count is printed in `output.txt` (limit 100). Text is in `defence.txt`.

## Alternative: cloud service (Google Drive / OneDrive)

**Solution.** Each student gets an account; the provider handles storage, indexing, versions and sharing links. The college manages accounts and policies through an admin console.

| Aspect | College server (my design) | Cloud service |
|--------|----------------------------|---------------|
| Time to find a file | Path lookup is O(depth), about 158 comparisons [C]; LAN round trip only | Server-side index, roughly O(log N) or hashed; but every open adds internet round-trip delay and login [A: tens of ms, not measured] |
| Time to open | Local network speed | Network speed; synced clients keep a local cache so repeat opens are fast |
| Storage used | One copy of data S, plus the college's own backup copies | Provider keeps several replicas (hidden from the quota), plus old versions and trash, plus a synced copy on each device the student uses; total is more than S |
| Privacy control | Full: college owns hardware, keys, permission bits, logs | College controls accounts and sharing policy only; data location and encryption keys sit with the provider; a "anyone with the link" share can leak a file |
| Running effort | College staff must run, patch and back up the server | Provider runs it; cost is per-user fees |

No measurements were made for this comparison; every statement above is reasoning, not data.

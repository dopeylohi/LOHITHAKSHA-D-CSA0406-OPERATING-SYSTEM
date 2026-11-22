import csv
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
rows = [r for r in csv.reader(open("fault_curve.csv")) if r and not r[0].startswith("#") and r[0] != "traversal"]
plt.figure(figsize=(6.5, 4))
for name in ("row-major", "column-major"):
    sel = [r for r in rows if r[0] == name]
    fr = [int(r[1]) for r in sel]
    plt.plot(fr, [int(r[2]) for r in sel], label=name + " LRU")
    plt.plot(fr, [int(r[3]) for r in sel], linestyle="--", label=name + " FIFO")
plt.yscale("log")
plt.xlabel("number of frames")
plt.ylabel("page faults (65,536 references)")
plt.title("Page faults vs frames, matrix traversal")
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig("fig_fault_curve.png", dpi=150)
print("wrote fig_fault_curve.png")

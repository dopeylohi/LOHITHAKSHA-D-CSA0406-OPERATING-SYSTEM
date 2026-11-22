import csv
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
rows = list(csv.DictReader(open("results.csv")))
for s in ("locality", "random"):
    r = [x for x in rows if x["string"] == s]
    fr = [int(x["frames"]) for x in r]
    plt.figure(figsize=(6, 4))
    for alg, mk in (("FIFO", "o"), ("LRU", "s"), ("Clock", "^"), ("Optimal", "x")):
        plt.plot(fr, [int(x[alg]) for x in r], marker=mk, label=alg)
    plt.xlabel("number of frames")
    plt.ylabel("page faults (10,000 references)")
    plt.title("Page faults vs frames, %s string" % s)
    plt.legend()
    plt.grid(True)
    plt.tight_layout()
    plt.savefig("fig_faults_%s.png" % s, dpi=150)
    print("wrote fig_faults_%s.png" % s)

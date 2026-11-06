# times every runtime bench, main vs the fix, on this mac's cpu
import os, re, statistics, subprocess, sys, time
B = "bfs editdist gameoflife hashmap histogram kmeans lexer mandelbrot merkle nbody queens raytrace symreg terrain tree-bitonic tree-matmul tree-radix".split()
ROUNDS = int(os.environ.get("ROUNDS", "7"))
SIDES = {"main": "../main", "fix": "."}
os.makedirs("out", exist_ok=True)

def build(side, b):
    c = f"out/{b}-{side}.c"
    subprocess.run(["bun", "bend2/main.ts", f"bench/runtime/{b}/main.bend", "-o", os.path.abspath(c)], cwd=SIDES[side], check=True, capture_output=True)
    src = open(c).read()
    objc = ["-x", "objective-c", "-fobjc-arc", "-fmodules"] if re.search(r"^#import ", src, re.M) else []
    subprocess.run(["clang", *objc, "-std=c11", "-O3", c, "-lpthread", "-lm", "-o", f"out/{b}-{side}"], check=True)

def run(side, b):
    t = time.perf_counter()
    p = subprocess.run([f"out/{b}-{side}"], capture_output=True, text=True, timeout=600)
    return time.perf_counter() - t, p.stdout.strip()

rows = []
for b in B:
    for s in SIDES: build(s, b)
    times = {s: [] for s in SIDES}; outs = {}
    for _ in range(ROUNDS):
        for s in SIDES:
            dt, o = run(s, b); times[s].append(dt); outs.setdefault(s, o)
    m, f = statistics.median(times["main"]), statistics.median(times["fix"])
    rows.append((b, m, f, f / m - 1, outs["main"] == outs["fix"], outs["fix"][:12]))
    print(f"{b:13} main {m:7.3f}s  fix {f:7.3f}s  {100*(f/m-1):+6.1f}%  same output: {outs['main'] == outs['fix']}", flush=True)

print("\n| bench | main (s) | fix (s) | change | same output |\n|---|---|---|---|---|")
for b, m, f, d, same, o in rows:
    print(f"| {b} | {m:.3f} | {f:.3f} | {100*d:+.1f}% | {'yes' if same else 'NO'} |")
geo = statistics.geometric_mean([f / m for _, m, f, *_ in rows])
print(f"\ngeomean fix/main: {geo:.4f} ({100*(geo-1):+.2f}%), median of {ROUNDS} interleaved runs each")
sys.exit(0 if all(r[4] for r in rows) else 1)

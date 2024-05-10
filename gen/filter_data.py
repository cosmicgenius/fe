"""
Filters out unparsable or "easy" GPT generated FEs
"""
import sys
import os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")) # Stupid python

import re
import time
from subprocess import Popen, PIPE, TimeoutExpired, DEVNULL
from config import build_path, gen_fe_path, filtered_fe_path

stat_regex = re.compile(r'\[w=(\d+),nw=(\d+),d=(\d+),la=(\d+)\]')
eval_period = 100

def main():
    gen = []
    with open(gen_fe_path, 'r', encoding="utf-8") as f:
        gen += f.readlines()
    
    print(f"Read {len(gen)} lines of clean data")

    filtered = set()
    t0 = time.time()
    for i, fe in enumerate(gen):
        proc = Popen([build_path, "--pretty=false", "--simp=2", "--simp_timeout=800"], 
                     encoding="utf-8", stdin=PIPE, stdout=PIPE, stderr=DEVNULL)
        try:
            stdout, _ = proc.communicate(f"h {fe}\ne\n", timeout=1)
            stats = [tuple(int(s) for s in m) for m in stat_regex.findall(stdout)]

            # must parse and have no solution
            # of the form f(a x + b) = stuff with no f's
            if len(stats) > 0 and all(s[1] != 2 for s in stats):
                filtered.add(fe)

        except TimeoutExpired:
            print(f"Number {i} taking too long; killed after 1s")
            proc.kill()
            stdout, _ = proc.communicate()

        if (i + 1) % eval_period == 0:
            print(f"Line {i + 1}: last {eval_period} lines took {time.time() - t0:.3f}s")
            t0 = time.time()

    filtered = list(filtered)

    print(f"Filtered to {len(filtered)} lines")

    with open(filtered_fe_path, 'w', encoding="utf-8") as f:
        f.write(''.join(filtered))

if __name__ == '__main__':
    main()

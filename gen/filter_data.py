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
blank_line_regex = re.compile(r'$^', flags=re.MULTILINE)
batch_size = 100

def main():
    gen = []
    with open(gen_fe_path, 'r', encoding="utf-8") as f:
        gen = f.readlines()
    
    print(f"Read {len(gen)} lines of clean data")

    filtered = set()
    t0 = time.time()
    l0 = len(filtered)
    for batch in range(0, len(gen), batch_size):
        size = len(gen[batch:batch+batch_size])
        proc = Popen([build_path, "--pretty=false", "--simp=2", "--simp_timeout=800", f"--batch_size={size}", "--threads=8"], 
                     encoding="utf-8", stdin=PIPE, stdout=PIPE, stderr=DEVNULL)
        try:
            stdout, _ = proc.communicate(''.join("h " + fe + "\ne\n" for fe in gen[batch:batch+size]), timeout=10)
            blocks = blank_line_regex.split(stdout)[2::2] # first block is echoed hypotheses, odd blocks are simplification echoes
            for i, b in enumerate(blocks):
                stats = [tuple(int(s) for s in m) for m in stat_regex.findall(b)]

                # must,
                # 1) parse 
                # 2) have no solution of the form f(a x + b) = stuff with no f's
                # 3) have some f(x)
                if len(stats) > 0 and all(s[1] != 2 for s in stats) and not all(s[1] == 0 for s in stats):
                    filtered.add(gen[batch + i])

        except TimeoutExpired:
            print(f"Batch {batch // batch_size + 1} taking too long; killed after 10s")
            proc.kill()
            stdout, _ = proc.communicate()

        print(f"Batch {batch // batch_size + 1} (Lines {batch + 1}-{batch + size}): " + 
              f"Took {time.time() - t0:.3f}s and got {len(filtered) - l0} new lines")
        t0 = time.time()
        l0 = len(filtered)

    filtered = list(filtered)

    print(f"Filtered to {len(filtered)} lines")

    with open(filtered_fe_path, 'w', encoding="utf-8") as f:
        f.write(''.join(filtered))

if __name__ == '__main__':
    main()

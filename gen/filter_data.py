# Filters out unparsable or "easy" GPT generated FEs
import sys
import os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")) # Stupid python

import re
from subprocess import Popen, PIPE, TimeoutExpired, DEVNULL
from config import build_path, gen_path, filtered_path

wregex = re.compile(r'\[weight=(\d+)]')
timeout = 1

def main():
    gen = []
    with open(gen_path, 'r', encoding="utf-8") as f:
        gen += f.readlines()
    
    print(f"Read {len(gen)} lines of clean data")

    filtered = set()
    for i, fe in enumerate(gen):
        proc = Popen([build_path, "--pretty=false", "--simp=2"], 
                     encoding="utf-8", stdin=PIPE, stdout=PIPE, stderr=DEVNULL)
        try:
            stdout, _ = proc.communicate(f"h {fe}\ne\n", timeout=timeout)
            weights = [int(w) for w in wregex.findall(stdout)]

            # must parse and have no linear solution
            # of the form a f(x) + b x = c where a,b,c are constants 
            if len(weights) > 0 and all(w == 0 or w > 6 for w in weights):
                filtered.add(fe)

        except TimeoutExpired:
            print(f"Number {i} taking too long; killed after {timeout}s")
            proc.kill()
            stdout, _ = proc.communicate()

    filtered = list(filtered)

    print(f"Filtered to {len(filtered)} lines")

    with open(filtered_path, 'w', encoding="utf-8") as f:
        f.write(''.join(filtered))

if __name__ == '__main__':
    main()

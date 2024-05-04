# Randomizes cleaned data
import sys
import os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")) # Stupid python

from subprocess import Popen, PIPE, TimeoutExpired, DEVNULL
from random import shuffle
from config import build_path, clean_path, rand_path

def main():
    cycles = int(input("How many randomization cycles?: "))
    clean = []
    with open(clean_path, 'r', encoding="utf-8") as f:
        clean += f.readlines()
    
    print(f"Read {len(clean)} lines of clean data")

    rands = set()
    for c in range(cycles):
        proc = Popen([build_path, "-ng", "-np", "-r"], encoding="utf-8", stdin=PIPE, stdout=PIPE, stderr=DEVNULL)
        try:
            stdout, _ = proc.communicate(''.join(f"h {p}" for p in clean) + "\ne\n", timeout=(len(clean) / 10))
            rands.update(stdout.split('\n'))
        except TimeoutExpired:
            print(f"Taking too long; killed after {len(clean) / 10}s")
            proc.kill()
            stdout, _ = proc.communicate()
        print(f"Cycle {c + 1}: {len(rands)} total results")

    # Shuffle
    rands = list(rands)
    shuffle(rands)

    # Remove spaces
    rands = [l.replace(' ', '') for l in rands]

    with open(rand_path, 'w', encoding="utf-8") as f:
        f.write('\n'.join(rands))
    print(f"Saved {len(rands)} lines of randomized data")

if __name__ == '__main__':
    main()

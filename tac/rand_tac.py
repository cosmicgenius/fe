# Generates training data for the tactic gpt 
import sys
import os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")) # Stupid python

import numpy as np
import random
import itertools
from config import filtered_fe_path, rand_tac_path

h_dropoff = 0.50
s_reps = 32000
a_reps = 8000

sub_dropoff = 0.15
x_dropoff = 0.5

def parse_inner(s):
    nested = 0
    last = 0
    for i, c in enumerate(s):
        if c == '(':
            if nested == 0:
                last = i
            nested += 1
        elif c == ')':
            nested -= 1
            if nested == 0:
                yield s[last + 1:i]

def main():
    with open(filtered_fe_path, 'r') as f:
        lines = [l.strip() for l in f.readlines()]
    
    print(f"Read {len(lines)} lines from {filtered_fe_path}")

    generated = []
    tot_lines = 0
    for reps, symb in [(s_reps, 's'), (a_reps, 'a')]:
        h_lens = np.random.geometric(p=h_dropoff, size=reps)
        h_batches = np.split(np.random.choice(lines, size=sum(h_lens)), np.cumsum(h_lens)[:-1])
        h_rand = np.random.uniform(size=reps)

        tot_lines += sum(h_lens) + reps

        # if sub
        if symb == 's':
            # look at stuff inside f(), and try that as a substitution
            possible_subs = sorted(set(sub for line in lines for sub in parse_inner(line)), key=len)
            print(f"Found {len(possible_subs)} possible substitutions")

            max_sub_len = len(possible_subs[-1])
            prob_cache = [(1 - sub_dropoff) ** l for l in range(max_sub_len + 1)]

            sub_probs = np.array([prob_cache[len(sub)] for sub in possible_subs])
            sub_lines = np.random.choice(possible_subs, size=reps, p=sub_probs / sum(sub_probs))

            # Make x1 and x2 equally probable
            x_probs = np.array([x_dropoff, *[x_dropoff * (1 - x_dropoff) ** i for i in range(5)]])
            sub_x = np.random.choice(range(len(x_probs)), size=reps, p=x_probs / sum(x_probs)) + 1

            generated.extend('\n'.join(itertools.chain(
                                ('h ' + l for l in batch),
                                (f"{symb} h{1 + int(hi * len(batch))} x{xi} {line}",)
                            )) for batch, line, hi, xi in zip(h_batches, sub_lines, h_rand, sub_x))

        else:
            add_lines = np.random.choice(lines, size=reps)

            generated.extend('\n'.join(itertools.chain(
                                ('h ' + l for l in batch),
                                (f"{symb} h{1 + int(hi * len(batch))} {line}",)
                            )) for batch, line, hi in zip(h_batches, add_lines, h_rand))

    random.shuffle(generated)
    with open(rand_tac_path, 'w') as f:
        f.writelines('\n\n'.join(generated))
    print(f"Wrote {len(generated)} distinct scenarios ({tot_lines} total content lines) to {rand_tac_path}")


if __name__ == '__main__':
    main()

# Generates training data for the tactic gpt 
import sys
import os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")) # Stupid python

import numpy as np
import random
from config import filtered_path, rand_tac_path

h_dropoff = 0.50
s_reps = 32000
a_reps = 8000

sub_dropoff = 0.12
x_dropoff = 0.5

def main():
    with open(filtered_path, 'r') as f:
        lines = [l.strip() for l in f.readlines()]

    generated = []
    for reps, symb in [(s_reps, 's'), (a_reps, 'a')]:
        h_lens = np.random.geometric(p=h_dropoff, size=reps)
        h_batches = np.split(np.random.choice(lines, size=sum(h_lens)), np.cumsum(h_lens)[:-1])
        h_rand = np.random.uniform(size=reps)

        # if sub
        if symb == 's':
            l_probs = np.array([sub_dropoff * (1 - sub_dropoff) ** len(l) for l in lines])
            sub_lines = np.random.choice(lines, size=reps, p=l_probs / sum(l_probs))

            # Make x1 and x2 equally probable
            x_probs = np.array([x_dropoff, *[x_dropoff * (1 - x_dropoff) ** i for i in range(5)]])
            sub_x = np.random.choice(range(len(x_probs)), size=reps, p=x_probs / sum(x_probs)) + 1

            for batch, line, hi, xi in zip(h_batches, sub_lines, h_rand, sub_x):
                generated.append('\n'.join(['h ' + l for l in batch] + [f"{symb} h{1 + int(hi * len(batch))} x{xi} {line}"]))

        else:
            add_lines = np.random.choice(lines, size=reps)

            for batch, line, hi in zip(h_batches, add_lines, h_rand):
                generated.append('\n'.join(['h ' + l for l in batch] + [f"{symb} h{1 + int(hi * len(batch))} {line}"]))

    random.shuffle(generated)
    with open(rand_tac_path, 'w') as f:
        f.writelines('\n\n'.join(generated))

if __name__ == '__main__':
    main()

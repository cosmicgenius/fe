"""
Tokenize fine tuning training data

Currently uses generic filtered gen data
"""
import sys
import os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")) # Stupid python

import pickle
import numpy as np
from config import meta_tac_path, filtered_fe_path, finetune_tac_path
from tokenize_base import bpe_replace

def main():
    print(f"Loading meta from {meta_tac_path} and data from {filtered_fe_path}...")
    with open(meta_tac_path, 'rb') as f:
        meta = pickle.load(f)

    if meta['args']['encoding'] == 'bpe':
        stoi, itos, enc, dec = meta['stoi'], meta['itos'], meta['enc'], meta['dec']
        def encode(s):
            L = [stoi[c] for c in s]
            for k, pair in enc.items():
                L = bpe_replace(L, pair[0], pair[1], k)
            return L
        def decode(l):
            return ''.join(dec[i] for i in l)

    elif meta['args']['encoding'] == 'char':
        stoi, itos = meta['stoi'], meta['itos']
        def encode(s):
            return [stoi[c] for c in s]
        def decode(l):
            return ''.join(itos[i] for i in l)
    
    else: 
        raise ValueError(f"Unknown encoding {meta['args']['encoding']!r}")

    with open(filtered_fe_path, 'r') as f:
        data = [f"h {l}" for l in f]
    print(data[:10])

    print("Encoding data...")

    #ids = np.array(encode(data), dtype=np.uint16)
    #ids.tofile(finetune_tac_path)

if __name__ == '__main__':
    main()


"""
Basic byte pair encoder with some 
dirty pickling for saving
"""
import pickle
import numpy as np
import itertools
from collections import Counter

from config import TokenizeConfig

# Replace all instances of [a, b] with c in L
def bpe_replace(L, a, b, c):
    replace_idx = [i for i in range(len(L) - 1) if L[i] == a and L[i+1] == b]
    if len(replace_idx) == 0: return L

    # delete overlaps, slow, but probabily ok
    mask = [True] * len(replace_idx)
    for i in range(1, len(replace_idx)):
        if replace_idx[i] - replace_idx[i-1] == 1 and mask[i-1]:
            mask[i] = False
    replace_idx = list(itertools.compress(replace_idx, mask))

    return L[:replace_idx[0]] + [c] + list(
           itertools.chain.from_iterable(L[i+2:j] + [c] for i, j in zip(replace_idx[:-1], replace_idx[1:]))) + \
           L[replace_idx[-1]+2:]

def tokenize(data: str, config: TokenizeConfig):
    chars = sorted(list(set(data)))
    vocab_size = len(chars)

    print(f"Number of distinct characters: {vocab_size:,}")

    # char to int
    stoi = { ch: i  for i, ch in enumerate(chars) }
    itos = { i : ch for i, ch in enumerate(chars) }


    if config.encoding == 'bpe':
        enc = {}
        # list where bpe[i] = s iff s encodes to i
        dec = list(chars)

        encoded_data = [stoi[c] for c in data]
        while vocab_size < config.max_vocab_size:
            if len(encoded_data) < 2: break

            # slow but that's ok
            counter = Counter(zip(encoded_data[:-1], encoded_data[1:]))
            pair, _ = counter.most_common(1)[0]

            enc[vocab_size] = pair
            dec.append(dec[pair[0]] + dec[pair[1]])
            print(f"Compressed pair: {(dec[pair[0]] + dec[pair[1]])!r}")
            
            encoded_data = bpe_replace(encoded_data, pair[0], pair[1], vocab_size)
            vocab_size += 1

        # Encode and decode
        def encode(s):
            L = [stoi[c] for c in s]
            for k, pair in enc.items():
                L = bpe_replace(L, pair[0], pair[1], k)
            return L
            
        #def decode(l):
        #    return ''.join(dec[i] for i in l)

        # Save meta information to help encoding/decoding later
        meta = {
            'args': config.__dict__,
            'vocab_size': vocab_size,
            'itos': itos,
            'stoi': stoi,
            'enc': enc,
            'dec': dec,
        }
    elif config.encoding == 'char': 
        def encode(s):
            return [stoi[c] for c in s]
        #def decode(l):
        #    return ''.join(itos[i] for i in l)
        meta = {
            'args': config.__dict__,
            'vocab_size': vocab_size,
            'itos': itos,
            'stoi': stoi,
        }
    else:
        raise ValueError(f"Unknown encoding: {config.encoding}")

    #print(encoded_data == encode(data))
    #print(data == decode(encoded_data))

    split = 0.9
    n = len(data)
    train_data = data[:int(n * split)]
    val_data = data[int(n * split):]

    # Encode to integers
    train_ids = encode(train_data)
    val_ids = encode(val_data)

    return train_ids, val_ids, meta

def tokenize_file(config: TokenizeConfig, input_path, train_path, val_path, meta_path):
    data = ""
    with open(input_path, 'r') as f:
        data = f.read()
    print(f"Total number of characters in dataset: {len(data):,}")

    train_ids, val_ids, meta = tokenize(data, config)
    vocab_size = meta['vocab_size']

    print(f"Final vocab size: {vocab_size:,}")
    print(f"train has {len(train_ids):,} tokens")
    print(f"val has {len(val_ids):,} tokens")

    # Export to files
    train_ids = np.array(train_ids, dtype=np.uint16)
    val_ids = np.array(val_ids, dtype=np.uint16)
    train_ids.tofile(train_path)
    val_ids.tofile(val_path)

    with open(meta_path, 'wb') as f:
        pickle.dump(meta, f)

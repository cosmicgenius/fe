"""
Encode by mapping chars to ints.
Basically verbatim from NanoGPT: https://github.com/karpathy/nanoGPT
"""
import pickle
import numpy as np
from config import rand_path, train_path, val_path, meta_path

def main():
    data = ""
    with open(rand_path, 'r') as f:
        data = f.read()
    print(f"length of dataset in characters: {len(data):,}")

    # vocab = all chars
    chars = sorted(list(set(data)))
    vocab_size = len(chars)
    print("all the unique characters:", ''.join(chars))
    print(f"vocab size: {vocab_size:,}")

    # Encode and decode
    stoi = { ch: i  for i, ch in enumerate(chars) }
    itos = { i : ch for i, ch in enumerate(chars) }
    def encode(s):
        return [stoi[c] for c in s] 
    #def decode(l):
    #    return ''.join([itos[i] for i in l])

    split = 0.9
    n = len(data)
    train_data = data[:int(n * split)]
    val_data = data[int(n * split):]

    # Encode to integers
    train_ids = encode(train_data)
    val_ids = encode(val_data)
    print(f"train has {len(train_ids):,} tokens")
    print(f"val has {len(val_ids):,} tokens")

    # Export to files
    train_ids = np.array(train_ids, dtype=np.uint16)
    val_ids = np.array(val_ids, dtype=np.uint16)
    train_ids.tofile(train_path)
    val_ids.tofile(val_path)

    # Save meta information to help encoding/decoding later
    meta = {
        'vocab_size': vocab_size,
        'itos': itos,
        'stoi': stoi,
    }

    with open(meta_path, 'wb') as f:
        pickle.dump(meta, f)

if __name__ == '__main__':
    main()

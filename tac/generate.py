"""
Sample from a trained model
Based on https://github.com/karpathy/nanoGPT/blob/main/sample.py
"""
import sys
import os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")) # Stupid python

import os
import pickle
import itertools
import time
from contextlib import nullcontext
import torch
from gpt import GPT
from config import models_path, meta_tac_path, GPTConfig, GenConfig, parse_args
from tokenize_base import bpe_replace

def generate(config: GenConfig, meta_path, models_path):
    print("Set configuration:")
    for k, v in config.__dict__.items():
        print(f"{k} = {v!r}")

    if torch.cuda.is_available() and torch.cuda.is_bf16_supported():
        config.dtype = 'bfloat16'

    if config.seed is not None:
        torch.manual_seed(config.seed)
        torch.cuda.manual_seed(config.seed)

    torch.backends.cuda.matmul.allow_tf32 = True # allow tf32 on matmul
    torch.backends.cudnn.allow_tf32 = True # allow tf32 on cudnn
    device_type = 'cuda' if 'cuda' in config.device else 'cpu' # for later use in torch.autocast
    ptdtype = {'float32': torch.float32, 'bfloat16': torch.bfloat16, 'float16': torch.float16}[config.dtype]
    ctx = nullcontext() if device_type == 'cpu' else torch.amp.autocast(device_type=device_type, dtype=ptdtype) # type: ignore

    # Init from a model saved in models_path
    print(f"Loading model from {os.path.join(models_path, config.model)}...")
    model_path = os.path.join(models_path, config.model)
    checkpoint = torch.load(model_path, map_location=config.device)
    gptconf = GPTConfig(**checkpoint['model_args'])
    model: GPT = GPT(gptconf)
    state_dict = checkpoint['model']

    # Stupid prefix again
    unwanted_prefix = '_orig_mod.'
    for k,v in list(state_dict.items()):
        if k.startswith(unwanted_prefix):
            state_dict[k[len(unwanted_prefix):]] = state_dict.pop(k)
    model.load_state_dict(state_dict)

    model.eval()
    model.to(config.device)
    if config.compile:
        print("Compiling model...")
        model: GPT = torch.compile(model) # type: ignore ; requires PyTorch 2.0 (optional)

    # look for the meta pickle in case it is available in the dataset folder
    print(f"Loading meta from {meta_path}...")
    with open(meta_path, 'rb') as f:
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

    # encode the beginning of the prompt
    start_ids = encode(config.start)

    block_size = gptconf.block_size
    max_new_tokens = 32
    batch_size = checkpoint['config']['batch_size']

    x = torch.tensor(start_ids, dtype=torch.long, device=config.device).repeat(batch_size, 1)

    # run generation
    out = ["" for _ in range(batch_size)]
    ended = [False] * batch_size
    iteration = 0
    with torch.no_grad():
        with ctx:
            while not all(ended):
                x = model.generate(x, max_new_tokens, temperature=config.temperature, top_k=config.top_k)
                new_data = [decode(x[i, -max_new_tokens:].tolist()) for i in range(batch_size)]

                for i in range(batch_size):
                    if not ended[i]:
                        out[i] += new_data[i]
                        ended[i] = ('\n' in new_data[i])
                        if ended[i]:
                            out[i] = out[i].split('\n')[0]

                # if over block size, cut down
                if x.size(1) > block_size:
                    x = x[:, -block_size:]
                
                # timing and logging
                iteration += 1

    print('\n-----\n'.join(config.start + rest for rest in out))

def main():
    args = parse_args()
    config = GenConfig(**args)

    generate(config, meta_tac_path, models_path)

if __name__ == '__main__':
    main()

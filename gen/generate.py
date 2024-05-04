"""
Sample from a trained model
Based on https://github.com/karpathy/nanoGPT/blob/main/sample.py
"""
import sys
import os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")) # Stupid python

import pickle
from contextlib import nullcontext
import torch
from gpt import GPT
from config import meta_path, models_path, GPTConfig, GenConfig, parse_args
from tokenize_data import bpe_replace

args = parse_args()
config = GenConfig(**args)

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

stoi, itos, enc, dec = meta['stoi'], meta['itos'], meta['enc'], meta['dec']
def encode(s):
    L = [stoi[c] for c in s]
    for k, pair in enc.items():
        L = bpe_replace(L, pair[0], pair[1], k)
    return L
    
def decode(l):
    return ''.join(dec[i] for i in l)

# encode the beginning of the prompt
start_ids = encode(config.start)
x = (torch.tensor(start_ids, dtype=torch.long, device=config.device)[None, ...])

max_new_tokens = 1024 # generate tokens in blocks of 1024
block_size = gptconf.block_size

# run generation
out = ""
lines = 0
with torch.no_grad():
    with ctx:
        while lines < config.num_lines:
            x = model.generate(x, max_new_tokens, temperature=config.temperature, top_k=config.top_k)
            new_data = decode(x[0, -max_new_tokens:].tolist())

            lines += new_data.count('\n')
            out += new_data
            # if over block size, cut down
            if x.size(1) > block_size:
                x = x[:, -block_size:]
            print(f"Generated {lines} lines")

print('\n'.join(out.split('\n')[:config.num_lines]))

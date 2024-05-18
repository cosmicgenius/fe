"""
Fine tune the tac language model
"""
import sys
import os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")) # Stupid python

import time
import pickle
from contextlib import nullcontext

import numpy as np
import torch
import dataclasses
import itertools
import re

from subprocess import Popen, PIPE, TimeoutExpired, DEVNULL

from gpt import GPT
from config import models_path, filtered_fe_path, build_path, meta_tac_path, parse_args, GPTConfig, FineTuneConfig, ft_configs
from tokenize_base import bpe_replace

stat_regex = re.compile(r'\[w=(\d+),nw=(\d+),d=(\d+),la=(\d+)\]')
blank_line_regex = re.compile(r'$^', flags=re.MULTILINE)

def ftune(config: FineTuneConfig, data_path, build_path, meta_path, models_path):
    print("Set configuration:")
    for k, v in config.__dict__.items():
        print(f"{k} = {v!r}")

    if torch.cuda.is_available() and torch.cuda.is_bf16_supported():
        config.dtype = 'bfloat16'

    tokens_per_iter = config.gradient_acc_steps * config.batch_size * config.block_size
    print(f"tokens per iteration will be: {tokens_per_iter:,}")

    torch.backends.cuda.matmul.allow_tf32 = True # allow tf32 on matmul
    torch.backends.cudnn.allow_tf32 = True # allow tf32 on cudnn
    device_type = 'cuda' if 'cuda' in config.device else 'cpu' # for later use in torch.autocast
    # note: float16 data type will automatically use a GradScaler
    ptdtype = {'float32': torch.float32, 'bfloat16': torch.bfloat16, 'float16': torch.float16}[config.dtype]
    ctx = nullcontext() if device_type == 'cpu' else torch.amp.autocast(device_type=device_type, dtype=ptdtype) # type: ignore

    with open(data_path, 'r') as f:
        data = np.array([l.strip() for l in f])

    # init these up here, can override if init_from='resume' (i.e. from a checkpoint)
    iter_num = 0

    # Get information from meta file
    with open(meta_path, 'rb') as f:
        meta = pickle.load(f)

    if meta['args']['encoding'] == 'bpe':
        stoi, enc, dec = meta['stoi'], meta['enc'], meta['dec']
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
            return ''.join(itos[int(i)] for i in l)

    else: 
        raise ValueError(f"Unknown encoding {meta['args']['encoding']!r}")

    vocab_size: int = meta['vocab_size']
    nl_tok: int = encode('\n')[0]
    print(f"Set vocab_size = {vocab_size}")

    # model init
    model_args = { f.name : getattr(config, f.name) for f in dataclasses.fields(GPTConfig) if f.name != 'vocab_size' }
    model_args['vocab_size'] = vocab_size # vocab_size comes from the data not the config

    # cant start fine tuning without a pre-trained model
    if config.source.endswith('.pt'):
        source_path = os.path.join(models_path, config.source)
        if config.override: config.out = source_path

        if not os.path.exists(source_path) or not os.path.isfile(source_path):
            raise ValueError(f"File {source_path:!s} not found")

        print(f"Fine-tuning the model at {source_path}")
        checkpoint = torch.load(source_path, map_location=config.device)

        # force these config attributes to be equal otherwise we can't even resume training
        # the rest of the attributes (e.g. dropout) can stay as desired from command line
        for k in ['n_layer', 'n_head', 'n_embd', 'block_size', 'bias', 'vocab_size']:
            if model_args[k] != checkpoint['model_args'][k]:
                raise ValueError(f"cannot resume training from model of different size \
                                 (config: {model_args[k]!r}, checkpoint: {checkpoint['model_args'][k]!r})")

        # create the model
        gptconf = GPTConfig(**model_args)
        model: GPT = GPT(gptconf)
        state_dict = checkpoint['model']

        # fix the keys of the state dictionary :(
        # honestly no idea how checkpoints sometimes get this prefix, have to debug more
        unwanted_prefix = '_orig_mod.'
        for k,v in list(state_dict.items()):
            if k.startswith(unwanted_prefix):
                state_dict[k[len(unwanted_prefix):]] = state_dict.pop(k)

        model.load_state_dict(state_dict)
        iter_num = checkpoint['iter_num']

        model.to(config.device)

        optimizer = model.configure_optimizers(config.weight_decay, config.learning_rate, (config.beta1, config.beta2), device_type)
        optimizer.load_state_dict(checkpoint['optimizer'])

        checkpoint = None # free up memory ??
    else:
        raise ValueError(f"Unknown source value {config.source:!s}")

    # initialize a GradScaler. If enabled=False scaler is a no-op
    scaler = torch.cuda.amp.GradScaler(enabled=(device_type == "cuda" and config.dtype == 'float16'))

    def get_batch(prompt_char):
        ix = np.random.randint(len(data))
        x = torch.tensor(encode(f"h {data[ix]}\n{prompt_char} ")).repeat(config.batch_size, 1) # (B, T) for some T

        if device_type == 'cuda':
            # pin x which allows us to move them to GPU asynchronously (non_blocking=True)
            x = x.pin_memory().to(config.device, non_blocking=True)
        else:
            x = x.to(config.device)
        return x

    # some heurstic reward function (negated) for on policy rl
    def neg_reward(tl, w, nw, d, la):
        return tl + 3 * w + 10 * nw + 2 * d * d + 0.3 * la

    def total_neg_reward(block, tac):
        # kill duplicates to stop behavior like f(x1) = x1 and f(x2) = x2 being two different things
        rewards = sorted(set(
            neg_reward(len(tac), *[int(s) for s in m]) 
            for m in stat_regex.findall(block)
        ))
        # top 3 rewards
        return sum(rewards[:3]) / len(rewards[:3])
        

    def get_rewards(base_prob, tacs):
        proc = Popen([build_path, "--pretty=false", "--simp=2", "--simp_timeout=800", 
                      f"--batch_size={config.batch_size + 1}", "--threads=8"], 
                     encoding="utf-8", stdin=PIPE, stdout=PIPE, stderr=DEVNULL)

        try:
            stdout, _ = proc.communicate(''.join([base_prob] + tacs), timeout=30)

            blocks = blank_line_regex.split(stdout)[2::2] # first block is echoed hypotheses, odd blocks are simplification echoes

            base_reward = total_neg_reward(blocks[0], base_prob) # calculate the reward for doing nothing, then subtract this
                                                                 # from the other rewards to calculate the "difference"

            return [total_neg_reward(b, t) - base_reward for b, t in zip(blocks[1:], tacs)]

        except TimeoutExpired:
            # Really shouldn't happen, 30s is a lot of time
            print(f"Batch taking too long; killed after 30s")
            proc.kill()
            stdout, _ = proc.communicate()

        return torch.full((len(tacs),), torch.inf)

    # compile the model
    if config.compile:
        print("compiling the model... (takes a ~minute)")
        unoptimized_model = model
        # requires PyTorch 2.0
        model: GPT = torch.compile(model) # type: ignore (complains about compiled model being a function)

    # training loop
    x = get_batch(config.tactic)
    print(x[0])
    t0 = time.time()
    local_iter_num = 0 # number of iterations in the lifetime of this process
    raw_model = model
    running_mfu = -1.0
    eps = 1e-6

    while True:
        # determine and set the learning rate for this iteration
        lr = config.learning_rate
        for param_group in optimizer.param_groups:
            param_group['lr'] = lr

        # evaluate the loss on train/val sets and write checkpoints
        """
        if iter_num % config.eval_interval == 0:
            losses = estimate_loss()
            print(f"step {iter_num}: train loss {losses['train']:.4f}, val loss {losses['val']:.4f}")
            if losses['val'] < best_val_loss or config.always_save_checkpoint:
                best_val_loss = losses['val']
                if iter_num > 0:
                    out_path = os.path.join(models_path, config.out)
                    checkpoint = {
                        'model': raw_model.state_dict(),
                        'model_args': model_args,
                        'optimizer': optimizer.state_dict(),
                        'iter_num': iter_num,
                        'best_val_loss': best_val_loss,
                        'config': config.__dict__,
                    }
                    print(f"saving checkpoint to {out_path}")
                    torch.save(checkpoint, out_path)
        """

        # forward backward update, with optional gradient accumulation to simulate larger batch size
        # and using the GradScaler if data type is float16
        loss = None
        for _ in range(config.gradient_acc_steps):
            with ctx:
                idx = torch.clone(x)
                idx, last_logits = model.forward_until_tok(idx, nl_tok)
                
                base_prob = '\n'.join(decode(x[0]).split('\n')) + '\ne\n' # remove last 
                tacs = [
                    decode(t[:x.shape[1]])
                    + decode(itertools.takewhile(lambda tok: tok != nl_tok, t[x.shape[1]:]))
                    + '\ne\n' for t in idx
                ]

                print("generated")

                rewards = torch.tensor(get_rewards(base_prob, tacs)).to(device=config.device)
                results = (rewards - rewards.mean()) / (rewards.std() + eps)

                ex = int(torch.argmin(rewards).item())
                print(f"Ex:\n{tacs[ex]}")
                print(f"R={rewards[ex].item()}")
                print(f"z={results[ex].item()}")
                                       
                loss = (results * last_logits).sum()
                loss = loss / config.gradient_acc_steps # scale the loss to account for gradient accumulation

            x = get_batch(config.tactic)

            # backward pass, with gradient scaling if training in fp16
            scaler.scale(loss).backward()

        # clip the gradient
        if config.grad_clip != 0.0:
            scaler.unscale_(optimizer)
            torch.nn.utils.clip_grad_norm_(model.parameters(), config.grad_clip)

        # step the optimizer and scaler if training in fp16
        scaler.step(optimizer)
        scaler.update()
        # flush the gradients as soon as we can, no need for this memory anymore
        optimizer.zero_grad(set_to_none=True)

        # timing and logging
        t1 = time.time()
        dt = t1 - t0
        t0 = t1
        if iter_num % config.log_interval == 0 and loss != None:
            # get loss as float. note: this is a CPU-GPU sync point
            # scale up to undo the division above, approximating the true total loss (exact would have been a sum)
            lossf = loss.item() * config.gradient_acc_steps
            if local_iter_num >= 5: # let the training loop settle a bit
                mfu = raw_model.estimate_mfu(config.batch_size * config.gradient_acc_steps, dt)
                running_mfu = mfu if running_mfu == -1.0 else 0.9*running_mfu + 0.1*mfu
            print(f"Iter {iter_num}: loss {lossf:.4f}, time {dt*1000:.2f}ms, mfu {running_mfu*100:.2f}%")

        iter_num += 1
        local_iter_num += 1

        # termination conditions
        if iter_num > config.max_iters:
            break

def main():
    args = parse_args()
    if 'config' in args and args['config'] is not None:
        config = ft_configs[args['config']]
        del args['config']

        config = FineTuneConfig(**(config | args)) # type: ignore
    else:
        config = FineTuneConfig(**args)

    ftune(config, filtered_fe_path, build_path, meta_tac_path, models_path)

if __name__ == '__main__':
    main()


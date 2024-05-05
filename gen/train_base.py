"""
Training script based on NanoGPT: https://github.com/karpathy/nanoGPT/blob/master/train.py
except without any DDP
"""
import sys
import os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")) # Stupid python

import time
import math
import pickle
from contextlib import nullcontext

import numpy as np
import torch
import dataclasses

from gpt import GPT
from config import GPTConfig, TrainConfig

def train(config: TrainConfig, train_path, val_path, meta_path, models_path):
    print("Set configuration:")
    for k, v in config.__dict__.items():
        print(f"{k} = {v}")

    if torch.cuda.is_available() and torch.cuda.is_bf16_supported():
        config.dtype = 'bfloat16'

    tokens_per_iter = config.gradient_acc_steps * config.batch_size * config.block_size
    print(f"tokens per iteration will be: {tokens_per_iter:,}")

    # torch.manual_seed(1434)
    torch.backends.cuda.matmul.allow_tf32 = True # allow tf32 on matmul
    torch.backends.cudnn.allow_tf32 = True # allow tf32 on cudnn
    device_type = 'cuda' if 'cuda' in config.device else 'cpu' # for later use in torch.autocast
    # note: float16 data type will automatically use a GradScaler
    ptdtype = {'float32': torch.float32, 'bfloat16': torch.bfloat16, 'float16': torch.float16}[config.dtype]
    ctx = nullcontext() if device_type == 'cpu' else torch.amp.autocast(device_type=device_type, dtype=ptdtype) # type: ignore

    def get_batch(split):
        # We recreate np.memmap every batch to avoid a memory leak, as per
        # https://stackoverflow.com/questions/45132940/numpy-memmap-memory-usage-want-to-iterate-once/61472122#61472122
        if split == 'train':
            data = np.memmap(train_path, dtype=np.uint16, mode='r')
        else:
            data = np.memmap(val_path, dtype=np.uint16, mode='r')
        ix = torch.randint(len(data) - config.block_size, (config.batch_size,))
        x = torch.stack([torch.from_numpy((data[i:i+config.block_size]).astype(np.int64)) for i in ix])
        y = torch.stack([torch.from_numpy((data[i+1:i+1+config.block_size]).astype(np.int64)) for i in ix])
        if device_type == 'cuda':
            # pin arrays x,y, which allows us to move them to GPU asynchronously (non_blocking=True)
            x, y = x.pin_memory().to(config.device, non_blocking=True), y.pin_memory().to(config.device, non_blocking=True)
        else:
            x, y = x.to(config.device), y.to(config.device)

        return x, y

    # init these up here, can override if init_from='resume' (i.e. from a checkpoint)
    iter_num = 0
    best_val_loss = 1e9

    # Get information from meta file
    with open(meta_path, 'rb') as f:
        meta = pickle.load(f)
    vocab_size: int = meta['vocab_size']
    print(f"Set vocab_size = {vocab_size}")

    # model init
    model_args = { f.name : getattr(config, f.name) for f in dataclasses.fields(GPTConfig) if f.name != 'vocab_size' }
    model_args['vocab_size'] = vocab_size # vocab_size comes from the data not the config
    if config.source == 'scratch':
        # init a new model from scratch
        print("Initializing a new model from scratch")
        # determine the vocab size we'll use for from-scratch training
        # why is pyright erroring here?
        gptconf = GPTConfig(**model_args)
        model: GPT = GPT(gptconf)

        model.to(config.device)

        optimizer = model.configure_optimizers(config.weight_decay, config.learning_rate, (config.beta1, config.beta2), device_type)

    elif config.source.endswith('.pt'):
        source_path = os.path.join(models_path, config.source)
        if config.override: config.out = source_path

        if not os.path.exists(source_path) or not os.path.isfile(source_path):
            raise ValueError(f"File {source_path:!s} not found")

        print(f"Resuming training from {source_path}")
        # resume training from a checkpoint.
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
        best_val_loss = checkpoint['best_val_loss']

        model.to(config.device)

        optimizer = model.configure_optimizers(config.weight_decay, config.learning_rate, (config.beta1, config.beta2), device_type)
        optimizer.load_state_dict(checkpoint['optimizer'])

        checkpoint = None # free up memory ??
    else:
        raise ValueError(f"Unknown source value {config.source:!s}")


    # initialize a GradScaler. If enabled=False scaler is a no-op
    scaler = torch.cuda.amp.GradScaler(enabled=(device_type == "cuda" and config.dtype == 'float16'))

    # compile the model
    if config.compile:
        print("compiling the model... (takes a ~minute)")
        unoptimized_model = model
        # requires PyTorch 2.0
        model: GPT = torch.compile(model) # type: ignore (complains about compiled model being a function)

    # helps estimate an arbitrarily accurate loss over either split using many batches
    @torch.no_grad()
    def estimate_loss():
        out = {}
        model.eval()
        for split in ['train', 'val']:
            losses = torch.zeros(config.eval_iters)
            for k in range(config.eval_iters):
                X, Y = get_batch(split)
                with ctx:
                    _, loss = model(X, Y)
                losses[k] = loss.item()
            out[split] = losses.mean()
        model.train()
        return out

    # learning rate decay scheduler (cosine with warmup)
    def get_lr(it):
        # 1) linear warmup for warmup_iters steps
        if it < config.warmup_iters:
            return config.learning_rate * it / config.warmup_iters
        # 2) if it > lr_decay_iters, return min learning rate
        if it > config.lr_decay_iters:
            return config.min_lr
        # 3) in between, use cosine decay down to min learning rate
        decay_ratio = (it - config.warmup_iters) / (config.lr_decay_iters - config.warmup_iters)
        assert 0 <= decay_ratio <= 1
        coeff = 0.5 * (1.0 + math.cos(math.pi * decay_ratio)) # coeff ranges 0..1
        return config.min_lr + coeff * (config.learning_rate - config.min_lr)

    # training loop
    X, Y = get_batch('train') # fetch the very first batch
    t0 = time.time()
    local_iter_num = 0 # number of iterations in the lifetime of this process
    raw_model = model
    running_mfu = -1.0
    while True:
        # determine and set the learning rate for this iteration
        lr = get_lr(iter_num) if config.decay_lr else config.learning_rate
        for param_group in optimizer.param_groups:
            param_group['lr'] = lr

        # evaluate the loss on train/val sets and write checkpoints
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

        if iter_num == 0 and config.eval_only:
            break

        # forward backward update, with optional gradient accumulation to simulate larger batch size
        # and using the GradScaler if data type is float16
        logits, loss = None, None
        for micro_step in range(config.gradient_acc_steps):
            with ctx:
                logits, loss = model(X, Y)
                loss = loss / config.gradient_acc_steps # scale the loss to account for gradient accumulation
            # immediately async prefetch next batch while model is doing the forward pass on the GPU
            X, Y = get_batch('train')
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
            print(f"iter {iter_num}: loss {lossf:.4f}, time {dt*1000:.2f}ms, mfu {running_mfu*100:.2f}%")
        iter_num += 1
        local_iter_num += 1

        # termination conditions
        if iter_num > config.max_iters:
            break

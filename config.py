import os
import sys
import datetime
from dataclasses import dataclass
from typing import Any

dirname = os.path.dirname(os.path.abspath(__file__))

build_path = os.path.join(dirname, "build", "main")
models_path = os.path.join(dirname, 'models')

raw_fe_paths = [os.path.join(dirname, 'data', name) for name 
                in os.listdir(os.path.join(dirname, 'data')) if name.startswith("raw")]
clean_fe_path = os.path.join(dirname, 'data', "clean-fe.txt") 
rand_fe_path = os.path.join(dirname, 'data', "rand-fe.txt")

train_fe_path = os.path.join(dirname, 'data', "train-fe.bin")
val_fe_path = os.path.join(dirname, 'data', "val-fe.bin")
meta_fe_path = os.path.join(dirname, 'data', "meta-fe.pkl")

gen_fe_path = os.path.join(dirname, 'data', 'gen-fe.txt')
filtered_fe_path = os.path.join(dirname, 'data', 'filtered-fe.txt')

rand_tac_path = os.path.join(dirname, 'data', 'rand-tac.txt')

train_tac_path = os.path.join(dirname, 'data', "train-tac.bin")
val_tac_path = os.path.join(dirname, 'data', "val-tac.bin")
meta_tac_path = os.path.join(dirname, 'data', "meta-tac.pkl")

@dataclass
class TokenizeConfig:
    encoding: str           = 'bpe' # 'bpe' or 'char'
    max_vocab_size: int     = 64

@dataclass
class GPTConfig:
    block_size: int
    vocab_size: int
    n_layer: int
    n_head: int
    n_embd: int
    dropout: float
    bias: bool # True: bias in Linears and LayerNorms, like GPT-2. False: a bit better and faster

@dataclass
class TrainConfig:
    # These defaults are supposedly set up for GPT-2 on openwebtext

    """ I/O """
    eval_interval: int              = 2000
    log_interval: int               = 10
    eval_iters: int                 = 200
    eval_only: bool                 = False # if True, script exits right after the first eval
    always_save_checkpoint: bool    = False # if True, always save a checkpoint after each eval
    # 'scratch' or a filename ending in '.pt' that will be sourced from models/
    source: str                     = 'scratch' 
    # name of output file that will be put in data/train-ckpt, default based on start time
    out: str                        = f"model-{datetime.datetime.now().strftime('%Y-%m-%d-%H:%M:%S')}.pt" 
    override: bool                  = True # override the source (i.e. set out to source) if it is a file (i.e. resuming)

    """ Data """
    gradient_acc_steps: int         = 5 * 8 # used to simulate larger batch sizes
    batch_size: int                 = 12 # if gradient_accumulation_steps > 1, this is the micro-batch size
    block_size: int                 = 1024

    """ Model """
    n_layer: int                    = 12
    n_head: int                     = 12
    n_embd: int                     = 768
    dropout: float                  = 0.0 # for pretraining 0 is good, for finetuning try 0.1+
    bias: bool                      = False # do we use bias inside LayerNorm and Linear layers?

    """ AdamW optimizer """
    learning_rate: float            = 6e-4 # max learning rate
    max_iters: int                  = 600000 # total number of training iterations
    weight_decay: float             = 1e-1
    beta1: float                    = 0.9
    beta2: float                    = 0.95
    grad_clip: float                = 1.0 # clip gradients at this value, or disable if == 0.0

    """ Learning rate decay """
    decay_lr: bool                  = True # whether to decay the learning rate
    warmup_iters: int               = 2000 # how many steps to warm up for
    lr_decay_iters: int             = 600000 # should be ~= max_iters per Chinchilla
    min_lr: float                   = 6e-5 # minimum learning rate, should be ~= learning_rate/10 per Chinchilla

    """ System """
    device: str                     = 'cuda' # examples: 'cpu', 'cuda', 'cuda:0', 'cuda:1' etc., or try 'mps' on macbooks
    dtype: str                      = 'float16' # 'float32', 'bfloat16', or 'float16', the latter will auto implement a GradScaler
    compile: bool                   = True # use PyTorch 2.0 to compile the model to be faster

train_configs = {
    'CPU': dict(
        eval_interval=250,
        eval_iters=20,
        log_interval=1,
        always_save_checkpoint=True,

        gradient_acc_steps=1,
        batch_size=12,
        block_size=64,

        n_layer=4,
        n_head=4,
        n_embd=128,
        dropout=0.0,

        learning_rate=1e-3,
        max_iters=2000,
        lr_decay_iters=2000,
        min_lr=1e-4,

        beta2=0.99,
        warmup_iters=100,

        device='cpu',
        compile=False
    ),
    'CUDA-1': dict(
        eval_interval=250, # keep frequent because we'll overfit
        eval_iters=200,

        gradient_acc_steps=1,
        batch_size=64,
        block_size=256,

        n_layer=6,
        n_head=6,
        n_embd=384,
        dropout=0.2,

        learning_rate=1e-3, # with baby networks can afford to go a bit higher
        max_iters=5000,
        lr_decay_iters=5000, # make equal to max_iters usually
        min_lr=1e-4, # learning_rate / 10 usually

        beta2=0.99, # make a bit bigger because number of tokens per iter is small
        warmup_iters=100, # not super necessary potentially

        device='cuda',
        compile=False
    )
}

@dataclass
class GenConfig:
    model: str                      # string 
    out: str                        = 'out' # ignored if init_from is not 'resume'
    start: str                      = "\n" # or "<|endoftext|>" or etc. Cannot specify a file for now
    num_lines: int                  = 100 # number of lines to generate
    temperature: float              = 0.8 # 1.0 = no change, < 1.0 = less random, > 1.0 = more random, in predictions
    top_k: float                    = 200 # retain only the top_k most likely tokens, clamp others to have 0 probability
    seed: int | None                = None

    dtype: str                      = 'float16' # 'float32', 'bfloat16', or 'float16', the latter will auto implement a GradScaler
    device: str                     = 'cuda' # examples: 'cpu', 'cuda', 'cuda:0', 'cuda:1', etc.
    compile: bool                   = False

@dataclass
class FineTuneConfig:
    source: str # filename ending in '.pt' that will be sourced from models/
    tactic: str # 's' or 'a'

    """ I/O """
    log_interval: int               = 5
    save_interval: int              = 25
    always_save_checkpoint: bool    = False # if True, always save a checkpoint
    # name of output file that will be put in data/train-ckpt, default based on start time
    out: str                        = f"model-{datetime.datetime.now().strftime('%Y-%m-%d-%H:%M:%S')}.pt" 
    override: bool                  = False # override the source (i.e. set out to source) if it is a file (i.e. resuming)

    """ Data """
    gradient_acc_steps: int         = 8 # used to simulate larger batch sizes
    batch_size: int                 = 12 # if gradient_accumulation_steps > 1, this is the micro-batch size
    block_size: int                 = 1024

    """ Model """
    n_layer: int                    = 12
    n_head: int                     = 12
    n_embd: int                     = 768
    dropout: float                  = 0.0 # for pretraining 0 is good, for finetuning try 0.1+
    bias: bool                      = False # do we use bias inside LayerNorm and Linear layers?

    """ AdamW optimizer """
    learning_rate: float            = 6e-4 # max learning rate
    max_iters: int                  = 600000 # total number of training iterations
    weight_decay: float             = 1e-1
    beta1: float                    = 0.9
    beta2: float                    = 0.95
    grad_clip: float                = 1.0 # clip gradients at this value, or disable if == 0.0

    """ System """
    device: str                     = 'cuda' # examples: 'cpu', 'cuda', 'cuda:0', 'cuda:1' etc., or try 'mps' on macbooks
    dtype: str                      = 'float16' # 'float32', 'bfloat16', or 'float16', the latter will auto implement a GradScaler
    compile: bool                   = True # use PyTorch 2.0 to compile the model to be faster

ft_configs = {
    'CPU': dict(
        log_interval=1,
        save_interval=5,
        always_save_checkpoint=True,

        gradient_acc_steps=1,
        batch_size=12,
        block_size=64,

        n_layer=4,
        n_head=4,
        n_embd=128,
        dropout=0.0,

        learning_rate=1e-4,
        max_iters=2100,
        beta2=0.99,

        device='cpu',
        compile=False
    ),
    'CUDA-1': dict(
        log_interval=5,
        save_interval=25,

        gradient_acc_steps=1,
        batch_size=64,
        block_size=256,

        n_layer=6,
        n_head=6,
        n_embd=384,
        dropout=0.2,

        learning_rate=1e-4,
        max_iters=5500,

        beta2=0.99, # make a bit bigger because number of tokens per iter is small

        device='cuda',
        compile=False
    )
}
# A bit sad argument parser for configs
# Based on https://github.com/karpathy/nanoGPT/blob/master/configurator.py
def parse_args() -> dict[str, Any]:
    args = {}
    for arg in sys.argv[1:]:   
        # assume it's a --key=value argument
        assert arg.startswith('--')
        key, val = arg.split('=')
        key = key[2:]

        # Try to parse as int or float, 
        # if not, keep it as string
        for type in [int, float]:
            try:
                val = type(val)
                break
            except ValueError:
                continue

        if val == "True": val = True
        if val == "False": val = False

        args[key] = val
    return args


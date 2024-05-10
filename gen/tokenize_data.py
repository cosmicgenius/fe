import sys
import os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")) # Stupid python

from config import rand_fe_path, train_fe_path, val_fe_path, meta_fe_path, TokenizeConfig, parse_args
from tokenize_base import tokenize_file

def main():
    config = TokenizeConfig(**parse_args())
    tokenize_file(config, rand_fe_path, train_fe_path, val_fe_path, meta_fe_path)

if __name__ == '__main__':
    main()

import sys
import os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")) # Stupid python

from config import rand_tac_path, train_tac_path, val_tac_path, meta_tac_path, TokenizeConfig, parse_args
from tokenize_base import tokenize_file

def main():
    config = TokenizeConfig(**parse_args())
    tokenize_file(config, rand_tac_path, train_tac_path, val_tac_path, meta_tac_path, protect_words=True)

if __name__ == '__main__':
    main()


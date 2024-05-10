import sys
import os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")) # Stupid python

from config import models_path, meta_tac_path, GenConfig, parse_args
from generate_base import generate

def main():
    args = parse_args()
    config = GenConfig(**args)

    generate(config, meta_tac_path, None, models_path)

if __name__ == '__main__':
    main()

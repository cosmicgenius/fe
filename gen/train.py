import sys
import os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")) # Stupid python

from config import train_fe_path, val_fe_path, meta_path, models_path, parse_args, TrainConfig, train_configs
from train_base import train

def main():
    args = parse_args()
    if 'config' in args and args['config'] is not None:
        config = train_configs[args['config']]
        del args['config']

        config = TrainConfig(**(config.__dict__ | args))
    else:
        config = TrainConfig(**args)

    train(config, train_fe_path, val_fe_path, meta_path, models_path)

if __name__ == '__main__':
    main()


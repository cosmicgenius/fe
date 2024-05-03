import os

dirname = os.path.dirname(__file__)

build_path = os.path.join(dirname, "..", "build", "main")
raw_paths = [os.path.join(dirname, 'data', name) for name 
                in os.listdir(os.path.join(dirname, 'data')) if name.startswith("raw")]
clean_path = os.path.join(dirname, 'data', "clean-fe.txt") 
rand_path = os.path.join(dirname, 'data', "rand-fe.txt")

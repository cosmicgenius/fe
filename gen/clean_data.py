from subprocess import Popen, PIPE, TimeoutExpired, DEVNULL
import os

raw_paths = [os.path.join(os.path.dirname(__file__), 'data', name) for name 
             in os.listdir(os.path.join(os.path.dirname(__file__), 'data')) if name.startswith("raw")]

clean_path = os.path.join(os.path.dirname(__file__), 'data', "clean-aops-fe.txt") 

def main():
    raw = []
    for path in raw_paths:
        with open(path, 'r', encoding="utf-8") as f:
            raw += f.readlines()
    
    print(f"Read {len(raw)} lines of raw data")

    proc = Popen(["./build/main", "-ng", "-np"], encoding="utf-8", stdin=PIPE, stdout=PIPE, stderr=DEVNULL)
    try:
        stdout, _ = proc.communicate(''.join(f"h {p}" for p in raw) + "\ne\n", timeout=(len(raw) / 10))
    except TimeoutExpired:
        print(f"Taking too long; killed after {len(raw) / 10}s")
        proc.kill()
        stdout, _ = proc.communicate()

    tot_len = stdout.count('\n')
    print(f"Generated {tot_len} lines of clean data")
    # remove duplicates
    clean = list(set(stdout.split('\n')))

    print(f"Removed {tot_len - len(clean)} duplicates")

    with open(clean_path, 'w', encoding="utf-8") as f:
        f.write('\n'.join(clean))
    print(f"Saved {len(clean)} lines of cleaned data")

if __name__ == '__main__':
    main()
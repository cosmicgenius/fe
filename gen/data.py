from subprocess import Popen, PIPE, TimeoutExpired
import os

raw_paths = [os.path.join(os.path.dirname(__file__), name) for name in
                [
                 "raw-aops-fe-hand.txt",
                 "raw-aops-fe-0-60.txt",
                 "raw-aops-fe-60-120.txt",
                 "raw-aops-fe-120-420.txt",
                 "raw-aops-fe-rev-0-300.txt",
                 "raw-aops-fe-rev-300-600.txt",
                 "raw-aops-fe-rev-600-900.txt",
                 "raw-aops-fe-rev-900-1200.txt",
                 "raw-aops-fe-faf-0-200.txt",
                 "raw-aops-fe-faf-200-400.txt",
                 "raw-aops-fe-faf-400-600.txt",
                ]]

clean_path = os.path.join(os.path.dirname(__file__), "clean-aops-fe.txt") 

def main():
    raw = []
    for path in raw_paths:
        with open(path, 'r', encoding="utf-8") as f:
            raw += f.readlines()
    
    print(f"Read {len(raw)} lines of raw data")

    proc = Popen(["./build/main", "-ng", "-np"], encoding="utf-8", stdin=PIPE, stdout=PIPE, stderr=PIPE)
    try:
        stdout, _ = proc.communicate('\n'.join(f"h {p}" for p in raw) + "\ne\n", timeout=(len(raw) / 10))
    except TimeoutExpired:
        print(f"Taking too long; killed after {len(raw) / 10}s")
        proc.kill()
        stdout, _ = proc.communicate()

    # remove duplicates
    clean = list(set(stdout.split('\n')))

    with open(clean_path, 'w', encoding="utf-8") as f:
        f.write('\n'.join(clean))
    print(f"Saved {len(clean)} lines of cleaned data")

if __name__ == '__main__':
    main()

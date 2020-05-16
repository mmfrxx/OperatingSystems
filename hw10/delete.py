import os, glob

for file in glob.glob("*.txt"):
    os.remove(file)
# Author: antoni4040 (https://github.com/antoni4040/Syspro-2019-Ergasia2-Client-Simulation)

from multiprocessing import Process
import subprocess
import os
import sys
import shutil
import random
import time
import signal

processes = []


def signal_handler(sig, frame):
    try:
        for proc in processes:
            proc.kill()
    except:
        pass
    sys.exit(0)


# Deletions
try:
    shutil.rmtree("common")
except:
    pass
for i in os.listdir("."):
    if os.path.isdir(os.path.join(".", i)):
        if i.endswith("mirror") or i.endswith("input"):
            shutil.rmtree(i)
    if os.path.isfile(os.path.join(".", i)):
        if i.endswith("_log") or i.startswith("log_file"):
            os.unlink(os.path.join(".", i))

# Run the client numOfInitialClients times and then every timeInterval create new client:
executable = sys.argv[1]

# Run with a single delete parameter to simply delete testing folders:
if executable == "delete":
    sys.exit()

numOfInitialClients = int(sys.argv[2])
timeInterval = int(sys.argv[3])

os.mkdir("common")

signal.signal(signal.SIGTSTP, signal_handler)

ID = 1
for i in range(numOfInitialClients):
    inputFileName = str(ID) + "_input"
    num_of_files = random.randint(10, 30)
    num_of_dirs = random.randint(5, 10)
    levels = random.randint(2, 4)
    print("\n\n./create_infiles.sh {} {} {} {}".format(
        inputFileName, num_of_files, num_of_dirs, levels))
    subprocess.check_call(
        [
            './create_infiles.sh', inputFileName, str(num_of_files),
            str(num_of_dirs), str(levels)])

    processes.append(subprocess.Popen(
        [
            "./" +
            executable, "-n", str(ID), "-c", "./common", "-i", "./" +
            inputFileName, "-m", "./" + str(ID) + "_mirror",
            "-b", "100", "-l",  str(ID) + "_log"
        ])
    )
    ID += 1

if timeInterval != None and timeInterval != 0:
    while True:
        time.sleep(timeInterval)
        inputFileName = str(ID) + "_input"
        num_of_files = random.randint(1, 100)
        num_of_dirs = random.randint(1, 30)
        levels = random.randint(1, 15)
        print("./create_infiles.sh {} {} {} {}".format(
            inputFileName, num_of_files, num_of_dirs, levels))
        subprocess.check_call(
            [
                './create_infiles.sh', inputFileName, str(num_of_files),
                str(num_of_dirs), str(levels)])

        process = subprocess.Popen(
            [
                "./" +
                executable, "-n", str(ID), "-c", "./common", "-i", "./" +
                inputFileName, "-m", "./" + str(ID) + "_mirror",
                "-b", "100", "-l", "log_file" + str(ID)
            ])
        processes.append(process)
        ID += 1
else:
    for i in processes:
        i.wait()

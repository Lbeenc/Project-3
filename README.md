# README - OSS and Worker Simulation

## Overview
This project simulates an operating system (`oss`) that manages multiple worker processes (`worker`). 
It uses shared memory and message queuesto coordinate worker processes in a controlled manner.

## Compilation
To compile the project run:
```
make all
```
This will generate the `oss` and `worker` executables.

## Running the Program
```
./oss [-n proc] [-s simul] [-t timelimit] [-i interval] [-f logfile]
```
### Options:
- `-n proc` → Maximum worker processes (default: 5)
- `-s simul` → Max simultaneous workers (default: 3)
- `-t timelimit` → Worker lifespan in seconds (default: 7)
- `-i interval` → Time between launching workers (default: 100ms)
- `-f logfile` → Log file for `oss` (default: `oss.log`)

### Example:
```
./oss -n 5 -s 2 -t 10 -i 100 -f oss.log
```

## Cleanup
To remove compiled files:
```
make clean
```
If needed, manually clear shared memory and message queues:
```
ipcrm -M <shm_id>
ipcrm -Q <msg_id>
```

## Author
Curtis Been

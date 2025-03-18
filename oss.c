#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#define MAX_PROCESSES 20
#define BILLION 1000000000

typedef struct {
    int occupied;        // Slot is occupied or not
    pid_t pid;           // Process ID
    int startSeconds;    // Time worker was created
    int startNano;       // Time worker was created
    int messagesSent;    // Number of messages sent
} PCB;

typedef struct {
    long mtype;
    int message;
} Message;

// Global Variables
PCB processTable[MAX_PROCESSES];
int shmClockID, msgQueueID;
int *shmClock;
FILE *logFile;
int totalMessagesSent = 0;
int processCount = 0;
int activeProcesses = 0;

// Function Declarations
void cleanup(int signal);
void incrementClock(int processes);
void printProcessTable();
int findEmptyPCBSlot();
void launchWorker(int slot, int maxTime);
void handleChildTermination();

int main(int argc, char *argv[]) {
    int opt, n = 5, s = 2, t = 5, i = 100;
    char *logFileName = "oss.log";

    while ((opt = getopt(argc, argv, "n:s:t:i:f:")) != -1) {
        switch (opt) {
            case 'n': n = atoi(optarg); break;
            case 's': s = atoi(optarg); break;
            case 't': t = atoi(optarg); break;
            case 'i': i = atoi(optarg); break;
            case 'f': logFileName = optarg; break;
            default:
                fprintf(stderr, "Usage: %s [-n proc] [-s simul] [-t max_time] [-i interval] [-f logfile]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    logFile = fopen(logFileName, "w");
    if (!logFile) {
        perror("Error opening log file");
        exit(EXIT_FAILURE);
    }

    // Setup Shared Memory for Simulated Clock
    shmClockID = shmget(IPC_PRIVATE, sizeof(int) * 2, IPC_CREAT | 0666);
    if (shmClockID == -1) {
        perror("shmget failed");
        exit(EXIT_FAILURE);
    }
    shmClock = (int *)shmat(shmClockID, NULL, 0);
    shmClock[0] = 0; // Seconds
    shmClock[1] = 0; // Nanoseconds

    // Setup Message Queue
    msgQueueID = msgget(IPC_PRIVATE, IPC_CREAT | 0666);
    if (msgQueueID == -1) {
        perror("msgget failed");
        exit(EXIT_FAILURE);
    }

    // Handle Ctrl+C and timeout signals
    signal(SIGINT, cleanup);
    signal(SIGALRM, cleanup);
    alarm(60); // 60-second real-time timeout

    // Initialize Process Table
    memset(processTable, 0, sizeof(processTable));

    // Main Execution Loop
    while (processCount < n || activeProcesses > 0) {
        incrementClock(activeProcesses);

        if (processCount < n && activeProcesses < s) {
            int slot = findEmptyPCBSlot();
            if (slot != -1) {
                launchWorker(slot, t);
            }
        }

        // Sending Messages in Round-Robin Fashion
        for (int j = 0; j < MAX_PROCESSES; j++) {
            if (processTable[j].occupied) {
                Message message = { .mtype = processTable[j].pid, .message = 1 };
                msgsnd(msgQueueID, &message, sizeof(message), 0);
                processTable[j].messagesSent++;
                totalMessagesSent++;

                fprintf(logFile, "OSS: Sent message to worker %d PID %d at %d:%d\n",
                        j, processTable[j].pid, shmClock[0], shmClock[1]);
                fflush(logFile);

                msgrcv(msgQueueID, &message, sizeof(message), processTable[j].pid, 0);
                fprintf(logFile, "OSS: Received message from worker %d PID %d at %d:%d\n",
                        j, processTable[j].pid, shmClock[0], shmClock[1]);

                if (message.message == 0) {
                    fprintf(logFile, "OSS: Worker %d PID %d is terminating.\n", j, processTable[j].pid);
                    processTable[j].occupied = 0;
                    activeProcesses--;
                    waitpid(processTable[j].pid, NULL, 0);
                }
            }
        }

        // Print Process Table Every 0.5 Simulated Seconds
        if (shmClock[1] % (500000000) == 0) {
            printProcessTable();
        }
    }

    // Final Summary
    printf("\nFinal Summary:\n");
    printf("Total workers launched: %d\n", processCount);
    printf("Total messages sent: %d\n", totalMessagesSent);

    fprintf(logFile, "\nFinal Summary:\n");
    fprintf(logFile, "Total workers launched: %d\n", processCount);
    fprintf(logFile, "Total messages sent: %d\n", totalMessagesSent);
    fflush(logFile);

    cleanup(0);
    return 0;
}

// Increment Clock Based on Active Processes
void incrementClock(int processes) {
    if (processes == 0) processes = 1;
    shmClock[1] += 250000000 / processes;
    if (shmClock[1] >= BILLION) {
        shmClock[0]++;
        shmClock[1] -= BILLION;
    }
}

// Print Process Table
void printProcessTable() {
    printf("\nOSS PID:%d SysClockS:%d SysClockNano:%d\n", getpid(), shmClock[0], shmClock[1]);
    printf("Process Table:\n");
    printf("Entry\tOccupied\tPID\tStartS\tStartN\tMessagesSent\n");

    for (int i = 0; i < MAX_PROCESSES; i++) {
        printf("%d\t%d\t%d\t%d\t%d\t%d\n",
               i, processTable[i].occupied, processTable[i].pid,
               processTable[i].startSeconds, processTable[i].startNano,
               processTable[i].messagesSent);
    }
}

// Find an Empty Slot in the Process Table
int findEmptyPCBSlot() {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (!processTable[i].occupied) return i;
    }
    return -1;
}

// Launch a Worker Process
void launchWorker(int slot, int maxTime) {
    int randSec = 1 + (rand() % maxTime);
    int randNano = rand() % BILLION;

    pid_t pid = fork();
    if (pid == 0) {
        char secArg[10], nanoArg[10];
        sprintf(secArg, "%d", randSec);
        sprintf(nanoArg, "%d", randNano);
        execl("./worker", "./worker", secArg, nanoArg, NULL);
        perror("execl failed");
        exit(1);
    }

    processTable[slot] = (PCB){1, pid, shmClock[0], shmClock[1], 0};
    processCount++;
    activeProcesses++;
}

// Cleanup Shared Memory & Message Queue
void cleanup(int signal) {
    msgctl(msgQueueID, IPC_RMID, NULL);
    shmctl(shmClockID, IPC_RMID, NULL);
    shmdt(shmClock);
    fclose(logFile);
    printf("\nCleaned up resources. Exiting...\n");
    exit(0);
}

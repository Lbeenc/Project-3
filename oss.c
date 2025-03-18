#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <string.h>

#define MAX_PROCESSES 20

// Message structure
struct msgbuf {
    long mtype;
    int data;
};

// Process Control Block
struct PCB {
    int occupied;
    pid_t pid;
    int startSeconds;
    int startNano;
    int messagesSent;
};

// Global Variables
struct PCB processTable[MAX_PROCESSES];
int msgQueueID;
FILE *logFile;

// Shared clock
int clockSeconds = 0, clockNano = 0;
int totalProcesses = 0, totalMessagesSent = 0;

// Cleanup on exit
void cleanup() {
    msgctl(msgQueueID, IPC_RMID, NULL);
    fclose(logFile);
    printf("OSS: Cleanup complete.\n");
}

// Signal handler for termination
void signalHandler(int sig) {
    printf("OSS: Caught signal %d. Terminating all workers.\n", sig);
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processTable[i].occupied) {
            kill(processTable[i].pid, SIGTERM);
        }
    }
    cleanup();
    exit(0);
}

// Increment clock
void incrementClock(int numChildren) {
    int increment = (numChildren > 0) ? (250000000 / numChildren) : 250000000;
    clockNano += increment;
    if (clockNano >= 1000000000) {
        clockSeconds++;
        clockNano -= 1000000000;
    }
}

// Launch a worker process
void launchWorker(int index, int maxLifetime) {
    if (processTable[index].occupied) return;

    pid_t pid = fork();
    if (pid == 0) {
        char secArg[10], nanoArg[10];
        snprintf(secArg, sizeof(secArg), "%d", (rand() % maxLifetime) + 1);
        snprintf(nanoArg, sizeof(nanoArg), "%d", rand() % 1000000000);
        execl("./worker", "./worker", secArg, nanoArg, NULL);
        perror("OSS: Exec failed");
        exit(1);
    }

    processTable[index].occupied = 1;
    processTable[index].pid = pid;
    processTable[index].startSeconds = clockSeconds;
    processTable[index].startNano = clockNano;
    processTable[index].messagesSent = 0;

    totalProcesses++;
    fprintf(logFile, "OSS: Launched worker %d at %d:%d\n", pid, clockSeconds, clockNano);
    fflush(logFile);
}

int main(int argc, char *argv[]) {
    int maxProcesses = 5, maxSimultaneous = 3, maxLifetime = 7, interval = 100;
    char logFileName[50] = "log.txt";

    // Parse command-line arguments
    int opt;
    while ((opt = getopt(argc, argv, "n:s:t:i:f:")) != -1) {
        switch (opt) {
            case 'n': maxProcesses = atoi(optarg); break;
            case 's': maxSimultaneous = atoi(optarg); break;
            case 't': maxLifetime = atoi(optarg); break;
            case 'i': interval = atoi(optarg); break;
            case 'f': strcpy(logFileName, optarg); break;
            default:
                fprintf(stderr, "Usage: %s -n [proc] -s [simul] -t [lifetime] -i [interval] -f [logfile]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // Setup message queue
    key_t key = ftok(".", 'm');
    msgQueueID = msgget(key, IPC_CREAT | 0666);
    if (msgQueueID == -1) {
        perror("OSS: Failed to create message queue");
        exit(1);
    }

    // Open log file
    logFile = fopen(logFileName, "w");
    if (!logFile) {
        perror("OSS: Cannot open log file");
        exit(1);
    }

    // Signal handling
    signal(SIGINT, signalHandler);
    signal(SIGALRM, signalHandler);
    alarm(60);

    // Main process loop
    int runningChildren = 0, nextProcess = 0;
    while (totalProcesses < maxProcesses || runningChildren > 0) {
        incrementClock(runningChildren);

        // Send a message to the next process
        if (runningChildren > 0) {
            struct msgbuf message;
            message.mtype = processTable[nextProcess].pid;
            message.data = 1;
            msgsnd(msgQueueID, &message, sizeof(message.data), 0);
            processTable[nextProcess].messagesSent++;
            totalMessagesSent++;

            fprintf(logFile, "OSS: Sent message to PID %d at %d:%d\n", processTable[nextProcess].pid, clockSeconds, clockNano);
            fflush(logFile);
        }

        // Receive a message from worker
        struct msgbuf response;
        if (msgrcv(msgQueueID, &response, sizeof(response.data), 0, IPC_NOWAIT) > 0) {
            int workerIndex = -1;
            for (int i = 0; i < MAX_PROCESSES; i++) {
                if (processTable[i].pid == response.mtype) {
                    workerIndex = i;
                    break;
                }
            }
            if (workerIndex != -1 && response.data == 0) {
                fprintf(logFile, "OSS: Worker PID %d is terminating.\n", processTable[workerIndex].pid);
                fflush(logFile);
                processTable[workerIndex].occupied = 0;
                runningChildren--;
            }
        }

        // Launch new workers
        if (totalProcesses < maxProcesses && runningChildren < maxSimultaneous) {
            launchWorker(totalProcesses, maxLifetime);
            runningChildren++;
        }

        // Select next process for messaging
        nextProcess = (nextProcess + 1) % MAX_PROCESSES;
        usleep(interval * 1000);
    }

    cleanup();
    return 0;
}

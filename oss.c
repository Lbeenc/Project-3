#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

#define MAX_PROCESSES 20
#define CLOCK_INCREMENT 250000000  // 250ms in nanoseconds

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

struct PCB processTable[MAX_PROCESSES];
int sysClockS = 0, sysClockNano = 0;
int msg_id;

// Cleanup function
void cleanup() {
    msgctl(msg_id, IPC_RMID, NULL);
    printf("OSS: Cleaning up message queue and exiting.\n");
}

// Signal handler for termination
void handle_signal(int sig) {
    printf("OSS: Caught signal %d. Terminating all workers.\n", sig);
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processTable[i].occupied) {
            kill(processTable[i].pid, SIGTERM);
        }
    }
    cleanup();
    exit(0);
}

int main(int argc, char *argv[]) {
    int n = 5, t = 7, i = 100;
    char logFileName[100] = "log.txt";

    // Parse command-line arguments
    int opt;
    while ((opt = getopt(argc, argv, "n:t:i:f:")) != -1) {
        switch (opt) {
            case 'n': n = atoi(optarg); break;
            case 't': t = atoi(optarg); break;
            case 'i': i = atoi(optarg); break;
            case 'f': strcpy(logFileName, optarg); break;
            default:
                fprintf(stderr, "Usage: %s -n proc -t time -i interval -f logfile\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // Create message queue
    key_t key = ftok("oss.c", 65);
    msg_id = msgget(key, 0666 | IPC_CREAT);

    // Setup signal handling
    signal(SIGINT, handle_signal);
    signal(SIGALRM, handle_signal);
    alarm(60); // Terminate after 60 real seconds

    int activeProcesses = 0, nextProcessIndex = 0;

    // Fork workers
    while (activeProcesses < n) {
        int freeSlot = -1;
        for (int j = 0; j < MAX_PROCESSES; j++) {
            if (!processTable[j].occupied) {
                freeSlot = j;
                break;
            }
        }
        if (freeSlot == -1) {
            printf("OSS: No available process slots.\n");
            break;
        }

        pid_t pid = fork();
        if (pid == 0) {  // Child process (Worker)
            char s_time[10], ns_time[10];
            sprintf(s_time, "%d", rand() % t + 1);
            sprintf(ns_time, "%d", rand() % 1000000000);
            execl("./worker", "./worker", s_time, ns_time, NULL);
            perror("OSS: execl failed");
            exit(1);
        } else {  // Parent process (OSS)
            printf("OSS: Forked worker PID %d at SysClock %d:%d\n", pid, sysClockS, sysClockNano);
            processTable[freeSlot].occupied = 1;
            processTable[freeSlot].pid = pid;
            processTable[freeSlot].startSeconds = sysClockS;
            processTable[freeSlot].startNano = sysClockNano;
            processTable[freeSlot].messagesSent = 0;
            activeProcesses++;
        }

        usleep(i * 1000);  // Launch interval
    }

    // Round-robin message passing
    while (activeProcesses > 0) {
        if (processTable[nextProcessIndex].occupied) {
            struct msgbuf message;
            message.mtype = processTable[nextProcessIndex].pid;
            message.data = 1;

            msgsnd(msg_id, &message, sizeof(message.data), 0);
            processTable[nextProcessIndex].messagesSent++;

            printf("OSS: Sent message to PID %d\n", processTable[nextProcessIndex].pid);

            msgrcv(msg_id, &message, sizeof(message.data), processTable[nextProcessIndex].pid, 0);
            printf("OSS: Received message from PID %d\n", processTable[nextProcessIndex].pid);

            if (message.data == 0) {  // Worker is terminating
                printf("OSS: Worker PID %d is terminating.\n", processTable[nextProcessIndex].pid);
                
                // Clean up process table
                waitpid(processTable[nextProcessIndex].pid, NULL, 0);
                processTable[nextProcessIndex].occupied = 0;
                
                activeProcesses--;  // Decrease active process count
            }
        }
        nextProcessIndex = (nextProcessIndex + 1) % MAX_PROCESSES;

        // stops dividing by zero
        if (activeProcesses > 0) {
            sysClockNano += CLOCK_INCREMENT / activeProcesses;
        }

        if (sysClockNano >= 1000000000) {
            sysClockS++;
            sysClockNano -= 1000000000;
        }
    }

    cleanup();
    return 0;
}

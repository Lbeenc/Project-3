#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

#define MAX_PROCESSES 20
#define BILLION 1000000000

// Process Control Block (PCB)
struct PCB {
    int occupied;
    pid_t pid;
    int startSeconds;
    int startNano;
    int messagesSent;
};

struct PCB processTable[MAX_PROCESSES] = {0};

// Shared Memory Clock
struct Clock {
    int seconds;
    int nanoseconds;
};

// Message Structure
struct Message {
    long type;
    int value;
};

// Shared memory and message queue IDs
int shm_id, msg_id;
struct Clock *clock_shm;
FILE *log_file;
int total_messages_sent = 0; // Track total messages

// Signal handler for cleanup
void cleanup(int sig) {
    msgctl(msg_id, IPC_RMID, NULL);
    shmdt(clock_shm);
    shmctl(shm_id, IPC_RMID, NULL);
    fclose(log_file);
    
    // Final summary
    printf("\nFinal Summary:\n");
    printf("Total Execution Time: %d seconds, %d nanoseconds\n", clock_shm->seconds, clock_shm->nanoseconds);
    printf("Total Messages Sent: %d\n", total_messages_sent);

    fprintf(log_file, "\nFinal Summary:\n");
    fprintf(log_file, "Total Execution Time: %d seconds, %d nanoseconds\n", clock_shm->seconds, clock_shm->nanoseconds);
    fprintf(log_file, "Total Messages Sent: %d\n", total_messages_sent);

    printf("\nCleaned up resources. Exiting...\n");
    exit(0);
}

// Function to print process table
void printProcessTable() {
    printf("\nOSS PID:%d SysClockS:%d SysClockNano:%d\n", getpid(), clock_shm->seconds, clock_shm->nanoseconds);
    printf("Process Table:\n");
    printf("Entry\tOccupied\tPID\tStartS\tStartN\tMessagesSent\n");

    for (int j = 0; j < MAX_PROCESSES; j++) {
        printf("%d\t%d\t\t%d\t%d\t%d\t%d\n",
               j, processTable[j].occupied, processTable[j].pid,
               processTable[j].startSeconds, processTable[j].startNano,
               processTable[j].messagesSent);
    }
    printf("\n");

    // Log to the file
    fprintf(log_file, "\nOSS PID:%d SysClockS:%d SysClockNano:%d\n", getpid(), clock_shm->seconds, clock_shm->nanoseconds);
    fprintf(log_file, "Process Table:\n");
    fprintf(log_file, "Entry\tOccupied\tPID\tStartS\tStartN\tMessagesSent\n");

    for (int j = 0; j < MAX_PROCESSES; j++) {
        fprintf(log_file, "%d\t%d\t\t%d\t%d\t%d\t%d\n",
                j, processTable[j].occupied, processTable[j].pid,
                processTable[j].startSeconds, processTable[j].startNano,
                processTable[j].messagesSent);
    }
    fprintf(log_file, "\n");
    fflush(log_file);
}

int main(int argc, char *argv[]) {
    int n = 5, s = 2, t = 5, i = 100;
    char log_filename[100] = "oss.log";

    int opt;
    while ((opt = getopt(argc, argv, "n:s:t:i:f:")) != -1) {
        switch (opt) {
            case 'n': n = atoi(optarg); break;
            case 's': s = atoi(optarg); break;
            case 't': t = atoi(optarg); break;
            case 'i': i = atoi(optarg); break;
            case 'f': snprintf(log_filename, sizeof(log_filename), "%s", optarg); break;
            default:
                fprintf(stderr, "Usage: %s -n proc -s simul -t timelimit -i interval -f logfile\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // Open log file
    log_file = fopen(log_filename, "w");
    if (!log_file) {
        perror("Error opening log file");
        exit(EXIT_FAILURE);
    }
    setbuf(log_file, NULL);  // Disable buffering for immediate writes

    // Setup shared memory
    key_t shm_key = ftok("shmfile", 65);
    shm_id = shmget(shm_key, sizeof(struct Clock), 0666 | IPC_CREAT);
    clock_shm = (struct Clock *)shmat(shm_id, NULL, 0);
    clock_shm->seconds = 0;
    clock_shm->nanoseconds = 0;

    // Setup message queue
    key_t msg_key = ftok("msgfile", 65);
    msg_id = msgget(msg_key, 0666 | IPC_CREAT);

    // Handle signals
    signal(SIGINT, cleanup);
    signal(SIGALRM, cleanup);
    alarm(60);  // Terminate after 60 seconds

    int process_count = 0;
    int active_processes = 0;
    int last_print_time = 0;

    while (process_count < n || active_processes > 0) {
        if (active_processes < s && process_count < n) {
            int slot = -1;
            for (int j = 0; j < MAX_PROCESSES; j++) {
                if (!processTable[j].occupied) {
                    slot = j;
                    break;
                }
            }

            if (slot != -1) {
                pid_t pid = fork();
                if (pid == 0) {
                    char sec[10], nano[10];
                    sprintf(sec, "%d", rand() % t + 1);
                    sprintf(nano, "%d", rand() % BILLION);
                    execl("./worker", "worker", sec, nano, NULL);
                } else {
                    processTable[slot].occupied = 1;
                    processTable[slot].pid = pid;
                    processTable[slot].startSeconds = clock_shm->seconds;
                    processTable[slot].startNano = clock_shm->nanoseconds;
                    processTable[slot].messagesSent = 0;
                    process_count++;
                    active_processes++;

                    // Print the process table after launching a new worker
                    printProcessTable();
                }
            }
        }

        for (int j = 0; j < MAX_PROCESSES; j++) {
            if (processTable[j].occupied) {
                struct Message msg;
                msg.type = processTable[j].pid;
                msg.value = 1;
                msgsnd(msg_id, &msg, sizeof(msg.value), 0);
                processTable[j].messagesSent++;
                total_messages_sent++;

                msgrcv(msg_id, &msg, sizeof(msg.value), processTable[j].pid, 0);
                if (msg.value == 0) {
                    waitpid(processTable[j].pid, NULL, 0);
                    processTable[j].occupied = 0;
                    active_processes--;
                }
            }
        }

        if (active_processes > 0) {
            clock_shm->nanoseconds += 250000000 / active_processes;
        } else {
            clock_shm->nanoseconds += 250000000;  // Prevent division by zero
        }
        if (clock_shm->nanoseconds >= BILLION) {
            clock_shm->seconds++;
            clock_shm->nanoseconds -= BILLION;
        }

        // Print the process table every 0.5 simulated seconds
        if (clock_shm->seconds * 1000 + clock_shm->nanoseconds / 1000000 >= last_print_time + 500) {
            printProcessTable();
            last_print_time = clock_shm->seconds * 1000 + clock_shm->nanoseconds / 1000000;
        }

        usleep(100000);
    }

    cleanup(0);  // Call cleanup to print final summary and exit
}

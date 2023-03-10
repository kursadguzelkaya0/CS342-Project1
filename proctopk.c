#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int main() {

    int k = 5; // numebr of words to find
    char outFile[] = "output.txt";  // output file name
    int n = 3; // number of input files
    char inFile1[] = "in1.txt";  // input file name
    char inFile2[] = "in2.txt";  // input file name
    char inFile3[] = "in3.txt";  // input file name

    char inputFiles[3][20] = {
        "in1.txt",
        "in2.txt",
        "in3.txt"
    };

    // Shared memory
    int shmid;
    key_t key = 1234;
    char *shared_memory;
    char *data = "Hello, world!";

    // Create shared memory segment
    shmid = shmget(key, 1024, 0666 | IPC_CREAT);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    // Attach shared memory segment to parent process
    shared_memory = shmat(shmid, NULL, 0);
    if (shared_memory == (char *) -1) {
        perror("shmat");
        exit(1);
    }

    pid_t pid;

    for (int i=0; i<n; i++) {
        pid = fork();

        if (pid == -1) {
            printf("Failed to create child process\n");
            return 1;
        } else if (pid == 0) {
            // This is the child process
            printf("Hello from child process!\n");

            FILE *fp;
            char line[100];
            char *word;

            fp = fopen(inputFiles[i], "r");
            if (fp == NULL) {
                printf("Failed to open file %s\n", inputFiles[i]);
                return 1;
            }

            fgets(line, 100, fp);
            word = strtok(line, " \t\n");
            while (word != NULL) {
                printf("Word: %s\n", word);
                word = strtok(NULL, " \t\n");
            }

            fclose(fp);

            return 0;
        } else {
            // This is the parent process
            printf("Hello from parent process!\n");

            int status;
            waitpid(pid, &status, 0);

            if (WIFEXITED(status)) {
                printf("Child process exited with status %d\n", WEXITSTATUS(status));
            }
        }

    }


    return 0;
}

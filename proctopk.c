#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define MAX_WORD_LEN 64

typedef struct {
    char word[MAX_WORD_LEN];
    int frequency;
} WordFreqPair;

int main() {

    int k = 5; // numebr of words to find
    char outFile[] = "output.txt";  // output file name
    int n = 3; // number of input files
    char inFile1[] = "in1.txt";  // input file name
    char inFile2[] = "in2.txt";  // input file name
    char inFile3[] = "in3.txt";  // input file name
    
    int shm_size = n * k * sizeof(WordFreqPair);

    char inputFiles[3][20] = {
        "in1.txt",
        "in2.txt",
        "in3.txt"
    };

    int shm_fd;
    char *shm_ptr;
    pid_t pid;
    WordFreqPair (*wordFreqPairs)[k];


    // create a shared memory object
    shm_fd = shm_open("/myshm", O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }

    // configure the size of the shared memory object
    ftruncate(shm_fd, sizeof(WordFreqPair[n][k]));

    // map the shared memory object into the address space of this process
    wordFreqPairs = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (wordFreqPairs == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }   

    for (int i=0; i<n; i++) {
        pid = fork();

        if (pid == -1) {
            printf("Failed to create child process\n");
            return 1;
        } else if (pid == 0) {
            // This is the child process
            printf("Hello from child process!\n");

            // Read file
            FILE *fp;
            char line[100];
            char word[MAX_WORD_LEN];

            fp = fopen(inputFiles[i], "r");
            if (fp == NULL) {
                printf("Failed to open file %s\n", inputFiles[i]);
                return 1;
            }

            while (fscanf(fp, "%s", word) != EOF) {
                printf("Word: %s\n", word);
            }

            fclose(fp);
            
            for (int j = 0; j < k; j++) {
                WordFreqPair pair;
                // initialize the members of the struct
                strncpy(pair.word, "example", MAX_WORD_LEN);
                pair.frequency = i*j;

                // child process writes to the shared memory
                wordFreqPairs[i][j] = pair;
            }

            // unmap the shared memory object from the child process
            if (munmap(wordFreqPairs, shm_size) == -1) {
                perror("munmap");
                exit(1);
            }

            return 0;
        } else {
            // parent process waits for the child to finish
            wait(NULL);

            // This is the parent process
            printf("Hello from parent process!\n");
        }

    }

    // parent process waits for the child to finish
    wait(NULL);

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < k; j++) {
            // read from the shared memory object
            printf("Received message word : %s\n", wordFreqPairs[i][j].word);
            printf("Received message freq : %d\n", wordFreqPairs[i][j].frequency);
        }
    }

    // unmap the shared memory object from the parent process
    if (munmap(wordFreqPairs, shm_size) == -1) {
        perror("munmap");
        exit(1);
    }

    // close the shared memory object
    if (close(shm_fd) == -1) {
        perror("close");
        exit(1);
    }

    // remove the shared memory object
    if (shm_unlink("/myshm") == -1) {
        perror("shm_unlink");
        exit(1);
    }

    return 0;
}

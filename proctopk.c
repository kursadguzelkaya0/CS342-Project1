#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <ctype.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define MAX_WORD_LEN 64
#define MAX_NUM_WORDS 1000

typedef struct {
    char word[MAX_WORD_LEN];
    int frequency;
} WordFreqPair;


int hash(char *word) {
    int hashedRes = 0;
    while (*word != '\0') {
        hashedRes = (hashedRes* 31 + *word) % MAX_NUM_WORDS;
        word++;
    }
    return hashedRes;
}

void readAndCreateHashTable(char* filename, int* noOfWords, WordFreqPair** hashTable) {

    // Read file
    FILE *fp;
    char word[MAX_WORD_LEN];


    //Initialize all elements
    for ( int i = 0; i < MAX_NUM_WORDS; i++ ) {
        (*hashTable)[i].word[0] = '\0'; //Set first characters to null so that any word is empty for now.
        
        (*hashTable)[i].frequency = 0;
    }

    fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("Failed to open file\n");
    }


    //Read the file and count words
    (*noOfWords) = 0;
    while (fscanf(fp, "%s", word) != EOF) {
        for(int i = 0; word[i]; i++) {
            word[i] = toupper(word[i]);
        }

        printf("Word: %s\n", word);

        int h = hash(word);
        if ( (*hashTable)[h].word[0] == '\0') {
            strcpy((*hashTable)[h].word, word);

            (*hashTable)[h].frequency = 1;
            *noOfWords = *noOfWords + 1;
        } else if (strcmp((*hashTable)[h].word, word) == 0) {
            // increment frequency count for existing word
            (*hashTable)[h].frequency++;
        } else {
            // handle hash collision by linear probing
            for (int j = (h + 1) % MAX_NUM_WORDS; j != h; j = (j + 1) % MAX_NUM_WORDS) {
                if ( (*hashTable)[j].word[0] == '\0') {
                    strcpy( (*hashTable)[j].word, word);
                    printf("Word2: %s\n", word);

                    (*hashTable)[j].frequency= 1;
                    *noOfWords = *noOfWords + 1;

                    break;
                } else if (strcmp((*hashTable)[j].word, word) == 0) {
                    (*hashTable)[j].frequency++;
                    break;
                }
            }
        }
    }

    fclose(fp);
}

void sortHashTable(WordFreqPair** hashTable) {
    // sort table in descending order
    int maxFreq;

    for ( int i = 0; i < MAX_NUM_WORDS; i++) {
        maxFreq = (*hashTable)[i].frequency;
        for ( int j = i + 1; j < MAX_NUM_WORDS; j++) {
            if ( (*hashTable)[j].frequency > maxFreq) {
                WordFreqPair tmp = (*hashTable)[i];
                (*hashTable)[i] = (*hashTable)[j];
                (*hashTable)[j] = tmp;
                maxFreq = (*hashTable)[i].frequency;
            }
        }
    }
}


int main( int argc, char* argv[] ) {

    int k = 5; // number of words to find
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

// output top K words and their frequency counts
   // for (int i = 0; i < k && i < noOfWords; i++) {
     //   printf("%s %d\n",  hashTable[i].word, hashTable[i].frequency);
    //}

    

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

            WordFreqPair* hashTable = (WordFreqPair*) malloc(MAX_NUM_WORDS*sizeof(char*));
            int noOfWords;

            readAndCreateHashTable(inputFiles[i], &noOfWords, &hashTable);

            for (int j = 0; j < MAX_NUM_WORDS; j++) {
                if(hashTable[j].word[0] == '\0') {
                    hashTable[j].frequency = 0;
                }
            }
        
            sortHashTable(&hashTable);    
            
            for (int j = 0; j < k; j++) {
                printf("word : %s\n", hashTable[j].word);
                printf("freq : %d\n", hashTable[j].frequency);
                if (hashTable[j].frequency > 0) {
                    // child process writes to the shared memory
                    wordFreqPairs[i][j] = hashTable[j];
                }
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

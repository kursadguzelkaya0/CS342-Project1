#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/mman.h>
#include <fcntl.h>

#define MAX_WORD_LEN 64
#define MAX_NUM_WORDS 1000
#define MAX_K 1000
#define SNAME "/myshm"

typedef struct {
    char word[MAX_WORD_LEN];
    int frequency;
} WordFreqPair;

WordFreqPair (*wordFreqPairs)[MAX_K];

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
        int h = hash(word);

        if ( (*hashTable)[h].word[0] == '\0') {
            strcpy((*hashTable)[h].word, word);

            (*hashTable)[h].frequency = 1;
            (*noOfWords) = (*noOfWords) + 1;
        } else if (strcmp((*hashTable)[h].word, word) == 0) {
            // increment frequency count for existing word
            (*hashTable)[h].frequency++;
        } else {
            // handle hash collision by linear probing
            for (int j = (h + 1) % MAX_NUM_WORDS; j != h; j = (j + 1) % MAX_NUM_WORDS) {
                if ( (*hashTable)[j].word[0] == '\0') {
                    strcpy( (*hashTable)[j].word, word);

                    (*hashTable)[j].frequency= 1;
                    (*noOfWords) = (*noOfWords) + 1;

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


int main(int argc, char* argv[]) {
    int K, N;
    char* outfile;
    char** infiles;

    // check for correct number of arguments
    if (argc < 5) {
        printf("Usage: %s <K> <outfile> <N> <infile1> .... <infileN>\n", argv[0]);
        return 1;
    }

    // read in K
    K = atoi(argv[1]);

    // read in outfile
    outfile = argv[2];

    // read in N
    N = atoi(argv[3]);

    // allocate space for input file names
    infiles = (char**) malloc(N * sizeof(char*));
    if (!infiles) {
        printf("Error: Could not allocate memory for input files.\n");
        return 1;
    }

    // read in input file names
    for (int i = 0; i < N; i++) {
        infiles[i] = argv[i + 4];
    }
    
    long shm_size = N * MAX_K * sizeof(WordFreqPair);

    int shm_fd;
    pid_t pid;

    // create a shared memory object
    shm_fd = shm_open(SNAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }

    // configure the size of the shared memory object
    ftruncate(shm_fd, shm_size );

    // map the shared memory object into the address space of this process
    wordFreqPairs = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (wordFreqPairs == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }   

    for (int i=0; i<N; i++) {
        pid = fork();

        if (pid == -1) {
            printf("Failed to create child process\n");
            return 1;
        } else if (pid == 0) {
            // This is the child process
            printf("Hello from child process!\n");

            WordFreqPair* hashTable = (WordFreqPair*) malloc( MAX_NUM_WORDS * sizeof(WordFreqPair));
            int noOfWords;

            readAndCreateHashTable(infiles[i], &noOfWords, &hashTable);
        
            sortHashTable(&hashTable);    
            
            for (int j = 0; j < K; j++) {
                printf("index of word in hashtable %d\n", j);
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
            // This is the parent process
            printf("Hello from parent process!\n");
        }

    }

    printf("Parent waiting for children...\n");
    for (int i = 0; i < N; i++) {
        wait(NULL); // wait for any child process to finish
    }
    printf("All children finished\n");

    //Initialize parentTable to find top K words
    WordFreqPair* parentTable = ( WordFreqPair* ) malloc( N*K*sizeof(WordFreqPair) );
    int tableSize = N*K;

    for ( int i = 0; i < tableSize; i++ ) {
        parentTable[i].word[0] = '\0'; //Set first characters to null so that any word is empty for now.
        parentTable[i].frequency = 0;
    }

    //Read the shared memory and count words
    int wordCount = 0;
    for ( int i = 0;  i <N; i++ ) {
        for ( int j = 0; j <K; j++ ) {

            char* word = wordFreqPairs[i][j].word;
            int freq = wordFreqPairs[i][j].frequency;
            int h = hash(word);

            if ( (parentTable)[h].word[0] == '\0') {
                strcpy((parentTable)[h].word, word);

                (parentTable)[h].frequency = freq;
                wordCount = wordCount + 1;
            } else if (strcmp((parentTable)[h].word, word) == 0) {
                // increment frequency count for existing word
                parentTable[h].frequency = parentTable[h].frequency + freq ;
            } else {
                // handle hash collision by linear probing
                for (int a = (h + 1) % MAX_NUM_WORDS; a != h; a = (a + 1) % MAX_NUM_WORDS) {
                    if ( (parentTable)[a].word[0] == '\0') {
                        strcpy( (parentTable)[a].word, word);

                        (parentTable)[a].frequency= 1;
                        wordCount = wordCount + 1;

                        break;
                    } else if (strcmp((parentTable)[a].word, word) == 0) {
                        parentTable[a].frequency = parentTable[a].frequency + freq ;
                        break;
                    }
                }
            }
        }
    }

    //Sort table
    sortHashTable(&parentTable);
    //Time to output
    FILE* ofp;
    ofp = fopen(outfile, "w");

    if (ofp == NULL) {
        printf("Error opening file!\n");
        return 1;
    }

    for (int i = 0; i < K; i++) {
            printf("%s", parentTable[i].word);
            fprintf(ofp,"%s", parentTable[i].word);
            fprintf(ofp," %d\n", parentTable[i].frequency);
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
    if (shm_unlink(SNAME) == -1) {
        perror("shm_unlink");
        exit(1);
    }

    return 0;
}

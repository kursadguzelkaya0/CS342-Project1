#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>


#define MAX_WORD_LEN 64
#define MAX_NUM_WORDS 1000

typedef struct {
    char word[MAX_WORD_LEN];
    int frequency;
} WordFreqPair;

typedef struct {
    char* infile;
    int threadNo;
    int K;
    int size;
} ThreadArgs;

WordFreqPair** wordFreqPairs;


int hash(char *word) {
    int hashedRes = 0;
    while (*word != '\0') {
        hashedRes = (hashedRes* 31 + *word) % MAX_NUM_WORDS;
        word++;
    }
    return hashedRes;
}

void readAndCreateHashTable(char* filename, int* noOfWords, WordFreqPair** hashTable, int* size) {

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
    int totalCount = 0;

    while (fscanf(fp, "%s", word) != EOF) {
        if ( totalCount > (*size * 0.7 ) ) {
            WordFreqPair* tmp = realloc(*hashTable, *size * 2 *sizeof(WordFreqPair));
            if ( tmp == NULL ) {
                printf("Error reallocating for new table");
            }
            else {
                printf("Reallocation successful");
                *size = *size * 2;
                *hashTable = tmp;
            }
        }
        for(int i = 0; word[i]; i++) {
            word[i] = toupper(word[i]);
        }
        int h = hash(word);
        totalCount++;


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

void sortHashTable(WordFreqPair** hashTable, int size) {
    // sort table in descending order
    int maxFreq;

    for ( int i = 0; i < size; i++) {
        maxFreq = (*hashTable)[i].frequency;
        for ( int j = i + 1; j < size; j++) {
            if ( (*hashTable)[j].frequency > maxFreq  || ( (*hashTable)[j].frequency == maxFreq && strcmp((*hashTable)[j].word, (*hashTable)[i].word) < 0)  ) {
                WordFreqPair tmp = (*hashTable)[i];
                (*hashTable)[i] = (*hashTable)[j];
                (*hashTable)[j] = tmp;
                maxFreq = (*hashTable)[i].frequency;
            }
        }
    }
}

void* thread_function(void* args) {

    ThreadArgs* thread_args = (ThreadArgs*)args;
    int threadNo = thread_args->threadNo;
    char* infile = thread_args->infile;
    int K = thread_args->K;
    int size = thread_args->size;

    // the function that will be run by each thread
    printf("Hello from thread %d\n", threadNo);

    WordFreqPair* hashTable = (WordFreqPair*) malloc( MAX_NUM_WORDS * sizeof(WordFreqPair));
    int noOfWords;
    readAndCreateHashTable(infile, &noOfWords, &hashTable, &size);

    sortHashTable(&hashTable, size);

    for (int j = 0; j < K; j++) {
        // child process writes to the shared memory
        wordFreqPairs[threadNo][j] = hashTable[j];
    }

    free(args);
    pthread_exit(NULL);
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
    // start the timer
    time_t t;
    srand((unsigned) time(&t));

    struct timeval starttime, endtime;
    gettimeofday(&starttime, NULL);

    // allocate space for result
    wordFreqPairs = (WordFreqPair*) malloc(N*sizeof(WordFreqPair*));
    for (int i = 0; i < N; i++) {
        wordFreqPairs[i] = malloc(K * sizeof(WordFreqPair));
    }

    pthread_t threads[N];
    int rc;

    for (int i = 0; i < N; i++) {
        ThreadArgs* args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
        args->infile = infiles[i];
        args->K = K;
        args->threadNo = i;
        args->size = 1000;
        printf("Creating thread %d\n", i);
        rc = pthread_create(&threads[i], NULL, thread_function, args);

        if (rc) {
            printf("Error creating thread %d\n", i);
            return -1;
        }
    }

    // wait for all threads to finish
    for (int i = 0; i < N; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("Threads finished\n");

    //Initialize parentTable to find top K words
    int maxTableSize = MAX_NUM_WORDS*10;
    WordFreqPair* parentTable = ( WordFreqPair* ) malloc( maxTableSize*sizeof(WordFreqPair) );

    for ( int i = 0; i < maxTableSize; i++ ) {
        parentTable[i].word[0] = '\0'; //Set first characters to null so that any word is empty for now.
        parentTable[i].frequency = 0;
    }

    //Read the global array and count words
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
    sortHashTable(&parentTable, maxTableSize);
    //Time to output
    FILE* ofp;
    ofp = fopen(outfile, "w");

    if (ofp == NULL) {
        printf("Error opening file!\n");
        return 1;
    }

    for (int i = 0; i < K; i++) {
        if ( parentTable[i].frequency > 0) {
            fprintf(ofp,"%s", parentTable[i].word);
            fprintf(ofp," %d\n", parentTable[i].frequency);
        }
    }


    free(infiles);
    gettimeofday(&endtime, NULL);
    // Calculate elapsed time
    long int elapsed;
    elapsed= (endtime.tv_sec - starttime.tv_sec) * 1000000L +
             (endtime.tv_usec - starttime.tv_usec);
    printf("Elapsed time: %ld microseconds\n", elapsed);
    return 0;
}
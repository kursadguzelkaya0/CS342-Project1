#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>


#define MAX_WORD_LEN 64

typedef struct {
    char word[MAX_WORD_LEN];
    int frequency;
} WordFreqPair;

typedef struct {
    WordFreqPair** wordFreqPairs;
    char* infile;
    int threadNo;
    int K;
} ThreadArgs;

void* thread_function(void* args) {

    ThreadArgs* thread_args = (ThreadArgs*)args;
    WordFreqPair** wordFreqPairs = thread_args->wordFreqPairs;
    int threadNo = thread_args->threadNo;
    char* infile = thread_args->infile;
    int K = thread_args->K;

    // the function that will be run by each thread
    printf("Hello from thread %d\n", threadNo);

    // Read file
    FILE *fp;
    char word[MAX_WORD_LEN];

    fp = fopen(infile, "r");
    if (fp == NULL) {
        printf("Failed to open file %s\n", infile);
    }

    while (fscanf(fp, "%s", word) != EOF) {
        printf("Word: %s\n", word);
    }

    fclose(fp);

    for (int j = 0; j < K; j++) {
        WordFreqPair* pair = (WordFreqPair*) malloc(sizeof(WordFreqPair));;
        // initialize the members of the struct
        strncpy(pair->word, "example", MAX_WORD_LEN);
        pair->frequency = threadNo*j;

        // child process writes to the shared memory
        wordFreqPairs[threadNo][j] = *pair;
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

    // allocate space for result
    WordFreqPair** wordFreqPairs = (WordFreqPair*) malloc(N*sizeof(WordFreqPair*));
    for (int i = 0; i < N; i++) {
        wordFreqPairs[i] = malloc(K * sizeof(WordFreqPair));
    }

    pthread_t threads[N];
    int rc;

    for (int i = 0; i < N; i++) {
        ThreadArgs* args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
        args->wordFreqPairs = wordFreqPairs;
        args->infile = infiles[i];
        args->K = K;
        args->threadNo = i;

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

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < K; j++) {
            // read from the shared memory object
            printf("Received message word : %s\n", wordFreqPairs[i][j].word);
            printf("Received message freq : %d\n", wordFreqPairs[i][j].frequency);
        }
    }


    free(infiles);
    return 0;
}
#include <iostream>
#include "concurrent_malloc.hpp"
#include "time.h"

#define NUM_INSERT_THREADS 2
#define NUM_DELETE_THREADS 2
#define NUM_SEARCH_THREADS 2
#define NUM_THREADS (NUM_INSERT_THREADS + NUM_DELETE_THREADS + NUM_SEARCH_THREADS)

#define INSERTIONS_PER_THREAD   100
#define DELETIONS_PER_THREAD    100
#define SEARCHES_PER_THREAD     100

pthread_mutex_t outputStream;

template <class V>
struct ArgsStruct
{
    ConcurrentTree<V> *mTree;
    int mPid;

    ArgsStruct(ConcurrentTree<V> *tree, int pid)
    {
        mTree = tree;
        mPid = pid;
    }
};

template <class V>
void *inserter(void* args)
{
    ArgsStruct<V> *myArgs = (ArgsStruct<V>*) args;

    for(int i=0; i<NUM_INSERT_THREADS; i++) {
        char buffer[8];
        uint32_t hash = 0;

        for(int i=0; i<7; i++) {
            buffer[i] = rand() % ('z' - 'a') + 'a';
            hash = (31 * hash) + (uint32_t) buffer[i];
        }
        
        buffer[7] = '\0';
        std::string str (buffer);
        std::string *str_ptr = (std::string *) malloc(str.size());

        myArgs->mTree->InsertOrUpdate(hash, str_ptr, myArgs->mPid);
    }

    return nullptr;
}

template <class V>
void *deleter(void* args)
{
    ArgsStruct<V> *myArgs = (ArgsStruct<V>*) args;

    return nullptr;
}

template <class V>
void *searcher(void* args)
{
    ArgsStruct<V> *myArgs = (ArgsStruct<V>*) args;

    return nullptr;

}

int main(void)
{
    srand(time(NULL));

    pthread_t* threads;
    threads = (pthread_t*)malloc(NUM_THREADS * sizeof(pthread_t));

    pthread_mutex_init(&outputStream, NULL);

    ConcurrentTree<std::string> *tree = new ConcurrentTree<std::string>(NUM_THREADS);

    for(int i = 0; i < NUM_INSERT_THREADS; i++) {
        pthread_create(&threads[i], NULL, inserter<std::string>, (void*) new ArgsStruct<std::string>(tree, i));
    }

    for(int i= NUM_INSERT_THREADS; i < NUM_DELETE_THREADS; i++) {
        pthread_create(&threads[i], NULL, deleter<std::string>, (void*) new ArgsStruct<std::string>(tree, i));
    }

    for(int i = NUM_INSERT_THREADS + NUM_DELETE_THREADS; i < NUM_SEARCH_THREADS; i++) {
        pthread_create(&threads[i], NULL, searcher<std::string>, (void*) new ArgsStruct<std::string>(tree, i));
    }

    for(int i = 0; i < NUM_THREADS; i++)
    {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&outputStream);
    free(threads);
}
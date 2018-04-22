#include <iostream>
#include "concurrent.hpp"

#define NUM_INSERT_THREADS 2
#define NUM_DELETE_THREADS 2
#define NUM_SEARCH_THREADS 2
#define NUM_THREADS (NUM_INSERT_THREADS + NUM_DELETE_THREADS + NUM_SEARCH_THREADS)

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

    int *hate = malloc(sizeof int);
    *hate = 10;

    myArgs->mTree->InsertOrUpdate(1, hate, myArgs->mPid);
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
    pthread_t* threads;
    threads = (pthread_t*)malloc(NUM_THREADS * sizeof(pthread_t));

    pthread_mutex_init(&outputStream, NULL);

    ConcurrentTree<uint32_t> *tree = new ConcurrentTree<uint32_t>(NUM_THREADS);

    for(int i = 0; i < NUM_INSERT_THREADS; i++) {
        pthread_create(&threads[i], NULL, inserter<uint32_t>, (void*) new ArgsStruct<uint32_t>(tree, i));
    }
    for(int i= NUM_INSERT_THREADS; i < NUM_DELETE_THREADS; i++) {
        pthread_create(&threads[i], NULL, deleter<uint32_t>, (void*) new ArgsStruct<uint32_t>(tree, i));
    }
    for(int i = NUM_INSERT_THREADS + NUM_DELETE_THREADS; i < NUM_SEARCH_THREADS; i++) {
        pthread_create(&threads[i], NULL, searcher<uint32_t>, (void*) new ArgsStruct<uint32_t>(tree, i));
    }

    for(int i = 0; i < NUM_THREADS; i++)
    {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&outputStream);
    free(threads);

}
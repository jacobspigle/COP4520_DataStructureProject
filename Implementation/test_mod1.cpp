#include <iostream>
#include "concurrent.hpp"
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
    uint32_t mSearchWeight;
    uint32_t mInsertWeight;
    uint32_t mDeleteWeight;
    int mOps;

    ArgsStruct(ConcurrentTree<V> *tree, int pid)
    {
        mTree = tree;
        mPid = pid;
    }

    ArgsStruct(ConcurrentTree<V> *tree, int pid, int ops, int sw, int iw, int dw)
    {
        mTree = tree;
        mPid = pid;
        mSearchWeight = sw;
        mInsertWeight = iw;
        mDeleteWeight = dw;
        mOps = ops;
    }
};

template <class V>
void *inserter(void* args)
{
    ArgsStruct<V> *myArgs = (ArgsStruct<V>*) args;

    for(int i=0; i<INSERTIONS_PER_THREAD; i++) {
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

    for(int i=0; i<DELETIONS_PER_THREAD; i++) {
        char buffer;
        uint32_t hash = 0;

        for(int i=0; i<7; i++) {
            buffer = rand() % ('z' - 'a') + 'a';
            hash = (31 * hash) + (uint32_t) buffer;
        }

        myArgs->mTree->Delete(hash, myArgs->mPid);
    }

    return nullptr;
}

template <class V>
void *searcher(void* args)
{
    ArgsStruct<V> *myArgs = (ArgsStruct<V>*) args;

    for(int i=0; i<SEARCHES_PER_THREAD; i++) {
        char buffer;
        uint32_t hash = 0;

        for(int i=0; i<7; i++) {
            buffer = rand() % ('z' - 'a') + 'a';
            hash = (31 * hash) + (uint32_t) buffer;
        }

        myArgs->mTree->Search(hash, myArgs->mPid);
    }

    return nullptr;

}

template <class V>
void *dynamic_worker(void *args)
{
    ArgsStruct *myArgs = (ArgsStruct *) args;
    uint32_t sw = myArgs->mSearchWeight;
    uint32_t iw = myArgs->mInsertWeight + sw;
    uint32_t dw = myArgs->mDeleteWeight + iw;
    int num_ops = myArgs->mOps;
    uint32_t roll;

    while(num_ops --> 0)
    {
        roll = rand() % total_weight;

        if(roll < sw) {
            // SEARCH
            char buffer;
            uint32_t hash = 0;

            for(int i=0; i<7; i++) {
                buffer = rand() % ('z' - 'a') + 'a';
                hash = (31 * hash) + (uint32_t) buffer;
            }

            myArgs->mTree->Search(hash, myArgs->mPid);
        }
        else if(roll < iw) {
            // INSERT or UPDATE
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
        else if(roll < dw) {
            // DELETE
            char buffer;
            uint32_t hash = 0;

            for(int i=0; i<7; i++) {
                buffer = rand() % ('z' - 'a') + 'a';
                hash = (31 * hash) + (uint32_t) buffer;
            }

            myArgs->mTree->Delete(hash, myArgs->mPid);
        }
        else {
            // 0 weight for each operation. no operation can be chosen.
            break;
        }
    }

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
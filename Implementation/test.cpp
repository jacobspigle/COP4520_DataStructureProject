#include <iostream>
#include "concurrent.hpp"

#define NUM_INSERT_THREADS 2
#define NUM_DELETE_THREADS 2
#define NUM_SEARCH_THREADS 2
#define NUM_THREADS (NUM_INSERT_THREADS + NUM_DELETE_THREADS + NUM_SEARCH_THREADS)

template <class V>
void inserter(ConcurrentTree<V> *tree, int pid)
{

}

template <class V>
void deleter(ConcurrentTree<V> *tree, int pid)
{

}

template <class V>
void searcher(ConcurrentTree<V> *tree, int pid)
{

}

int main(void)
{
    auto tree = new ConcurrentTree<uint32_t>(NUM_THREADS);
    
    for(int i=0; i<NUM_THREADS; i++) {

    }
}
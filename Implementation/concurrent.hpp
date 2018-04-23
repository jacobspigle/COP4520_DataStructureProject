#include <memory>
#include <pthread.h>
#include <climits>

#define PointerNode PackedPointer
#define NextNode PackedPointer
#define StateNode PackedPointer

enum Status {WAITING = 0, IN_PROGRESS = 1, COMPLETED = 2};
enum Flag {FREE = 0, OWNED = 1};
enum Type {SEARCH, INSERT, UPDATE, DELETE};
enum Color {RED, BLACK, UNCOLORED};
enum Gate {VALUE};

template <class V>
class DataNode;

template <class T, class U>
class PackedPointer
{
public: 
    T *mPackedPointer;

    PackedPointer()
    {
        mPackedPointer = nullptr;
    }

    PackedPointer(T* packedPointer)
    {
        mPackedPointer = packedPointer;
    }

    PackedPointer(T* packedPointer, U tag)
    {
        uint64_t mask = (uint64_t) 0b11 << 62;
        mPackedPointer = (T*) ((uint64_t) packedPointer & (!mask));
        mask = (uint64_t) tag << 62;
        mPackedPointer = (T*)((uint64_t) packedPointer | mask);
    }

    void setTag(U tag)
    {
        uint64_t mask = (uint64_t) 0b11 << 62;
        mPackedPointer = (T*) ((uint64_t) mPackedPointer & (!mask));
        mask = (uint64_t) tag << 62;
        mPackedPointer = (T*)((uint64_t) mPackedPointer | mask);
    }

    void setPointerAndPreserveTag(T *pointer)
    {
        U tag = getTag();
        mPackedPointer = pointer;
        setTag(tag);
    }

    void setStatus(Status status)
    {
        setTag(status);
    }

    U getTag()
    {
        return (U) ((uint64_t) mPackedPointer >> 62);
    }

    Status getStatus()
    {
        return getTag();
    }

    Flag getFlag()
    {
        return getTag();
    }

    T *getPointer()
    {
        uint64_t remove_tag = (uint64_t) 0b11 << 62;
        T *pointer = mPackedPointer;
        pointer = (T *) ((uint64_t) pointer & remove_tag);
        return pointer;
    }

    T *getDataNode()
    {
        return getPointer();
    }

    T *getPointerNode()
    {
        return getPointer();
    }

    T *getPosition()
    {
        return getPointer();
    }
};

template <class V>
class ValueRecord
{
public:
    V *mValue;
    uint32_t mGate;

    ValueRecord(V *value, uint32_t gate)
    {
        mValue = value;
        mGate = gate;
    }
};

template <class V>
union Position {PointerNode<DataNode<V>, Flag> *windowLocation; ValueRecord<V> *valueRecord;};

template <class V>
class OperationRecord
{
public:
    Type mType;
    uint32_t mKey;
    uint32_t mPid;
    V *mValue;
    StateNode<Position<V>, Status> *mState;

    OperationRecord(Type type, uint32_t key, V *value)
    {
        mType = type;
        mKey = key;
        mValue = value;
        mPid = -1;
    }
};

template <class V>
class DataNode
{
public:
    Color mColor;
    uint32_t mKey;
    ValueRecord<V> *mValData;
    OperationRecord<V> *mOpData;

    PointerNode<DataNode<V>, Flag> *mLeft;
    PointerNode<DataNode<V>, Flag> *mRight;

    PackedPointer<PointerNode<DataNode<V>, Flag>, Status> *mNext;
    
    // defaults to sentinel values
    DataNode()
    {
        mColor = BLACK;
        mKey = UINT32_MAX;
        mValData = new ValueRecord<V>(nullptr, 0);
        mLeft = nullptr;
        mRight = nullptr;
        mOpData = nullptr;
        mNext = nullptr;
    }

    DataNode *clone()
    {
        DataNode *copy = new DataNode();
        copy->mColor = mColor;
        copy->mKey = mKey;
        copy->mValData = mValData;
        copy->mLeft = mLeft;
        copy->mRight = mRight;
        return copy;
    }
};

template <class V>
class ConcurrentTree
{
public:
    PackedPointer<DataNode<V>, Flag> *pRoot;
    OperationRecord<V> **ST, **MT;
    uint32_t mNumThreads;
    uint32_t mIndex;

    ConcurrentTree(int numThreads)
    {
        mIndex = 0;
        mNumThreads = numThreads;

        pRoot = new PackedPointer<DataNode<V>, Flag>(new DataNode<V>(), Flag::FREE);

        ST = (OperationRecord<V>**) malloc (sizeof(OperationRecord<V>*) * numThreads);
        MT = (OperationRecord<V>**) malloc (sizeof(OperationRecord<V>*) * numThreads);
        for (int i = 0; i < numThreads; i++)
        {
            ST[i] = nullptr;
            MT[i] = nullptr;
        }
    }

    V* Search(uint32_t key, int myid);
    void InsertOrUpdate(uint32_t key, V *value, int myid);
    void Delete(uint32_t key, int myid);
    uint32_t Select();
    void Traverse(OperationRecord<V> *opData);
    void ExecuteOperation(OperationRecord<V> *opData, int myid);
    void InjectOperation(OperationRecord<V> *opData);
    void ExecuteWindowTransaction(PointerNode<DataNode<V>, Flag> *pNode, DataNode<V> *dNode);
    bool ExecuteCheapWindowTransaction(PointerNode<DataNode<V>, Flag> *pNode, DataNode<V> *dNode);
    void SlideWindowDown(PointerNode<DataNode<V>, Flag> *pMoveFrom, DataNode<V> *dMoveFrom, PointerNode<DataNode<V>, Flag> *pMoveTo, DataNode<V> *dMoveTo);
};

#include "concurrent.tcc"
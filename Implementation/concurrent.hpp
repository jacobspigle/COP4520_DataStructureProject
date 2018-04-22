#include <memory>
#include <pthread.h>
#include <climits>

#define PointerNode PackedPointer
#define NextNode PackedPointer

enum Status {WAITING = 0, IN_PROGRESS = 1, COMPLETED = 2};
enum Flag {FREE = 0, OWNED = 1};
enum Type {SEARCH, INSERT, UPDATE, DELETE};
enum Color {RED, BLACK, UNCOLORED};
enum Gate {FUCK};

template <class V>
union Position {PointerNode<V> *windowLocation, ValueRecord<V> *valueRecord};

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

    PackedPointer(U tag, T* packedPointer)
    {
        uint64_t mask = (uint64_t) 0b11 << 62;
        mPackedPointer = (uint64_t) packedPointer & (!mask);
        mask = (uint64_t) tag << 62;
        mPackedPointer = (uint64_t) packedPointer | mask;
    }

    void setTag(U tag)
    {
        uint64_t mask = (uint64_t) 0b11 << 62;
        mPackedPointer = (uint64_t) mPackedPointer & (!mask);
        mask = (uint64_t) tag << 62;
        mPackedPointer = (uint64_t) mPackedPointer | mask;
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
        pointer = (uint64_t) pointer & remove_tag;
        return pointer;
    }

    DataNode<V> *getDataNode()
    {
        return getPointer();
    }

    PointerNode<V> *getPointerNode()
    {
        return getPointer();
    }

    Position<V> *getPosition()
    {
        return getPointer();
    }
};

template <class V>
class ValueRecord
{
    V *mValue;
    uint32_t mGate;

    ValueRecord(V *value, uint32_t gate)
    {
        mValue = value;
        mGate = gate;
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

    PointerNode<V> *mLeft;
    PointerNode<V> *mRight;

    PackedPointer<V, Status> *mNext;
    
    // defaults to sentinel values
    DataNode()
    {
        mColor = BLACK;
        mKey = UINT32_MAX;
        mValData = new ValueRecord(nullptr, 0);
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
class OperationRecord
{
    Type mType;
    uint32_t mKey;
    uint32_t mPid;
    V *mValue;
    PackedPointer<Position<V>, Status> *mState;

    OperationRecord(Type type, uint32_t key, V *value)
    {
        mType = type;
        mKey = key;
        mValue = value;
    }
};

template <class V>
class ConcurrentTree
{
public:
    PackedPointer *pRoot;
    OperationRecord *ST[], *MT[];
    uint32_t mNumThreads;
    uint32_t mIndex;

    ConcurrentTree(int numThreads)
    {
        mIndex = 0;
        mNumThreads = numThreads;

        pRoot = new PackedPointer(Flag.FREE, new DataNode<V>());

        ST = (OperationRecord**) malloc (sizeof(OperationRecord*) * numThreads);
        MT = (OperationRecord**) malloc (sizeof(OperationRecord*) * numThreads);
        for (int i = 0; i < numThreads; i++)
        {
            ST[i] = nullptr;
            MT[i] = nullptr;
        }
    }

    V* Search(uint32_t key);
    void InsertOrUpdate(uint32_t key, V value);
    void Delete(uint32_t key);
    uint32_t Select(ConcurrentTree<V> *Tree);
    void Traverse(OperationRecord<V> *opData);
    void ExecuteOperation(OperationRecord<V> *opData);
    void InjectOperation(OperationRecord<V> *opData);
    void ExecuteWindowTransaction(PointerNode<V> *pNode, DataNode<V> *dNode);
    bool ExecuteCheapWindowTransaction(PointerNode<V> *pNode, DataNode<V> *dNode);
    void SlideWindowDown(PointerNode<V> *pMoveFrom, DataNode<V> *dMoveFrom, PointerNode<V> *pMoveTo, DataNode<V> *dMoveTo);
};

#include "concurrent.tcc"
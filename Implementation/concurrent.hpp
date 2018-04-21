#include <memory>
#include <pthread.h>
#include <climits>



enum Status {WAITING = 0, IN_PROGRESS = 1, COMPLETED = 2};
enum Flag {FREE = 0, OWNED = 1};
enum Type {SEARCH, INSERT, UPDATE, DELETE};
enum Color {RED, BLACK, UNCOLORED};

template <class T, class U>
class PackedPointer
{
public: 
    T* mPackedPointer;

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

    DataNode *mLeft;
    DataNode *mRight;

    PackedPointer<V, Status> mNext;
    
    //sentinel use only ***
    DataNode()
    {
        mColor = BLACK;
        mKey = UINT32_MAX;
        mValData = new ValueRecord(nullptr, 0);
        mLeft = nullptr;
        mRight = nullptr;
    }

    DataNode *clone()
    {
        DataNode *copy = new DataNode();
        copy->mColor = mColor;
        copy->mKey = mKey;
        copy->mValData = mValData;
        copy->mOpData = mOpData;
        copy->mLeft = mLeft;
        copy->mRight = mRight;
        copy->mNext = mNext;
        return copy;
    }
};

template <class V>
class OperationRecord
{
    Type mType;
    uint32_t mKey;
    V mValue;
    uint32_t mPid;
    PackedPointer<V, Status> mState;

    OperationRecord(Type type, uint32_t key, V value)
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
    ConcurrentTree() {};
    V Search(K key);
    void InsertOrUpdate(K key, V value);
    void Delete(K key);
    void Traverse(DataNode<V> opData);
    void ExecuteOperation(DataNode<V> opData);
    void InjectOperation(DataNode<V> opData);
    void ExecuteWindowTransaction(DataNode<V> pNode, DataNode<K,V> dNode);
    bool ExecuteCheapWindowTransaction(DataNode<V> pNode, DataNode<V> dNode);
    void SlideWindowDown(PointerNode<V> pMoveFrom, PointerNode<V> dMoveFrom, PointerNode<V> pMoveTo, PointerNode<V> dMoveTo);
};

#include "concurrent.tcc"
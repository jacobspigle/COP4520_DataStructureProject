#include <memory>
#include <pthread.h>
#include <climits>

// #define PointerNode PackedPointer
// #define NextNode PackedPointer
// #define StateNode PackedPointer

enum Status {NO_STATUS = -1, WAITING = 0, IN_PROGRESS = 1, COMPLETED = 2};
enum Flag {FREE = 0, OWNED = 1};
enum Type {SEARCH, INSERT, UPDATE, DELETE};
enum Color {RED, BLACK, UNCOLORED};
enum Gate {VALUE};

template <class V>
class DataNode;

template <class V>
class ValueRecord;

template <class T, class U>
class PointerNode;

template <class V>
union Position {PointerNode<DataNode<V>, Flag> *windowLocation; ValueRecord<V> *valueRecord;};

// TODO: use LSB instead of MSB for pointer packing because of segfaulting
// std::align is helpful for guaranteeing memory alignment
// need to update doc to reflect this

template <class T, class U>
class StateNode
{
public: 
    T *mPackedPointer;

    StateNode()
    {
        mPackedPointer = (T *) malloc(sizeof(T));
    }

    StateNode(T* packedPointer)
    {
        mPackedPointer = (T *) malloc(sizeof(T));
        mPackedPointer = packedPointer;
    }

    StateNode(T *packedPointer, U tag)
    {
        mPackedPointer = (T *) malloc(sizeof(T));
        uint64_t mask = (uint64_t) 0b11 << 62;
        mPackedPointer = (T*) ((uint64_t) packedPointer & (~mask));
        mask = (uint64_t) tag << 62;
        mPackedPointer = (T*)((uint64_t) packedPointer | mask);
    }

    void setPointerAndPreserveTag(T *pointer)
    {
        U tag = getTag();
        mPackedPointer = pointer;
        setTag(tag);
    }

    T *unpack()
    {
        uint64_t remove_tag = (uint64_t) 0b11 << 62;
        T *pointer = mPackedPointer;
        pointer = (T *) ((uint64_t) pointer & remove_tag);
        return pointer;
    }

    StateNode<T, U> *getCleanSelfPointer()
    {
        uint64_t remove_tag = (uint64_t) 0b11 << 62;
        StateNode<T, U> *pointer = this;
        pointer = (StateNode<T, U> *) ((uint64_t) pointer & remove_tag);
        return pointer;
    }

    void setTag(U tag)
    {
        uint64_t mask = (uint64_t) 0b11 << 62;
        mPackedPointer = (T*) ((uint64_t) mPackedPointer & (~mask));
        mask = (uint64_t) tag << 62;
        mPackedPointer = (T*)((uint64_t) mPackedPointer | mask);
    }

    U getTag()
    {
        return (U) ((uint64_t) mPackedPointer >> 62);
    }

    void setStatus(Status status)
    {
        setTag(status);
    }

    Status getStatus()
    {
        return getTag();
    }

    void setFlag(Flag flag)
    {
        setTag(flag);
    }

    Flag getFlag()
    {
        return getTag();
    }
};

template <class T, class U>
class NextNode
{
public: 
    T *mPackedPointer;

    NextNode()
    {
        mPackedPointer = (T *) malloc(sizeof(T));
        mPackedPointer = nullptr;
    }

    NextNode(T* packedPointer)
    {
        mPackedPointer = (T *) malloc(sizeof(T));
        mPackedPointer = packedPointer;
    }

    NextNode(T *packedPointer, U tag)
    {
        mPackedPointer = (T *) malloc(sizeof(T));
        uint64_t mask = (uint64_t) 0b11 << 62;
        mPackedPointer = (T*) ((uint64_t) packedPointer & (~mask));
        mask = (uint64_t) tag << 62;
        mPackedPointer = (T*)((uint64_t) packedPointer | mask);
    }

    void setPointerAndPreserveTag(T *pointer)
    {
        U tag = getTag();
        mPackedPointer = pointer;
        setTag(tag);
    }

    T *unpack()
    {
        uint64_t remove_tag = (uint64_t) 0b11 << 62;
        T *pointer = mPackedPointer;
        pointer = (T *) ((uint64_t) pointer & remove_tag);
        return pointer;
    }

    NextNode<T, U> *getCleanSelfPointer()
    {
        uint64_t remove_tag = (uint64_t) 0b11 << 62;
        NextNode<T, U> *pointer = this;
        pointer = (NextNode<T, U> *) ((uint64_t) pointer & remove_tag);
        return pointer;
    }

    void setTag(U tag)
    {
        uint64_t mask = (uint64_t) 0b11 << 62;
        mPackedPointer = (T*) ((uint64_t) mPackedPointer & (~mask));
        mask = (uint64_t) tag << 62;
        mPackedPointer = (T*)((uint64_t) mPackedPointer | mask);
    }

    U getTag()
    {
        return (U) ((uint64_t) mPackedPointer >> 62);
    }

    void setStatus(Status status)
    {
        setTag(status);
    }

    Status getStatus()
    {
        return getTag();
    }

    void setFlag(Flag flag)
    {
        setTag(flag);
    }

    Flag getFlag()
    {
        return getTag();
    }
};

template <class T, class U>
class PointerNode
{
public: 
    T *mPackedPointer;

    PointerNode()
    {
        mPackedPointer = (T *) malloc(sizeof(T));
        mPackedPointer = nullptr;
    }

    PointerNode(T* packedPointer)
    {
        mPackedPointer = (T *) malloc(sizeof(T));
        mPackedPointer = packedPointer;
    }

    PointerNode(T *packedPointer, U tag)
    {
        mPackedPointer = (T *) malloc(sizeof(T));
        uint64_t mask = (uint64_t) 0b11 << 62;
        mPackedPointer = (T*) ((uint64_t) packedPointer & (~mask));
        mask = (uint64_t) tag << 62;
        mPackedPointer = (T*)((uint64_t) packedPointer | mask);
    }

    void setPointerAndPreserveTag(T *pointer)
    {
        U tag = getTag();
        mPackedPointer = pointer;
        setTag(tag);
    }

    T *unpack()
    {
        uint64_t remove_tag = (uint64_t) 0b11 << 62;
        T *pointer = mPackedPointer;
        pointer = (T *) ((uint64_t) pointer & remove_tag);
        return pointer;
    }

    PointerNode<T, U> *getCleanSelfPointer()
    {
        uint64_t remove_tag = (uint64_t) 0b11 << 62;
        PointerNode<T, U> *pointer = this;
        pointer = (PointerNode<T, U> *) ((uint64_t) pointer & remove_tag);
        return pointer;
    }

    void setTag(U tag)
    {
        uint64_t mask = (uint64_t) 0b11 << 62;
        mPackedPointer = (T*) ((uint64_t) mPackedPointer & (~mask));
        mask = (uint64_t) tag << 62;
        mPackedPointer = (T*)((uint64_t) mPackedPointer | mask);
    }

    U getTag()
    {
        return (U) ((uint64_t) mPackedPointer >> 62);
    }

    void setStatus(Status status)
    {
        setTag(status);
    }

    Status getStatus()
    {
        return getTag();
    }

    void setFlag(Flag flag)
    {
        setTag(flag);
    }

    Flag getFlag()
    {
        return getTag();
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

        mValue = nullptr;
        mState = (StateNode<Position<V>, Status> *) malloc(sizeof(StateNode<Position<V>, Status>));
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

    NextNode<Position<V>, Status> *mNext;
    
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
    PointerNode<DataNode<V>, Flag> *pRoot;
    OperationRecord<V> **ST, **MT;
    uint32_t mNumThreads;
    uint32_t mIndex;

    ConcurrentTree(int numThreads)
    {
        mIndex = 0;
        mNumThreads = numThreads;

        // initialize pRoot with sentinel-valued DataNode
        auto pRoot = new PointerNode<DataNode<V>, Flag>(new DataNode<V>(), Flag::FREE);

        ST = (OperationRecord<V>**) malloc (sizeof(OperationRecord<V>*) * numThreads);
        MT = (OperationRecord<V>**) malloc (sizeof(OperationRecord<V>*) * numThreads);
        for (int i = 0; i < numThreads; i++) {
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
    void ExecuteWindowTransaction(Position<V> *pNode, DataNode<V> *dNode);
    bool ExecuteCheapWindowTransaction(Position<V> *pNode, DataNode<V> *dNode);
    void SlideWindowDown(Position<V> *pMoveFrom, DataNode<V> *dMoveFrom, Position<V> *pMoveTo, DataNode<V> *dMoveTo);
    Position<V> *GetPRootAsPosition();
    Position<V> *GetPointerNodeAsPosition(PointerNode<DataNode<V>, Flag> *pointerNode);
};

#include "concurrent.tcc"
#include <memory>
#include <pthread.h>

enum Status {WAITING = 0, IN_PROGRESS = 1, COMPLETED = 2};
enum Flag {FREE = 0, OWNED = 1};
enum Type {SEARCH, INSERT, UPDATE, DELETE};
enum Color {RED, BLACK, UNCOLORED};

template <class U>
class PackedPointer
{
public: 
    std::string* mPackedPointer;

    PackedPointer() = delete;

    PackedPointer(U tag, std::string* packedPointer)
    {
        uint64_t mask = (uint64_t) 0b11 << 62;
        mPackedPointer = (uint64_t) packedPointer & (!mask);
        mask = (uint64_t) tag << 62;
        mPackedPointer = (uint64_t) packedPointer | mask;
    }

    U getTag()
    {
        return (U) ((uint64_t) mPackedPointer >> 62);
    }
};

class ValueRecord
{
public:
    std::string mValue;
    uint32_t mGate;

    ValueRecord(std::string value, uint32_t gate)
    {
        mValue = value;
        mGate = gate;
    }
};

class DataNode
{
public:
    Color mColor;
    uint32_t mKey;
    ValueRecord *mValData;
    OperationRecord *mOpData;

    DataNode *mLeft;
    DataNode *mRight;

    PackedPointer<Status> mNext;
    
    //sentinel use only ***
    DataNode()
    {
        mColor = BLACK;
        mValData = new ValueRecord(NULL, 0);
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

class OperationRecord
{
    Type mType;
    uint32_t mKey;
    std::string mValue;
    uint32_t mPid;
    PackedPointer<Status> mState;

    OperationRecord(Type type, uint32_t key, std::string value)
    {
        mType = type;
        mKey = key;
        mValue = value;
    }
};

class ConcurrentTree
{
public:
    ConcurrentTree() {};
    std::string Search(K key);
    void InsertOrUpdate(K key, std::string value);
    void Delete(K key);
    void Traverse(DataNode<std::string> opData);
    void ExecuteOperation(DataNode<std::string> opData);
    void InjectOperation(DataNode<std::string> opData);
    void ExecuteWindowTransaction(DataNode<std::string> pNode, DataNode<K,std::string> dNode);
    bool ExecuteCheapWindowTransaction(DataNode<std::string> pNode, DataNode<std::string> dNode);
    void SlideWindowDown(PointerNode<std::string> pMoveFrom, PointerNode<std::string> dMoveFrom, PointerNode<std::string> pMoveTo, PointerNode<std::string> dMoveTo);
};

#include "concurrent.tcc"
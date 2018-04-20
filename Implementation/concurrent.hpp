#include <memory>

enum Status {WAITING, IN_PROGRESS, COMPLETED};
enum Type {SEARCH, INSERT, UPDATE, DELETE};
enum Color {RED, BLACK, UNCOLORED};
enum Flag {FREE, OWNED};

template <class V>
class ValueRecord
{
    V value;
    uint32_t gate;
};

template <class V>
class State
{
public:
    Status status;
    ValueRecord<V> *position;

    State()
    {

    }
};

template <class V>
class DataNode
{
public:
    Color color;
    State<V> state;
    K key;
    V value;
    uint32_t pid;

    DataNode *left;
    DataNode *right;

    DataNode()
    {
        color = UNCOLORED;
        left = nullptr;
        right = nullptr;
    }

    DataNode *clone()
    {
        DataNode *copy = new DataNode();
        copy.color  = color;
        copy.state  = state;
        copy.key    = key;
        copy.value  = value;
        copy.pid    = pid;
        copy.left   = left;
        copt.right  = right;
    }
};

template <class V>
class PointerNode
{
    Flag flag;
    std::shared_ptr<DataNode<V>> dNode;
};

template <class V>
class OperationRecord
{
    Type type;
    State<V> state;
    V value;
    K key;
    uint32_t pid;
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
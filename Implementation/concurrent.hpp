#include <memory>

enum Status {WAITING, IN_PROGRESS, COMPLETED};
enum Type {SEARCH, INSERT, UPDATE, DELETE};
enum Color {RED, BLACK, UNCOLORED};
enum Flag {FREE, OWNED};

template <class K, class V>
class ValueRecord
{
    V value;
    uint32_t gate;
};

template <class K, class V>
class State
{
    Status status;
    std::shared_ptr<ValueRecord<K, V>> position;
};

template <class K, class V>
class DataNode
{
    Color color;
    State<K, V> state;
    K key;
    V value;
    uint32_t pid;
};

template <class K, class V>
class PointerNode
{
    Flag flag;
    std::shared_ptr<DataNode<K, V>> dNode;
};

template <class K, class V>
class OperationRecord
{
    Type type;
    State<K, V> state;
    V value;
    K key;
    uint32_t pid;
};

template <class K, class V>
class ConcurrentTree
{
public:
    ConcurrentTree() {};
    V Search(K key);
    void InsertOrUpdate(K key, V value);
    void Delete(K key);
    void Traverse(DataNode<K, V> opData);
    void ExecuteOperation(DataNode<K, V> opData);
    void InjectOperation(DataNode<K, V> opData);
    void ExecuteWindowTransaction(DataNode<K, V> pNode, DataNode<K,V> dNode);
    bool ExecuteCheapWindowTransaction(DataNode<K, V> pNode, DataNode<K, V> dNode);
    void SlideWindowDown(PointerNode<K, V> pMoveFrom, PointerNode<K, V> dMoveFrom, PointerNode<K, V> pMoveTo, PointerNode<K, V> dMoveTo);
};

#include "concurrent.tcc"
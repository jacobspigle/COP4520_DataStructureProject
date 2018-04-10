class Enums
{
    static enum Status {WAITING, IN_PROGRESS, COMPLETED};
    static enum Type {SEARCH, INSERT, UPDATE, DELETE};
    static enum Color {RED, BLACK, NONE};
    static enum Flag {FREE, OWNED};
};

template <class K, class V>
class ConcurrentTree
{
public:
    ConcurrentTree() {};
    V Search(K key);
    void InsertOrUpdate(K key, V value);
    void Delete(K key);
    void Traverse(DataNode opData);
    void ExecuteOperation(DataNode opData);
    void InjectOperation(DataNode opData);

private:
    struct State
    {
        Enums::Status status;
        std::shared_ptr<ValueRecord> position;
    };

    class ValueRecord
    {
        V value;
        uint32_t gate;
    };

    class DataNode
    {
        Enums::Color color;
        Enums::State state;
        K key;
        V value;
        uint32_t pid;
    };

    class PointerNode
    {
        Enums::Flag flag;
        std::shared_ptr<DataNode> dNode;
    };

    class OperationRecord
    {
        Enums::Type type;
        Enums::State state;
        V value;
        K key;
        uint32_t pid;
    };
};
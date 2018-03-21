#include <thread>

namespace RedBlackTree
{
    template <class K, class V>
    class ConcurrentTree
    {
    public:
        ConcurrentTree() {};
        bool Search(K key);
        void InsertOrUpdate(K key, V value);
        void Delete(K key);
        void Travers(DataNode opData);
        void ExecuteOperation(DataNode opData);
    
    private:
        struct State
        {
            enum Status {WAITING, IN_PROGRESS, COMPLETED};

            Status status;
            std::shared_ptr<ValueRecord> position;
        };

        class ValueRecord
        {
            V value;
            uint32_t gate;
        };

        class DataNode
        {
            enum Color {RED, BLACK, NONE};

            Color color;
            State state;
            K key;
            V value;
            uint32_t pid;
        };

        class PointerNode
        {
            enum Flag {FREE, OWNED};

            Flag flag;
            std::shared_ptr<DataNode> dNode;
        };

        class OperationRecord
        {
            enum Type {SEARCH, INSERT, UPDATE, DELETE};

            Type type;
            State state;
            V value;
            K key;
            uint32_t pid;
        };
    };
}
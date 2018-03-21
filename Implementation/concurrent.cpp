#include "concurrent.hpp"

namespace RedBlackTree
{
    template <class K, class V>
    V ConcurrentTree::Search(K key)
    {
        // create and initialize a new operation record
        DataNode opData = new DataNode(Status::SEARCH, key, nullptr);

        // initialize the operation state
        opData->state = Status::IN_PROGRESS;

        // initialize the search table entry
        ST[myid].opData = opData;

        // traverse the tree
        Traverse(opData);

        if(opData->state->position == nullptr) {
            // read the value stored in the record and return it
            return opData->value;
        }
        else {
            return nullptr;
        }
    }

    template <class K, class V>
    void ConcurrentTree::InsertOrUpdate(K key, V value)
    {
        V valData = nullptr;

        // phase 1: determine if the key already exists in the tree
        Search(key);

        valData = ST[myid].opData->state->position;

        if(valData == nullptr) {
            // phase 2: try to add the key-value pair to the tree using the MTL-framework
            // select a search operation to help at the end of phase 2 to ensure wait freedom
            uint32_t pid = select(); // the process selected to help in round-robin manner
            pidOpData := ST[pid].opData;

            // create and initialize a new operation record
            opData = new DataNode(Status::INSERT, key, value);

            // add the key-value pair to the tree
            ExecuteOperation(opData);
            valData = opData->state->position;

            // help the selected search operation complete
            if(pidOpData != nullptr) {
                traverse( pidOpData );
            }
        }

        if(valData != nullptr) {
            // phase 3: update the value in the record using Chuong et al.'s algorithm
            // TODO: implement Chuong et al.'s algorithm
        }
    }

    template <class K, class V>
    void ConcurrentTree::Delete(K key)
    {
        // phase 1: determine if the key already exists in the tree
        if(Search(key)) {
            // phase 2: try to delete the key from the tree using the MTL-framework
            // select a search operation to help at the end of phase 2 to ensure wait-freedom
            uint32_t pid = Select(); // the process selected to help in a round-robin manner
            DataNode pidOpData = ST[pid].opData;

            // create and initialize a new operation record
            DataNode opData = new DataNode(Status::DELETE, key, nullptr);

            //remove the key from the tree
            ExecuteOperation(opData);

            if(pidOpData != nullptr) {
                // help the selected search operation complete
                Traverse(pidOpData);
            }
        }
    }

    template <class K, class V>
    void ConcurrentTree::Traverse(DataNode opData)
    {
        // start from the root of the tree
        DataNode dCurrent = pRoot;

        while(dCurrent->left || dCurrent->right)
        {
            // abort the traversal if no longer needed
            if(opData->state->status == Status::Completed) {
                return;
            }

            // find the next node to visit
            if(dCurrent->left && opData->key < dCurrent->key) {
                dCurrent = dCurrent->left;
            }
            else if(dCurrent->right) {
                dCurrent = dCurrent->right;
            }
        }

        V valData;

        if(dCurrent->key = opData->key) {
            valData = dCurrent->valData;
        }
        else {
            valData = nullptr;
        }

        opData->state = COMPLETED;
        opData->value = valData;
    }

    template <class K, class V>
    void ConcurrentTree::ExecuteOperation(DataNode opData)
    {
        // initialize the operation state
        opData->state = Status::WAITING;

        // initialize the modify table entry
        MT[myid].opData = opData;

        // select a modify operation to help later at the end to ensure wait-freedom
        pid = the process selected to help in round-robin manner;
        pidOpData := MT[pid].opData;

        // inject the operation into the tree
        injectOperation(opData);

        // repeatedly execute transactions until the operation completes
        pCurrent->status = opData->state;
        while(pCurrent->status != Status::COMPLETED)
        {
            dCurrent := pCurrent dNode;

            if(dCurrent->opData == opData) {
                ExecuteWindowTransaction(pCurrent, dCurrent);
            }

            pCurrent->status = opData->state;
        }

        if(OpData->pid != null) {
            // help inject the selected operation
            injectOperation(pidOpData);
        }
    }

    template <class K, class V>
    void ConcurrentTree::InjectOperation(DataNode opData)
    {
        // repeatedly try until the operation is injected into the tree
        while(opData->state->status == Status::WAITING)
        {
            DataNode dRoot = pRoot->dNode;

            // execute a window transaction, if needed
            if(dRoot->opData != nullptr) {
                ExecuteWindowTransaction(pRoot, dRoot);
            }

            // read the address of the data node again
            DataNode dNow = pRoot->dNode;

            // if they match, then try to inject the operation into the tree,
            // othewise restart
            if(dRoot == dNow)
            {
                DataNode dCopy = clone(dRoot);
                dCopy->opData = opData;

                // try to obtain the ownership of the root of the tree
                result = pRoot.compare_and_swap_strong({FREE, dRoot}, {OWNED, dCopy});

                if(result) {
                    // the operation has been successfully injected
                    // update the operation state
                    opData->state.compare_and_swap_strong({WAITING, pRoot}, {IN_PROGRESS, pRoot});
                }
            }
        }
    }
}
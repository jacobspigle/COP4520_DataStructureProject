template <class V>
V ConcurrentTree<V>::Search(uint32_t key)
{
    // create and initialize a new operation record
    OperationRecord<V> opData = new OperationRecord<V>(Status.SEARCH, key, nullptr);

    // initialize the operation state
    opData->mState->setTag(Status.IN_PROGRESS);

    // initialize the search table entry
    ST[myid].opData = opData;

    // traverse the tree
    Traverse(opData);

    if(opData->state == nullptr) {
        // read the value stored in the record and return it
        return opData->value;
    }
    else {
        return nullptr;
    }
}

template <class V>
void ConcurrentTree<V>::InsertOrUpdate(uint32_t key, V value)
{
    V *valData = nullptr;

    // phase 1: determine if the key already exists in the tree
    Search(key);

    valData = ST[myid].opData->state;

    if(valData == nullptr) {
        // phase 2: try to add the key-value pair to the tree using the MTL-framework
        // select a search operation to help at the end of phase 2 to ensure wait freedom
        uint32_t pid = select(); // the process selected to help in round-robin manner
        OperationRecord<V> pidOpData = ST[pid].opData;

        // create and initialize a new operation record
        OperationRecord<V> opData = new OperationRecord<V>(Status.INSERT, key, value);

        // add the key-value pair to the tree
        ExecuteOperation(opData);
        valData = opData->state;

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

template <class V>
void ConcurrentTree<V>::Delete(uint32_t key)
{
    // phase 1: determine if the key already exists in the tree
    if(Search(key)) {
        // phase 2: try to delete the key from the tree using the MTL-framework
        // select a search operation to help at the end of phase 2 to ensure wait-freedom
        uint32_t pid = Select(); // the process selected to help in a round-robin manner
        OperationRecord<V> pidOpData = ST[pid].opData;

        // create and initialize a new operation record
        OperationRecord<V> opData = new OperationRecord<V>(Status.DELETE, key, nullptr);

        //remove the key from the tree
        ExecuteOperation(opData);

        if(pidOpData != nullptr) {
            // help the selected search operation complete
            Traverse(pidOpData);
        }
    }
}

template <class V>
void ConcurrentTree<V>::Traverse(OperationRecord<V> opData)
{
    // start from the root of the tree
    DataNode<V> dCurrent = pRoot;

    while(dCurrent->left || dCurrent->right)
    {
        // abort the traversal if no longer needed
        if(opData->state->getTag() == Status.Completed) {
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

    ValueRecord<V> *valData;

    if(dCurrent->key = opData->key) {
        valData = dCurrent->valData;
    }
    else {
        valData = nullptr;
    }

    opData->state->setTag(Status.COMPLETED);
    opData->value = valData;
}

template <class V>
void ConcurrentTree<V>::ExecuteOperation(OperationRecord<V> opData)
{
    // initialize the operation state
    opData->state->setTag(Status.WAITING);

    // initialize the modify table entry
    MT[myid].opData = opData;

    // select a modify operation to help later at the end to ensure wait-freedom
    pid = the process selected to help in round-robin manner;
    OperationRecord<V> *pidOpData = MT[pid].opData;

    // inject the operation into the tree
    injectOperation(opData);

    // repeatedly execute transactions until the operation completes
    PackedPointer *pCurrent = opData->mState;
    while(pCurrent->getTag() != Status.COMPLETED)
    {
        DataNode<V> *dCurrent = *pCurrent;

        if(dCurrent->mOpData == opData) {
            ExecuteWindowTransaction(pCurrent, dCurrent);
        }

        pCurrent = opData->state;
    }

    if(opData->pid != null) {
        // help inject the selected operation
        injectOperation(pidOpData);
    }
}

template <class V>
void ConcurrentTree<V>::InjectOperation(DataNode<V> opData)
{
    // repeatedly try until the operation is injected into the tree
    while(opData->state->status == Status.WAITING)
    {
        DataNode<V> dRoot = pRoot->dNode;

        // execute a window transaction, if needed
        if(dRoot->opData != nullptr) {
            ExecuteWindowTransaction(pRoot, dRoot);
        }

        // read the address of the data node again
        DataNode<V> dNow = pRoot->dNode;

        // if they match, then try to inject the operation into the tree,
        // othewise restart
        if(dRoot == dNow)
        {
            DataNode<V> dCopy = clone(dRoot);
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


template<class V>
void ConcurrentTree<V>::ExecuteWindowTransaction(DataNode<V> pNode, DataNode<uint32_t,V> dNode)
{
    // execute a window transaction for the operation stored in dNode
    DataNode<V> opData = dNode->opData;
    {flag, dCurrent} = read( pNode ); // read the contents of pNode again
    if (dCurrent->opData = opData) {
        if (flag == OWNED) {
            if (pNode == pRoot) {
                // the operation may have just been injected into the tree, but the operation
                state may not have been updated yet; update the state
                CAS( opData->state, {WAITING, pRoot}, {IN PROGRESS, pRoot} );
            }
            if(executeCheapWindowTransaction( pNode, dCurrent ) == false) {
                // traverse the window using Tarjan’s algorithm, making copies as required
                windowSoFar = {clone( dCurrent )};
                while(more nodes need to be added to windowSoFar)
                {
                    pNextToAdd = the address of the pointer node of the next tree node to be copied;
                    dNextToAdd = pNextToAdd dNode;
                    // help the operation located at this node, if any, move out of the way
                    if (dNextToAdd->opData != null) {
                        executeWindowTransaction(pNextToAdd, dNextToAdd);
                    }
                    // read the address of the data node again as it may have changed
                    dNextToAdd = pNextToAdd dNode;
                    // copy pNextToAdd and dNextToAdd, and add them to windowSoFar;
                }
                // window has been copied; now apply transformations dictated by Tarjan’ algorithm to windowSoFar;
                dWindowRoot = // the address of the data node now acting as window root in windowSoFar;
                if (last/terminal window transaction) {
                    status = COMPLETED;
                    pMoveTo = (
                    the address of the record containing the value : if an update operation;
                    null : otherwise;
                }
                else {
                    status = IN PROGRESS;
                    pMoveTo = // the address of the pointer node of the node in windowSoFar to which the operation will now move;
                    pMoveTo flag = OWNED;
                    dMoveTo = pMoveTo dNode;
                    dMoveTo->opData = opData;
                }

                dWindowRoot->opData = opData;
                dWindowRoot->next = {status, pMoveTo};

                // replace the tree window with the local copy and release the ownership
                CAS(pNode, {OWNED, dCurrent}, {FREE, dW indowRoot});
            }
        }

        // at this point, no operation should own pNode; may still need to update the
        // operation state with the new position of the operation window
        dNow = pNode dNode;

        if (dNow->opData = opData) {
            CAS(opData->state, {IN_PROGRESS, pNode}, dNow->next);
        }
    }
}

template<class V>
bool ConcurrentTree<V>::ExecuteCheapWindowTransaction(DataNode<V> pNode, DataNode<V> dNode)
{
    DataNode opData = dNode->opDate;
    uint32_t pid = opData->pid;

    // traverse the tree window using Tarjan’s algorithm
    while(true)
    {
        DataNode pNextToVisit = // the address of the pointer node of the next tree node to be visited;
        DataNode dNextToVisit = // pNextToVisit dNode;

        if ((opData->state)->position = pNode) {
            return true; // abort; transaction already executed
        }
        // if there is an operation residing at the node, then help it move out of the way
        if (dNextToVisit->opData != null) {
            // there are several cases to consider
            if ((dNextToVisit->opData)->pid != pid) {
                // the operation residing at the node belongs to a different process
                executeWindowTransaction(pNextToVisit, dNextToVisit);
                // read the address of the data node again as it may have changed
                dNextToV isit = pNextToV isit dNode;
                if ((opData->state) position != pNode) {
                    return true; // abort; transaction already executed
                }
            }
            else if (dNextToV isit->opData = dNode->opData) {
                // partial window transaction has already been executed; complete it if needed
                if ((opData->state) position = pNode) {
                    slideWindowDown(pNode, dNode, pNextToVisit, dNextToVisit);
                }
                return true;
            }
            else if (MT[pid].opData != opData) {
                return true; // abort; transaction already executed
            }
        }

        // visit dNextToVisit;
    }
    if (no transformation needs to be applied to the tree window) {
        if (last/terminal window transaction) {
            if(update operation) {
                pMoveTo = // the address of the record containing the value
            }
            else {
                pMoveTo = nullptr;
            }
            
            dMoveTo = null;
        }
        else {
            pMoveTo = // the address of the pointer node of the node in the tree to which the operation will now move;
            dMoveTo = pMoveTo dNode;
        }

        if (opData->state) position = pNode {
            slideWindowDown( pNode, dNode, pMoveTo, dMoveTo );
        }

        return true;
    }
    else {
        return false;
    }
}

template<class V>
void ConcurrentTree<V>::SlideWindowDown(PointerNode<V> pMoveFrom, PointerNode<V> dMoveFrom, PointerNode<V> pMoveTo, PointerNode<V> dMoveTo)
{
    DataNode opData = dMoveFrom->opData;

    // copy the data node of the current window location
    dCopyMoveFrom = clone( dMoveFrom );
    dCopyMoveFrom->opData = opData;

    if(dMoveTo != null) {
        dCopyMoveFrom->next = {IN PROGRESS, pMoveTo};
    }
    else {
        dCopyMoveFrom->next = {COMPLETED, pMoveTo};
    }

    // copy the data node of the next window location, if needed
    if(dMoveTo != null) {
        if(dMoveTo->opData != opData) {
            dCopyMoveTo = clone(dMoveTo);
            dCopyMoveTo->opData = opData;
            // acquire the ownership of the next window location
            CAS( pMoveTo, {FREE, dMoveTo}, {OWNED, dCopyMoveTo} );
        }
    }

    // release the ownership of the current window location and update the operation state
    CAS( pMoveFrom, {OWNED, dMoveFrom}, {FREE, dCopyMoveFrom} );
    CAS( opData->state, {IN PROGRESS, pMoveFrom}, dCopyMoveFrom->next );
}
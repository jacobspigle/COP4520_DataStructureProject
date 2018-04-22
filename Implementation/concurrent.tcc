template <class V>
V ConcurrentTree<V>::Search(uint32_t key)
{
    // create and initialize a new operation record
    OperationRecord<V> opData = new OperationRecord<V>(Status.SEARCH, key, nullptr);

    // initialize the operation state
    opData->mState->setTag(Status.IN_PROGRESS);

    // initialize the search table entry
    ST[myid] = opData;

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
void ConcurrentTree<V>::InsertOrUpdate(ConcurrentTree<V> *tree, uint32_t key, V value)
{
    V *valData = nullptr;

    // phase 1: determine if the key already exists in the tree
    Search(key);

    valData = tree->ST[myid]->mState;

    if(valData == nullptr) {
        // phase 2: try to add the key-value pair to the tree using the MTL-framework
        // select a search operation to help at the end of phase 2 to ensure wait freedom
        uint32_t pid = Select(); // the process selected to help in round-robin manner
        OperationRecord<V> *pidOpData = tree->ST[pid];

        // create and initialize a new operation record
        OperationRecord<V> *opData = new OperationRecord<V>(Status.INSERT, key, value);

        // add the key-value pair to the tree
        ExecuteOperation(opData);
        valData = opData->mState;

        // help the selected search operation complete
        if(pidOpData != nullptr) {
            Traverse( pidOpData );
        }
    }

    if(valData != nullptr) {
        // phase 3: update the value in the record using Chuong et al.'s algorithm
        // TODO: implement Chuong et al.'s algorithm
        /*
            valData->mValue = value;
        */
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
        OperationRecord<V> pidOpData = ST[pid];

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
void ConcurrentTree<V>::Select(ConcurrentTree<V> *Tree)
{
    uint32_t fetched_pid = Tree->mIndex;
    Tree->mIndex = (Tree->mIndex + 1) % Tree->mNumThreads;
    return fetched_pid;
}

template <class V>
void ConcurrentTree<V>::Traverse(OperationRecord<V> *opData)
{
    // start from the root of the tree
    DataNode<V> *dCurrent = pRoot->mPackedPointer;

    // find a leaf
    while(dCurrent->mLeft != nullptr || dCurrent->mRight != nullptr)
    {
        // abort the traversal if no longer needed
        if(opData->mState->getTag() == Status.Completed) {
            return;
        }

        // find the next node to visit
        if(dCurrent->mLeft && opData->mKey < dCurrent->mKey) {
            dCurrent = dCurrent->mLeft->getDataNode();
        }
        else if(dCurrent->mRight) {
            dCurrent = dCurrent->mRight->getDataNode();
        }
    }

    // leafy stuff
    ValueRecord<V> *valData;

    if(dCurrent->mKey = opData->mKey) {
        valData = dCurrent->mValData;
    }
    else {
        valData = nullptr;
    }

    opData->mState = valData;
    opData->mState->setTag(Status.COMPLETED);
}

template <class V>
void ConcurrentTree<V>::ExecuteOperation(ConcurrentTree<V> tree, OperationRecord<V> *opData)
{
    // initialize the operation state
    opData->mState->setTag(Status.WAITING);

    // initialize the modify table entry
    MT[myid] = opData;

    // select a modify operation to help later at the end to ensure wait-freedom
    uint32_t pid = tree->Select(); // the process selected to help in round-robin manner;

    OperationRecord<V> *pidOpData = MT[pid];

    // inject the operation into the tree
    tree->InjectOperation(opData);

    // repeatedly execute transactions until the operation completes
    PackedPointer *pCurrent = opData->mState;
    while(pCurrent->getTag() != Status.COMPLETED)
    {
        DataNode<V> *dCurrent = pCurrent->getDataNode();

        if(dCurrent->mOpData == opData) {
            ExecuteWindowTransaction(pCurrent, dCurrent);
        }

        pCurrent = opData->mState;
    }

    if(opData->mPid != null) {
        // help inject the selected operation
        InjectOperation(pidOpData);
    }
}

template <class V>
void ConcurrentTree<V>::InjectOperation(ConcurrentTree<V> tree, OperationRecord<V> *opData)
{
    // repeatedly try until the operation is injected into the tree
    while(opData->state->getTag() == Status.WAITING)
    {
        DataNode<V> *dRoot = tree->pRoot->getDataNode();

        // execute a window transaction, if needed
        if(dRoot->mOpData != nullptr) {
            ExecuteWindowTransaction(tree->pRoot, dRoot);
        }

        // read the address of the data node again
        DataNode<V> *dNow = tree->pRoot->getDataNode();

        // if they match, then try to inject the operation into the tree,
        // othewise restart
        if(dRoot == dNow)
        {
            DataNode<V> *dCopy = dRoot->clone();
            dCopy->mOpData = opData;

            PointerNode<V> *dRootFree = new PointerNode<V>(dRoot, Flag.FREE);
            PointerNode<V> *dCopyOwned = new PointerNode<V>(dCopy, Flag.OWNED);

            // try to obtain the ownership of the root of the tree
            if(__sync_bool_compare_and_swap(tree->pRoot, dRootFree, dCopyOwned)) {
                // the operation has been successfully injected
                // update the operation state
                PointerNode<V> *pRootWaiting = new PointerNode<V>(tree->pRoot->getDataNode(), Status.WAITING);
                PointerNode<V> *pRootInProgress = new PointerNode<V>(tree->pRoot->getDataNode(), Status.IN_PROGRESS);

                if(!__sync_bool_compare_and_swap(opData->state, pRootWaiting, pRootInProgress)) {
                    free(pRootInProgress);
                }

                free(pRootWaiting);
            }
            else {
                free(dCopyFree);
            }

            free(dRootFree);
        }
    }
}


template<class V>
void ConcurrentTree<V>::ExecuteWindowTransaction(ConcurrentTree<V> *tree, PointerNode<V> *pNode, DataNode<V> *dNode)
{
    // execute a window transaction for the operation stored in dNode
    OperationRecord<V> *opData = dNode->mOpData;
    PointerNode<V> *pCurrent = pNode; // read the contents of pNode again
    if (dCurrent->mOpData == opData) {
        if (pCurrent->getFlag() == Flag.OWNED) {
            if (pNode->getPointerNode() == tree->pRoot->getPointerNode()) {
                // the operation may have just been injected into the tree, but the operation
                // state may not have been updated yet; update the state
                PointerNode<V> *pRootWaiting = new PointerNode<V>(tree->pRoot->getDataNode(), Status.WAITING);
                PointerNode<V> *pRootInProgress = new PointerNode<V>(tree->pRoot->getDataNode(), Status.IN_PROGRESS);

                if(!__sync_bool_compare_and_swap(opData->state, pRootWaiting, pRootInProgress)) {
                    free(pRootInProgress);
                }

                free(pRootWaiting);
            }


            if(ExecuteCheapWindowTransaction(pNode, pCurrent->getDataNode()) == false) {

                // traverse the window using Tarjan’s algorithm, making copies as required
                
                DataNode<V> *windowSoFar = pCurrent->getDataNode()->clone();

                PointerNode<V> *pNextToAdd
                DataNode<V> *dNextToAdd;

                bool leftAcquired = false;
                bool rightAcquired = false;

                while(true)
                {
                    bool isLeft = false;

                    if(!leftAcquired) {
                        pNextToAdd = pCurrent->mLeft; // the address of the pointer node of the next tree node to be copied;
                        isLeft = true;
                    }
                    else if(!rightAcquired) {
                        pNextToAdd = pCurrent->mRight; // the address of the pointer node of the next tree node to be copied;
                    }
                    else {
                        break;
                    }

                    if(pNextToAdd == nullptr) {
                        if(isLeft) {
                            leftAcquired = true;
                        }
                        else {
                            rightAcquired = true;
                        }

                        continue;
                    }

                    dNextToAdd = pNextToAdd->getDataNode();

                    // help the operation located at this node, if any, move out of the way
                    if (dNextToAdd->mOpData != nullptr) {
                        ExecuteWindowTransaction(pNextToAdd, dNextToAdd);
                    }

                    // read the address of the data node again as it may have changed
                    dNextToAdd = pNextToAdd->getDataNode();

                    // copy pNextToAdd and dNextToAdd, and add them to windowSoFar;
                    if(isLeft) {
                        windowSoFar->mLeft = new PointerNode<V>(dNextToAdd->clone(), pNextToAdd->getFlag());
                        leftAcquired = true;
                    }
                    else {
                        windowSoFar->mRight = new PointerNode<V>(dNextToAdd->clone(), pNextToAdd->getFlag());
                        rightAcquired = true;
                    }
                }

                DataNode<V> *dWindowRoot = windowSoFar;
                // window has been copied; now apply transformations dictated by Tarjan’ algorithm to windowSoFar;
                if(!(windowSoFar->mLeft == nullptr && windowSoFar->mRight == nullptr)) {
                    // rotate

                    // dWindowRoot = the address of the data node now acting as window root in windowSoFar;
                }

                PointerNode<V> *pMoveTo;

                if (windowSoFar->mNext == nullptr/*last/terminal window transaction*/) {
                    opData->mState->setStatus(COMPLETED);

                    // the address of the record containing the value : if an update operation;
                    // null : otherwise;
                    if(opData->mType == Type.UPDATE) {
                        pMoveTo = opData->mType->mPackedPointer;
                    }
                    else {
                        pMoveTo = nullptr;
                    )
                }
                else {
                    opData->mState->setStatus(IN_PROGRESS);
                    pMoveTo = windowSoFar->mNext; // the address of the pointer node of the node in windowSoFar to which the operation will now move;
                    pMoveTo->setFlag(Flag.OWNED);
                    dMoveTo = pMoveTo->getDataNode();
                    dMoveTo->mOpData = opData;
                }

                dWindowRoot->mOpData = opData;
                dWindowRoot->mNext = new NextNode<V>(pMoveTo->mPackedPointer); // {status, pMoveTo};

                // replace the tree window with the local copy and release the ownership
                DataNode<V> *dCurrentOwned = new DataNode<V>(dCurrent->mPackedPointer, Flag.OWNED);
                DataNode<V> *dWindowRootFree = new DataNode<V>(dWindowRoot->mPackedPointer, Flag.FREE);

                if(!__sync_bool_compare_and_swap(pNode, {OWNED, dCurrent}, {FREE, dWindowRoot})) {
                    free(dWindowRootFree);
                }

                free(dCurrentOwned);
            }
        }

        // at this point, no operation should own pNode; may still need to update the
        // operation state with the new position of the operation window
        DataNode<V> *dNow = pNode->getDataNode();

        if (dNow->mOpData == opData) {
            PointerNode<V> *pNodeInProgress = new PointerNode<V>(pNode->mPackedPointer, Status.IN_PROGRESS);
            if(!__sync_bool_compare_and_swap(opData->mState, pNodeInProgress, dNow->mNext)) {
                free(pNodeInProgress);
            }
        }
    }
}

template<class V>
bool ConcurrentTree<V>::ExecuteCheapWindowTransaction(PointerNode<V> *pNode, DataNode<V> *dNode)
{
    OperationRecord<V> opData = dNode->mOpData;
    uint32_t pid = opData->mPid;

    // traverse the tree window using Tarjan’s algorithm

    while(true)
    {
        PointerNode<V> *pNextToVisit = dNode->mNext; // the address of the pointer node of the next tree node to be visited;
        DataNode<V> *dNextToVisit = pNextToVisit->getDataNode(); // pNextToVisit dNode;

        if (opData->mState->getPointerNode() == pNode->getPointerNode()) {
            return true; // abort; transaction already executed
        }
        // if there is an operation residing at the node, then help it move out of the way
        if (dNextToVisit->mOpData != nullptr) {
            // there are several cases to consider
            if (dNextToVisit->mOpData->mPid != pid) {
                // the operation residing at the node belongs to a different process
                ExecuteWindowTransaction(pNextToVisit, dNextToVisit);
                
                // read the address of the data node again as it may have changed
                dNextToVisit = pNextToVisit->getDataNode();
                if (opData->mState->getPointerNode() != pNode->getPointerNode()) {
                    return true; // abort; transaction already executed
                }
            }
            else if (dNextToVisit->mOpData == dNode->mOpData) {
                // partial window transaction has already been executed; complete it if needed
                if (opData->mState->getPointerNode() == pNode->getPointerNode()) {
                    SlideWindowDown(pNode, dNode, pNextToVisit, dNextToVisit);
                }
                return true;
            }
            else if (MT[pid] != opData) {
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
            dMoveTo = pMoveTodNode;
        }

        if (opData->state) position = pNode {
            SlideWindowDown(pNode, dNode, pMoveTo, dMoveTo);
        }

        return true;
    }
    else {
        return false;
    }
}

template<class V>
void ConcurrentTree<V>::SlideWindowDown(PointerNode<V> *pMoveFrom, DataNode<V> *dMoveFrom, PointerNode<V> *pMoveTo, DataNode<V> *dMoveTo)
{
    DataNode<V> *opData = dMoveFrom->mOpData;

    // copy the data node of the current window location
    dCopyMoveFrom = clone( dMoveFrom );
    dCopyMoveFrom->mOpData = opData;

    if(dMoveTo != nullptr) {
        pMoveTo->setStatus(Status.IN_PROGRESS);
        dCopyMoveFrom->next = pMoveTo;
    }
    else {
        pMoveTo->setStatus(Status.COMPLETED);
        dCopyMoveFrom->next = pMoveTo;
    }

    // copy the data node of the next window location, if needed
    if(dMoveTo != nullptr) {
        if(dMoveTo->mOpData != opData) {
            dCopyMoveTo = dMoveTo.clone();
            dCopyMoveTo->mOpData = opData;

            // acquire the ownership of the next window location
            CAS( pMoveTo, {FREE, dMoveTo}, {OWNED, dCopyMoveTo} );
        }
    }

    // release the ownership of the current window location and update the operation state
    CAS( pMoveFrom, {OWNED, dMoveFrom}, {FREE, dCopyMoveFrom} );
    CAS( opData->state, {IN PROGRESS, pMoveFrom}, dCopyMoveFrom->next );
}
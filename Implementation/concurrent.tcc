template <class V>
V *ConcurrentTree<V>::Search(uint32_t key, int myid)
{
    // create and initialize a new operation record
    OperationRecord<V> *opData = new OperationRecord<V>(Type::SEARCH, key, nullptr);

    // initialize the operation state
    opData->mState->setTag(Status::IN_PROGRESS);

    // initialize the search table entry
    ST[myid] = opData;

    // traverse the tree
    Traverse(opData);

    if(opData->mState == nullptr) {
        // read the value stored in the record and return it
        return opData->mValue;
    }
    else {
        return nullptr;
    }
}

template <class V>
void ConcurrentTree<V>::InsertOrUpdate(uint32_t key, V *value, int myid)
{
    ValueRecord<V> *valData = nullptr;

    // phase 1: determine if the key already exists in the tree
    Search(key, myid);

    valData = this->ST[myid]->mState->getPosition()->valueRecord;

    if(valData == nullptr) {
        // phase 2: try to add the key-value pair to the tree using the MTL-framework
        // select a search operation to help at the end of phase 2 to ensure wait freedom
        uint32_t pid = Select(); // the process selected to help in round-robin manner
        OperationRecord<V> *pidOpData = this->ST[pid];

        // create and initialize a new operation record
        OperationRecord<V> *opData = new OperationRecord<V>(Type::INSERT, key, value);

        // add the key-value pair to the tree
        ExecuteOperation(opData, myid);
        valData = opData->mState->getPosition()->valueRecord;

        // help the selected search operation complete
        if(pidOpData != nullptr) {
            Traverse(pidOpData);
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
void ConcurrentTree<V>::Delete(uint32_t key, int myid)
{
    // phase 1: determine if the key already exists in the tree
    if(Search(key, myid)) {
        // phase 2: try to delete the key from the tree using the MTL-framework
        // select a search operation to help at the end of phase 2 to ensure wait-freedom
        uint32_t pid = Select(); // the process selected to help in a round-robin manner
        OperationRecord<V> pidOpData = ST[pid];

        // create and initialize a new operation record
        OperationRecord<V> opData = new OperationRecord<V>(Type::DELETE, key, nullptr);

        //remove the key from the tree
        ExecuteOperation(opData);

        if(pidOpData != nullptr) {
            // help the selected search operation complete
            Traverse(pidOpData);
        }
    }
}

template <class V>
uint32_t ConcurrentTree<V>::Select()
{
    uint32_t fetched_pid = this->mIndex;
    this->mIndex = (this->mIndex + 1) % this->mNumThreads;
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
        if(opData->mState->getTag() == Status::COMPLETED) {
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
    Position<V> *valData;

    if(dCurrent->mKey == opData->mKey) {
        valData->valueRecord = dCurrent->mValData;
    }
    else {
        valData->valueRecord = nullptr;
    }

    opData->mState->setPointerAndPreserveTag(valData);
    opData->mState->setStatus(Status::COMPLETED);
}

template <class V>
void ConcurrentTree<V>::ExecuteOperation(OperationRecord<V> *opData, int myid)
{
    // initialize the operation state
    opData->mState->setStatus(Status::WAITING);

    // initialize the modify table entry
    MT[myid] = opData;

    // select a modify operation to help later at the end to ensure wait-freedom
    uint32_t pid = this->Select(); // the process selected to help in round-robin manner;

    OperationRecord<V> *pidOpData = MT[pid];

    // inject the operation into the tree
    this->InjectOperation(opData);

    // repeatedly execute transactions until the operation completes
    auto sCurrent = opData->mState;
    auto pCurrent = sCurrent->getPosition()->windowLocation;
    while(sCurrent->getStatus() != Status::COMPLETED)
    {
        DataNode<V> *dCurrent = pCurrent->getDataNode();

        if(dCurrent->mOpData == opData) {
            ExecuteWindowTransaction(pCurrent, dCurrent);
        }

        sCurrent = opData->mState;
    }

    if(opData->mPid != -1) {
        // help inject the selected operation
        InjectOperation(pidOpData);
    }
}

template <class V>
void ConcurrentTree<V>::InjectOperation(OperationRecord<V> *opData)
{
    // repeatedly try until the operation is injected into the tree
    while(opData->mState->getTag() == Status::WAITING)
    {
        DataNode<V> *dRoot = this->pRoot->getDataNode();

        // execute a window transaction, if needed
        if(dRoot->mOpData != nullptr) {
            ExecuteWindowTransaction(this->pRoot, dRoot);
        }

        // read the address of the data node again
        DataNode<V> *dNow = this->pRoot->getDataNode();

        // if they match, then try to inject the operation into the tree,
        // othewise restart
        if(dRoot == dNow)
        {
            DataNode<V> *dCopy = dRoot->clone();
            dCopy->mOpData = opData;

            auto pRootFree = new PointerNode<DataNode<V>, Flag>(dRoot, Flag::FREE);
            auto pCopyOwned = new PointerNode<DataNode<V>, Flag>(dCopy, Flag::OWNED);

            // try to obtain the ownership of the root of the tree
            if(__sync_bool_compare_and_swap(&(this->pRoot), pRootFree, pCopyOwned)) {
                // the operation has been successfully injected
                // update the operation state
                auto pRootWaiting = new PointerNode<DataNode<V>, Flag>(this->pRoot->getDataNode(), Status::WAITING);
                auto pRootInProgress = new PointerNode<DataNode<V>, Flag>(this->pRoot->getDataNode(), Status::IN_PROGRESS);

                if(!__sync_bool_compare_and_swap(&(opData->mState), pRootWaiting->mPackedPointer, pRootInProgress->mPackedPointer)) {
                    free(pRootInProgress);
                }

                free(pRootWaiting);
            }
            else {
                free(pCopyOwned);
            }

            free(pRootFree);
        }
    }
}


template<class V>
void ConcurrentTree<V>::ExecuteWindowTransaction(PointerNode<DataNode<V>, Flag> *pNode, DataNode<V> *dNode)
{
    // execute a window transaction for the operation stored in dNode
    OperationRecord<V> *opData = dNode->mOpData;
    PointerNode<DataNode<V>, Flag> *pCurrent = pNode; // read the contents of pNode again
    if (pCurrent->getDataNode()->mOpData == opData) {
        if (pCurrent->getFlag() == Flag::OWNED) {
            if (pNode->getPointerNode() == this->pRoot->getPointerNode()) {
                // the operation may have just been injected into the tree, but the operation
                // state may not have been updated yet; update the state
                auto pRootWaiting = new StateNode<Position<V>, Status>(this->pRoot->getDataNode(), Status::WAITING);
                auto pRootInProgress = new StateNode<Position<V>, Status>(this->pRoot->getDataNode(), Status::IN_PROGRESS);

                if(!__sync_bool_compare_and_swap(&(opData->state), pRootWaiting->mPackedPointer, pRootInProgress->mPackedPointer)) {
                    free(pRootInProgress);
                }

                free(pRootWaiting);
            }


            if(ExecuteCheapWindowTransaction(pNode, pCurrent->getDataNode()) == false) {

                // traverse the window using Tarjan’s algorithm, making copies as required
                
                DataNode<V> *windowSoFar = pCurrent->getDataNode()->clone();

                PointerNode<DataNode<V>, Flag> *pNextToAdd;
                DataNode<V> *dNextToAdd;

                bool leftAcquired = false;
                bool rightAcquired = false;

                while(true)
                {
                    bool isLeft = false;

                    if(!leftAcquired) {
                        pNextToAdd = pCurrent->getDataNode()->mLeft; // the address of the pointer node of the next tree node to be copied;
                        isLeft = true;
                    }
                    else if(!rightAcquired) {
                        pNextToAdd = pCurrent->getDataNode()->mRight; // the address of the pointer node of the next tree node to be copied;
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
                        windowSoFar->mLeft = new PointerNode<DataNode<V>, Flag>(dNextToAdd->clone(), pNextToAdd->getFlag());
                        leftAcquired = true;
                    }
                    else {
                        windowSoFar->mRight = new PointerNode<DataNode<V>, Flag>(dNextToAdd->clone(), pNextToAdd->getFlag());
                        rightAcquired = true;
                    }
                }

                DataNode<V> *dWindowRoot = windowSoFar;
                // window has been copied; now apply transformations dictated by Tarjan’ algorithm to windowSoFar;
                if(!(windowSoFar->mLeft == nullptr && windowSoFar->mRight == nullptr)) {
                    // rotate

                    // dWindowRoot = the address of the data node now acting as window root in windowSoFar;
                }

                PointerNode<DataNode<V>, Flag> *pMoveTo;
                DataNode<V> *dMoveTo;

                if (windowSoFar->mNext == nullptr/*last/terminal window transaction*/) {
                    opData->mState->setStatus(COMPLETED);

                    // the address of the record containing the value : if an update operation;
                    // null : otherwise;
                    if(opData->mType == Type::UPDATE) {
                        pMoveTo = opData->mType->mPackedPointer;
                    }
                    else {
                        pMoveTo = nullptr;
                    }
                }
                else {
                    opData->mState->setStatus(IN_PROGRESS);
                    pMoveTo = windowSoFar->mNext; // the address of the pointer node of the node in windowSoFar to which the operation will now move;
                    pMoveTo->setFlag(Flag::OWNED);
                    dMoveTo = pMoveTo->getDataNode();
                    dMoveTo->mOpData = opData;
                }

                dWindowRoot->mOpData = opData;
                dWindowRoot->mNext = new NextNode<PointerNode<DataNode<V>, Flag>, Status>(pMoveTo->mPackedPointer); // {status, pMoveTo};

                // replace the tree window with the local copy and release the ownership
                PointerNode<DataNode<V>, Flag> *pCurrentOwned = new PointerNode<DataNode<V>, Flag>(pCurrent->getDataNode(), Flag::OWNED);
                PointerNode<DataNode<V>, Flag> *pWindowRootFree = new PointerNode<DataNode<V>, Flag>(dWindowRoot->mPackedPointer, Flag::FREE);

                if(!__sync_bool_compare_and_swap(&(pNode), pCurrentOwned, pWindowRootFree)) {
                    free(pWindowRootFree);
                }

                free(pCurrentOwned);
            }
        }

        // at this point, no operation should own pNode; may still need to update the
        // operation state with the new position of the operation window
        DataNode<V> *dNow = pNode->getDataNode();

        if (dNow->mOpData == opData) {
            auto pNodeInProgress = new StateNode<Position<V>, Status>(pNode->mPackedPointer, Status::IN_PROGRESS);
            if(!__sync_bool_compare_and_swap(&(opData->mState), pNodeInProgress, dNow->mNext)) {
                free(pNodeInProgress);
            }
        }
    }
}

template<class V>
bool ConcurrentTree<V>::ExecuteCheapWindowTransaction(PointerNode<DataNode<V>, Flag> *pNode, DataNode<V> *dNode)
{
    OperationRecord<V> opData = dNode->mOpData;
    uint32_t pid = opData->mPid;

    // traverse the tree window using Tarjan’s algorithm
    DataNode<V> *dWindow = this->pRoot->getDataNode()->clone();
    while(dWindow->mLeft != nullptr || dWindow->mRight != nullptr)
    {
        if(dWindow->mLeft != nullptr) {
            dWindow = dWindow->mLeft->getDataNode();
        }
        else {
            dWindow = dWindow->mRight->getDataNode();
        }
    }

    bool traverseLeft = false;
    bool traverseRight = false;

    while(true)
    {
        if(dWindow->mLeft != nullptr) {
            traverseLeft = true;
        }
        else if(dWindow->mRight != nullptr) {
            traverseRight = true;
        }
        else {
            return false;
        }

        PointerNode<DataNode<V>, Flag> *pNextToVisit = dNode->mNext; // the address of the pointer node of the next tree node to be visited;
        DataNode<V> *dNextToVisit = pNextToVisit->getDataNode(); // pNextToVisit dNode;

        if (opData->mState->getPointerNode() == pNode->getPointerNode()) {
            return true; // abort; transaction already executed
        }
        // if there is an operation residing at the node, then help it move out of the way
        if (dNextToVisit->mOpData != nullptr) {
            if(traverseLeft) {
                dWindow = dWindow->mLeft->getDataNode();
                traverseLeft = false;
            }
            else {
                dWindow = dWindow->mRight->getDataNode();
                traverseRight = false;
            }

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
    if (dWindow->mNext == this->pRoot->getDataNode()) {

        PointerNode<DataNode<V>, Flag> *pMoveTo = new PointerNode<DataNode<V>, Flag>();
        DataNode<V> *dMoveTo = new DataNode<V>();

        // if not sentinel
        if (opData->mState->mValue != nullptr) {
            if(opData->mState->getStatus() == Type::UPDATE) {
                pMoveTo = this->pRoot->getDataNode()->mNext;
            }
            else {
                pMoveTo = nullptr;
            }
            
            dMoveTo = nullptr;
        }
        else {
            pMoveTo = dWindow->mNext; // the address of the pointer node of the node in the tree to which the operation will now move;
            dMoveTo = pMoveTo->getDataNode();
        }

        if(opData->mState->getPointerNode() == pNode) {
            SlideWindowDown(pNode, dNode, pMoveTo, dMoveTo);
        }

        return true;
    }
    else {
        return false;
    }
}

template<class V>
void ConcurrentTree<V>::SlideWindowDown(PointerNode<DataNode<V>, Flag> *pMoveFrom, DataNode<V> *dMoveFrom, PointerNode<DataNode<V>, Flag> *pMoveTo, DataNode<V> *dMoveTo)
{
    DataNode<V> *opData = dMoveFrom->mOpData;

    // copy the data node of the current window location
    DataNode<V> *dCopyMoveFrom = dMoveFrom->clone();
    dCopyMoveFrom->mOpData = opData;

    if(dMoveTo != nullptr) {
        pMoveTo->setStatus(Status::IN_PROGRESS);
        dCopyMoveFrom->mNext = pMoveTo;
    }
    else {
        pMoveTo->setStatus(Status::COMPLETED);
        dCopyMoveFrom->mNext = pMoveTo;
    }

    // copy the data node of the next window location, if needed
    if(dMoveTo != nullptr) {
        if(dMoveTo->mOpData != opData) {
            DataNode<V> *dCopyMoveTo = dMoveTo->clone();
            dCopyMoveTo->mOpData = opData;

            // acquire the ownership of the next window location
            DataNode<V> *pMoveToFree = new DataNode<V>(dMoveTo, Flag::FREE); // {FREE, dMoveTo}
            DataNode<V> *dCopyMoveToOwned = new DataNode<V>(dCopyMoveTo, Flag::OWNED); // {OWNED, dCopyMoveTo}

            if(!__sync_bool_compare_and_swap(&(pMoveTo), pMoveToFree->mPackedPointer, dCopyMoveToOwned->mPackedPointer)) {
                free(dCopyMoveToOwned);
            }

            free(pMoveToFree);
        }
    }

    // release the ownership of the current window location and update the operation state
    PointerNode<DataNode<V>, Flag> *pMoveFromOwned = new PointerNode<DataNode<V>, Flag>(dMoveFrom, Flag::OWNED); // {OWNED, dMoveFrom}
    PointerNode<DataNode<V>, Flag> *dCopyMoveFromFree = new PointerNode<DataNode<V>, Flag>(dCopyMoveFrom, Flag::FREE); // {FREE, dCopyMoveFrom}

    if(!__sync_bool_compare_and_swap(&(pMoveFrom), pMoveFromOwned, dCopyMoveFromFree)) {
        free(dCopyMoveFromFree);
    }

    free(pMoveFromOwned);

    PointerNode<DataNode<V>, Flag> *pMoveFromInProgress = new PointerNode<DataNode<V>, Flag>(dMoveFrom, Status::IN_PROGRESS); // {IN PROGRESS, pMoveFrom}

    __sync_bool_compare_and_swap(&(opData->mState), pMoveFromInProgress->mPackedPointer, dCopyMoveFrom->mNext);
    
    free(pMoveFromInProgress);
}
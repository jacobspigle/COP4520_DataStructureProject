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

    valData = this->ST[myid]->mState->unpack()->valueRecord;

    if(valData == nullptr) {
        // phase 2: try to add the key-value pair to the tree using the MTL-framework
        // select a search operation to help at the end of phase 2 to ensure wait freedom
        uint32_t pid = Select(); // the process selected to help in round-robin manner
        OperationRecord<V> *pidOpData = this->ST[pid];

        // create and initialize a new operation record
        OperationRecord<V> *opData = new OperationRecord<V>(Type::INSERT, key, value);

        // add the key-value pair to the tree
        ExecuteOperation(opData, myid);
        valData = opData->mState->unpack()->valueRecord;

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
        OperationRecord<V> *pidOpData = ST[pid];

        // create and initialize a new operation record
        OperationRecord<V> *opData = new OperationRecord<V>(Type::DELETE, key, nullptr);
        // = (OperationRecord<V> *) malloc(sizeof(OperationRecord<V>));
        // opData->InitializeOperationRecord(Type::DELETE, key, nullptr);
        // opData->mType = Type::DELETE;
        // opData->mKey = key;
        // opData->mValue = nullptr;

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
    DataNode<V> *dCurrent = this->pRoot->unpack();

    // find a leaf
    while(dCurrent->mLeft != nullptr || dCurrent->mRight != nullptr)
    {
        // abort the traversal if no longer needed
        if(opData->mState->getTag() == Status::COMPLETED) {
            return;
        }

        // find the next node to visit
        if(dCurrent->mLeft && opData->mKey < dCurrent->mKey) {
            dCurrent = dCurrent->mLeft->mPackedPointer;
        }
        else if(dCurrent->mRight) {
            dCurrent = dCurrent->mRight->mPackedPointer;
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
    StateNode<Position<V>, Status> *sCurrent = opData->mState;
    Position<V> *pCurrent = sCurrent->mPackedPointer;
    while(sCurrent->getStatus() != Status::COMPLETED)
    {
        DataNode<V> *dCurrent = pCurrent->windowLocation->mPackedPointer;

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
        DataNode<V> *dRoot = this->pRoot->mPackedPointer;

        // execute a window transaction, if needed
        if(dRoot->mOpData != nullptr) {
            ExecuteWindowTransaction(this->GetPRootAsPosition(), dRoot);
        }

        // read the address of the data node again
        DataNode<V> *dNow = this->pRoot->mPackedPointer;

        // if they match, then try to inject the operation into the tree,
        // othewise restart
        if(dRoot == dNow)
        {
            DataNode<V> *dCopy = dRoot->clone();
            dCopy->mOpData = opData;

            auto pRootFree = new PointerNode<DataNode<V>, Flag>(dRoot, Flag::FREE);
            auto pCopyOwned = new PointerNode<DataNode<V>, Flag>(dCopy, Flag::OWNED);

            // try to obtain the ownership of the root of the tree
            if(__sync_bool_compare_and_swap(&(this->pRoot->mPackedPointer), pRootFree->mPackedPointer, pCopyOwned->mPackedPointer)) {
                // the operation has been successfully injected
                // update the operation state
                auto pRootAsPosition = new Position<V>();
                pRootAsPosition->windowLocation = this->pRoot;

                auto pRootWaiting = new StateNode<Position<V>, Status>(pRootAsPosition, Status::WAITING);
                auto pRootInProgress = new StateNode<Position<V>, Status>(pRootAsPosition, Status::IN_PROGRESS);

                __sync_bool_compare_and_swap(&(opData->mState->mPackedPointer), pRootWaiting->mPackedPointer, pRootInProgress->mPackedPointer);

                free(pRootInProgress);
                free(pRootWaiting);
            }

            free(pCopyOwned);
            free(pRootFree);
        }
    }
}


template<class V>
void ConcurrentTree<V>::ExecuteWindowTransaction(Position<V> *pNode, DataNode<V> *dNode)
{
    // execute a window transaction for the operation stored in dNode
    OperationRecord<V> *opData = dNode->mOpData;
    PointerNode<DataNode<V>, Flag> *pCurrent = pNode->windowLocation; // read the contents of pNode again
    if (pCurrent->unpack()->mOpData == opData) {
        if (pCurrent->getFlag() == Flag::OWNED) {
            if (pNode->windowLocation->unpack() == this->pRoot->unpack()) {
                // the operation may have just been injected into the tree, but the operation
                // state may not have been updated yet; update the state
                auto pRootAsPosition = new Position<V>();
                pRootAsPosition->windowLocation = this->pRoot;

                auto pRootWaiting = new StateNode<Position<V>, Status>(pRootAsPosition, Status::WAITING);
                auto pRootInProgress = new StateNode<Position<V>, Status>(pRootAsPosition, Status::IN_PROGRESS);

                __sync_bool_compare_and_swap(&(opData->mState->mPackedPointer), pRootWaiting->mPackedPointer, pRootInProgress->mPackedPointer);

                free(pRootInProgress);
                free(pRootWaiting);
            }

            if(ExecuteCheapWindowTransaction(pNode, pCurrent->mPackedPointer) == false) {

                // traverse the window using Tarjan’s algorithm, making copies as required
                
                DataNode<V> *windowSoFar = pCurrent->unpack()->clone();

                PointerNode<DataNode<V>, Flag> *pNextToAdd;
                DataNode<V> *dNextToAdd;

                bool leftAcquired = false;
                bool rightAcquired = false;

                while(true)
                {
                    bool isLeft = false;

                    if(!leftAcquired) {
                        pNextToAdd = pCurrent->unpack()->mLeft; // the address of the pointer node of the next tree node to be copied;
                        isLeft = true;
                    }
                    else if(!rightAcquired) {
                        pNextToAdd = pCurrent->unpack()->mRight; // the address of the pointer node of the next tree node to be copied;
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

                    dNextToAdd = pNextToAdd->mPackedPointer;

                    // help the operation located at this node, if any, move out of the way
                    if (dNextToAdd->mOpData != nullptr) {
                        ExecuteWindowTransaction(this->GetPointerNodeAsPosition(pNextToAdd), dNextToAdd);
                    }

                    // read the address of the data node again as it may have changed
                    dNextToAdd = pNextToAdd->mPackedPointer;

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

                Position<V> *pMoveTo;
                DataNode<V> *dMoveTo;

                if (windowSoFar->mNext == nullptr/*last/terminal window transaction*/) {
                    opData->mState->setStatus(COMPLETED);

                    // the address of the record containing the value : if an update operation;
                    // null : otherwise;
                    if(opData->mType == Type::UPDATE) {
                        pMoveTo->valueRecord = opData->mState->unpack()->valueRecord;
                    }
                    else {
                        pMoveTo = nullptr;
                    }
                }
                else {
                    opData->mState->setStatus(IN_PROGRESS);
                    pMoveTo = windowSoFar->mNext->mPackedPointer; // the address of the pointer node of the node in windowSoFar to which the operation will now move;
                    pMoveTo->windowLocation->setFlag(Flag::OWNED);
                    dMoveTo = pMoveTo->windowLocation->mPackedPointer;
                    dMoveTo->mOpData = opData;
                }

                dWindowRoot->mOpData = opData;
                auto pMoveToAsNextNode = new NextNode<Position<V>, Status>(pMoveTo, dWindowRoot->mOpData->mState->getStatus());
                dWindowRoot->mNext->mPackedPointer = pMoveTo; // {status, pMoveTo};

                // replace the tree window with the local copy and release the ownership
                auto pCurrentOwned = new PointerNode<DataNode<V>, Flag>(pCurrent->unpack(), Flag::OWNED);
                auto pWindowRootFree = new PointerNode<DataNode<V>, Flag>(dWindowRoot, Flag::FREE);

                // TODO: Verify
                __sync_bool_compare_and_swap(&(pNode->windowLocation->mPackedPointer), pCurrentOwned->mPackedPointer, pWindowRootFree->mPackedPointer);

                free(pWindowRootFree);
                free(pCurrentOwned);
            }
        }

        // at this point, no operation should own pNode; may still need to update the
        // operation state with the new position of the operation window
        DataNode<V> *dNow = pNode->windowLocation->mPackedPointer;

        if (dNow->mOpData == opData) {            
            auto sNodeInProgress = new StateNode<Position<V>, Status>(pNode, Status::IN_PROGRESS);
            __sync_bool_compare_and_swap(&(opData->mState->mPackedPointer), sNodeInProgress->mPackedPointer, dNow->mNext->mPackedPointer);

            free(sNodeInProgress);
        }
    }
}

template<class V>
bool ConcurrentTree<V>::ExecuteCheapWindowTransaction(Position<V> *pNode, DataNode<V> *dNode)
{
    OperationRecord<V> *opData = dNode->mOpData;
    uint32_t pid = opData->mPid;

    // traverse the tree window using Tarjan’s algorithm
    DataNode<V> *dWindow = this->pRoot->unpack()->clone();
    while(dWindow->mLeft != nullptr || dWindow->mRight != nullptr)
    {
        if(dWindow->mLeft != nullptr) {
            dWindow = dWindow->mLeft->mPackedPointer;
        }
        else {
            dWindow = dWindow->mRight->mPackedPointer;
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

        Position<V> *pNextToVisit = dNode->mNext->mPackedPointer; // the address of the pointer node of the next tree node to be visited;
        DataNode<V> *dNextToVisit = pNextToVisit->windowLocation->mPackedPointer; // pNextToVisit dNode;

        if (opData->mState->unpack()->windowLocation->unpack() == pNode->windowLocation->unpack()) {
            return true; // abort; transaction already executed
        }
        // if there is an operation residing at the node, then help it move out of the way
        if (dNextToVisit->mOpData != nullptr) {
            if(traverseLeft) {
                dWindow = dWindow->mLeft->mPackedPointer;
                traverseLeft = false;
            }
            else {
                dWindow = dWindow->mRight->mPackedPointer;
                traverseRight = false;
            }

            // there are several cases to consider
            if (dNextToVisit->mOpData->mPid != pid) {
                // the operation residing at the node belongs to a different process
                ExecuteWindowTransaction(pNextToVisit, dNextToVisit);

                // read the address of the data node again as it may have changed
                dNextToVisit = pNextToVisit->windowLocation->mPackedPointer;
                if (opData->mState->unpack()->windowLocation != pNode->windowLocation) {
                    return true; // abort; transaction already executed
                }
            }
            else if (dNextToVisit->mOpData == dNode->mOpData) {
                // partial window transaction has already been executed; complete it if needed
                if (opData->mState->unpack()->windowLocation != pNode->windowLocation) {
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
    if (dWindow->mNext->unpack()->windowLocation == this->GetPRootAsPosition()->windowLocation) {

        PointerNode<DataNode<V>, Flag> *pMoveTo = new PointerNode<DataNode<V>, Flag>();
        DataNode<V> *dMoveTo = new DataNode<V>();

        // if not sentinel
        if (opData->mValue != nullptr) {
            if(opData->mType == Type::UPDATE) {
                pMoveTo = this->pRoot;
            }
            else {
                pMoveTo = nullptr;
            }
            
            dMoveTo = nullptr;
        }
        else {
            pMoveTo = dWindow->mNext->unpack()->windowLocation; // the address of the pointer node of the node in the tree to which the operation will now move;
            dMoveTo = pMoveTo->mPackedPointer;
        }

        if(opData->mState->unpack()->windowLocation == pNode->windowLocation) {
            SlideWindowDown(pNode, dNode, this->GetPointerNodeAsPosition(pMoveTo), dMoveTo);
        }

        return true;
    }
    else {
        return false;
    }
}

template<class V>
void ConcurrentTree<V>::SlideWindowDown(Position<V> *pMoveFrom, DataNode<V> *dMoveFrom, Position<V> *pMoveTo, DataNode<V> *dMoveTo)
{
    OperationRecord<V> *opData = dMoveFrom->mOpData;

    // copy the data node of the current window location
    DataNode<V> *dCopyMoveFrom = dMoveFrom->clone();
    dCopyMoveFrom->mOpData = opData;

    if(dMoveTo != nullptr) {
        dCopyMoveFrom->mNext = new NextNode<Position<V>, Status>(pMoveTo, Status::IN_PROGRESS);
    }
    else {
        dCopyMoveFrom->mNext = new NextNode<Position<V>, Status>(pMoveTo, Status::COMPLETED);
    }

    // copy the data node of the next window location, if needed
    if(dMoveTo != nullptr) {
        if(dMoveTo->mOpData != opData) {
            DataNode<V> *dCopyMoveTo = dMoveTo->clone();
            dCopyMoveTo->mOpData = opData;

            // acquire the ownership of the next window location
            auto pMoveToFree = new PointerNode<DataNode<V>, Flag>(dMoveTo, Flag::FREE); // {FREE, dMoveTo}
            auto dCopyMoveToOwned = new PointerNode<DataNode<V>, Flag>(dCopyMoveTo, Flag::OWNED); // {OWNED, dCopyMoveTo}

            __sync_bool_compare_and_swap(&(pMoveTo->windowLocation->mPackedPointer), pMoveToFree->mPackedPointer, dCopyMoveToOwned->mPackedPointer);

            free(dCopyMoveToOwned);
            free(pMoveToFree);
        }
    }

    // release the ownership of the current window location and update the operation state
    auto pMoveFromOwned = new PointerNode<DataNode<V>, Flag>(dMoveFrom, Flag::OWNED); // {OWNED, dMoveFrom}

    __sync_bool_compare_and_swap(&(pMoveFrom->windowLocation->mPackedPointer), pMoveFromOwned->mPackedPointer, dCopyMoveFrom->mNext->unpack()->windowLocation->mPackedPointer);

    free(pMoveFromOwned);

    auto pMoveFromInProgress = new StateNode<Position<V>, Status>(pMoveFrom, Status::IN_PROGRESS); // {IN PROGRESS, pMoveFrom}

    __sync_bool_compare_and_swap(&(opData->mState->mPackedPointer), pMoveFromInProgress->mPackedPointer, dCopyMoveFrom->mNext->mPackedPointer);
    
    free(pMoveFromInProgress);
}

template <class V>
Position<V> *ConcurrentTree<V>::GetPRootAsPosition()
{
    Position<V> *pRootPosition = new Position<V>();
    pRootPosition->windowLocation = pRoot;
    return pRootPosition;
}

template <class V>
Position<V> *ConcurrentTree<V>::GetPointerNodeAsPosition(PointerNode<DataNode<V>, Flag> *pointerNode)
{
    Position<V> *pNodePosition = new Position<V>();
    pNodePosition->windowLocation = pointerNode;
    return pNodePosition;
}
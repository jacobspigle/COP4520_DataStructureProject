template <class V>
V *ConcurrentTree<V>::Search(uint32_t key, int myid)
{
    // create and initialize a new operation record
    //OperationRecord<V> *opData = new OperationRecord<V>(Type::SEARCH, key, nullptr);
    OperationRecord<V> *opData = (OperationRecord<V> *)TM_ALLOC(sizeof(OperationRecord<V>));
    opData->InitializeOperationRecord(Type::SEARCH, key, nullptr);

    // initialize the operation state
    opData->mState->setTag(Status::IN_PROGRESS);

    // initialize the search table entry
    TM_WRITE(ST[myid], opData);

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

    TM_WRITE(valData, this->ST[myid]->mState->unpack()->valueRecord);

    if(valData == nullptr) {
        // phase 2: try to add the key-value pair to the tree using the MTL-framework
        // select a search operation to help at the end of phase 2 to ensure wait freedom
        uint32_t pid = TM_READ(Select()); // the process selected to help in round-robin manner
        OperationRecord<V> *pidOpData = TM_READ(this->ST[pid]);

        // create and initialize a new operation record
        //OperationRecord<V> *opData = new OperationRecord<V>(Type::INSERT, key, value);
        OperationRecord<V> *opData = (OperationRecord<V> *)TM_ALLOC(sizeof(OperationRecord<V>));
        opData->InitializeOperationRecord(Type::INSERT, key, value);

        // add the key-value pair to the tree
        ExecuteOperation(opData, myid);
        TM_WRITE(valData, opData->mState->unpack()->valueRecord);

        // help the selected search operation complete
        if(pidOpData != nullptr) {
            Traverse(pidOpData);
        }
    }

    if(valData != nullptr) {
        int numChecked = 0;
        int pid;
        while (numChecked < TM_READ(this->mNumThreads))
        {
            pid = TM_READ(Select());

            if (MT[pid]!=nullptr)
            {
                TM_WRITE(MT[pid]->mValData, valData);
                break;
            }

            num_checked++;
        }
    }
}

template <class V>
void ConcurrentTree<V>::Delete(uint32_t key, int myid)
{
    // phase 1: determine if the key already exists in the tree
    if(TM_READ(Search(key, myid))) {
        // phase 2: try to delete the key from the tree using the MTL-framework
        // select a search operation to help at the end of phase 2 to ensure wait-freedom
        uint32_t pid = TM_READ(Select()); // the process selected to help in a round-robin manner
        OperationRecord<V> *pidOpData = TM_READ(ST[pid]);

        // create and initialize a new operation record
        //OperationRecord<V> *opData = new OperationRecord<V>(Type::DELETE, key, nullptr);
        OperationRecord<V> *opData = (OperationRecord<V> *) TM_ALLOC(sizeof(OperationRecord<V>));
        opData->InitializeOperationRecord(Type::DELETE, key, nullptr);

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
    uint32_t fetched_pid = TM_READ(this->mIndex);
    TM_WRITE(this->mIndex, (this->mIndex + 1) % this->mNumThreads);
    return fetched_pid;
}

template <class V>
void ConcurrentTree<V>::Traverse(OperationRecord<V> *opData)
{
    // start from the root of the tree
    DataNode<V> *dCurrent = TM_READ(this->pRoot->unpack());

    // find a leaf
    while(dCurrent->mLeft != nullptr || dCurrent->mRight != nullptr)
    {
        // abort the traversal if no longer needed
        if(opData->mState->getTag() == Status::COMPLETED) {
            return;
        }

        // find the next node to visit
        if(TM_READ(dCurrent->mLeft) && TM_READ(opData->mKey) < TM_READ(dCurrent->mKey)) {
            TM_WRITE(dCurrent, dCurrent->mLeft->mPackedPointer);
        }
        else if(dCurrent->mRight) {
            TM_WRITE(dCurrent, dCurrent->mRight->mPackedPointer);
        }
    }

    // leafy stuff
    Position<V> *valData;

    if(TM_READ(dCurrent->mKey) == TM_READ(opData->mKey)) {
        TM_WRITE(valData->valueRecord, dCurrent->mValData);
    }
    else {
        TM_WRITE(valData->valueRecord = nullptr);
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
    TM_WRITE(MT[myid], opData);

    // select a modify operation to help later at the end to ensure wait-freedom
    uint32_t pid = TM_READ(this->Select()); // the process selected to help in round-robin manner;

    OperationRecord<V> *pidOpData = TM_READ(MT[pid]);

    // inject the operation into the tree
    this->InjectOperation(opData);

    // repeatedly execute transactions until the operation completes
    StateNode<Position<V>, Status> *sCurrent = TM_READ(opData->mState);
    Position<V> *pCurrent = TM_READ(sCurrent->mPackedPointer);
    while(sCurrent->getStatus() != Status::COMPLETED)
    {
        DataNode<V> *dCurrent = TM_READ(pCurrent->windowLocation->mPackedPointer);

        if(TM_READ(dCurrent->mOpData) == TM_READ(opData)) {
            ExecuteWindowTransaction(pCurrent, dCurrent);
        }

        TM_WRITE(sCurrent, opData->mState);
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
        DataNode<V> *dRoot = TM_READ(this->pRoot->mPackedPointer);

        // execute a window transaction, if needed
        if(dRoot->mOpData != nullptr) {
            ExecuteWindowTransaction(this->GetPRootAsPosition(), dRoot);
        }

        // read the address of the data node again
        DataNode<V> *dNow = TM_READ(this->pRoot->mPackedPointer);

        // if they match, then try to inject the operation into the tree,
        // othewise restart
        if(TM_READ(dRoot) == TM_READ(dNow))
        {
            DataNode<V> *dCopy = TM_READ(dRoot->clone());
            TM_WRITE(dCopy->mOpData, opData);

            //auto pRootFree = new PointerNode<DataNode<V>, Flag>(dRoot, Flag::FREE);
            PointerNode<DataNode<V>, Flag> *pRootFree = (PointerNode<DataNode<V>, Flag> *)TM_ALLOC(sizeof(PointerNode<DataNode<V>, Flag>));
            pRootFree->InitializePointerNode(dRoot, Flag::FREE);
            //auto pCopyOwned = new PointerNode<DataNode<V>, Flag>(dCopy, Flag::OWNED);
            PointerNode<DataNode<V>, Flag> *pCopyOwned = (PointerNode<DataNode<V>, Flag> *)TM_ALLOC(sizeof(PointerNode<DataNode<V>, Flag>));
            pCopyOwned->InitializePointerNode(dCopy, Flag::OWNED);

            // try to obtain the ownership of the root of the tree
            if(TM_WRITE(&(this->pRoot->mPackedPointer), pCopyOwned->mPackedPointer)) {
                // the operation has been successfully injected
                // update the operation state
                //auto pRootAsPosition = new Position<V>();
                Position<V> *pRootAsPosition = (Position<V> *)TM_ALLOC(sizeof(Position<V>));
                pRootAsPosition->windowLocation = TM_READ(this->pRoot);

                //auto pRootWaiting = new StateNode<Position<V>, Status>(pRootAsPosition, Status::WAITING);
                StateNode<Position<V>, Status> *pRootWaiting = (StateNode<Position<V>, Status> *)TM_ALLOC(sizeof(StateNode<Position<V>, Status>));
                pRootWaiting->InitializeStateNode(pRootAsPosition, Status::WAITING);
                //auto pRootInProgress = new StateNode<Position<V>, Status>(pRootAsPosition, Status::IN_PROGRESS);
                StateNode<Position<V>, Status> *pRootInProgress = (StateNode<Position<V>, Status> *)TM_ALLOC(sizeof(StateNode<Position<V>, Status>));
                pRootInProgress->InitializeStateNode(pRootAsPosition, Status::IN_PROGRESS);

                TM_WRITE(&(opData->mState->mPackedPointer), pRootInProgress->mPackedPointer);

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
    OperationRecord<V> *opData = TM_READ(dNode->mOpData);
    PointerNode<DataNode<V>, Flag> *pCurrent = TM_READ(pNode->windowLocation); // read the contents of pNode again
    if (pCurrent->unpack()->mOpData == TM_READ(opData)) {
        if (pCurrent->getFlag() == Flag::OWNED) {
            if (pNode->windowLocation->unpack() == TM_READ(this->pRoot->unpack())) {
                // the operation may have just been injected into the tree, but the operation
                // state may not have been updated yet; update the state
                //auto pRootAsPosition = new Position<V>();
                Position<V> *pRootAsPosition = (Position<V> *)TM_ALLOC(sizeof(Position<V>));
                pRootAsPosition->windowLocation = TM_READ(this->pRoot);

                //auto pRootWaiting = new StateNode<Position<V>, Status>(pRootAsPosition, Status::WAITING);
                StateNode<Position<V>, Status> *pRootWaiting = (StateNode<Position<V>, Status> *)TM_ALLOC(sizeof(StateNode<Position<V>, Status>));
                pRootWaiting->InitializeStateNode(pRootAsPosition, Status::WAITING);
                //auto pRootInProgress = new StateNode<Position<V>, Status>(pRootAsPosition, Status::IN_PROGRESS);
                StateNode<Position<V>, Status> *pRootInProgress = (StateNode<Position<V>, Status> *)TM_ALLOC(sizeof(StateNode<Position<V>, Status>));
                pRootInProgress->InitializeStateNode(pRootAsPosition, Status::IN_PROGRESS);

                TM_WRITE(&(opData->mState->mPackedPointer), pRootInProgress->mPackedPointer);

                free(pRootInProgress);
                free(pRootWaiting);
            }

            if(ExecuteCheapWindowTransaction(pNode, pCurrent->mPackedPointer) == false) {

                // traverse the window using Tarjan’s algorithm, making copies as required
                
                DataNode<V> *windowSoFar = TM_READ(pCurrent->unpack()->clone());

                PointerNode<DataNode<V>, Flag> *pNextToAdd;
                DataNode<V> *dNextToAdd;

                bool leftAcquired = false;
                bool rightAcquired = false;

                while(true)
                {
                    bool isLeft = false;

                    if(!leftAcquired) {
                        TM_WRITE(pNextToAdd, pCurrent->unpack()->mLeft); // the address of the pointer node of the next tree node to be copied;
                        isLeft = true;
                    }
                    else if(!rightAcquired) {
                        TM_WRITE(pNextToAdd = pCurrent->unpack()->mRight); // the address of the pointer node of the next tree node to be copied;
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

                    TM_WRITE(dNextToAdd, pNextToAdd->mPackedPointer);

                    // help the operation located at this node, if any, move out of the way
                    if (dNextToAdd->mOpData != nullptr) {
                        ExecuteWindowTransaction(this->GetPointerNodeAsPosition(pNextToAdd), dNextToAdd);
                    }

                    // read the address of the data node again as it may have changed
                    TM_WRITE(dNextToAdd, pNextToAdd->mPackedPointer);

                    // copy pNextToAdd and dNextToAdd, and add them to windowSoFar;
                    if(isLeft) {
                        //windowSoFar->mLeft = new PointerNode<DataNode<V>, Flag>(dNextToAdd->clone(), pNextToAdd->getFlag());
                        windowSoFar->mLeft = (PointerNode<DataNode<V>, Flag> *)TM_ALLOC(sizeof(PointerNode<DataNode<V>, Flag>));
                        windowSoFar->mLeft->InitializePointerNode(dNextToAdd->clone(), pNextToAdd->getFlag());
                        leftAcquired = true;
                    }
                    else {
                        //windowSoFar->mRight = new PointerNode<DataNode<V>, Flag>(dNextToAdd->clone(), pNextToAdd->getFlag());
                        windowSoFar->mRight = (PointerNode<DataNode<V>, Flag> *)TM_ALLOC(sizeof(PointerNode<DataNode<V>, Flag>));
                        windowSoFar->mRight->InitializePointerNode(dNextToAdd->clone(), pNextToAdd->getFlag());
                        rightAcquired = true;
                    }
                }

                DataNode<V> *dWindowRoot = TM_READ(windowSoFar);
                // window has been copied; now apply transformations dictated by Tarjan’ algorithm to windowSoFar;
                if(windowSoFar->mLeft != nullptr && windowSoFar->mRight != nullptr) {
                    DataNode<V> *tempRight = TM_READ(windowSoFar->mRight);
                    TM_WRITE(windowSoFar, windowSoFar->mLeft);
                    TM_WRITE(windowSoFar->mRight, windowSoFar);
                    TM_WRITE(windowSoFar->mRight->getDataNode()->mRight, tempRight);
                }

                Position<V> *pMoveTo;
                DataNode<V> *dMoveTo;

                if (windowSoFar->mNext == nullptr/*last/terminal window transaction*/) {
                    opData->mState->setStatus(COMPLETED);

                    // the address of the record containing the value : if an update operation;
                    // null : otherwise;
                    if(opData->mType == Type::UPDATE) {
                        TM_WRITE(pMoveTo->valueRecord, opData->mState->unpack()->valueRecord);
                    }
                    else {
                        pMoveTo = nullptr;
                    }
                }
                else {
                    opData->mState->setStatus(IN_PROGRESS);
                    TM_WRITE(pMoveTo, windowSoFar->mNext->mPackedPointer); // the address of the pointer node of the node in windowSoFar to which the operation will now move;
                    pMoveTo->windowLocation->setFlag(Flag::OWNED);
                    TM_WRITE(dMoveTo, pMoveTo->windowLocation->mPackedPointer);
                    TM_WRITE(dMoveTo->mOpData opData);
                }

                TM_WRITE(dWindowRoot->mOpData, opData);
                //auto pMoveToAsNextNode = new NextNode<Position<V>, Status>(pMoveTo, dWindowRoot->mOpData->mState->getStatus());
                NextNode<Position<V>, Status> *pMoveToAsNextNode = (NextNode<Position<V>, Status> *)TM_ALLOC(sizeof(NextNode<Position<V>, Status>));
                pMoveToAsNextNode->InitializeNextNode(pMoveTo, dWindowRoot->mOpData->mState->getStatus());
                TM_WRITE(dWindowRoot->mNext->mPackedPointer, pMoveTo); // {status, pMoveTo};

                // replace the tree window with the local copy and release the ownership
                //auto pCurrentOwned = new PointerNode<DataNode<V>, Flag>(pCurrent->mPackedPointer, Flag::OWNED);
                PointerNode<DataNode<V>, Flag> *pCurrentOwned = (PointerNode<DataNode<V>, Flag> *)TM_ALLOC(sizeof(PointerNode<DataNode<V>, Flag>));
                pCurrentOwned->InitializePointerNode(pCurrent->unpack(), Flag::OWNED);
                //auto pWindowRootFree = new PointerNode<DataNode<V>, Flag>(dWindowRoot, Flag::FREE);
                PointerNode<DataNode<V>, Flag> *pWindowRootFree = (PointerNode<DataNode<V>, Flag> *)TM_ALLOC(sizeof(PointerNode<DataNode<V>, Flag>));
                pWindowRootFree->InitializePointerNode(dWindowRoot, Flag::FREE);

                // TODO: Verify
                TM_WRITE(&(pNode->windowLocation->mPackedPointer), pWindowRootFree->mPackedPointer);

                free(pWindowRootFree);
                free(pCurrentOwned);
            }
        }

        // at this point, no operation should own pNode; may still need to update the
        // operation state with the new position of the operation window
        DataNode<V> *dNow = TM_READ(pNode->windowLocation->mPackedPointer);

        if (dNow->mOpData == TM_READ(opData)) {            
            //auto sNodeInProgress = new StateNode<Position<V>, Status>(pNode, Status::IN_PROGRESS);
            StateNode<Position<V>, Status> *sNodeInProgress = (StateNode<Position<V>, Status> *)TM_ALLOC(sizeof(StateNode<Position<V>, Status>));
            sNodeInProgress->InitializeStateNode(pNode, Status::IN_PROGRESS);
            
            TM_WRITE(&(opData->mState->mPackedPointer), dNow->mNext->mPackedPointer);

            free(sNodeInProgress);
        }
    }
}

template<class V>
bool ConcurrentTree<V>::ExecuteCheapWindowTransaction(Position<V> *pNode, DataNode<V> *dNode)
{
    OperationRecord<V> *opData = TM_READ(dNode->mOpData);
    uint32_t pid = TM_READ(opData->mPid);

    // traverse the tree window using Tarjan’s algorithm
    DataNode<V> *dWindow = TM_READ(this->pRoot->unpack()->clone());
    while(dWindow->mLeft != nullptr || dWindow->mRight != nullptr)
    {
        if(dWindow->mLeft != nullptr) {
            TM_WRITE(dWindow, dWindow->mLeft->mPackedPointer);
        }
        else {
            TM_WRITE(dWindow, dWindow->mRight->mPackedPointer);
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

        Position<V> *pNextToVisit = TM_READ(dNode->mNext->mPackedPointer); // the address of the pointer node of the next tree node to be visited;
        DataNode<V> *dNextToVisit = TM_READ(pNextToVisit->windowLocation->mPackedPointer); // pNextToVisit dNode;

        if (TM_READ(opData->mState->unpack()->windowLocation->unpack()) == TM_READ(pNode->windowLocation->unpack())) {
            return true; // abort; transaction already executed
        }
        // if there is an operation residing at the node, then help it move out of the way
        if (dNextToVisit->mOpData != nullptr) {
            if(traverseLeft) {
                TM_WRITE(dWindow, dWindow->mLeft->mPackedPointer);
                traverseLeft = false;
            }
            else {
                TM_WRITE(dWindow, dWindow->mRight->mPackedPointer);
                traverseRight = false;
            }

            // there are several cases to consider
            if (TM_READ(dNextToVisit->mOpData->mPid) != TM_READ(pid)) {
                // the operation residing at the node belongs to a different process
                ExecuteWindowTransaction(pNextToVisit, dNextToVisit);

                // read the address of the data node again as it may have changed
                TM_WRITE(dNextToVisit, pNextToVisit->windowLocation->mPackedPointer);
                if (TM_READ(opData->mState->unpack()->windowLocation) != TM_READ(pNode->windowLocation)) {
                    return true; // abort; transaction already executed
                }
            }
            else if (TM_READ(dNextToVisit->mOpData) == TM_READ(dNode->mOpData)) {
                // partial window transaction has already been executed; complete it if needed
                if (TM_READ(opData->mState->unpack()->windowLocation) != TM_READ(pNode->windowLocation)) {
                    SlideWindowDown(pNode, dNode, pNextToVisit, dNextToVisit);
                }
                return true;
            }
            else if (TM_READ(MT[pid]) != TM_READ(opData)) {
                return true; // abort; transaction already executed
            }
        }

        // visit dNextToVisit;
    }
    if (TM_READ(dWindow->mNext->unpack()->windowLocation) == TM_READ(this->GetPRootAsPosition()->windowLocation)) {

        //PointerNode<DataNode<V>, Flag> *pMoveTo = new PointerNode<DataNode<V>, Flag>();
        //DataNode<V> *dMoveTo = new DataNode<V>();
        PointerNode<DataNode<V>, Flag> *pMoveTo = (PointerNode<DataNode<V>, Flag> *)TM_ALLOC(sizeof(PointerNode<DataNode<V>, Flag>));
        pMoveTo->InitializePointerNode();

        DataNode<V> *dMoveTo = (DataNode<V> *)TM_ALLOC(sizeof(DataNode<V>));
        dMoveTo->InitializeDataNode();

        // if not sentinel
        if (opData->mValue != nullptr) {
            if(opData->mType == Type::UPDATE) {
                TM_WRITE(pMoveTo, this->pRoot);
            }
            else {
                TM_WRITE(pMoveTo, nullptr);
            }
            
            TM_WRITE(dMoveTo, nullptr);
        }
        else {
            TM_WRITE(pMoveTo, dWindow->mNext->unpack()->windowLocation); // the address of the pointer node of the node in the tree to which the operation will now move;
            TM_WRITE(dMoveTo, pMoveTo->mPackedPointer);
        }

        if(TM_READ(opData->mState->unpack()->windowLocation) == TM_READ(pNode->windowLocation)) {
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
    OperationRecord<V> *opData = TM_READ(dMoveFrom->mOpData);

    // copy the data node of the current window location
    DataNode<V> *dCopyMoveFrom = TM_READ(dMoveFrom->clone());
    TM_WRITE(dCopyMoveFrom->mOpData, opData);

    if(dMoveTo != nullptr) {
        //dCopyMoveFrom->mNext = new NextNode<Position<V>, Status>(pMoveTo, Status::IN_PROGRESS);
        dCopyMoveFrom->mNext = (NextNode<Position<V>, Status> *)TM_ALLOC(sizeof(NextNode<Position<V>, Status>));
        dCopyMoveFrom->mNext->InitializeNextNode(pMoveTo, Status::IN_PROGRESS);
    }
    else {
        //dCopyMoveFrom->mNext = new NextNode<Position<V>, Status>(pMoveTo, Status::COMPLETED);
        dCopyMoveFrom->mNext = (NextNode<Position<V>, Status> *)TM_ALLOC(sizeof(NextNode<Position<V>, Status>));
        dCopyMoveFrom->mNext->InitializeNextNode(pMoveTo, Status::COMPLETED);
    }

    // copy the data node of the next window location, if needed
    if(dMoveTo != nullptr) {
        if(TM_READ(dMoveTo->mOpData) != TM_READ(opData)) {
            DataNode<V> *dCopyMoveTo = TM_READ(dMoveTo->clone());
            TM_WRITE(dCopyMoveTo->mOpData = opData);

            // acquire the ownership of the next window location
            //auto pMoveToFree = new PointerNode<DataNode<V>, Flag>(dMoveTo, Flag::FREE); // {FREE, dMoveTo}
            PointerNode<DataNode<V>, Flag> *pMoveToFree = (PointerNode<DataNode<V>, Flag> *)TM_ALLOC(sizeof(PointerNode<DataNode<V>, Flag>));
            pMoveToFree->InitializePointerNode(dMoveTo, Flag::FREE);
            //auto dCopyMoveToOwned = new PointerNode<DataNode<V>, Flag>(dCopyMoveTo, Flag::OWNED); // {OWNED, dCopyMoveTo}
            PointerNode<DataNode<V>, Flag> *pCopyMoveToOwned = (PointerNode<DataNode<V>, Flag> *)TM_ALLOC(sizeof(PointerNode<DataNode<V>, Flag>));
            pCopyMoveToOwned->InitializePointerNode(dCopyMoveTo, Flag::OWNED);

            TM_WRITE(&(pMoveTo->windowLocation->mPackedPointer), pCopyMoveToOwned->mPackedPointer);

            free(pCopyMoveToOwned);
            free(pMoveToFree);
        }
    }

    // release the ownership of the current window location and update the operation state
    //auto pMoveFromOwned = new PointerNode<DataNode<V>, Flag>(dMoveFrom, Flag::OWNED); // {OWNED, dMoveFrom}
    PointerNode<DataNode<V>, Flag> *pMoveFromOwned = (PointerNode<DataNode<V>, Flag> *)TM_ALLOC(sizeof(PointerNode<DataNode<V>, Flag>));
    pMoveFromOwned->InitializePointerNode(dMoveFrom, Flag::OWNED);

    TM_WRITE(&(pMoveFrom->windowLocation->mPackedPointer), dCopyMoveFrom->mNext->unpack()->windowLocation->mPackedPointer);

    free(pMoveFromOwned);

    //auto pMoveFromInProgress = new StateNode<Position<V>, Status>(pMoveFrom, Status::IN_PROGRESS); // {IN PROGRESS, pMoveFrom}
    StateNode<Position<V>, Status> *pMoveFromInProgress = (StateNode<Position<V>, Status> *)TM_ALLOC(sizeof(StateNode<Position<V>, Status>));
    pMoveFromInProgress->InitializeStateNode(pMoveFrom, Status::IN_PROGRESS);

    TM_WRITE(&(opData->mState->mPackedPointer), dCopyMoveFrom->mNext->mPackedPointer);
    
    free(pMoveFromInProgress);
}

template <class V>
Position<V> *ConcurrentTree<V>::GetPRootAsPosition()
{
    //Position<V> *pRootPosition = new Position<V>();
    Position<V> *pRootPosition = (Position<V> *)TM_ALLOC(sizeof(Position<V>));

    TM_WRITE(pRootPosition->windowLocation, pRoot);
    return pRootPosition;
}

template <class V>
Position<V> *ConcurrentTree<V>::GetPointerNodeAsPosition(PointerNode<DataNode<V>, Flag> *pointerNode)
{
    //Position<V> *pNodePosition = new Position<V>();
    Position<V> *pNodePosition = (Position<V> *)TM_ALLOC(sizeof(Position<V>));

    TM_WRITE(pNodePosition->windowLocation, pointerNode);
    return pNodePosition;
}
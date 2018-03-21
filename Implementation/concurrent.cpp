#include "concurrent.hpp"

Value search( key )
opData := create( SEARCH, key, ⊥ ) ; // create and initialize a new operation record
opData→state := {IN PROGRESS, null} ; // initialize the operation state
ST[myid].opData := opData ; // initialize the search table entry
traverse( opData ) ; // traverse the tree
if (opData→state) position 6= null then
read the value stored in the record using Chuong et al.’s algorithm and return it;
else
return ⊥;

insertOrUpdate( key, value )
valData = null; // phase 1: determine if the key already exists in the tree
search( key );
valData := (ST[myid].opData->state) position;
if valData = null then
// phase 2: try to add the key-value pair to the tree using the MTL-framework
// select a search operation to help at the end of phase 2 to ensure wait-freedom
pid := the process selected to help in round-robin manner;
pidOpData := ST[pid].opData;
opData := create( INSERT, key, value ) ; // create and initialize a new operation record
executeOperation( opData ) ; // add the key-value pair to the tree
valData := (opData->state) position;
if pidOpData 6= null then
traverse( pidOpData ) ; // help the selected search operation complete
if valData 6= null then
// phase 3: update the value in the record using Chuong et al. ’s algorithm

delete( key )
begin
// phase 1: determine if the key already exists in the tree
if search( key ) then
// phase 2: try to delete the key from the tree using the MTL-framework
// select a search operation to help at the end of phase 2 to ensure wait-freedom
pid := the process selected to help in round-robin manner;
pidOpData := ST[pid].opData;
opData := create( DELETE, key, ⊥ ) ; // create and initialize a new operation record
executeOperation( opData ) ; // remove the key from the tree
if pidOpData 6= null then
traverse( pidOpData ) ; // help the selected search operation complete
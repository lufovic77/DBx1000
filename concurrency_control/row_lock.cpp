#include "row.h"
#include "txn.h"
#include "row_lock.h"
#include "mem_alloc.h"
#include "manager.h"

#if CC_ALG == NO_WAIT || CC_ALG == WAIT_DIE || CC_ALG == DL_DETECT

#define CONFLICT(a, b) (a != LOCK_NONE && b != LOCK_NONE) && (a==LOCK_EX || b==LOCK_EX)



void Row_lock::init(row_t * row) {
	_row = row;
#if ATOMIC_WORD
	lock = 0;
#else
	#if !USE_LOCKTABLE
		latch = new pthread_mutex_t;
		pthread_mutex_init(latch, NULL);
		blatch = false;
	#endif
		
		lock_type = LOCK_NONE_T;
		ownerCounter = 0;
#endif

}

RC Row_lock::lock_get(lock_t type, txn_man * txn) {
	uint64_t *txnids = NULL;
	int txncnt = 0;
	return lock_get(type, txn, txnids, txncnt);
}

RC Row_lock::lock_get(lock_t type, txn_man * txn, uint64_t* &txnids, int &txncnt) {
		uint64_t starttime = get_sys_clock();
	assert (CC_ALG == DL_DETECT || CC_ALG == NO_WAIT || CC_ALG == WAIT_DIE);
	RC rc;
#if ATOMIC_WORD
	//assert(type<3);
	#if !USE_LOCKTABLE
	uint64_t lock_local=lock;
	if(CONFLICT(LOCK_TYPE(lock_local), type))
	{
		rc = Abort;
	}
	else
	{
		for(;;)
		{
			lock_local = lock;
			if(CONFLICT(LOCK_TYPE(lock_local), type))
			{
				rc = Abort;
				break;
			}
			if(ATOM_CAS(lock, lock_local, ADD_TYPE(COUNTER(lock_local)+1, type)))
			{
				rc = RCOK;
				break;
			}
			PAUSE
		}
		/*
		if(ATOM_CAS(lock, lock_local, ADD_TYPE(COUNTER(lock_local)+1, type)))
		{
			rc = RCOK;
		}
		else
		{
			rc = Abort;
		}
		*/
	}
	
	#else
	// we are inside an atomic section
	bool conflict = CONFLICT(LOCK_TYPE(lock), type);
	if(conflict)
	{
		rc = Abort;
	}
	else
	{
		lock = ADD_TYPE(COUNTER(lock)+1, type);
		rc = RCOK;
	}
	#endif
#else
#if !USE_LOCKTABLE  // otherwise we don't need a latch here.
	if (g_central_man)
	{
		glob_manager->lock_row(_row);
	}
	else {
		while(!ATOM_CAS(blatch, false, true)) PAUSE;
		//pthread_mutex_lock( latch );
	}
#endif
	bool conflict = CONFLICT(lock_type, type);// conflict_lock(lock_type, type);
	if (conflict) { 
		// Cannot be added to the owner list.
		if (CC_ALG == NO_WAIT) {
			rc = Abort;
		}
	} else {
		INC_INT_STATS(time_debug6, get_sys_clock() - starttime);
		ownerCounter ++;
		lock_type = type;
        rc = RCOK;
	}
#if !USE_LOCKTABLE  // otherwise we don't need a latch here.
	if (g_central_man)
		glob_manager->release_row(_row);
	else
		blatch = false;
		//pthread_mutex_unlock( latch );
#endif
#endif
	INC_INT_STATS(time_debug7, get_sys_clock() - starttime);
	return rc;
}


RC Row_lock::lock_release(txn_man * txn) {	

	if (g_central_man)
		glob_manager->lock_row(_row);
	else 
		pthread_mutex_lock( latch );

	// Try to find the entry in the owners
	LockEntry * en = owners;
	LockEntry * prev = NULL;

	while (en != NULL && en->txn != txn) {
		prev = en;
		en = en->next;
	}
	if (en) { // find the entry in the owner list
		if (prev) prev->next = en->next;
		else owners = en->next;
		return_entry(en);
		owner_cnt --;
		if (owner_cnt == 0)
			lock_type = LOCK_NONE;
	} else {
		// Not in owners list, try waiters list.
		en = waiters_head;
		while (en != NULL && en->txn != txn)
			en = en->next;
		ASSERT(en);
		LIST_REMOVE(en);
		if (en == waiters_head)
			waiters_head = en->next;
		if (en == waiters_tail)
			waiters_tail = en->prev;
		return_entry(en);
		waiter_cnt --;
	}

	if (owner_cnt == 0)
		ASSERT(lock_type == LOCK_NONE);
#if DEBUG_ASSERT && CC_ALG == WAIT_DIE 
		for (en = waiters_head; en != NULL && en->next != NULL; en = en->next)
			assert(en->next->txn->get_ts() < en->txn->get_ts());
#endif

	LockEntry * entry;
	// If any waiter can join the owners, just do it!
	while (waiters_head && !conflict_lock(lock_type, waiters_head->type)) {
		LIST_GET_HEAD(waiters_head, waiters_tail, entry);
		STACK_PUSH(owners, entry);
		owner_cnt ++;
		waiter_cnt --;
		ASSERT(entry->txn->lock_ready == false);
		entry->txn->lock_ready = true;
		lock_type = entry->type;
	} 
	ASSERT((owners == NULL) == (owner_cnt == 0));

	if (g_central_man)
		glob_manager->release_row(_row);
	else
		pthread_mutex_unlock( latch );

	return RCOK;
}

bool Row_lock::conflict_lock(lock_t l1, lock_t l2) {
	if (l1 == LOCK_NONE || l2 == LOCK_NONE)
		return false;
    else if (l1 == LOCK_EX || l2 == LOCK_EX)
        return true;
	else
		return false;
}

LockEntry * Row_lock::get_entry() {
	LockEntry * entry = (LockEntry *) 
		mem_allocator.alloc(sizeof(LockEntry), _row->get_part_id());
	return entry;
}
void Row_lock::return_entry(LockEntry * entry) {
	mem_allocator.free(entry, sizeof(LockEntry));
}

#endif

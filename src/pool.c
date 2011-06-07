/*
 * Copyright (C) Mark Pentland 2011 <mark.pent@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */
#include "mogile_fs.h"
#include "logger.h"
#include <apr_strings.h>
#include <apr_errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>





tracker_pool * mfs_pool_init_quick(char *tracker_list) {
	//count the number of , in the string
	char *search_pointer = tracker_list;
	int comma_count=0;
	while((search_pointer = strchr(search_pointer, ',')) != NULL) {
		comma_count ++;
		*search_pointer++;
	}
	tracker_pool * trackers = mfs_pool_init(comma_count + 1);
	char * tok_state;
	char * token = apr_strtok(tracker_list, ",", &tok_state);
	//tracker_list = strtok(tracker_list, ",");
	while(token != NULL) {
		search_pointer = strchr(token, ':');
		if(search_pointer == NULL) {
			mfs_log(LOG_CRIT, "mfs_pool_init_quick: invalid tracker definition: %s. It must be in the form address:port", tracker_list);
			return NULL;
		}
		*search_pointer = '\0';
		*search_pointer ++;
		
		mfs_pool_register_tracker(trackers, token, atoi(search_pointer));
		token = apr_strtok(NULL, ",", &tok_state);
	}
  	return trackers;
}

tracker_pool * mfs_pool_init(int tracker_count) {
	srand(1); //we dont need truly random... just good distribution...
	apr_pool_t *p;
	if(apr_pool_create(&p,NULL) != APR_SUCCESS) {
		mfs_log(LOG_CRIT, "Unable to create apr_pool");
		return NULL;
	}
	apr_thread_rwlock_t *lock;
	apr_status_t rv = apr_thread_rwlock_create(&lock,p);
	if(rv != APR_SUCCESS) {
		mfs_log_apr(LOG_CRIT, rv, p, "Unable to create apr_thread_rwlock_t:");
		return NULL;
	}
	
	tracker_pool * pool = apr_palloc(p, sizeof(tracker_pool));
	pool->max_tracker_count = tracker_count;
	pool->trackers = (tracker_info*)apr_palloc(p, sizeof(tracker_info) * tracker_count);
	pool->tracker_count = 0;
	pool->active_trackers = (int*)apr_palloc(p, sizeof(int) * tracker_count); //indexes of active trackers
	pool->active_tracker_count = 0;
	pool->inactive_trackers  = (int*)apr_palloc(p, sizeof(int) * tracker_count); //indexes of inactive trackers
	pool->inactive_tracker_count = 0; 
	pool->connection_pools = (tracker_connection_pool*)apr_pcalloc(p, sizeof(tracker_connection_pool) * tracker_count); //array of collection pools whose index matches trackers array
	
	pool->lock = lock; //used to lock when changing active trackers
	pool->pool = p;
	apr_thread_mutex_create(&pool->maintenance_mutex, APR_THREAD_MUTEX_UNNESTED, p);
    apr_thread_cond_create(&pool->maintenance_cond, p);
	pool->maintenance_thread = NULL;
	pool->maintenance_thread_running = false;
	pool->pool_running = true;
	pool->maintenance_thread_check_count=0;
	
	return pool;
}


void mfs_destroy_connection_pool(tracker_pool * pool, int tracker_index) {
	tracker_connection_pool_entry * entry = NULL;
	while((entry = mfs_pool_get_connection(pool, tracker_index, pool->pool, false, 0)) != NULL) {
		mfs_pool_destroy_connection(entry);
	}
	apr_thread_mutex_destroy(pool->connection_pools[tracker_index].lock);
}

void mfs_destroy_pool(tracker_pool * pool) {
	int i;
	apr_status_t rv;
	for(i=0; i < pool->max_tracker_count; i++) {
		mfs_destroy_connection_pool(pool, i);
	}
	apr_thread_mutex_destroy(pool->maintenance_mutex);
	apr_thread_cond_destroy(pool->maintenance_cond);
	apr_thread_rwlock_destroy(pool->lock);
	apr_pool_destroy(pool->pool);
}



void mfs_pool_register_tracker(tracker_pool * trackers, char *address, int port) {
	if(trackers->tracker_count == trackers->max_tracker_count) {
		mfs_log(LOG_CRIT, "Unable to mfs_pool_register_tracker: max tracker count reached (the calling application has incorectly passed tracker_count to mfs_pool_init)");
		return;
	}
	mfs_log(LOG_DEBUG, "Registering tracker %s:%d", address, port);
	if(mfs_tracker_init2(address, port, trackers->pool, &trackers->trackers[trackers->tracker_count])!= APR_SUCCESS) {
		return;
	}
	apr_status_t rv = apr_thread_mutex_create(&trackers->connection_pools[trackers->tracker_count].lock, APR_THREAD_MUTEX_DEFAULT, trackers->pool);
	if(rv != APR_SUCCESS) {
		mfs_log_apr(LOG_CRIT, rv, trackers->pool, "Unable to create connection pool mutex:");
		return;
	}
	trackers->connection_pools[trackers->tracker_count].connection_stack = apr_palloc(trackers->pool, sizeof(tracker_connection_stack));
	APR_RING_INIT(trackers->connection_pools[trackers->tracker_count].connection_stack, _tracker_connection_pool_entry, link);
	trackers->active_trackers[trackers->tracker_count] = trackers->tracker_count; //mark as active...
	trackers->active_tracker_count++;
	trackers->tracker_count++;
	if(trackers->tracker_count == trackers->max_tracker_count) {
		mfs_pool_start_maintenance_thread(trackers);
	}
}


tracker_list * mfs_pool_list_active_trackers(tracker_pool * trackers, apr_pool_t *pool) {
	if(trackers->active_tracker_count==0) return NULL;
	tracker_list *list = apr_palloc(pool, sizeof(tracker_list));
	//we make a copy so we dont have to worry about synchronisation later on
	apr_status_t rv = apr_thread_rwlock_rdlock(trackers->lock);
	if(rv != APR_SUCCESS) {
		mfs_log_apr(LOG_CRIT, rv, pool, "Unable to read-lock pool mutex:");
		return NULL;
	}
	list->tracker_indexes = apr_pmemdup(pool,trackers->active_trackers, sizeof(int) * trackers->active_tracker_count); //copy the active list
	list->tracker_count = trackers->active_tracker_count;
	rv = apr_thread_rwlock_unlock(trackers->lock);
	if(rv != APR_SUCCESS) {
		mfs_log_apr(LOG_ERR, rv, pool, "Unable to unlock read-lock pool mutex:");
	}
	list->start_postion = (int)rint(((float)rand() / RAND_MAX) * (list->tracker_count-1) );
	list->current_position = -1; //use -1 to mean we havnt started...
	return list;
}

tracker_list * mfs_pool_list_inactive_trackers(tracker_pool * trackers, apr_pool_t *pool) {
	if(trackers->inactive_tracker_count==0) return NULL;
	tracker_list *list = apr_palloc(pool, sizeof(tracker_list));
	//we make a copy so we dont have to worry about synchronisation later on
	apr_status_t rv = apr_thread_rwlock_rdlock(trackers->lock);
	if(rv != APR_SUCCESS) {
		mfs_log_apr(LOG_CRIT, rv, pool, "Unable to read-lock pool mutex:");
		return NULL;
	}
	list->tracker_indexes = apr_pmemdup(pool,trackers->inactive_trackers, sizeof(int) * trackers->inactive_tracker_count); //copy the active list
	list->tracker_count = trackers->inactive_tracker_count;
	rv = apr_thread_rwlock_unlock(trackers->lock);
	if(rv != APR_SUCCESS) {
		mfs_log_apr(LOG_ERR, rv, pool, "Unable to unlock read-lock pool mutex:");
	}
	list->start_postion = (int)rint(((float)rand() / RAND_MAX) * (list->tracker_count-1) );
	list->current_position = -1; //use -1 to mean we havnt started...
	return list;
}

tracker_info * mfs_pool_next_tracker(tracker_list * list, tracker_pool *trackers) {
	if(list->current_position==-1) {
		list->current_position = list->start_postion;
	} else {
		list->current_position ++;
		if(list->current_position >= list->tracker_count) list->current_position = 0; //we have looped
		if(list->current_position == list->start_postion) return NULL; //we are back to the start...
	}
	return &trackers->trackers[mfs_pool_current_tracker_index(list)];
}

int mfs_pool_current_tracker_index(tracker_list * list) {
	if(list->current_position==-1) {
		return list->tracker_indexes[list->start_postion];
	} else {
		return list->tracker_indexes[list->current_position];
	}
}

//find a value and remove it from the array
bool remove_from_array(int *values,  int length, int test) {
	int i;
	bool found=false;
	for(i=0; i < length; i++) {
		if(found) {
			values[i] = values[i+1];
		} else if(values[i] == test) {
			found = true;
			values[i] = values[i+1];
		}
	}
	return found;
}

void mfs_pool_activate(tracker_pool * trackers, int tracker_index, apr_pool_t *pool) {
	apr_status_t rv = apr_thread_rwlock_wrlock(trackers->lock);
	if(rv != APR_SUCCESS) {
		mfs_log_apr(LOG_CRIT, rv, pool, "Unable to write-lock pool mutex:");
		return;
	}
	if(remove_from_array(trackers->inactive_trackers,  trackers->inactive_tracker_count, tracker_index)) { //we may get multiple activate calls... discard if already active...
		trackers->inactive_tracker_count--;
		trackers->active_trackers[trackers->active_tracker_count] = tracker_index;
		trackers->active_tracker_count ++;
	}
	rv = apr_thread_rwlock_unlock(trackers->lock);
	if(rv != APR_SUCCESS) {
		mfs_log_apr(LOG_ERR, rv, pool, "Unable to unlock write-lock pool mutex:");
	}
}

void mfs_pool_deactivate(tracker_pool * trackers, int tracker_index, apr_pool_t *pool) {
	apr_status_t rv = apr_thread_rwlock_wrlock(trackers->lock);
	if(rv != APR_SUCCESS) {
		mfs_log_apr(LOG_CRIT, rv, pool, "Unable to write-lock pool mutex:");
		return;
	}
	bool tracker_removed = remove_from_array(trackers->active_trackers,  trackers->active_tracker_count, tracker_index);
	if(tracker_removed) { //we may get multiple activate calls... discard if already active...
		trackers->active_tracker_count--;
		trackers->inactive_trackers[trackers->inactive_tracker_count] = tracker_index;
		trackers->inactive_tracker_count ++;
	}
	rv = apr_thread_rwlock_unlock(trackers->lock);
	if(rv != APR_SUCCESS) {
		mfs_log_apr(LOG_ERR, rv, pool, "Unable to unlock write-lock pool mutex:");
	}
	if(tracker_removed) {
		//we need to remove the connections from the deactivated tracker...
		tracker_connection_pool_entry *entry;
		while((entry = mfs_pool_get_connection(trackers, tracker_index, pool, false, 1)) != NULL) {
			mfs_pool_destroy_connection(entry);
		}
	}
}

tracker_connection_pool_entry * mfs_pool_get_connection(tracker_pool *trackers, int tracker_index, apr_pool_t *pool, bool create_new, apr_interval_time_t timeout) {
	return mfs_pool_get_connection_ex(trackers, tracker_index, pool, &create_new, timeout);
}

tracker_connection_pool_entry * mfs_pool_get_connection_ex(tracker_pool *trackers, int tracker_index, apr_pool_t *pool, bool *create_new, apr_interval_time_t timeout) {
	//get the connection pool
	tracker_connection_pool * cp = &trackers->connection_pools[tracker_index];
	//lock the connection pool
	apr_status_t rv = apr_thread_mutex_lock(cp->lock);
	if(rv != APR_SUCCESS) {
		mfs_log_apr(LOG_CRIT, rv, pool, "Unable to mutex lock on connection pool for tracker %d during mfs_pool_get_connection:", tracker_index);
		return NULL;
	}
	tracker_connection_pool_entry *next_connection_entry;
	//pop next connection from stack
	if(APR_RING_EMPTY(cp->connection_stack, _tracker_connection_pool_entry, link)) {
		next_connection_entry = NULL;
	} else {
		next_connection_entry = APR_RING_FIRST(cp->connection_stack);
		APR_RING_REMOVE(next_connection_entry, link);
		cp->connection_count--;
	}
	//unlock connection pool
	rv = apr_thread_mutex_unlock(cp->lock);
	if(rv != APR_SUCCESS) {
		mfs_log_apr(LOG_ERR, rv, pool, "Unable to unlock mutex lock on connection pool for tracker %d during mfs_pool_get_connection:", tracker_index);
	}
	if((!*create_new) || (next_connection_entry != NULL)) { //we found a connection... return it...(or we dont allow creating new connections)
		*create_new = false;
		return next_connection_entry;
	}
	//try and connect
	apr_pool_t *c_pool;
	rv = apr_pool_create(&c_pool,NULL);
	if(rv != APR_SUCCESS) {
		mfs_log_apr(LOG_CRIT, rv, pool, "Unable to create apr_pool for connection for tracker %d:", tracker_index);
		return NULL;
	}
	tracker_connection * connection;
	rv = mfs_tracker_connect(&trackers->trackers[tracker_index], &connection, c_pool, timeout);
	//if fail, deactive then return NULL
	if(rv != APR_SUCCESS) {
		apr_pool_destroy(c_pool);  	
		mfs_log_apr(LOG_CRIT, rv, pool, "Unable to connect to tracker %d, deactivating:", tracker_index);
		mfs_pool_deactivate(trackers, tracker_index, pool);
		return NULL;
	}
	//wrap the new connection in a tracker_connection_pool_entry and return it. we use the connections memory pool because they persist for same duration
	next_connection_entry = (tracker_connection_pool_entry*)apr_pcalloc(c_pool, sizeof(tracker_connection_pool_entry)); 
	next_connection_entry->connection = connection;
	*create_new = true;
	return next_connection_entry;
}

void mfs_pool_return_connection(tracker_pool *trackers, int tracker_index, tracker_connection_pool_entry * connection_entry, apr_pool_t *pool) {
	//set the last used to now
	connection_entry->last_used = apr_time_now();
		
	//get the connection pool
	tracker_connection_pool * cp = &trackers->connection_pools[tracker_index];
	//lock the connection pool
	apr_status_t rv = apr_thread_mutex_lock(cp->lock);
	if(rv != APR_SUCCESS) {
		mfs_log_apr(LOG_CRIT, rv, pool, "Unable to mutex lock on connection pool for tracker %d during mfs_pool_return_connection:", tracker_index);
		return;
	}
	APR_RING_INSERT_HEAD(cp->connection_stack, connection_entry, _tracker_connection_pool_entry, link);
	cp->connection_count++;
	//unlock connection pool
	rv = apr_thread_mutex_unlock(cp->lock);
	if(rv != APR_SUCCESS) {
		mfs_log_apr(LOG_ERR, rv, pool, "Unable to unlock mutex lock on connection pool for tracker %d during mfs_pool_return_connection:", tracker_index);
	}
}

void mfs_pool_destroy_connection(tracker_connection_pool_entry * connection_entry) {
	mfs_tracker_destroy_connection(connection_entry->connection); //the connection has the pool which connection_entry was allocated from
}
bool mfs_allow_maintenance_thread = true;

void mfs_pool_disable_maintenance() {
	mfs_allow_maintenance_thread = false;
}
void mfs_pool_enable_maintenance() {
	mfs_allow_maintenance_thread = true;
}

void mfs_pool_start_maintenance_thread(tracker_pool *trackers) {
	if(mfs_allow_maintenance_thread) {
		trackers->pool_running = true;
		apr_threadattr_t *thd_attr;
		apr_threadattr_create(&thd_attr, trackers->pool);
		apr_status_t rv = apr_thread_create(&trackers->maintenance_thread, thd_attr, mfs_pool_maintenance, (void*)trackers, trackers->pool);
		if(rv != APR_SUCCESS) {
			mfs_log_apr(LOG_CRIT, rv, trackers->pool, "Unable to start mfs_pool_maintenance thread.:");
		}
	}
}

void mfs_pool_stop_maintenance_thread(tracker_pool *trackers) {
	if(trackers->maintenance_thread != NULL) {
		trackers->pool_running = false;
		apr_thread_mutex_lock(trackers->maintenance_mutex);
		apr_thread_cond_signal(trackers->maintenance_cond);
		apr_thread_mutex_unlock(trackers->maintenance_mutex);
		apr_status_t rv2;
		apr_status_t rv = apr_thread_join(&rv2, trackers->maintenance_thread);
	}
}

void* APR_THREAD_FUNC mfs_pool_maintenance(apr_thread_t *thd, void *data) {
	tracker_pool *trackers = (tracker_pool *)data;
	trackers->maintenance_thread_running = true;
	while(trackers->pool_running) {
		apr_pool_t *pool;
		apr_status_t rv = apr_pool_create(&pool,NULL);
		if(rv != APR_SUCCESS) {
			mfs_log_apr(LOG_CRIT, rv, pool, "Unable to create apr_pool for mfs_pool_maintain:");
		} else {
			mfs_pool_test_inactive_trackers(trackers, pool);
			mfs_pool_expire_active_trackers(trackers, pool); 
			apr_pool_destroy(pool);  
		}
		trackers->maintenance_thread_check_count++;
		if(trackers->maintenance_thread_check_count == APR_UINT32_MAX) {
			trackers->maintenance_thread_check_count = 0;
		}
		//use a condition variable to sleep allowing us to wake it from mfs_pool_stop_maintenance_thread 
		apr_thread_mutex_lock(trackers->maintenance_mutex);
		apr_thread_cond_timedwait(trackers->maintenance_cond, trackers->maintenance_mutex, apr_time_from_sec(MFS_POOL_MAINTENANCE_POLL_TIME));
		apr_thread_mutex_unlock(trackers->maintenance_mutex);
	}
	apr_thread_exit(thd, APR_SUCCESS);
	return NULL;
}

void mfs_pool_test_inactive_trackers(tracker_pool *trackers, apr_pool_t *pool) {
	tracker_list * inactive = mfs_pool_list_inactive_trackers(trackers, pool);
	if(inactive != NULL) {
		tracker_info *tracker;
		tracker_request_parameters * params = mfs_tracker_init_parameters(pool);
		mfs_tracker_add_parameter(params, "domain",  "ping_domain", pool);
		mfs_tracker_add_parameter(params, "key",  "ping_key", pool);
		mfs_tracker_add_parameter(params, "noverify",  "1", pool);
		apr_hash_t *result = apr_hash_make(pool);
		while((tracker = mfs_pool_next_tracker(inactive, trackers)) != NULL) {
			//we call a faux get_paths command on the tracker to see if its now active...
			tracker_connection_pool_entry * connection_entry = mfs_pool_get_connection(trackers, mfs_pool_current_tracker_index(inactive), pool, true, DEFAULT_TRACKER_TIMEOUT);
			if(connection_entry != NULL) { //well we can connect!
				bool ok;
				apr_status_t rv = mfs_tracker_request(connection_entry->connection, "get_paths", params, &ok, result, pool, DEFAULT_TRACKER_TIMEOUT);
				if(rv == APR_SUCCESS) {
					//should we check further?... for now we will assume the tracker is ok...
					mfs_log(LOG_INFO, "Tracker %s:%d is now active", tracker->address, tracker->port);
					mfs_pool_activate(trackers, mfs_pool_current_tracker_index(inactive), pool);
					mfs_pool_return_connection(trackers, mfs_pool_current_tracker_index(inactive), connection_entry, pool);
				} else {
					mfs_log_apr(LOG_DEBUG, rv, pool, "Tracker %s:%d failed after connect. Remaining inactive:", tracker->address, tracker->port);
					mfs_pool_destroy_connection(connection_entry);
				}
			}
		}
	}
}

void mfs_pool_expire_active_trackers(tracker_pool *trackers, apr_pool_t *pool) {
	tracker_list * active = mfs_pool_list_active_trackers(trackers, pool);
	tracker_connection_pool_entry * last_entry;
	if(active != NULL) {
		tracker_info *tracker;
		while((tracker = mfs_pool_next_tracker(active, trackers)) != NULL) {
			int tracker_index = mfs_pool_current_tracker_index(active);
			tracker_connection_pool * cp = &trackers->connection_pools[tracker_index];
			last_entry = mfs_pool_get_expired_trackers(cp, pool);
			//now do the stuff that may take a bit of time...
			while(last_entry != NULL) {
				tracker_connection_pool_entry * to_delete = last_entry;
				last_entry = APR_RING_NEXT(last_entry,link);
				mfs_pool_destroy_connection(to_delete);
			}
		}
	}
}

tracker_connection_pool_entry * mfs_pool_get_expired_trackers(tracker_connection_pool * cp, apr_pool_t *pool) {
	tracker_connection_pool_entry * connection_entry, * last_entry=NULL;
	apr_time_t cutoff = apr_time_now() - apr_time_from_sec(MFS_CONNECTION_EXPIRE_TIME);  //MFS_CONNECTION_EXPIRE_TIME seconds ago

	apr_status_t rv = apr_thread_mutex_lock(cp->lock);
	if(rv != APR_SUCCESS) {
		mfs_log_apr(LOG_CRIT, rv, pool, "Unable to mutex lock on connection pool for tracker during mfs_pool_get_expired_trackers:");
		return NULL;
	}
	for (connection_entry = APR_RING_LAST(cp->connection_stack); 
	    	((connection_entry != APR_RING_SENTINEL(cp->connection_stack, _tracker_connection_pool_entry, link)) && (connection_entry->last_used < cutoff)); 
	    	connection_entry = APR_RING_PREV(connection_entry, link)) {
		last_entry = connection_entry; //this entry needs to expire...
	}
	if(last_entry != NULL) { //we are now pointing to the most recently used connection that needs to expire
		connection_entry = APR_RING_LAST(cp->connection_stack);
		APR_RING_UNSPLICE(last_entry,connection_entry,link); //remove from last_entry to connection_entry (APR_RING_LAST) from stack
	}
	//unlock connection pool
	rv = apr_thread_mutex_unlock(cp->lock);
	if(rv != APR_SUCCESS) {
		mfs_log_apr(LOG_ERR, rv, pool, "Unable to unlock mutex lock on connection pool for tracker during mfs_pool_get_expired_trackers:");
	}
	if(last_entry != NULL) {
		APR_RING_NEXT(connection_entry,link) = NULL; //so we know when to stop...(APR_RING_UNSPLICE leaves it dangling) 
	}
	return last_entry;		
}
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
#include <stdbool.h>

apr_status_t mfs_request_do(tracker_pool *trackers, char *action, tracker_request_parameters *parameters, bool *ok, apr_hash_t *result, apr_pool_t *pool, apr_interval_time_t timeout) {
	apr_status_t rv = APR_ECONNREFUSED; //default to APR_ECONNREFUSED becuase if we dont call the server its becuase they are all down
	bool auto_allocate_pool;
	if(pool == NULL) {
		if((rv=apr_pool_create(&pool,NULL)) != APR_SUCCESS) {
			mfs_log(LOG_CRIT, "Unable to create APR memory pool. Error=%d", rv);
			return rv;
		}
		auto_allocate_pool = true;
	} else {
		auto_allocate_pool = false;
	}
	tracker_list * list = mfs_pool_list_active_trackers(trackers, pool);
	if(list == NULL) {
		mfs_log(LOG_ERR, "Unable to get active tracker when attempting action '%s'", action);
		return APR_ECONNREFUSED;
	}
	tracker_info *tracker;
	bool connection_finished = false; //used to cut-out early from while loop...
	//loop through the trackers (the list iterator is already starting at a random spot)
	while((!connection_finished) && ((tracker = mfs_pool_next_tracker(list, trackers)) != NULL)) {
		int tracker_index = mfs_pool_current_tracker_index(list);
		bool keep_trying_tracker = true;
		while((keep_trying_tracker)&&(!connection_finished)) {
			bool is_new_connection=true;
			tracker_connection_pool_entry * connection_entry = mfs_pool_get_connection_ex(trackers, tracker_index, pool, &is_new_connection, timeout);
			if(is_new_connection) {
				keep_trying_tracker = false; //this is a new connection... we wont get a cached connection error...
			}
			if(connection_entry != NULL) {
				rv  = mfs_tracker_request(connection_entry->connection, action, parameters, ok, result, pool, timeout);
				if(rv != APR_SUCCESS) {
					//should we report the tracker as down?
					//if not then maybe we should clear the connection pool? (in case cached connections are stale (tcp))
					//for now we will just destroy the connection and try another tracker...the pool will get exhausted and then it will deactivate when connect fails..
					//if(!keep_trying_tracker) {
						//the error occured on a fresh connection... there is something wrong with the tracker...
					//}
					mfs_pool_destroy_connection(connection_entry);
				} else {
					mfs_pool_return_connection(trackers, tracker_index, connection_entry, pool); //return the connection to the pool
					connection_finished = true;
				}
			} else {
				keep_trying_tracker = false;
			}
		}
	}
	
	if(auto_allocate_pool) {
		apr_pool_destroy(pool);
	}
	return rv; //this will be APR_SUCCESS or the last failed status
}
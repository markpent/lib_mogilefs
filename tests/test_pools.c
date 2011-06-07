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
#include "test_pools.h"
#include "test_server.h"
#include "common.h"


void test_pool_init() {
	mfs_pool_disable_maintenance();
	char tracker_list_str[] = "127.0.0.1:9991,127.0.0.1:9992,127.0.0.1:9993";
	tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

	CU_ASSERT_EQUAL(trackers->tracker_count, 3);
	CU_ASSERT_STRING_EQUAL(trackers->trackers[0].address, "127.0.0.1");
	CU_ASSERT_EQUAL(trackers->trackers[0].port, 9991);
	CU_ASSERT_STRING_EQUAL(trackers->trackers[1].address, "127.0.0.1");
	CU_ASSERT_EQUAL(trackers->trackers[1].port, 9992);
	CU_ASSERT_STRING_EQUAL(trackers->trackers[2].address, "127.0.0.1");
	CU_ASSERT_EQUAL(trackers->trackers[2].port, 9993);
	
}

void test_pool_tracker_iteration() {
	mfs_pool_disable_maintenance();
	apr_pool_t *pool = mfs_test_get_pool();
	char tracker_list_str[] = "127.0.0.1:9991,127.0.0.1:9992,127.0.0.1:9993";
	tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);
	tracker_list * list = mfs_pool_list_active_trackers(trackers, pool);
	CU_ASSERT_PTR_NOT_NULL_FATAL(list);
	tracker_info *tracker;
	int count = 0;
	while((tracker = mfs_pool_next_tracker(list, trackers)) != NULL) {
		count++;
	}
	CU_ASSERT_EQUAL(count, 3);
	list = mfs_pool_list_inactive_trackers(trackers, pool);
	CU_ASSERT_PTR_NULL(list);

	//check a disabled tracker
	mfs_pool_deactivate(trackers, 0, pool);
	list = mfs_pool_list_active_trackers(trackers, pool);
	CU_ASSERT_PTR_NOT_NULL_FATAL(list);
	count = 0;
	while((tracker = mfs_pool_next_tracker(list, trackers)) != NULL) {
		count++;
		CU_ASSERT_NOT_EQUAL(mfs_pool_current_tracker_index(list), 0);
	}
	CU_ASSERT_EQUAL(count, 2);
	//lets iterate over the disabled
	list = mfs_pool_list_inactive_trackers(trackers, pool);
	CU_ASSERT_PTR_NOT_NULL_FATAL(list);
	count = 0;
	while((tracker = mfs_pool_next_tracker(list, trackers)) != NULL) {
		count++;
		CU_ASSERT_EQUAL(mfs_pool_current_tracker_index(list), 0);
	}
	CU_ASSERT_EQUAL(count, 1);
	//test disabling same tracker
	mfs_pool_deactivate(trackers, 0, pool);
	CU_ASSERT_EQUAL(trackers->inactive_tracker_count, 1);
	CU_ASSERT_EQUAL(trackers->active_tracker_count, 2);

	//test enabling tracker again
	mfs_pool_activate(trackers, 0, pool);
	list = mfs_pool_list_active_trackers(trackers, pool);
	CU_ASSERT_PTR_NOT_NULL_FATAL(list);
	count = 0;
	while((tracker = mfs_pool_next_tracker(list, trackers)) != NULL) {
		count++;
	}
	CU_ASSERT_EQUAL(count, 3);
	list = mfs_pool_list_inactive_trackers(trackers, pool);
	CU_ASSERT_PTR_NULL(list);
	
}

void test_pool_connecting() {
	bool ok;
	mfs_pool_disable_maintenance();
	apr_pool_t *p = mfs_test_get_pool();
	char tracker_list_str[] = "127.0.0.1:9991,127.0.0.1:9992";
	tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

	
	//test basic ok response
	{
		char test_response[] = "OK 123 abc=def\r\n";
		
		test_server_handle * handle = test_start_basic_server(test_response, 9991, p);

		tracker_connection_pool_entry * connection_entry = mfs_pool_get_connection(trackers, 0, p, true, DEFAULT_TRACKER_TIMEOUT);
		CU_ASSERT_PTR_NOT_NULL_FATAL(connection_entry);

		tracker_request_parameters * params = mfs_tracker_init_parameters(p);
		mfs_tracker_add_parameter(params, "A",  "B", p);

		apr_hash_t *result = apr_hash_make(p);
		apr_status_t rv  = mfs_tracker_request(connection_entry->connection, "TEST", params, &ok, result, p, DEFAULT_TRACKER_TIMEOUT);
		
		CU_ASSERT_EQUAL(rv, APR_SUCCESS);
		if(rv == APR_SUCCESS) {
			CU_ASSERT_EQUAL(ok, true);
			CU_ASSERT_STRING_EQUAL("def", apr_hash_get(result, "abc", APR_HASH_KEY_STRING));
		}
		mfs_pool_return_connection(trackers, 0, connection_entry, p);

		//i should be able to get the connection now when create_new is false
		connection_entry = mfs_pool_get_connection(trackers, 0, p, false, DEFAULT_TRACKER_TIMEOUT);
		CU_ASSERT_PTR_NOT_NULL(connection_entry);
		mfs_pool_return_connection(trackers, 0, connection_entry, p);
		stop_test_server(handle);

		//test now trying to connect to the server
		connection_entry = mfs_pool_get_connection(trackers, 0, p, false, DEFAULT_TRACKER_TIMEOUT); //even tho the server is down the connection is cached....
		CU_ASSERT_PTR_NOT_NULL_FATAL(connection_entry);
		rv  = mfs_tracker_request(connection_entry->connection, "TEST", params, &ok, result, p, DEFAULT_TRACKER_TIMEOUT);
		CU_ASSERT_NOT_EQUAL(rv, APR_SUCCESS);
		mfs_pool_return_connection(trackers, 0, connection_entry, p); //normally it would be destroyed, but we want to test mfs_pool_deactivate destroying it...

		CU_ASSERT_FALSE(APR_RING_EMPTY(trackers->connection_pools[0].connection_stack, _tracker_connection_pool_entry, link)); //make sure the connection pool has something...
		mfs_pool_deactivate(trackers, 0, p);
		CU_ASSERT_TRUE(APR_RING_EMPTY(trackers->connection_pools[0].connection_stack, _tracker_connection_pool_entry, link)); //make sure the mfs_pool_deactivate call removed them...
		CU_ASSERT_EQUAL(trackers->inactive_tracker_count, 1);
		CU_ASSERT_EQUAL(trackers->active_tracker_count, 1);
		
		//there are no pooled connections right now... lets try and get one...
		connection_entry = mfs_pool_get_connection(trackers, 1, p, true, DEFAULT_TRACKER_TIMEOUT);
		CU_ASSERT_PTR_NULL(connection_entry); //no connection should be returned...
		CU_ASSERT_EQUAL(trackers->inactive_tracker_count, 2);
		CU_ASSERT_EQUAL(trackers->active_tracker_count, 0);

	}
}

void test_pool_maintenance_expire_active() {
	bool ok;
	mfs_pool_disable_maintenance();
	apr_pool_t *p = mfs_test_get_pool();

	//char test_response[] = "OK 123 abc=def\r\n";
	//test_server_handle * handle = test_start_basic_server(test_response, 9991, p);

	char tracker_list_str[] = "127.0.0.1:9991,127.0.0.1:9992";
	tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

	tracker_connection_pool_entry * connection_entry;// = mfs_pool_get_connection(trackers, 0, p, true);

	apr_time_t cutoff1 = apr_time_now() - apr_time_from_sec(MFS_CONNECTION_EXPIRE_TIME + 1); 
	apr_time_t cutoff2 = apr_time_now() - apr_time_from_sec(MFS_CONNECTION_EXPIRE_TIME + 2); 

	//add 2 connections and set thier last used before MFS_CONNECTION_EXPIRE_TIME
	connection_entry = (tracker_connection_pool_entry*)apr_pcalloc(p, sizeof(tracker_connection_pool_entry)); 
	connection_entry->connection = NULL;
	mfs_pool_return_connection(trackers, 0, connection_entry, p);
	connection_entry->last_used = cutoff1;

	connection_entry = (tracker_connection_pool_entry*)apr_pcalloc(p, sizeof(tracker_connection_pool_entry)); 
	connection_entry->connection = NULL;
	mfs_pool_return_connection(trackers, 0, connection_entry, p);
	connection_entry->last_used = cutoff2;

	//add another but dont set the time (will be now())
	connection_entry = (tracker_connection_pool_entry*)apr_pcalloc(p, sizeof(tracker_connection_pool_entry)); 
	connection_entry->connection = NULL;
	mfs_pool_return_connection(trackers, 0, connection_entry, p);

	connection_entry = mfs_pool_get_expired_trackers(&trackers->connection_pools[0], p);
	CU_ASSERT_PTR_NOT_NULL_FATAL(connection_entry);
	CU_ASSERT_EQUAL(connection_entry->last_used, cutoff2);

	connection_entry = APR_RING_NEXT(connection_entry,link);
	CU_ASSERT_PTR_NOT_NULL_FATAL(connection_entry);
	CU_ASSERT_EQUAL(connection_entry->last_used, cutoff1);
	
	connection_entry = APR_RING_NEXT(connection_entry,link);
	CU_ASSERT_PTR_NULL(connection_entry);

	//make sure there is still the top connection still there...
	connection_entry = mfs_pool_get_connection(trackers, 0, p, false, DEFAULT_TRACKER_TIMEOUT);
	CU_ASSERT_PTR_NOT_NULL(connection_entry); //top connection should be returned...
	
	//stop_test_server(handle);
}

void test_pool_maintenance_activate_inactive() {
	bool ok;
	mfs_pool_disable_maintenance();
	apr_pool_t *p = mfs_test_get_pool();

	

	char tracker_list_str[] = "127.0.0.1:9991,127.0.0.1:9992";
	tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

	tracker_connection_pool_entry * connection_entry = mfs_pool_get_connection(trackers, 0, p, true, DEFAULT_TRACKER_TIMEOUT);
	CU_ASSERT_PTR_NULL(connection_entry);
	connection_entry = mfs_pool_get_connection(trackers, 1, p, true, DEFAULT_TRACKER_TIMEOUT);
	CU_ASSERT_PTR_NULL(connection_entry);
	//both trackers should now be marked inactive...
	CU_ASSERT_EQUAL(trackers->inactive_tracker_count, 2);
	CU_ASSERT_EQUAL(trackers->active_tracker_count, 0);

	char test_response[] = "OK 123 abc=def\r\n";
	test_server_handle * handle = test_start_basic_server(test_response, 9991, p);

	mfs_pool_test_inactive_trackers(trackers, p); //tracker 0 should come back
	CU_ASSERT_EQUAL(trackers->inactive_tracker_count, 1);
	CU_ASSERT_EQUAL(trackers->active_tracker_count, 1);
	test_server_handle * handle2 = test_start_basic_server(test_response, 9992, p);
	mfs_pool_test_inactive_trackers(trackers, p); //tracker 1 should come back
	CU_ASSERT_EQUAL(trackers->inactive_tracker_count, 0);
	CU_ASSERT_EQUAL(trackers->active_tracker_count, 2);
	stop_test_server(handle);
	stop_test_server(handle2);
}

//actually run the thread...
void test_pool_maintenance_thread() {
	//first setup all the conditions we want to test:
	//2 servers, 1 with 1 active pooled connection, the other is marked as down but it actually up
	//set the time last used of pooled connection to > MFS_CONNECTION_EXPIRE_TIME ago
	//start the thread
	//result: 2 trackers now activated, and active pool is empty...

	bool ok;
	mfs_pool_disable_maintenance();
	apr_pool_t *p = mfs_test_get_pool();
	tracker_connection_pool_entry * connection_entry;
	
	char tracker_list_str[] = "127.0.0.1:9991,127.0.0.1:9992";
	tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

	char test_response[] = "OK 123 abc=def\r\n";
	test_server_handle * handle = test_start_basic_server(test_response, 9991, p);
	
	{
		connection_entry = mfs_pool_get_connection(trackers, 0, p, true, DEFAULT_TRACKER_TIMEOUT);
		CU_ASSERT_PTR_NOT_NULL_FATAL(connection_entry);

		tracker_request_parameters * params = mfs_tracker_init_parameters(p);
		mfs_tracker_add_parameter(params, "A",  "B", p);

		apr_hash_t *result = apr_hash_make(p);
		apr_status_t rv  = mfs_tracker_request(connection_entry->connection, "TEST", params, &ok, result, p, DEFAULT_TRACKER_TIMEOUT);
		
		CU_ASSERT_EQUAL(rv, APR_SUCCESS);
		if(rv == APR_SUCCESS) {
			CU_ASSERT_EQUAL(ok, true);
			CU_ASSERT_STRING_EQUAL("def", apr_hash_get(result, "abc", APR_HASH_KEY_STRING));
		}
		mfs_pool_return_connection(trackers, 0, connection_entry, p);
		connection_entry->last_used = apr_time_now() - apr_time_from_sec(MFS_CONNECTION_EXPIRE_TIME + 1); 
		//we now have a connection in the pool that should get expired...
	}
	connection_entry = mfs_pool_get_connection(trackers, 1, p, true, DEFAULT_TRACKER_TIMEOUT);
	CU_ASSERT_PTR_NULL(connection_entry);
	CU_ASSERT_EQUAL(trackers->inactive_tracker_count, 1);
	CU_ASSERT_EQUAL(trackers->active_tracker_count, 1);
	//2nd tracker should now be marked inactive...
	test_server_handle * handle2 = test_start_basic_server(test_response, 9992, p);
	//mfs_pool_test_inactive_trackers(trackers, p); //tracker 1 should come back
	mfs_pool_enable_maintenance();
	mfs_pool_start_maintenance_thread(trackers);
	while(!trackers->maintenance_thread_running) {
		apr_sleep(100);
	}
	while(trackers->maintenance_thread_check_count==0) {
		apr_sleep(100);
	}
	//result: 2 trackers now activated, and active pool is empty...
	CU_ASSERT_EQUAL(trackers->inactive_tracker_count, 0);
	CU_ASSERT_EQUAL(trackers->active_tracker_count, 2);
	connection_entry = mfs_pool_get_connection(trackers, 0, p, false, DEFAULT_TRACKER_TIMEOUT);
	CU_ASSERT_PTR_NULL(connection_entry); //the entry should have been expired...
	mfs_pool_stop_maintenance_thread(trackers);
	stop_test_server(handle);
	stop_test_server(handle2);
}
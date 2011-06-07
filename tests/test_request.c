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
#include "test_request.h"
#include "common.h"
#include "mogile_fs.h"
#include <stdbool.h>
#include "test_server.h"

void test_request_all_ok() {
	//basic test where everything is working as it should be
	bool ok;
	mfs_pool_disable_maintenance();
	apr_pool_t *p = mfs_test_get_pool();
	char tracker_list_str[] = "127.0.0.1:9991,127.0.0.1:9992";
	tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

	char test_response[] = "OK 123 abc=def\r\n";
	test_server_handle * handle1 = test_start_looped_server(test_response, 9991, p);
	test_server_handle * handle2 = test_start_looped_server(test_response, 9992, p);
	apr_status_t rv;
	int i;
	
	
	for(i=0; i < 5; i++) {
		apr_pool_t *rp = mfs_test_get_pool();
		tracker_request_parameters * params = mfs_tracker_init_parameters(rp);
		mfs_tracker_add_parameter(params, "A",  "B", rp);
		apr_hash_t *result = apr_hash_make(rp);
		rv = mfs_request_do(trackers, "TEST_REQUEST", params, &ok, result, rp, DEFAULT_TRACKER_TIMEOUT);
		CU_ASSERT_EQUAL(rv, APR_SUCCESS);
		if(rv == APR_SUCCESS) {
			CU_ASSERT_EQUAL(ok, true);
			CU_ASSERT_STRING_EQUAL("def", apr_hash_get(result, "abc", APR_HASH_KEY_STRING));
		}
		apr_pool_destroy(rp);
	}
	stop_test_server(handle1);
	stop_test_server(handle2);
}

void test_request_all_ok_no_pool() {
	//basic test where everything is working as it should be
	bool ok;
	mfs_pool_disable_maintenance();
	apr_pool_t *p = mfs_test_get_pool();
	char tracker_list_str[] = "127.0.0.1:9991,127.0.0.1:9992";
	tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

	char test_response[] = "OK 123 abc=def\r\n";
	test_server_handle * handle1 = test_start_looped_server(test_response, 9991, p);
	test_server_handle * handle2 = test_start_looped_server(test_response, 9992, p);
	apr_status_t rv;
	int i;
	
	
	for(i=0; i < 5; i++) {
		apr_pool_t *rp = mfs_test_get_pool();
		tracker_request_parameters * params = mfs_tracker_init_parameters(rp);
		mfs_tracker_add_parameter(params, "A",  "B", rp);
		apr_hash_t *result = apr_hash_make(rp);
		rv = mfs_request_do(trackers, "TEST_REQUEST", params, &ok, result, NULL, DEFAULT_TRACKER_TIMEOUT);
		CU_ASSERT_EQUAL(rv, APR_SUCCESS);
		if(rv == APR_SUCCESS) {
			CU_ASSERT_EQUAL(ok, true);
			CU_ASSERT_STRING_EQUAL("def", apr_hash_get(result, "abc", APR_HASH_KEY_STRING));
		}
		apr_pool_destroy(rp);
	}
	stop_test_server(handle1);
	stop_test_server(handle2);
}


void test_request_reconnect() {
	//basic test where everything is working as it should be
	bool ok;
	mfs_pool_disable_maintenance();
	apr_pool_t *p = mfs_test_get_pool();
	char tracker_list_str[] = "127.0.0.1:9991";
	tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

	char test_response[] = "OK 123 abc=def\r\n";
	
	apr_status_t rv;
	int i;
	
	for(i=0; i < 5; i++) {
		test_server_handle * handle1 = test_start_looped_server(test_response, 9991, p);
		apr_pool_t *rp = mfs_test_get_pool();
		tracker_request_parameters * params = mfs_tracker_init_parameters(rp);
		mfs_tracker_add_parameter(params, "A",  "B", rp);
		apr_hash_t *result = apr_hash_make(rp);
		rv = mfs_request_do(trackers, "TEST_REQUEST", params, &ok, result, rp, DEFAULT_TRACKER_TIMEOUT);
		CU_ASSERT_EQUAL(rv, APR_SUCCESS);
		if(rv == APR_SUCCESS) {
			CU_ASSERT_EQUAL(ok, true);
			CU_ASSERT_STRING_EQUAL("def", apr_hash_get(result, "abc", APR_HASH_KEY_STRING));
		}
		apr_pool_destroy(rp);
		stop_test_server(handle1);
	}
	
}
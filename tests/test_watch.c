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

#include "test_watch.h"
#include "common.h"
#include <apr_pools.h>
#include <apr_strings.h>
#include "test_server.h"
#include <assert.h>
 
void test_watch_calling() {
	bool ok;
	apr_pool_t *p = mfs_test_get_pool();
	tracker_info *tracker;

	char tracker_list_str[] = "127.0.0.1:9991";
	tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

	watch_data * watch = init_watch(0, trackers, p, NULL);
	

	char test_buf[1000];
	
	//test basic ok response
	{
		char test_response[] = "ABCD\r\n1234\r\n";
		
		test_server_handle * handle = test_start_basic_server(test_response, 9991, p);

		CU_ASSERT_EQUAL(APR_SUCCESS, get_next_watch_line(watch, test_buf, 999));

		CU_ASSERT_STRING_EQUAL(test_buf, "ABCD");

		stop_test_server(handle); //the local buffer should have the next line....
		
		CU_ASSERT_EQUAL(APR_SUCCESS, get_next_watch_line(watch, test_buf, 999));

		CU_ASSERT_STRING_EQUAL(test_buf, "1234");
		
		

		
	}
}


void test_watch_parsing() {
	apr_pool_t *p = mfs_test_get_pool();
	tracker_info *tracker;

	char tracker_list_str[] = "127.0.0.1:9991";
	tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

	watch_data * watch = init_watch(0, trackers, p, "1234");
	

	char test_buf[1000];
	
	//test basic ok response
	{
		char test_response[] = "ABCD\r\n[dsgdfg] [cache][abcd] http://abcd.com\r\n[dsgdfg] [cache][1234] http://xxxx.com\r\nBBBB\r\n[dsgdfg] [cache][324324324] http://yyyy.com\r\n";
		
		test_server_handle * handle = test_start_basic_server(test_response, 9991, p);

		CU_ASSERT_EQUAL(APR_SUCCESS, get_next_watch_cache_line(watch, test_buf, 999));

		CU_ASSERT_STRING_EQUAL(test_buf, "http://abcd.com");

		//CU_ASSERT_EQUAL(APR_SUCCESS, get_next_watch_cache_line(watch, test_buf, 999));

		//CU_ASSERT_STRING_EQUAL(test_buf, "http://xxxx.com");

		
		
		CU_ASSERT_EQUAL(APR_SUCCESS, get_next_watch_cache_line(watch, test_buf, 999));

		CU_ASSERT_STRING_EQUAL(test_buf, "http://yyyy.com");
		
		stop_test_server(handle); //the local buffer should have the next line....

		
	}

}
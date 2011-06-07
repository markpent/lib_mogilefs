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

#include "test_file.h"
#include "test_http_server.h"
#include "test_server.h"
#include "common.h"
#include "mogile_fs.h"
#include <apr_time.h>

void test_file_system_get_paths_ok() {
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();

	char test_response[] = "OK 123 paths=3&path1=http%3A%2F%2F127.0.0.1%3A8081%2Fpath%2Fone&path2=http%3A%2F%2F127.0.0.1%3A8082%2Fpath%2Ftwo&path3=http%3A%2F%2F127.0.0.1%3A8083%2Fpath%2Fthree\r\n";
	
	test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

	char tracker_list_str[] = "127.0.0.1:9991";
	tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

	char **paths;
	int path_count;
	rv = mfs_get_paths(file_system, "domain", "key", true, &paths, &path_count, p);
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, rv);
	
	stop_test_server(tracker_handle);

	CU_ASSERT_EQUAL_FATAL(path_count, 3);
	CU_ASSERT_STRING_EQUAL("http://127.0.0.1:8081/path/one", paths[0]);
	CU_ASSERT_STRING_EQUAL("http://127.0.0.1:8082/path/two", paths[1]);
	CU_ASSERT_STRING_EQUAL("http://127.0.0.1:8083/path/three", paths[2]);

    mfs_close_file_system(file_system);
	
	apr_pool_destroy(p); 
}

void test_file_system_get_paths_corrupt() {
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();

	char test_response[] = "OK 123 paths=3&path1=http%3A%2F%2F127.0.0.1%3A8081%2Fpath%2Fone&path3=http%3A%2F%2F127.0.0.1%3A8083%2Fpath%2Fthree\r\n";
	
	test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

	char tracker_list_str[] = "127.0.0.1:9991";
	tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

	char **paths;
	int path_count;
	rv = mfs_get_paths(file_system, "domain", "key", true, &paths, &path_count, p);
	CU_ASSERT_EQUAL(APR_EGENERAL, rv);
	
	stop_test_server(tracker_handle);
	mfs_close_file_system(file_system);
	apr_pool_destroy(p); 
}


void test_file_system_get_paths_error_zero() {
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();

	{
		char test_response[] = "OK 123 paths=0\r\n";
	
		test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

		char tracker_list_str[] = "127.0.0.1:9991";
		tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

		CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

		char **paths;
		int path_count;
		rv = mfs_get_paths(file_system, "domain", "key", true, &paths, &path_count, p);
		CU_ASSERT_EQUAL(APR_EGENERAL, rv);
	
		stop_test_server(tracker_handle);
		mfs_close_file_system(file_system);
	}

	{
		char test_response[] = "OK 123 paths=asdf\r\n";
	
		test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

		char tracker_list_str[] = "127.0.0.1:9991";
		tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

		CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

		char **paths;
		int path_count;
		rv = mfs_get_paths(file_system, "domain", "key", true, &paths, &path_count, p);
		CU_ASSERT_EQUAL(APR_EGENERAL, rv);
	
		stop_test_server(tracker_handle);
		mfs_close_file_system(file_system);
	}
	{
		char test_response[] = "OK 123 nopaths=none\r\n";
	
		test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

		char tracker_list_str[] = "127.0.0.1:9991";
		tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

		CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

		char **paths;
		int path_count;
		rv = mfs_get_paths(file_system, "domain", "key", true, &paths, &path_count, p);
		CU_ASSERT_EQUAL(APR_EGENERAL, rv);
	
		stop_test_server(tracker_handle);
		mfs_close_file_system(file_system);
	}
	apr_pool_destroy(p); 
}


void test_file_system_get_paths_error_code() {
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();

	char test_response[] = "ERR NOTFOUND Path Not Found\r\n";
	
	test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

	char tracker_list_str[] = "127.0.0.1:9991";
	tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

	char **paths;
	int path_count;
	rv = mfs_get_paths(file_system, "domain", "key", true, &paths, &path_count, p);
	CU_ASSERT_EQUAL(APR_EGENERAL, rv);
	
	stop_test_server(tracker_handle);
	mfs_close_file_system(file_system);
	apr_pool_destroy(p); 
}


void test_file_system_delete_ok() {
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();

	char test_response[] = "OK 123 some=thing\r\n";
	
	test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

	char tracker_list_str[] = "127.0.0.1:9991";
	tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

	char **paths;
	int path_count;
	rv = mfs_delete(file_system, "domain", "key", p);

	stop_test_server(tracker_handle);
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, rv);
	mfs_close_file_system(file_system);
	apr_pool_destroy(p); 
}


void test_file_system_delete_fail() {
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();
	//error response...
	{
		char test_response[] = "ERR 123 something\r\n";
	
		test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

		char tracker_list_str[] = "127.0.0.1:9991";
		tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

		CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

		char **paths;
		int path_count;
		rv = mfs_delete(file_system, "domain", "key", p);

		stop_test_server(tracker_handle);
		CU_ASSERT_EQUAL_FATAL(APR_EGENERAL, rv);
		mfs_close_file_system(file_system);
	}
	//corrupt response...
	{
		char test_response[] = "sdfgERR 123 something\r\n";
	
		test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

		char tracker_list_str[] = "127.0.0.1:9991";
		tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

		CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

		char **paths;
		int path_count;
		rv = mfs_delete(file_system, "domain", "key", p);

		stop_test_server(tracker_handle);
		CU_ASSERT_EQUAL_FATAL(APR_EFTYPE, rv);
		mfs_close_file_system(file_system);
	}
	apr_pool_destroy(p); 
}


void test_file_system_sleep_ok() {
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();

	char test_response[] = "OK 123 some=thing\r\n";
	
	test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

	char tracker_list_str[] = "127.0.0.1:9991";
	tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

	char **paths;
	int path_count;
	rv = mfs_sleep(file_system, 100, p);

	stop_test_server(tracker_handle);
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, rv);
	mfs_close_file_system(file_system);
	apr_pool_destroy(p); 
}


void test_file_system_sleep_fail() {
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();
	//error response...
	{
		char test_response[] = "ERR 123 something\r\n";
	
		test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

		char tracker_list_str[] = "127.0.0.1:9991";
		tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

		CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

		char **paths;
		int path_count;
		rv = mfs_sleep(file_system,  100, p);

		stop_test_server(tracker_handle);
		CU_ASSERT_EQUAL_FATAL(APR_EGENERAL, rv);
		mfs_close_file_system(file_system);
	}
	//corrupt response...
	{
		char test_response[] = "sdfgERR 123 something\r\n";
	
		test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

		char tracker_list_str[] = "127.0.0.1:9991";
		tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

		CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

		char **paths;
		int path_count;
		rv = mfs_sleep(file_system,  100, p);

		stop_test_server(tracker_handle);
		CU_ASSERT_EQUAL_FATAL(APR_EFTYPE, rv);
		mfs_close_file_system(file_system);
	}
	apr_pool_destroy(p); 
}


void test_file_system_rename_ok() {
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();

	char test_response[] = "OK 123 some=thing\r\n";
	
	test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

	char tracker_list_str[] = "127.0.0.1:9991";
	tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

	char **paths;
	int path_count;
	rv = mfs_rename(file_system, "domain", "key", "to_key", p);

	stop_test_server(tracker_handle);
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, rv);
	mfs_close_file_system(file_system);
	apr_pool_destroy(p); 
}


void test_file_system_rename_fail() {
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();
	//error response...
	{
		char test_response[] = "ERR 123 something\r\n";
	
		test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

		char tracker_list_str[] = "127.0.0.1:9991";
		tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

		CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

		char **paths;
		int path_count;
		rv = mfs_rename(file_system, "domain", "key", "to_key", p);

		stop_test_server(tracker_handle);
		CU_ASSERT_EQUAL_FATAL(APR_EGENERAL, rv);
		mfs_close_file_system(file_system);
	}
	//corrupt response...
	{
		char test_response[] = "sdfgERR 123 something\r\n";
	
		test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

		char tracker_list_str[] = "127.0.0.1:9991";
		tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

		CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

		char **paths;
		int path_count;
		rv = mfs_rename(file_system, "domain", "key", "to_key", p);

		stop_test_server(tracker_handle);
		CU_ASSERT_EQUAL_FATAL(APR_EFTYPE, rv);
		mfs_close_file_system(file_system);
	}
	apr_pool_destroy(p); 
}

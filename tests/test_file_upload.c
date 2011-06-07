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
 
#include "test_file_upload.h"
#include "test_http_server.h"
#include "test_server.h"
#include "common.h"
#include "mogile_fs.h"
#include <apr_time.h>

void test_file_put_ok_bytes() {
	
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, NULL));

	char  original_uri[] = "http://127.0.0.1:8081/test/put";
	apr_uri_t *uri = apr_palloc(p, sizeof(apr_uri_t));
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS,apr_uri_parse (p, original_uri, uri));
		
	char data[] = "THIS IS THE PUT DATA";
	long dlen = strlen(data);
	test_http_server *handle = start_test_http_server(8081, "PUT DONE", 200, &test_http_server_ok_handler);
	CU_ASSERT_PTR_NOT_NULL_FATAL(handle);
	rv = mfs_file_server_put(file_system, uri, original_uri, data, &dlen, NULL, p);
	stop_test_http_server(handle);
	CU_ASSERT_STRING_EQUAL("PUT", apr_hash_get(handle->log, "METHOD", APR_HASH_KEY_STRING));
	CU_ASSERT_STRING_EQUAL("/test/put", apr_hash_get(handle->log, "URL", APR_HASH_KEY_STRING));
	size_t *len = apr_hash_get(handle->log, "DATA_LENGTH", APR_HASH_KEY_STRING);
	void *result_data = apr_hash_get(handle->log, "DATA", APR_HASH_KEY_STRING);
	CU_ASSERT_EQUAL_FATAL(strlen(data), *len);
	CU_ASSERT_NSTRING_EQUAL(data, result_data, *len);
	mfs_close_file_system(file_system);
	apr_pool_destroy(p); 
}

void test_file_put_ok_file() {
	
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, NULL));

	char  original_uri[] = "http://127.0.0.1:8081/test/put";
	apr_uri_t *uri = apr_palloc(p, sizeof(apr_uri_t));
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS,apr_uri_parse (p, original_uri, uri));
		
	char data[] = "THIS IS THE PUT DATA";
	long dlen = -1;

	apr_file_t *file;
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, apr_file_open(&file, "/tmp/mogilefs_test", APR_WRITE | APR_TRUNCATE | APR_CREATE, 0x775, p));

	apr_file_puts(data, file);	
	
	apr_file_close(file);

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, apr_file_open(&file, "/tmp/mogilefs_test", APR_READ, 0x775, p));
	
	 
	
	test_http_server *handle = start_test_http_server(8081, "PUT DONE", 200, test_http_server_ok_handler);
	CU_ASSERT_PTR_NOT_NULL_FATAL(handle);
	rv = mfs_file_server_put(file_system, uri, original_uri, NULL, &dlen, file, p);
	stop_test_http_server(handle);
	CU_ASSERT_STRING_EQUAL("PUT", apr_hash_get(handle->log, "METHOD", APR_HASH_KEY_STRING));
	CU_ASSERT_STRING_EQUAL("/test/put", apr_hash_get(handle->log, "URL", APR_HASH_KEY_STRING));
	size_t *len = apr_hash_get(handle->log, "DATA_LENGTH", APR_HASH_KEY_STRING);
	void *result_data = apr_hash_get(handle->log, "DATA", APR_HASH_KEY_STRING);
	CU_ASSERT_EQUAL_FATAL(strlen(data), *len);
	CU_ASSERT_NSTRING_EQUAL(data, result_data, *len);
	mfs_close_file_system(file_system);
	apr_pool_destroy(p); 
}


void test_file_put_ok_bytes_keepalive() {
	
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, NULL));

	char  original_uri[] = "http://127.0.0.1:8081/test/put";
	apr_uri_t *uri = apr_palloc(p, sizeof(apr_uri_t));
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS,apr_uri_parse (p, original_uri, uri));
		
	char data[] = "THIS IS THE PUT DATA";
	long dlen = strlen(data);
	
	test_http_server *handle = start_test_http_server(8081, "PUT DONE", 200, test_http_server_ok_handler);
	CU_ASSERT_PTR_NOT_NULL_FATAL(handle);
	
	rv = mfs_file_server_put(file_system, uri, original_uri, data, &dlen, NULL, p);
	
	CU_ASSERT_STRING_EQUAL("PUT", apr_hash_get(handle->log, "METHOD", APR_HASH_KEY_STRING));
	CU_ASSERT_STRING_EQUAL("/test/put", apr_hash_get(handle->log, "URL", APR_HASH_KEY_STRING));
	size_t *len = apr_hash_get(handle->log, "DATA_LENGTH", APR_HASH_KEY_STRING);
	void *result_data = apr_hash_get(handle->log, "DATA", APR_HASH_KEY_STRING);
	CU_ASSERT_EQUAL_FATAL(strlen(data), *len);
	CU_ASSERT_NSTRING_EQUAL(data, result_data, *len);

	mfs_file_server *file_server;
	mfs_http_connection *conn;
	long connect_count;
	
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_get_file_server(file_system, uri, &file_server));
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, apr_reslist_acquire(file_server->connections, (void**)&conn));

	curl_easy_getinfo(conn->curl, CURLINFO_NUM_CONNECTS, &connect_count);
	CU_ASSERT_EQUAL(1, connect_count);

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, apr_reslist_release(file_server->connections, conn));
	

	char data2[] = "THIS IS OTHER PUT DATA";
	long d2len = strlen(data2);
	rv = mfs_file_server_put(file_system, uri, original_uri, data2, &d2len, NULL, p);
	
	CU_ASSERT_STRING_EQUAL("PUT", apr_hash_get(handle->log, "METHOD", APR_HASH_KEY_STRING));
	CU_ASSERT_STRING_EQUAL("/test/put", apr_hash_get(handle->log, "URL", APR_HASH_KEY_STRING));
	len = apr_hash_get(handle->log, "DATA_LENGTH", APR_HASH_KEY_STRING);
	result_data = apr_hash_get(handle->log, "DATA", APR_HASH_KEY_STRING);
	CU_ASSERT_EQUAL_FATAL(strlen(data2), *len);
	CU_ASSERT_NSTRING_EQUAL(data2, result_data, *len);


	stop_test_http_server(handle);
	mfs_close_file_system(file_system);
	apr_pool_destroy(p); 
}


void test_file_put_fail_no_connect() {
	
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, NULL));

	char  original_uri[] = "http://127.0.0.1:8081/test/put";
	apr_uri_t *uri = apr_palloc(p, sizeof(apr_uri_t));
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS,apr_uri_parse (p, original_uri, uri));
		
	char data[] = "THIS IS THE PUT DATA";
	long dlen = strlen(data);
	
	rv = mfs_file_server_put(file_system, uri, original_uri, data, &dlen, NULL, p);
	CU_ASSERT_EQUAL(APR_EGENERAL, rv);
	mfs_close_file_system(file_system);
	apr_pool_destroy(p); 
}


void test_file_system_upload_bytes_ok() {
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();

	char test_response[] = "OK 123 fid=123&devid=456&path=http%3A%2F%2F127.0.0.1%3A8081%2Ftest%2Fput\r\n";
	//because we are using a basic server, the create_close call will fail first time, testing the retry loop (basic server only allows 1 call per connection)
	test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

	test_http_server *file_server_handle = start_test_http_server(8081, "PUT DONE", 200, test_http_server_ok_handler);
	
	char tracker_list_str[] = "127.0.0.1:9991";
	tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

	char data[] = "THIS IS THE PUT DATA";
	long dlen = strlen(data);
	rv = mfs_store_bytes(file_system, "some_domain", "some/storage/key", "some_storage_class", p, data, strlen(data));

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, rv);

	stop_test_server(tracker_handle);
	stop_test_http_server(file_server_handle);

	CU_ASSERT_STRING_EQUAL("PUT", apr_hash_get(file_server_handle->log, "METHOD", APR_HASH_KEY_STRING));
	CU_ASSERT_STRING_EQUAL("/test/put", apr_hash_get(file_server_handle->log, "URL", APR_HASH_KEY_STRING));
	size_t *len = apr_hash_get(file_server_handle->log, "DATA_LENGTH", APR_HASH_KEY_STRING);
	void *result_data = apr_hash_get(file_server_handle->log, "DATA", APR_HASH_KEY_STRING);

	CU_ASSERT_EQUAL_FATAL(strlen(data), *len);
	CU_ASSERT_NSTRING_EQUAL(data, result_data, *len);
	mfs_close_file_system(file_system);
	apr_pool_destroy(p); 
}


void test_file_system_upload_file_ok_no_pool() {
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();

	char test_response[] = "OK 123 fid=123&devid=456&path=http%3A%2F%2F127.0.0.1%3A8081%2Ftest%2Fput\r\n";
	//because we are using a basic server, the create_close call will fail first time, testing the retry loop (basic server only allows 1 call per connection)
	test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

	test_http_server *file_server_handle = start_test_http_server(8081, "PUT DONE", 200, test_http_server_ok_handler);
	
	char tracker_list_str[] = "127.0.0.1:9991";
	tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

	char data[] = "THIS IS THE PUT DATA";
	long dlen = strlen(data);

	apr_file_t *file;
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, apr_file_open(&file, "/tmp/mogilefs_test", APR_WRITE | APR_TRUNCATE | APR_CREATE, 0x775, p));

	apr_file_puts(data, file);	
	
	apr_file_close(file);

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, apr_file_open(&file, "/tmp/mogilefs_test", APR_READ, 0x775, p));
	
	rv = mfs_store_file(file_system, "some_domain", "some/storage/key", "some_storage_class", NULL, file);

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, rv);

	stop_test_server(tracker_handle);
	stop_test_http_server(file_server_handle);

	CU_ASSERT_STRING_EQUAL("PUT", apr_hash_get(file_server_handle->log, "METHOD", APR_HASH_KEY_STRING));
	CU_ASSERT_STRING_EQUAL("/test/put", apr_hash_get(file_server_handle->log, "URL", APR_HASH_KEY_STRING));
	size_t *len = apr_hash_get(file_server_handle->log, "DATA_LENGTH", APR_HASH_KEY_STRING);
	void *result_data = apr_hash_get(file_server_handle->log, "DATA", APR_HASH_KEY_STRING);

	CU_ASSERT_EQUAL_FATAL(strlen(data), *len);
	CU_ASSERT_NSTRING_EQUAL(data, result_data, *len);
	mfs_close_file_system(file_system);
	apr_pool_destroy(p); 
}

void test_file_system_upload_bytes_timeout() {
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();

	char test_response[] = "OK 123 fid=123&devid=456&path=http%3A%2F%2F127.0.0.1%3A8081%2Ftest%2Fput\r\n";
	//because we are using a basic server, the create_close call will fail first time, testing the retry loop (basic server only allows 1 call per connection)
	test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

	test_http_server *file_server_handle = start_test_http_server(8081, "PUT DONE", 200, test_http_server_timeout_handler);
	file_server_handle->sleep_duration = apr_time_from_sec(5);
	
	char tracker_list_str[] = "127.0.0.1:9991";
	tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

	char data[] = "THIS IS THE PUT DATA";
	long dlen = strlen(data);
	file_system->file_server_timeout = 500000; //500 ms
	rv = mfs_store_bytes(file_system, "some_domain", "some/storage/key", "some_storage_class", p, data, strlen(data));

	CU_ASSERT_NOT_EQUAL_FATAL(APR_SUCCESS, rv);

	stop_test_server(tracker_handle);
	stop_test_http_server(file_server_handle);
	mfs_close_file_system(file_system);
	apr_pool_destroy(p); 
}


void test_file_system_upload_bytes_corrupt_tracker() {
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();

	//missing fid
	{
		char test_response[] = "OK 123 fidx=123&devid=456&path=http%3A%2F%2F127.0.0.1%3A8081%2Ftest%2Fput\r\n";
		//because we are using a basic server, the create_close call will fail first time, testing the retry loop (basic server only allows 1 call per connection)
		test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

		char tracker_list_str[] = "127.0.0.1:9991";
		tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

		CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

		char data[] = "THIS IS THE PUT DATA";
		long dlen = strlen(data);
		rv = mfs_store_bytes(file_system, "some_domain", "some/storage/key", "some_storage_class", p, data, strlen(data));

		CU_ASSERT_NOT_EQUAL(APR_SUCCESS, rv);

		stop_test_server(tracker_handle);
		mfs_close_file_system(file_system);
	}

	//missing devid
	{
		char test_response[] = "OK 123 fid=123&devidx=456&path=http%3A%2F%2F127.0.0.1%3A8081%2Ftest%2Fput\r\n";
		//because we are using a basic server, the create_close call will fail first time, testing the retry loop (basic server only allows 1 call per connection)
		test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

		char tracker_list_str[] = "127.0.0.1:9991";
		tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

		CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

		char data[] = "THIS IS THE PUT DATA";
		long dlen = strlen(data);
		rv = mfs_store_bytes(file_system, "some_domain", "some/storage/key", "some_storage_class", p, data, strlen(data));

		CU_ASSERT_NOT_EQUAL(APR_SUCCESS, rv);

		stop_test_server(tracker_handle);
		mfs_close_file_system(file_system);
	}

	//missing path
	{
		char test_response[] = "OK 123 fid=123&devid=456&pathx=http%3A%2F%2F127.0.0.1%3A8081%2Ftest%2Fput\r\n";
		//because we are using a basic server, the create_close call will fail first time, testing the retry loop (basic server only allows 1 call per connection)
		test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

		char tracker_list_str[] = "127.0.0.1:9991";
		tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

		CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

		char data[] = "THIS IS THE PUT DATA";
		long dlen = strlen(data);
		rv = mfs_store_bytes(file_system, "some_domain", "some/storage/key", "some_storage_class", p, data, strlen(data));

		CU_ASSERT_NOT_EQUAL(APR_SUCCESS, rv);

		stop_test_server(tracker_handle);
		mfs_close_file_system(file_system);
	}

	//invalid path
	{
		char test_response[] = "OK 123 fid=123&devid=456&path=http%3A%2F127.0.0.1%3A8081%2Ftest%2Fput\r\n";
		//because we are using a basic server, the create_close call will fail first time, testing the retry loop (basic server only allows 1 call per connection)
		test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

		char tracker_list_str[] = "127.0.0.1:9991";
		tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

		CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

		char data[] = "THIS IS THE PUT DATA";
		long dlen = strlen(data);
		rv = mfs_store_bytes(file_system, "some_domain", "some/storage/key", "some_storage_class", p, data, strlen(data));

		CU_ASSERT_NOT_EQUAL(APR_SUCCESS, rv);

		stop_test_server(tracker_handle);
		mfs_close_file_system(file_system);
	}

	//more invalid path
	{
		char test_response[] = "OK 123 fid=123&devid=456&path=asfasdfasdf\r\n";
		//because we are using a basic server, the create_close call will fail first time, testing the retry loop (basic server only allows 1 call per connection)
		test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

		char tracker_list_str[] = "127.0.0.1:9991";
		tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

		CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

		char data[] = "THIS IS THE PUT DATA";
		long dlen = strlen(data);
		rv = mfs_store_bytes(file_system, "some_domain", "some/storage/key", "some_storage_class", p, data, strlen(data));

		CU_ASSERT_NOT_EQUAL(APR_SUCCESS, rv);

		stop_test_server(tracker_handle);
		mfs_close_file_system(file_system);
	}

	//tracker returns garbage
	{
		char test_response[] = "sdf46werg sgfdhsdy54s drtse\r\n";
		//because we are using a basic server, the create_close call will fail first time, testing the retry loop (basic server only allows 1 call per connection)
		test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

		char tracker_list_str[] = "127.0.0.1:9991";
		tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

		CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

		char data[] = "THIS IS THE PUT DATA";
		long dlen = strlen(data);
		rv = mfs_store_bytes(file_system, "some_domain", "some/storage/key", "some_storage_class", p, data, strlen(data));

		CU_ASSERT_NOT_EQUAL(APR_SUCCESS, rv);

		stop_test_server(tracker_handle);
		mfs_close_file_system(file_system);
	}

	//tracker errors
	{
		char test_response[] = "ERR SOME_ERROR Test Error\r\n";
		//because we are using a basic server, the create_close call will fail first time, testing the retry loop (basic server only allows 1 call per connection)
		test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

		char tracker_list_str[] = "127.0.0.1:9991";
		tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

		CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

		char data[] = "THIS IS THE PUT DATA";
		long dlen = strlen(data);
		rv = mfs_store_bytes(file_system, "some_domain", "some/storage/key", "some_storage_class", p, data, strlen(data));

		CU_ASSERT_NOT_EQUAL(APR_SUCCESS, rv);

		stop_test_server(tracker_handle);
		mfs_close_file_system(file_system);
	}
	
	apr_pool_destroy(p); 

}
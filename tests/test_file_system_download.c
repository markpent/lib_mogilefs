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
 
#include "test_file_system_download.h"
#include "test_http_server.h"
#include "test_server.h"
#include "common.h"
#include "mogile_fs.h"
#include <apr_time.h>

void test_file_system_get_ok_bytes() {
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();

	char test_response[] = "OK 123 paths=1&path1=http%3A%2F%2F127.0.0.1%3A8081%2Fpath%2Fone\r\n";
	
	test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

	char tracker_list_str[] = "127.0.0.1:9991";
	tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

	
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

	char data[] ="THIS IS THE GET DATA";
	test_http_server *handle = start_test_http_server(8081, data, 200, &test_http_server_ok_handler);
	CU_ASSERT_PTR_NOT_NULL_FATAL(handle);

	
	void *bytes;
	apr_size_t total_bytes;
	apr_file_t *file = NULL;

	rv = mfs_get_file_or_bytes(file_system, "domain", "key", &total_bytes, &bytes, &file, p, NULL);

	stop_test_http_server(handle);
	stop_test_server(tracker_handle);

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, rv);
	
	CU_ASSERT_STRING_EQUAL("GET", apr_hash_get(handle->log, "METHOD", APR_HASH_KEY_STRING));
	CU_ASSERT_STRING_EQUAL("/path/one", apr_hash_get(handle->log, "URL", APR_HASH_KEY_STRING));
	
	CU_ASSERT_EQUAL_FATAL(strlen(data), total_bytes);
	CU_ASSERT_NSTRING_EQUAL(data, bytes, total_bytes);
	mfs_close_file_system(file_system);
	apr_pool_destroy(p); 
}

void test_file_system_get_ok_file() {
	
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();

	char test_response[] = "OK 123 paths=1&path1=http%3A%2F%2F127.0.0.1%3A8081%2Fpath%2Fone\r\n";
	
	test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

	char tracker_list_str[] = "127.0.0.1:9991";
	tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

	
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

	char data[] ="THIS IS THE GET DATA";
	test_http_server *handle = start_test_http_server(8081, data, 200, &test_http_server_ok_handler);
	CU_ASSERT_PTR_NOT_NULL_FATAL(handle);
	

	void *bytes;
	apr_size_t total_bytes;
	apr_file_t *file = NULL;

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS,apr_file_open(&file, "/tmp/mogfilefs_test_download", APR_READ | APR_WRITE | APR_CREATE | APR_TRUNCATE, APR_OS_DEFAULT, p));
	
	rv = mfs_get_file(file_system, "domain", "key", &total_bytes, &file, p);

	
	stop_test_http_server(handle);
	stop_test_server(tracker_handle);

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, rv);
	CU_ASSERT_STRING_EQUAL("GET", apr_hash_get(handle->log, "METHOD", APR_HASH_KEY_STRING));
	CU_ASSERT_STRING_EQUAL("/path/one", apr_hash_get(handle->log, "URL", APR_HASH_KEY_STRING));

	char tmp[100];
	apr_size_t tmp_len=100;
	
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, apr_file_read(file, tmp, &tmp_len));
	
	
	CU_ASSERT_EQUAL_FATAL(strlen(data), total_bytes);
	CU_ASSERT_EQUAL_FATAL(strlen(data), tmp_len);
	CU_ASSERT_NSTRING_EQUAL(data, tmp, total_bytes);
	mfs_close_file_system(file_system);
	apr_pool_destroy(p); 
}


void test_file_system_get_ok_either() {

	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();

	char test_response[] = "OK 123 paths=1&path1=http%3A%2F%2F127.0.0.1%3A8081%2Fpath%2Fone\r\n";
	
	test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

	char tracker_list_str[] = "127.0.0.1:9991";
	tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

	
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

	char data[] ="THIS IS THE GET DATA";
	test_http_server *handle = start_test_http_server(8081, data, 200, &test_http_server_ok_handler);
	CU_ASSERT_PTR_NOT_NULL_FATAL(handle);
	

	file_system->max_buffer_size = 10; //this will force it to use file
	
	void *bytes;
	apr_size_t total_bytes;
	apr_file_t *file = NULL;

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS,apr_file_open(&file, "/tmp/mogfilefs_test_download", APR_READ | APR_WRITE | APR_CREATE | APR_TRUNCATE, APR_OS_DEFAULT, p));
	
	rv = mfs_get_file_or_bytes(file_system, "domain", "key", &total_bytes, &bytes, &file, p, NULL);

	
	stop_test_http_server(handle);
	stop_test_server(tracker_handle);

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, rv);
	CU_ASSERT_STRING_EQUAL("GET", apr_hash_get(handle->log, "METHOD", APR_HASH_KEY_STRING));
	CU_ASSERT_STRING_EQUAL("/path/one", apr_hash_get(handle->log, "URL", APR_HASH_KEY_STRING));

	char tmp[100];
	apr_size_t tmp_len=100;

	CU_ASSERT_PTR_NOT_NULL_FATAL(file);
	
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, apr_file_read(file, tmp, &tmp_len));
	
	
	CU_ASSERT_EQUAL_FATAL(strlen(data), total_bytes);
	CU_ASSERT_EQUAL_FATAL(strlen(data), tmp_len);
	CU_ASSERT_NSTRING_EQUAL(data, tmp, total_bytes);
	mfs_close_file_system(file_system);
	apr_pool_destroy(p); 
}

void test_file_system_get_ok_brigade_large() {
	
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();

	char test_response[] = "OK 123 paths=1&path1=http%3A%2F%2F127.0.0.1%3A8081%2Fpath%2Fone\r\n";
	
	test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

	char tracker_list_str[] = "127.0.0.1:9991";
	tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

	
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

	apr_size_t buf_len = 1024 * 1024 * 10; //10 MB
	char *big_buffer = apr_palloc(p, buf_len + 1); //10 MB
	int i;
	for(i=0; i < buf_len; i++) {
		big_buffer[i] = 'A';
	}
	big_buffer[buf_len] = '\0';
	
	
	test_http_server *handle = start_test_http_server(8081, big_buffer, 200, &test_http_server_ok_handler);
	CU_ASSERT_PTR_NOT_NULL_FATAL(handle);

	void *bytes;
	apr_size_t total_bytes;
	apr_file_t *file = NULL;

	apr_bucket_brigade *brigade = apr_brigade_create(p, apr_bucket_alloc_create(p));
	
	rv = mfs_get_brigade(file_system, "domain", "key", &total_bytes, brigade, p);
	
	stop_test_http_server(handle);
	stop_test_server(tracker_handle);

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, rv);
	CU_ASSERT_STRING_EQUAL("GET", apr_hash_get(handle->log, "METHOD", APR_HASH_KEY_STRING));
	CU_ASSERT_STRING_EQUAL("/path/one", apr_hash_get(handle->log, "URL", APR_HASH_KEY_STRING));


	apr_size_t test_len;
	char *test_buffer;

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, apr_brigade_pflatten(brigade, &test_buffer, &test_len, p));
	
	
	CU_ASSERT_EQUAL_FATAL(buf_len, total_bytes);
	CU_ASSERT_EQUAL_FATAL(buf_len, test_len);
	CU_ASSERT_NSTRING_EQUAL(big_buffer, test_buffer, total_bytes);
	mfs_close_file_system(file_system);
	apr_pool_destroy(p); 
}


void test_file_system_get_failover_bytes() {

	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();

	char test_response[] = "OK 123 paths=2&path1=http%3A%2F%2F127.0.0.1%3A8081%2Fpath%2Fone&path2=http%3A%2F%2F127.0.0.1%3A8082%2Fpath%2Ftwo\r\n";
	
	test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

	char tracker_list_str[] = "127.0.0.1:9991";
	tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

	
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

	char data[] ="THIS IS THE GET DATA";
	test_http_server *handle = start_test_http_server(8082, data, 200, &test_http_server_ok_handler);
	CU_ASSERT_PTR_NOT_NULL_FATAL(handle);

	
	void *bytes;
	apr_size_t total_bytes;
	apr_file_t *file = NULL;

	rv = mfs_get_file_or_bytes(file_system, "domain", "key", &total_bytes, &bytes, &file, p, NULL);

	stop_test_http_server(handle);
	stop_test_server(tracker_handle);

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, rv);
	
	CU_ASSERT_STRING_EQUAL("GET", apr_hash_get(handle->log, "METHOD", APR_HASH_KEY_STRING));
	CU_ASSERT_STRING_EQUAL("/path/two", apr_hash_get(handle->log, "URL", APR_HASH_KEY_STRING));
	
	CU_ASSERT_EQUAL_FATAL(strlen(data), total_bytes);
	CU_ASSERT_NSTRING_EQUAL(data, bytes, total_bytes);
	mfs_close_file_system(file_system);
	apr_pool_destroy(p); 
}


void test_file_system_get_failover_file() {
	
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();

	char test_response[] = "OK 123 paths=2&path1=http%3A%2F%2F127.0.0.1%3A8081%2Fpath%2Fone&path2=http%3A%2F%2F127.0.0.1%3A8082%2Fpath%2Ftwo\r\n";
	
	test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

	char tracker_list_str[] = "127.0.0.1:9991";
	tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

	
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

	apr_size_t buf_len = 1024 * 1024 * 10; //10 MB
	char *big_buffer = apr_palloc(p, buf_len + 1); //10 MB
	int i;
	for(i=0; i < buf_len; i++) {
		big_buffer[i] = 'A';
	}
	big_buffer[buf_len] = '\0';
	
	
	test_http_server *handle1 = start_test_http_server(8081, big_buffer, 200, &test_http_server_send_half_handler);
	CU_ASSERT_PTR_NOT_NULL_FATAL(handle1);

	test_http_server *handle2 = start_test_http_server(8082, big_buffer, 200, &test_http_server_ok_handler);
	CU_ASSERT_PTR_NOT_NULL_FATAL(handle2);


	

	void *bytes;
	apr_size_t total_bytes;
	apr_file_t *file = NULL;

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS,apr_file_open(&file, "/tmp/mogfilefs_test_download", APR_READ | APR_WRITE | APR_CREATE | APR_TRUNCATE, APR_OS_DEFAULT, p));
	
	rv = mfs_get_file(file_system, "domain", "key", &total_bytes, &file, p);

	
	stop_test_http_server(handle1);
	stop_test_http_server(handle2);
	stop_test_server(tracker_handle);

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, rv);
	CU_ASSERT_STRING_EQUAL("GET", apr_hash_get(handle2->log, "METHOD", APR_HASH_KEY_STRING));
	CU_ASSERT_STRING_EQUAL("/path/two", apr_hash_get(handle2->log, "URL", APR_HASH_KEY_STRING));

	char *tmp = apr_palloc(p, buf_len + 100); //10 MB
	apr_size_t tmp_len=buf_len + 100;
	
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, apr_file_read(file, tmp, &tmp_len));
	
	
	CU_ASSERT_EQUAL_FATAL(buf_len, total_bytes);
	CU_ASSERT_EQUAL_FATAL(buf_len, tmp_len);
	CU_ASSERT_NSTRING_EQUAL(big_buffer, tmp, total_bytes);
	mfs_close_file_system(file_system);
	apr_pool_destroy(p); 
}


void test_file_system_get_failover_brigade() {
	
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();

	char test_response[] = "OK 123 paths=2&path1=http%3A%2F%2F127.0.0.1%3A8081%2Fpath%2Fone&path2=http%3A%2F%2F127.0.0.1%3A8082%2Fpath%2Ftwo\r\n";
	
	test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

	char tracker_list_str[] = "127.0.0.1:9991";
	tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

	
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

	apr_size_t buf_len = 1024 * 1024 * 10; //10 MB
	char *big_buffer = apr_palloc(p, buf_len + 1); //10 MB
	int i;
	for(i=0; i < buf_len; i++) {
		big_buffer[i] = 'A';
	}
	big_buffer[buf_len] = '\0';
	
	
	test_http_server *handle1 = start_test_http_server(8081, big_buffer, 200, &test_http_server_send_half_handler);
	CU_ASSERT_PTR_NOT_NULL_FATAL(handle1);

	test_http_server *handle2 = start_test_http_server(8082, big_buffer, 200, &test_http_server_ok_handler);
	CU_ASSERT_PTR_NOT_NULL_FATAL(handle2);


	

	void *bytes;
	apr_size_t total_bytes;
	apr_file_t *file = NULL;

	apr_bucket_brigade *brigade = apr_brigade_create(p, apr_bucket_alloc_create(p));
	
	rv = mfs_get_brigade(file_system, "domain", "key", &total_bytes, brigade, p);
	
	stop_test_http_server(handle1);
	stop_test_http_server(handle2);
	stop_test_server(tracker_handle);

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, rv);
	CU_ASSERT_STRING_EQUAL("GET", apr_hash_get(handle2->log, "METHOD", APR_HASH_KEY_STRING));
	CU_ASSERT_STRING_EQUAL("/path/two", apr_hash_get(handle2->log, "URL", APR_HASH_KEY_STRING));


	apr_size_t test_len;
	char *test_buffer;

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, apr_brigade_pflatten(brigade, &test_buffer, &test_len, p));
	
	
	CU_ASSERT_EQUAL_FATAL(buf_len, total_bytes);
	CU_ASSERT_EQUAL_FATAL(buf_len, test_len);
	CU_ASSERT_NSTRING_EQUAL(big_buffer, test_buffer, total_bytes);
	mfs_close_file_system(file_system);
	apr_pool_destroy(p);
}


void test_file_system_get_fail_bytes() {
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();

	char test_response[] = "OK 123 paths=2&path1=http%3A%2F%2F127.0.0.1%3A8081%2Fpath%2Fone&path2=http%3A%2F%2F127.0.0.1%3A8082%2Fpath%2Ftwo\r\n";
	
	test_server_handle * tracker_handle = test_start_basic_server(test_response, 9991, p);

	char tracker_list_str[] = "127.0.0.1:9991";
	tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

	
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));


	
	void *bytes;
	apr_size_t total_bytes;
	apr_file_t *file = NULL;

	rv = mfs_get_file_or_bytes(file_system, "domain", "key", &total_bytes, &bytes, &file, p, NULL);

	stop_test_server(tracker_handle);

	CU_ASSERT_EQUAL_FATAL(APR_EGENERAL, rv);
	mfs_close_file_system(file_system);
	apr_pool_destroy(p); 
}
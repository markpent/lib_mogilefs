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
 
#include "test_file_download.h"
#include "test_http_server.h"
#include "test_server.h"
#include "common.h"
#include "mogile_fs.h"
#include <apr_time.h>

void test_file_get_ok_bytes() {
	
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, NULL));

	char  original_uri[] = "http://127.0.0.1:8081/test/get";
	apr_uri_t *uri = apr_palloc(p, sizeof(apr_uri_t));
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS,apr_uri_parse (p, original_uri, uri));
	char data[] ="THIS IS THE GET DATA";
	test_http_server *handle = start_test_http_server(8081, data, 200, &test_http_server_ok_handler);
	CU_ASSERT_PTR_NOT_NULL_FATAL(handle);

	void *bytes;
	apr_size_t total_bytes;
	apr_file_t *file = NULL;
	
	rv = mfs_file_server_get(file_system, uri, original_uri, &bytes, &total_bytes, &file, NULL, p, NULL);
	stop_test_http_server(handle);
	CU_ASSERT_STRING_EQUAL("GET", apr_hash_get(handle->log, "METHOD", APR_HASH_KEY_STRING));
	CU_ASSERT_STRING_EQUAL("/test/get", apr_hash_get(handle->log, "URL", APR_HASH_KEY_STRING));
	
	CU_ASSERT_EQUAL_FATAL(strlen(data), total_bytes);
	CU_ASSERT_NSTRING_EQUAL(data, bytes, total_bytes);
	mfs_close_file_system(file_system);
	apr_pool_destroy(p); 
}

void test_file_get_ok_file() {
	
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, NULL));

	char  original_uri[] = "http://127.0.0.1:8081/test/get";
	apr_uri_t *uri = apr_palloc(p, sizeof(apr_uri_t));
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS,apr_uri_parse (p, original_uri, uri));
	char data[] ="THIS IS THE GET DATA";
	test_http_server *handle = start_test_http_server(8081, data, 200, &test_http_server_ok_handler);
	CU_ASSERT_PTR_NOT_NULL_FATAL(handle);

	void *bytes;
	apr_size_t total_bytes;
	apr_file_t *file = NULL;

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS,apr_file_open(&file, "/tmp/mogfilefs_test_download", APR_READ | APR_WRITE | APR_CREATE | APR_TRUNCATE, APR_OS_DEFAULT, p));
	
	rv = mfs_file_server_get(file_system, uri, original_uri, &bytes, &total_bytes, &file, NULL, p, NULL);
	stop_test_http_server(handle);
	CU_ASSERT_STRING_EQUAL("GET", apr_hash_get(handle->log, "METHOD", APR_HASH_KEY_STRING));
	CU_ASSERT_STRING_EQUAL("/test/get", apr_hash_get(handle->log, "URL", APR_HASH_KEY_STRING));

	char tmp[100];
	apr_size_t tmp_len=100;
	
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, apr_file_read(file, tmp, &tmp_len));
	
	
	CU_ASSERT_EQUAL_FATAL(strlen(data), total_bytes);
	CU_ASSERT_EQUAL_FATAL(strlen(data), tmp_len);
	CU_ASSERT_NSTRING_EQUAL(data, tmp, total_bytes);
	mfs_close_file_system(file_system);
	apr_pool_destroy(p); 
}

void test_file_get_ok_either() {
	
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, NULL));

	file_system->max_buffer_size = 10; //this will force it to use file

	char  original_uri[] = "http://127.0.0.1:8081/test/get";
	apr_uri_t *uri = apr_palloc(p, sizeof(apr_uri_t));
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS,apr_uri_parse (p, original_uri, uri));
	char data[] ="THIS IS THE GET DATA";
	test_http_server *handle = start_test_http_server(8081, data, 200, &test_http_server_ok_handler);
	CU_ASSERT_PTR_NOT_NULL_FATAL(handle);

	void *bytes;
	apr_size_t total_bytes;
	apr_file_t *file = NULL;
	
	rv = mfs_file_server_get(file_system, uri, original_uri, &bytes, &total_bytes, &file, NULL, p, NULL);
	stop_test_http_server(handle);
	CU_ASSERT_STRING_EQUAL("GET", apr_hash_get(handle->log, "METHOD", APR_HASH_KEY_STRING));
	CU_ASSERT_STRING_EQUAL("/test/get", apr_hash_get(handle->log, "URL", APR_HASH_KEY_STRING));

	CU_ASSERT_PTR_NOT_NULL_FATAL(file);
	
	char tmp[100];
	apr_size_t tmp_len=100;
	
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, apr_file_read(file, tmp, &tmp_len));
	
	
	CU_ASSERT_EQUAL_FATAL(strlen(data), total_bytes);
	CU_ASSERT_EQUAL_FATAL(strlen(data), tmp_len);
	CU_ASSERT_NSTRING_EQUAL(data, tmp, total_bytes);
	mfs_close_file_system(file_system);
	apr_pool_destroy(p); 
}


void test_file_get_ok_either_large() {
	
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, NULL));


	char  original_uri[] = "http://127.0.0.1:8081/test/get";
	apr_uri_t *uri = apr_palloc(p, sizeof(apr_uri_t));
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS,apr_uri_parse (p, original_uri, uri));

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
	
	rv = mfs_file_server_get(file_system, uri, original_uri, &bytes, &total_bytes, &file, NULL, p, NULL);
	stop_test_http_server(handle);
	CU_ASSERT_STRING_EQUAL("GET", apr_hash_get(handle->log, "METHOD", APR_HASH_KEY_STRING));
	CU_ASSERT_STRING_EQUAL("/test/get", apr_hash_get(handle->log, "URL", APR_HASH_KEY_STRING));

	CU_ASSERT_PTR_NOT_NULL_FATAL(file);

	apr_size_t test_len=buf_len + 100;
	char *test_buffer = apr_palloc(p, test_len);
	
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, apr_file_read(file, test_buffer, &test_len));
	
	
	CU_ASSERT_EQUAL_FATAL(buf_len, total_bytes);
	CU_ASSERT_EQUAL_FATAL(buf_len, test_len);
	CU_ASSERT_NSTRING_EQUAL(big_buffer, test_buffer, total_bytes);
	mfs_close_file_system(file_system);
	apr_pool_destroy(p); 
}


void test_file_get_ok_brigade_large() {
	
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, NULL));


	char  original_uri[] = "http://127.0.0.1:8081/test/get";
	apr_uri_t *uri = apr_palloc(p, sizeof(apr_uri_t));
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS,apr_uri_parse (p, original_uri, uri));

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
	
	rv = mfs_file_server_get(file_system, uri, original_uri, &bytes, &total_bytes, &file, brigade, p, NULL);
	stop_test_http_server(handle);
	CU_ASSERT_STRING_EQUAL("GET", apr_hash_get(handle->log, "METHOD", APR_HASH_KEY_STRING));
	CU_ASSERT_STRING_EQUAL("/test/get", apr_hash_get(handle->log, "URL", APR_HASH_KEY_STRING));


	apr_size_t test_len;
	char *test_buffer;

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, apr_brigade_pflatten(brigade, &test_buffer, &test_len, p));
	
	
	CU_ASSERT_EQUAL_FATAL(buf_len, total_bytes);
	CU_ASSERT_EQUAL_FATAL(buf_len, test_len);
	CU_ASSERT_NSTRING_EQUAL(big_buffer, test_buffer, total_bytes);
	mfs_close_file_system(file_system);
	apr_pool_destroy(p); 
}

void test_file_get_fail_brigade_large() {
	
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, NULL));


	char  original_uri[] = "http://127.0.0.1:8081/test/get";
	apr_uri_t *uri = apr_palloc(p, sizeof(apr_uri_t));
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS,apr_uri_parse (p, original_uri, uri));

	apr_size_t buf_len = 1024 * 1024 * 10; //10 MB
	char *big_buffer = apr_palloc(p, buf_len + 1); //10 MB
	int i;
	for(i=0; i < buf_len; i++) {
		big_buffer[i] = 'A';
	}
	big_buffer[buf_len] = '\0';
	
	
	test_http_server *handle = start_test_http_server(8081, big_buffer, 200, &test_http_server_send_half_handler);
	CU_ASSERT_PTR_NOT_NULL_FATAL(handle);

	void *bytes;
	apr_size_t total_bytes;
	apr_file_t *file = NULL;

	apr_bucket_brigade *brigade = apr_brigade_create(p, apr_bucket_alloc_create(p));
	
	rv = mfs_file_server_get(file_system, uri, original_uri, &bytes, &total_bytes, &file, brigade, p, NULL);
	printf("back");
	stop_test_http_server(handle);
	CU_ASSERT_STRING_EQUAL("GET", apr_hash_get(handle->log, "METHOD", APR_HASH_KEY_STRING));
	CU_ASSERT_STRING_EQUAL("/test/get", apr_hash_get(handle->log, "URL", APR_HASH_KEY_STRING));

	CU_ASSERT_EQUAL_FATAL(APR_EGENERAL,rv);
	
	apr_size_t test_len;
	char *test_buffer;

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, apr_brigade_pflatten(brigade, &test_buffer, &test_len, p));
	
	//make sure the brigade was cleared out
	CU_ASSERT_EQUAL_FATAL(0, test_len);
	CU_ASSERT_NSTRING_EQUAL("", test_buffer, test_len);
	mfs_close_file_system(file_system);
	apr_pool_destroy(p); 
}


void test_file_get_fail_brigade_large_with_data() {
	
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, NULL));


	char  original_uri[] = "http://127.0.0.1:8081/test/get";
	apr_uri_t *uri = apr_palloc(p, sizeof(apr_uri_t));
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS,apr_uri_parse (p, original_uri, uri));

	apr_size_t buf_len = 1024 * 1024 * 10; //10 MB
	char *big_buffer = apr_palloc(p, buf_len + 1); //10 MB
	int i;
	for(i=0; i < buf_len; i++) {
		big_buffer[i] = 'A';
	}
	big_buffer[buf_len] = '\0';
	
	
	test_http_server *handle = start_test_http_server(8081, big_buffer, 200, &test_http_server_send_half_handler);
	CU_ASSERT_PTR_NOT_NULL_FATAL(handle);

	void *bytes;
	apr_size_t total_bytes;
	apr_file_t *file = NULL;

	apr_bucket_brigade *brigade = apr_brigade_create(p, apr_bucket_alloc_create(p));

	apr_bucket * bucket = apr_bucket_transient_create("TEST", 4, brigade->bucket_alloc);
	APR_BUCKET_INSERT_AFTER(APR_BRIGADE_LAST(brigade), bucket);
	
	rv = mfs_file_server_get(file_system, uri, original_uri, &bytes, &total_bytes, &file, brigade, p, NULL);
	stop_test_http_server(handle);
	CU_ASSERT_STRING_EQUAL("GET", apr_hash_get(handle->log, "METHOD", APR_HASH_KEY_STRING));
	CU_ASSERT_STRING_EQUAL("/test/get", apr_hash_get(handle->log, "URL", APR_HASH_KEY_STRING));

	CU_ASSERT_EQUAL_FATAL(APR_EGENERAL,rv);
	
	apr_size_t test_len;
	char *test_buffer;

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, apr_brigade_pflatten(brigade, &test_buffer, &test_len, p));
	
	//make sure the brigade was cleared out
	CU_ASSERT_EQUAL_FATAL(4, test_len);
	CU_ASSERT_NSTRING_EQUAL("TEST", test_buffer, test_len);
	mfs_close_file_system(file_system);
	apr_pool_destroy(p); 
}


void test_file_get_timeout_bytes() {
	
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, NULL));
	file_system->file_server_timeout = apr_time_from_sec(1);

	char  original_uri[] = "http://127.0.0.1:8081/test/get";
	apr_uri_t *uri = apr_palloc(p, sizeof(apr_uri_t));
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS,apr_uri_parse (p, original_uri, uri));
	char data[] ="THIS IS THE GET DATA";
	test_http_server *handle = start_test_http_server(8081, data, 200, &test_http_server_timeout_handler);
	CU_ASSERT_PTR_NOT_NULL_FATAL(handle);
	handle->sleep_duration = apr_time_from_sec(10);

	void *bytes;
	apr_size_t total_bytes;
	apr_file_t *file = NULL;
	
	rv = mfs_file_server_get(file_system, uri, original_uri, &bytes, &total_bytes, &file, NULL, p, NULL);
	stop_test_http_server(handle);
	CU_ASSERT_STRING_EQUAL("GET", apr_hash_get(handle->log, "METHOD", APR_HASH_KEY_STRING));
	CU_ASSERT_STRING_EQUAL("/test/get", apr_hash_get(handle->log, "URL", APR_HASH_KEY_STRING));
	
	CU_ASSERT_EQUAL_FATAL(APR_EGENERAL,rv);
	mfs_close_file_system(file_system);
	apr_pool_destroy(p); 
}


void test_file_get_timeout_brigade_large_with_data() {
	
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, NULL));


	char  original_uri[] = "http://127.0.0.1:8081/test/get";
	apr_uri_t *uri = apr_palloc(p, sizeof(apr_uri_t));
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS,apr_uri_parse (p, original_uri, uri));

	apr_size_t buf_len = 1024 * 1024 * 10; //10 MB
	char *big_buffer = apr_palloc(p, buf_len + 1); //10 MB
	int i;
	for(i=0; i < buf_len; i++) {
		big_buffer[i] = 'A';
	}
	big_buffer[buf_len] = '\0';
	
	
	test_http_server *handle = start_test_http_server(8081, big_buffer, 200, &test_http_server_send_half_handler);
	CU_ASSERT_PTR_NOT_NULL_FATAL(handle);
	handle->sleep_duration = apr_time_from_sec(3); //this will cause a pause halfway of 3 seconds instead of a failure...

	void *bytes;
	apr_size_t total_bytes;
	apr_file_t *file = NULL;

	apr_bucket_brigade *brigade = apr_brigade_create(p, apr_bucket_alloc_create(p));

	apr_bucket * bucket = apr_bucket_transient_create("TEST", 4, brigade->bucket_alloc);
	APR_BUCKET_INSERT_AFTER(APR_BRIGADE_LAST(brigade), bucket);
	
	rv = mfs_file_server_get(file_system, uri, original_uri, &bytes, &total_bytes, &file, brigade, p, NULL);
	stop_test_http_server(handle);
	CU_ASSERT_STRING_EQUAL("GET", apr_hash_get(handle->log, "METHOD", APR_HASH_KEY_STRING));
	CU_ASSERT_STRING_EQUAL("/test/get", apr_hash_get(handle->log, "URL", APR_HASH_KEY_STRING));

	CU_ASSERT_EQUAL_FATAL(APR_EGENERAL,rv);
	
	apr_size_t test_len;
	char *test_buffer;

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, apr_brigade_pflatten(brigade, &test_buffer, &test_len, p));
	
	//make sure the brigade was cleared out
	CU_ASSERT_EQUAL_FATAL(4, test_len);
	CU_ASSERT_NSTRING_EQUAL("TEST", test_buffer, test_len);
	mfs_close_file_system(file_system);
	apr_pool_destroy(p); 
}




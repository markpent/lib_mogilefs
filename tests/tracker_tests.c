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
#include "tracker_tests.h"
#include "common.h"
#include <apr_pools.h>
#include <apr_strings.h>
#include "test_server.h"
#include <assert.h>

#define TEST_PORT 28976


void test_tracker_encoding() {
	apr_pool_t *p = mfs_test_get_pool();
	int length;
	char *result = mfs_tracker_url_encode("basic", p, &length);
	CU_ASSERT_EQUAL(length, 5);
	CU_ASSERT_STRING_EQUAL(result, "basic");

	result = mfs_tracker_url_encode("a space", p, &length);
	CU_ASSERT_EQUAL(length, 7);
	CU_ASSERT_STRING_EQUAL(result, "a+space");

	result = mfs_tracker_url_encode("amp&=", p, &length);
	CU_ASSERT_EQUAL(length, 9);
	CU_ASSERT_STRING_EQUAL(result, "amp%26%3d");
	
	apr_pool_destroy(p);
}

void test_request_building() {
	apr_pool_t *p = mfs_test_get_pool();
	tracker_request_parameters * params = mfs_tracker_init_parameters(p);

	mfs_tracker_add_parameter(params, "key",  "value", p);
	apr_size_t length;
	char * result = mfs_tracker_build_request("TEST", params, p, &length);
	CU_ASSERT_EQUAL(length, 16);
	CU_ASSERT_STRING_EQUAL(result, "TEST key=value\r\n");

	mfs_tracker_add_parameter(params, "key2",  "value2", p);
	result = mfs_tracker_build_request("TEST", params, p, &length);
	CU_ASSERT_EQUAL(length, 28);
	CU_ASSERT_STRING_EQUAL(result, "TEST key=value&key2=value2\r\n");

	mfs_tracker_add_parameter(params, "a&b",  "c&d", p);
	result = mfs_tracker_build_request("TEST", params, p, &length);
	CU_ASSERT_EQUAL(length, 40);
	CU_ASSERT_STRING_EQUAL(result, "TEST key=value&key2=value2&a%26b=c%26d\r\n");
	
	apr_pool_destroy(p);
}

void test_meta_data() {
	apr_pool_t *p = mfs_test_get_pool();

	tracker_request_parameters * meta = mfs_tracker_init_parameters(p);

	mfs_tracker_add_meta_data(meta, "meta1",  "value1", false, p);
	mfs_tracker_add_meta_data(meta, "meta2",  "value2", false, p);
	
	tracker_request_parameters * params = mfs_tracker_init_parameters(p);
	mfs_tracker_add_parameter(params, "key",  "value", p);

	mfs_tracker_copy_parameter_pointers(meta, params, p);
			
	apr_size_t length;
	char * result = mfs_tracker_build_request("TEST", params, p, &length);
	//printf("length 0f '%s' is %d",result, (int)strlen(result)); 
	CU_ASSERT_EQUAL(length, 133);
	CU_ASSERT_STRING_EQUAL(result, "TEST plugin.meta.keys=2&key=value&plugin.meta.key0=meta1&plugin.meta.value0=value1&plugin.meta.key1=meta2&plugin.meta.value1=value2\r\n");

	//add meta data and encode straight away
	mfs_tracker_add_meta_data(params, "meta3",  "value3", true, p);
	result = mfs_tracker_build_request("TEST", params, p, &length);
	//printf("length 0f '%s' is %d",result, (int)strlen(result)); 
	CU_ASSERT_EQUAL(length, 182);
	CU_ASSERT_STRING_EQUAL(result, "TEST plugin.meta.keys=3&key=value&plugin.meta.key0=meta1&plugin.meta.value0=value1&plugin.meta.key1=meta2&plugin.meta.value1=value2&plugin.meta.key2=meta3&plugin.meta.value2=value3\r\n");

	
	apr_pool_destroy(p);
}

void test_response_parsing() {
	bool ok;
	apr_pool_t *p = mfs_test_get_pool();
	apr_hash_t *result = apr_hash_make(p);
	
	//test basic ok response
	{
		char test_response[] = "OK 123 abc=def";
		apr_status_t rv = mfs_tracker_parse_response(test_response, strlen(test_response), &ok, result, p);
		CU_ASSERT_EQUAL(rv, APR_SUCCESS);
		CU_ASSERT_EQUAL(ok, true);
		CU_ASSERT_STRING_EQUAL("def", apr_hash_get(result, "abc", APR_HASH_KEY_STRING));
	}

	//test multiple params ok response
	{
		char test_response[] = "OK 123 abc=def&x=y&aaa=bbb";
		apr_status_t rv = mfs_tracker_parse_response(test_response, strlen(test_response), &ok, result, p);
		CU_ASSERT_EQUAL(rv, APR_SUCCESS);
		CU_ASSERT_EQUAL(ok, true);
		CU_ASSERT_STRING_EQUAL("def", apr_hash_get(result, "abc", APR_HASH_KEY_STRING));
		CU_ASSERT_STRING_EQUAL("y", apr_hash_get(result, "x", APR_HASH_KEY_STRING));
		CU_ASSERT_STRING_EQUAL("bbb", apr_hash_get(result, "aaa", APR_HASH_KEY_STRING));
	}
	//test multiple params ok response with encoding
	{
		char test_response[] = "OK 123 abc=def&x=%25a%26&aaa=bbb";
		apr_status_t rv = mfs_tracker_parse_response(test_response, strlen(test_response), &ok, result, p);
		CU_ASSERT_EQUAL(rv, APR_SUCCESS);
		CU_ASSERT_EQUAL(ok, true);
		CU_ASSERT_STRING_EQUAL("def", apr_hash_get(result, "abc", APR_HASH_KEY_STRING));
		CU_ASSERT_STRING_EQUAL("%a&", apr_hash_get(result, "x", APR_HASH_KEY_STRING));
		CU_ASSERT_STRING_EQUAL("bbb", apr_hash_get(result, "aaa", APR_HASH_KEY_STRING));
	}
	//test ok response without FID
	{
		char test_response[] = "OK abc=def&x=%25a%26&aaa=bbb";
		apr_status_t rv = mfs_tracker_parse_response(test_response, strlen(test_response), &ok, result, p);
		CU_ASSERT_EQUAL(rv, APR_SUCCESS);
		CU_ASSERT_EQUAL(ok, true);
		CU_ASSERT_STRING_EQUAL("def", apr_hash_get(result, "abc", APR_HASH_KEY_STRING));
		CU_ASSERT_STRING_EQUAL("%a&", apr_hash_get(result, "x", APR_HASH_KEY_STRING));
		CU_ASSERT_STRING_EQUAL("bbb", apr_hash_get(result, "aaa", APR_HASH_KEY_STRING));
	}
	//test corrupt ok response
	{
		char test_response[] = "OK 123 abc=def&x&aaa=bbb";
		apr_status_t rv = mfs_tracker_parse_response(test_response, strlen(test_response), &ok, result, p);
		CU_ASSERT_EQUAL(rv, APR_EFTYPE);
	}
	//test corrupt ok response
	{
		char test_response[] = "OK";
		apr_status_t rv = mfs_tracker_parse_response(test_response, strlen(test_response), &ok, result, p);
		CU_ASSERT_EQUAL(rv, APR_EFTYPE);
	}
	//test corrupt ok response
	{
		char test_response[] = "OK   ";
		apr_status_t rv = mfs_tracker_parse_response(test_response, strlen(test_response), &ok, result, p);
		CU_ASSERT_EQUAL(rv, APR_EFTYPE);
	}
	//test corrupt ok response
	{
		char test_response[] = "OK 123 ";
		apr_status_t rv = mfs_tracker_parse_response(test_response, strlen(test_response), &ok, result, p);
		CU_ASSERT_EQUAL(rv, APR_EFTYPE);
	}
	//test ok response with param with no value
	{
		char test_response[] = "OK 123 abc=def&x=&aaa=bbb";
		apr_status_t rv = mfs_tracker_parse_response(test_response, strlen(test_response), &ok, result, p);
		CU_ASSERT_EQUAL(ok, true);
		CU_ASSERT_STRING_EQUAL("def", apr_hash_get(result, "abc", APR_HASH_KEY_STRING));
		CU_ASSERT_STRING_EQUAL("", apr_hash_get(result, "x", APR_HASH_KEY_STRING));
		CU_ASSERT_STRING_EQUAL("bbb", apr_hash_get(result, "aaa", APR_HASH_KEY_STRING));
	}

	//test basic error response
	{
		char test_response[] = "ERR 123 this is the longer error";
		apr_status_t rv = mfs_tracker_parse_response(test_response, strlen(test_response), &ok, result, p);
		CU_ASSERT_EQUAL(rv, APR_SUCCESS);
		CU_ASSERT_EQUAL(ok, false);
		CU_ASSERT_STRING_EQUAL("123", apr_hash_get(result, MFS_TRACKER_ERROR_CODE, APR_HASH_KEY_STRING));
		CU_ASSERT_STRING_EQUAL("this is the longer error", apr_hash_get(result, MFS_TRACKER_ERROR_DESC, APR_HASH_KEY_STRING));
	}
	//test encoded error response
	{
		char test_response[] = "ERR 123 sdfsd98%5E*%26%5EKJH)";
		apr_status_t rv = mfs_tracker_parse_response(test_response, strlen(test_response), &ok, result, p);
		CU_ASSERT_EQUAL(rv, APR_SUCCESS);
		CU_ASSERT_EQUAL(ok, false);
		CU_ASSERT_STRING_EQUAL("123", apr_hash_get(result, MFS_TRACKER_ERROR_CODE, APR_HASH_KEY_STRING));
		CU_ASSERT_STRING_EQUAL("sdfsd98^*&^KJH)", apr_hash_get(result, MFS_TRACKER_ERROR_DESC, APR_HASH_KEY_STRING));
	}
	//test corrupt error response
	{
		char test_response[] = "ERR 123";
		apr_status_t rv = mfs_tracker_parse_response(test_response, strlen(test_response), &ok, result, p);
		CU_ASSERT_EQUAL(rv, APR_EFTYPE);
	}
	//test corrupt error response
	{
		char test_response[] = "ERR ";
		apr_status_t rv = mfs_tracker_parse_response(test_response, strlen(test_response), &ok, result, p);
		CU_ASSERT_EQUAL(rv, APR_EFTYPE);
	}
}

void test_request_calling() {
	bool ok;
	apr_pool_t *p = mfs_test_get_pool();
	tracker_info *tracker;
	CU_ASSERT_FATAL(mfs_tracker_init("127.0.0.1", TEST_PORT, p, &tracker)==APR_SUCCESS);
	
	
	//test basic ok response
	{
		char test_response[] = "OK 123 abc=def\r\n";
		
		test_server_handle * handle = test_start_basic_server(test_response, TEST_PORT, p);
		tracker_connection * connection;
		CU_ASSERT_FATAL(mfs_tracker_connect(tracker, &connection, p, DEFAULT_TRACKER_TIMEOUT)==APR_SUCCESS);

		tracker_request_parameters * params = mfs_tracker_init_parameters(p);
		mfs_tracker_add_parameter(params, "A",  "B", p);

		apr_hash_t *result = apr_hash_make(p);
		apr_status_t rv  = mfs_tracker_request(connection, "TEST", params, &ok, result, p, DEFAULT_TRACKER_TIMEOUT);
		stop_test_server(handle);
		CU_ASSERT_EQUAL(rv, APR_SUCCESS);
		if(rv == APR_SUCCESS) {
			CU_ASSERT_EQUAL(ok, true);
			CU_ASSERT_STRING_EQUAL("def", apr_hash_get(result, "abc", APR_HASH_KEY_STRING));
		}
	}
	//test basic error response
	{
		char test_response[] = "ERR 123 Not+Good\r\n";
		
		test_server_handle * handle = test_start_basic_server(test_response, TEST_PORT, p);
		tracker_connection * connection;
		CU_ASSERT_FATAL(mfs_tracker_connect(tracker, &connection, p, DEFAULT_TRACKER_TIMEOUT)==APR_SUCCESS);

		tracker_request_parameters * params = mfs_tracker_init_parameters(p);
		mfs_tracker_add_parameter(params, "A",  "B", p);

		apr_hash_t *result = apr_hash_make(p);
		apr_status_t rv  = mfs_tracker_request(connection, "TEST", params, &ok, result, p, DEFAULT_TRACKER_TIMEOUT);
		stop_test_server(handle);
		CU_ASSERT_EQUAL(rv, APR_SUCCESS);
		if(rv == APR_SUCCESS) {
			CU_ASSERT_STRING_EQUAL("123", apr_hash_get(result, MFS_TRACKER_ERROR_CODE, APR_HASH_KEY_STRING));
			CU_ASSERT_STRING_EQUAL("Not Good", apr_hash_get(result, MFS_TRACKER_ERROR_DESC, APR_HASH_KEY_STRING));
		}
	}
	//test just going over 1000 byte buffer...
	{
		char test_response[] = "OK 123 r1=v1"; //12 chars
		char test_param[] = "&ABCD=EFGH"; //10 chars

		char *response_string = NULL;
		response_string = test_response;
		int i;
		for(i=0; i < 100; i++) {
			response_string = apr_pstrcat(p, response_string, test_param, NULL);
		}
		response_string = apr_pstrcat(p, response_string, "\r\n", NULL);
		//reponse_string is now 1012 chars long...
		
		test_server_handle * handle = test_start_basic_server(response_string, TEST_PORT, p);
		tracker_connection * connection;
		CU_ASSERT_FATAL(mfs_tracker_connect(tracker, &connection, p, DEFAULT_TRACKER_TIMEOUT)==APR_SUCCESS);

		tracker_request_parameters * params = mfs_tracker_init_parameters(p);
		mfs_tracker_add_parameter(params, "A",  "B", p);

		apr_hash_t *result = apr_hash_make(p);
		apr_status_t rv  = mfs_tracker_request(connection, "TEST", params, &ok, result, p, DEFAULT_TRACKER_TIMEOUT);
		stop_test_server(handle);
		CU_ASSERT_EQUAL(rv, APR_SUCCESS);
		if(rv == APR_SUCCESS) {
			CU_ASSERT_EQUAL(ok, true);
			CU_ASSERT_STRING_EQUAL("v1", apr_hash_get(result, "r1", APR_HASH_KEY_STRING));
			CU_ASSERT_STRING_EQUAL("EFGH", apr_hash_get(result, "ABCD", APR_HASH_KEY_STRING));
		}
	}
	//test just going way over 1000 byte buffer...
	{
		char test_response[] = "OK 123 r1=v1"; //12 chars
		char test_param[] = "&ABCD=EFGH"; //10 chars

		char *response_string = NULL;
		response_string = test_response;
		int i;
		for(i=0; i < 1000; i++) {
			response_string = apr_pstrcat(p, response_string, test_param, NULL);
		}
		response_string = apr_pstrcat(p, response_string, "\r\n", NULL);
		//reponse_string is now 10012 chars long...
		
		test_server_handle * handle = test_start_basic_server(response_string, TEST_PORT, p);
		tracker_connection * connection;
		CU_ASSERT_FATAL(mfs_tracker_connect(tracker, &connection, p, DEFAULT_TRACKER_TIMEOUT)==APR_SUCCESS);

		tracker_request_parameters * params = mfs_tracker_init_parameters(p);
		mfs_tracker_add_parameter(params, "A",  "B", p);

		apr_hash_t *result = apr_hash_make(p);
		apr_status_t rv  = mfs_tracker_request(connection, "TEST", params, &ok, result, p, DEFAULT_TRACKER_TIMEOUT);
		stop_test_server(handle);
		CU_ASSERT_EQUAL(rv, APR_SUCCESS);
		if(rv == APR_SUCCESS) {
			CU_ASSERT_EQUAL(ok, true);
			CU_ASSERT_STRING_EQUAL("v1", apr_hash_get(result, "r1", APR_HASH_KEY_STRING));
			CU_ASSERT_STRING_EQUAL("EFGH", apr_hash_get(result, "ABCD", APR_HASH_KEY_STRING));
		}
	}


}
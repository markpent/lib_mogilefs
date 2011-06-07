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
 
#include "CUnit/Basic.h"
#include <stdio.h>
#include "common.h"
#include "tracker_tests.h"
#include "test_pools.h"
#include "test_request.h"
#include "test_file.h"
#include "test_file_upload.h"
#include "test_file_download.h"
#include "test_file_system_download.h"
#include "test_real_server.h"
#include "test_watch.h"
#include <apr_general.h>

int test_tracker();
int test_pools();
int test_request();
int test_file_system();
int test_file_upload();
int test_file_download();
int test_file_system_download();
int test_real_server();

int main(int argc, const char * const argv[]) {
	apr_status_t rv = apr_app_initialize(&argc, &argv, NULL);
	if(rv != APR_SUCCESS) {
		printf("APR library failed to initialize: %d", rv);
		return (1);
	}
	/* initialize the CUnit test registry */
    if (CUE_SUCCESS != CU_initialize_registry())
		return CU_get_error();
	int result;
	result = test_tracker();
	if(result != 0) return result;

	result = test_pools();
	if(result != 0) return result;

	result = test_watch();
	if(result != 0) return result;

	result = test_request();
	if(result != 0) return result;

	result = test_file_system();
	if(result != 0) return result;
		
	result = test_file_upload();
	if(result != 0) return result;

	result = test_file_download();
	if(result != 0) return result;

	result = test_file_system_download();
	if(result != 0) return result;

	result = test_real_server();
	if(result != 0) return result;
	CU_basic_set_mode(CU_BRM_VERBOSE);
	CU_basic_run_tests();
	printf("\n");
	CU_basic_show_failures(CU_get_failure_list());
	printf("\n\n");

	//printf("apr_terminate()\n");
	apr_terminate();
	/* Clean up registry and return */
	//printf("CU_cleanup_registry()\n");
   	CU_cleanup_registry();
	//printf("Done\n");
   	return CU_get_error();

}

int test_tracker() {
	CU_pSuite pSuite = NULL;
	pSuite = CU_add_suite("Tracker Testing Suite", NULL, NULL);
	if (NULL == pSuite) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	/* add the tests to the suite */
	if (
	(NULL == CU_add_test(pSuite, "test_tracker_encoding", test_tracker_encoding)) ||
	(NULL == CU_add_test(pSuite, "test_request_building", test_request_building)) ||
	(NULL == CU_add_test(pSuite, "test_meta_data", test_meta_data)) ||
	(NULL == CU_add_test(pSuite, "test_response_parsing", test_response_parsing)) || 
	(NULL == CU_add_test(pSuite, "test_request_calling", test_request_calling))
	    )
	{
		CU_cleanup_registry();
		return CU_get_error();
	}
	return 0;
}

int test_pools() {
	CU_pSuite pSuite = NULL;
	pSuite = CU_add_suite("Pool Testing Suite", NULL, NULL);
	if (NULL == pSuite) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	/* add the tests to the suite */
	if (
	(NULL == CU_add_test(pSuite, "test_pool_init", test_pool_init))  ||
	(NULL == CU_add_test(pSuite, "test_pool_tracker_iteration", test_pool_tracker_iteration)) ||
	(NULL == CU_add_test(pSuite, "test_pool_connecting", test_pool_connecting)) || 
	(NULL == CU_add_test(pSuite, "test_pool_maintenance_expire_active", test_pool_maintenance_expire_active)) ||
	(NULL == CU_add_test(pSuite, "test_pool_maintenance_activate_inactive", test_pool_maintenance_activate_inactive)) ||
	(NULL == CU_add_test(pSuite, "test_pool_maintenance_thread", test_pool_maintenance_thread)) 
	    )
	{
		CU_cleanup_registry();
		return CU_get_error();
	}
	return 0;
}

int test_watch() {
	CU_pSuite pSuite = NULL;
	pSuite = CU_add_suite("Watch Testing Suite", NULL, NULL);
	if (NULL == pSuite) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	/* add the tests to the suite */
	if (
	(NULL == CU_add_test(pSuite, "test_watch_calling", test_watch_calling)) ||
	(NULL == CU_add_test(pSuite, "test_watch_parsing", test_watch_parsing)) /*||
	(NULL == CU_add_test(pSuite, "test_meta_data", test_meta_data)) ||
	(NULL == CU_add_test(pSuite, "test_response_parsing", test_response_parsing)) || 
	(NULL == CU_add_test(pSuite, "test_request_calling", test_request_calling))*/
	    )
	{
		CU_cleanup_registry();
		return CU_get_error();
	}
	return 0;
}


int test_request() {
	CU_pSuite pSuite = NULL;
	pSuite = CU_add_suite("Request Testing Suite", NULL, NULL);
	if (NULL == pSuite) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	/* add the tests to the suite */
	if (
	(NULL == CU_add_test(pSuite, "test_request_all_ok", test_request_all_ok)) ||
	(NULL == CU_add_test(pSuite, "test_request_all_ok_no_pool", test_request_all_ok_no_pool))  ||
	(NULL == CU_add_test(pSuite, "test_request_reconnect", test_request_reconnect)) /*|| 
	(NULL == CU_add_test(pSuite, "test_pool_maintenance_expire_active", test_pool_maintenance_expire_active)) ||
	(NULL == CU_add_test(pSuite, "test_pool_maintenance_activate_inactive", test_pool_maintenance_activate_inactive)) ||
	(NULL == CU_add_test(pSuite, "test_pool_maintenance_thread", test_pool_maintenance_thread)) */
	    )
	{
		CU_cleanup_registry();
		return CU_get_error();
	}
	return 0;
}

int test_file_system() {
	CU_pSuite pSuite = NULL;
	pSuite = CU_add_suite("File Server Tracker Calls Testing Suite", NULL, NULL);
	if (NULL == pSuite) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	/* add the tests to the suite */
	if (
	(NULL == CU_add_test(pSuite, "test_file_system_get_paths_ok", test_file_system_get_paths_ok))  ||
	(NULL == CU_add_test(pSuite, "test_file_system_get_paths_corrupt", test_file_system_get_paths_corrupt))   ||
	(NULL == CU_add_test(pSuite, "test_file_system_get_paths_error_zero", test_file_system_get_paths_error_zero)) || 
	(NULL == CU_add_test(pSuite, "test_file_system_get_paths_error_code", test_file_system_get_paths_error_code)) ||
	(NULL == CU_add_test(pSuite, "test_file_system_delete_ok", test_file_system_delete_ok))  ||
	(NULL == CU_add_test(pSuite, "test_file_system_delete_fail",test_file_system_delete_fail)) ||
	(NULL == CU_add_test(pSuite, "test_file_system_sleep_ok", test_file_system_sleep_ok)) ||
	(NULL == CU_add_test(pSuite, "test_file_system_sleep_fail", test_file_system_sleep_fail)) ||
	(NULL == CU_add_test(pSuite, "test_file_system_rename_ok", test_file_system_rename_ok)) ||
	(NULL == CU_add_test(pSuite, "test_file_system_rename_fail", test_file_system_rename_fail))
	    )
	{
		CU_cleanup_registry();
		return CU_get_error();
	}
	return 0;

}

int test_file_upload() {
	CU_pSuite pSuite = NULL;
	pSuite = CU_add_suite("File Server Upload Testing Suite", NULL, NULL);
	if (NULL == pSuite) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	/* add the tests to the suite */
	if (
	(NULL == CU_add_test(pSuite, "test_file_put_ok_bytes", test_file_put_ok_bytes))  ||
	(NULL == CU_add_test(pSuite, "test_file_put_ok_file", test_file_put_ok_file)) ||
	(NULL == CU_add_test(pSuite, "test_file_put_ok_bytes_keepalive", test_file_put_ok_bytes_keepalive)) || 
	(NULL == CU_add_test(pSuite, "test_file_put_fail_no_connect", test_file_put_fail_no_connect))  ||
	(NULL == CU_add_test(pSuite, "test_file_system_upload_bytes_ok", test_file_system_upload_bytes_ok)) ||
	(NULL == CU_add_test(pSuite, "test_file_system_upload_file_ok_no_pool", test_file_system_upload_file_ok_no_pool)) ||
	(NULL == CU_add_test(pSuite, "test_file_system_upload_bytes_timeout", test_file_system_upload_bytes_timeout)) ||
	(NULL == CU_add_test(pSuite, "test_file_system_upload_bytes_corrupt_tracker", test_file_system_upload_bytes_corrupt_tracker)) 
	    )
	{
		CU_cleanup_registry();
		return CU_get_error();
	}
	return 0;
}

int test_file_download() {
	CU_pSuite pSuite = NULL;
	pSuite = CU_add_suite("File Server Download Testing Suite", NULL, NULL);
	if (NULL == pSuite) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	/* add the tests to the suite */
	if (
	(NULL == CU_add_test(pSuite, "test_file_get_ok_bytes", test_file_get_ok_bytes)) ||
	(NULL == CU_add_test(pSuite, "test_file_get_ok_file", test_file_get_ok_file))  ||
	(NULL == CU_add_test(pSuite, "test_file_get_ok_either", test_file_get_ok_either)) || 
	(NULL == CU_add_test(pSuite, "test_file_get_ok_either_large", test_file_get_ok_either_large))  ||
	(NULL == CU_add_test(pSuite, "test_file_get_ok_brigade_large", test_file_get_ok_brigade_large)) ||
	(NULL == CU_add_test(pSuite, "test_file_get_fail_brigade_large", test_file_get_fail_brigade_large)) ||
	(NULL == CU_add_test(pSuite, "test_file_get_fail_brigade_large_with_data", test_file_get_fail_brigade_large_with_data)) ||
	(NULL == CU_add_test(pSuite, "test_file_get_timeout_bytes", test_file_get_timeout_bytes)) ||
	(NULL == CU_add_test(pSuite, "test_file_get_timeout_brigade_large_with_data", test_file_get_timeout_brigade_large_with_data))
	    )
	{
		CU_cleanup_registry();
		return CU_get_error();
	}
	return 0;
}

int test_file_system_download() {
	CU_pSuite pSuite = NULL;
	pSuite = CU_add_suite("File System Download Testing Suite", NULL, NULL);
	if (NULL == pSuite) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	/* add the tests to the suite */
	if (
	(NULL == CU_add_test(pSuite, "test_file_system_get_ok_bytes", test_file_system_get_ok_bytes))  ||
	(NULL == CU_add_test(pSuite, "test_file_system_get_ok_file", test_file_system_get_ok_file))  ||
	(NULL == CU_add_test(pSuite, "test_file_system_get_ok_either", test_file_system_get_ok_either)) || 
	(NULL == CU_add_test(pSuite, "test_file_system_get_ok_brigade_large", test_file_system_get_ok_brigade_large)) ||
	(NULL == CU_add_test(pSuite, "test_file_system_get_failover_bytes", test_file_system_get_failover_bytes)) ||
	(NULL == CU_add_test(pSuite, "test_file_system_get_failover_file", test_file_system_get_failover_file)) ||
	(NULL == CU_add_test(pSuite, "test_file_system_get_failover_brigade", test_file_system_get_failover_brigade)) ||
	(NULL == CU_add_test(pSuite, "test_file_system_get_fail_bytes", test_file_system_get_fail_bytes))
	    )
	{
		CU_cleanup_registry();
		return CU_get_error();
	}
	return 0;
}

int test_real_server() {
	CU_pSuite pSuite = NULL;
	pSuite = CU_add_suite("Real Server Testing Suite", NULL, NULL);
	if (NULL == pSuite) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	/* add the tests to the suite */
	if (
	(NULL == CU_add_test(pSuite, "test_real_server_ok", test_real_server_ok)) ||
	(NULL == CU_add_test(pSuite, "test_real_server_upload", test_real_server_upload))   ||
	(NULL == CU_add_test(pSuite, "test_real_server_with_filepath", test_real_server_with_filepath)) /*||
	(NULL == CU_add_test(pSuite, "test_file_system_get_ok_brigade_large", test_file_system_get_ok_brigade_large)) ||
	(NULL == CU_add_test(pSuite, "test_file_system_get_failover_bytes", test_file_system_get_failover_bytes)) ||
	(NULL == CU_add_test(pSuite, "test_file_system_get_failover_file", test_file_system_get_failover_file)) ||
	(NULL == CU_add_test(pSuite, "test_file_system_get_failover_brigade", test_file_system_get_failover_brigade)) ||
	(NULL == CU_add_test(pSuite, "test_file_system_get_fail_bytes", test_file_system_get_fail_bytes))*/
	    )
	{
		CU_cleanup_registry();
		return CU_get_error();
	}
	return 0;
}


apr_pool_t * mfs_test_get_pool() {
	apr_pool_t *new_pool;
	if(apr_pool_create(&new_pool,NULL) != APR_SUCCESS) {
		printf("UNABLE TO CREATE APR POOL!");
		return NULL;
	}
	return new_pool;
}

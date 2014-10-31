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
#include "test_real_server.h"
#include "common.h"
#include "../src/mogile_fs.h"
#include <apr_time.h>
#include <apr_strings.h>

void test_real_server_ok() {
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();

		
	char tracker_list_str[] = REAL_SERVER_TRACKERS;
	tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

	char data[] = "THIS IS THE PUT DATA";
	long dlen = strlen(data);
	rv = mfs_store_bytes(file_system, REAL_SERVER_DOMAIN, "/tests/upload_test/test1", REAL_SERVER_CLASS, p, data, strlen(data));

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, rv);


	void *bytes;
	apr_size_t total_bytes;
	apr_file_t *file = NULL;

	rv = mfs_get_file_or_bytes(file_system, REAL_SERVER_DOMAIN, "/tests/upload_test/test1", &total_bytes, &bytes, &file, p, NULL, -1);

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, rv);
	
	CU_ASSERT_EQUAL_FATAL(strlen(data), total_bytes);
	CU_ASSERT_NSTRING_EQUAL(data, bytes, total_bytes);

	rv = mfs_rename_filepath(file_system, REAL_SERVER_DOMAIN, "/tests/upload_test/test1", "/tests/upload_test/test2", p);
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, rv);

	rv = mfs_get_file_or_bytes(file_system, REAL_SERVER_DOMAIN, "/tests/upload_test/test2", &total_bytes, &bytes, &file, p, NULL, -1);

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, rv);
	
	CU_ASSERT_EQUAL_FATAL(strlen(data), total_bytes);
	CU_ASSERT_NSTRING_EQUAL(data, bytes, total_bytes);

	rv = mfs_delete(file_system, REAL_SERVER_DOMAIN, "/tests/upload_test/test2", p);
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, rv);

	rv = mfs_get_file_or_bytes(file_system, REAL_SERVER_DOMAIN, "/tests/upload_test/test2", &total_bytes, &bytes, &file, p, NULL, -1);

	CU_ASSERT_EQUAL(APR_EBADPATH, rv);
	mfs_close_file_system(file_system);
	apr_pool_destroy(p); 
}


void test_real_server_upload() {
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();

		
	char tracker_list_str[] = REAL_SERVER_TRACKERS;
	tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

	char data[] = "THIS IS THE PUT DATA";
	long dlen = strlen(data);
	int i;
	for(i=0; i < 50; i++) {
		char *key = apr_psprintf(p, "/tests/upload_test/bulk_%d",i); //tests/upload_test/bulk_%d"
		rv = mfs_store_bytes(file_system, REAL_SERVER_DOMAIN, key, REAL_SERVER_CLASS, p, data, strlen(data));
		printf(".");
		CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, rv);
	}

	mfs_close_file_system(file_system);
	
	apr_pool_destroy(p); 
}


void test_real_server_with_filepath() {
	mfs_file_system *file_system;
	apr_status_t rv;
	apr_pool_t *p = mfs_test_get_pool();

		
	char tracker_list_str[] = REAL_SERVER_TRACKERS;
	tracker_pool * trackers = mfs_pool_init_quick(tracker_list_str);

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_init_file_system(&file_system, trackers));

	char data[] = "THIS IS THE PUT DATA";
	long dlen = strlen(data);
	rv = mfs_store_bytes_filepath(file_system, REAL_SERVER_DOMAIN, "/fp_tests/upload_test/test1", REAL_SERVER_CLASS, p, data, strlen(data), 0);

	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, rv);

	mfs_filepath_entry *entries;
	int entry_count;

	rv = mfs_list_directory(file_system, REAL_SERVER_DOMAIN, "/fp_tests/upload_test", &entries, &entry_count, p);
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, rv);
	CU_ASSERT_EQUAL_FATAL(entry_count, 1);
	
	CU_ASSERT_STRING_EQUAL(entries[0].name, "test1");

	mfs_filepath_entry entry;
	rv = mfs_path_info(file_system, REAL_SERVER_DOMAIN, "/fp_tests/upload_test/test1", &entry, p);
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, rv);
	CU_ASSERT_EQUAL(entry.type, TYPE_FILE);
//	printf("File size=%ld\n", entry.size);
	CU_ASSERT_EQUAL(entry.size, 20);

	rv = mfs_path_info(file_system, REAL_SERVER_DOMAIN, "/fp_tests/upload_test", &entry, p);
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, rv);
	CU_ASSERT_EQUAL(entry.type, TYPE_DIRECTORY);

	long long server_id=0;
	rv = mfs_create_directory(file_system, REAL_SERVER_DOMAIN, "/fp_tests/upload_test2/dirtest", p, &server_id);
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, rv);
	CU_ASSERT_NOT_EQUAL(0, server_id);
	
	rv = mfs_list_directory(file_system, REAL_SERVER_DOMAIN, "/fp_tests/upload_test2", &entries, &entry_count, p);
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, rv);
	CU_ASSERT_EQUAL_FATAL(entry_count, 1);
	
	CU_ASSERT_STRING_EQUAL(entries[0].name, "dirtest");
	CU_ASSERT_EQUAL(entries[0].type, TYPE_DIRECTORY);

	server_id=0;
	rv = mfs_create_link(file_system, REAL_SERVER_DOMAIN, "/fp_tests/upload_test2/dirtest/symlink", "/a/link/test", p, &server_id);
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, rv);
	CU_ASSERT_NOT_EQUAL(0, server_id);

	rv = mfs_list_directory(file_system, REAL_SERVER_DOMAIN, "/fp_tests/upload_test2/dirtest", &entries, &entry_count, p);
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, rv);
	CU_ASSERT_EQUAL_FATAL(entry_count, 1);
	
	CU_ASSERT_STRING_EQUAL(entries[0].name, "symlink");
	CU_ASSERT_EQUAL(entries[0].type, TYPE_SYMLINK);

	//make sure we cant delete a directory that isnt empty
	rv = mfs_delete_filepath_node(file_system, REAL_SERVER_DOMAIN, "/fp_tests/upload_test2/dirtest", p);
	CU_ASSERT_NOT_EQUAL(APR_SUCCESS, rv);
	
	
	mfs_filepath_stats stats;
	CU_ASSERT_EQUAL_FATAL(APR_SUCCESS, mfs_stats_filepath(file_system,  REAL_SERVER_DOMAIN, &stats, p));
	

	
	mfs_close_file_system(file_system);
	apr_pool_destroy(p); 
}


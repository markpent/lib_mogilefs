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
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <stdint.h>
#include <stdarg.h>
#include <microhttpd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <apr_general.h>
#include <apr_network_io.h>
#include <apr_hash.h>
#include <apr_pools.h>

typedef struct {
	struct MHD_Daemon * deamon;
	apr_pool_t *pool;
	apr_hash_t *log; //hash tables
	char *response;
	int response_length;
	int response_code;
	long sleep_duration; //used in timeout tests...
} test_http_server;


test_http_server * start_test_http_server(int port, char *response, int response_code, MHD_AccessHandlerCallback handler);
void stop_test_http_server(test_http_server *handle);

int test_http_server_ok_handler(void * cls,
		    struct MHD_Connection * connection,
		    const char * url,
		    const char * method,
            const char * version,
		    const char * upload_data,
		    size_t * upload_data_size, 
    		void ** ptr);

int test_http_server_fail_handler(void * cls,
		    struct MHD_Connection * connection,
		    const char * url,
		    const char * method,
            const char * version,
		    const char * upload_data,
		    size_t * upload_data_size, 
    		void ** ptr);

int test_http_server_timeout_handler(void * cls,
		    struct MHD_Connection * connection,
		    const char * url,
		    const char * method,
            const char * version,
		    const char * upload_data,
		    size_t * upload_data_size, 
    		void ** ptr);

int test_http_server_send_half_handler(void * cls,
		    struct MHD_Connection * connection,
		    const char * url,
		    const char * method,
            const char * version,
		    const char * upload_data,
		    size_t * upload_data_size, 
    		void ** ptr);

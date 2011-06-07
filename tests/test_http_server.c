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

#include "test_http_server.h"
#include "common.h"
#include <apr_strings.h>

int test_http_server_ok_handler(void * cls,
		    struct MHD_Connection * connection,
		    const char * url,
		    const char * method,
            const char * version,
		    const char * upload_data,
		    size_t * upload_data_size, 
    		void ** ptr) {

	test_http_server * handle = cls;
	struct MHD_Response * response;
	int ret;
	int * c_count;
	if (*ptr == NULL) {
		handle->log = apr_hash_make(handle->pool);
	  	/* The first time only the headers are valid,
		 do not respond in the first round... */
	  	*ptr = cls; //just to flag its not null....
	  	int * c_count = apr_palloc(handle->pool, sizeof(int));
		*c_count = 0;
		*ptr = c_count;
	  	return MHD_YES;
	}
	c_count = *ptr;
	if(*c_count == 0) {
		//log the request....
		apr_hash_set(handle->log,  apr_pstrdup(handle->pool, "METHOD"), APR_HASH_KEY_STRING, apr_pstrdup(handle->pool, method));
		apr_hash_set(handle->log,  apr_pstrdup(handle->pool, "URL"), APR_HASH_KEY_STRING, apr_pstrdup(handle->pool, url));
		apr_hash_set(handle->log,  apr_pstrdup(handle->pool, "VERSION"), APR_HASH_KEY_STRING, apr_pstrdup(handle->pool, version));
		apr_hash_set(handle->log,  apr_pstrdup(handle->pool, "DATA_LENGTH"), APR_HASH_KEY_STRING, apr_pmemdup (handle->pool, upload_data_size, sizeof(size_t)));
		apr_hash_set(handle->log,  apr_pstrdup(handle->pool, "DATA"), APR_HASH_KEY_STRING, apr_pmemdup (handle->pool, upload_data, *upload_data_size));
	}
	*c_count = *c_count+1;
	if(*upload_data_size == 0) {
		response = MHD_create_response_from_data(strlen(handle->response),
						   (void*) handle->response,
						   MHD_NO,
						   MHD_NO);
		ret = MHD_queue_response(connection,
				   handle->response_code,
				   response);
		MHD_destroy_response(response);
		return ret;
	} else {
		*upload_data_size = 0; //we processed everything?
		return MHD_YES;
	}
}

//this will fail after the header is read...
int test_http_server_fail_handler(void * cls,
		    struct MHD_Connection * connection,
		    const char * url,
		    const char * method,
            const char * version,
		    const char * upload_data,
		    size_t * upload_data_size, 
    		void ** ptr) {
	test_http_server * handle = cls;
	struct MHD_Response * response;
	int ret;

	return MHD_NO; //serious error!
}

int test_http_server_timeout_handler(void * cls,
		    struct MHD_Connection * connection,
		    const char * url,
		    const char * method,
            const char * version,
		    const char * upload_data,
		    size_t * upload_data_size, 
    		void ** ptr) {

	test_http_server * handle = cls;
	struct MHD_Response * response;
	int ret;
	int * c_count;
	if (*ptr == NULL) {
		handle->log = apr_hash_make(handle->pool);
	  	/* The first time only the headers are valid,
		 do not respond in the first round... */
	  	*ptr = cls; //just to flag its not null....
	  	int * c_count = apr_palloc(handle->pool, sizeof(int));
		*c_count = 0;
		*ptr = c_count;
	  	return MHD_YES;
	}
	c_count = *ptr;
	if(*c_count == 0) {
		//log the request....
		apr_hash_set(handle->log,  apr_pstrdup(handle->pool, "METHOD"), APR_HASH_KEY_STRING, apr_pstrdup(handle->pool, method));
		apr_hash_set(handle->log,  apr_pstrdup(handle->pool, "URL"), APR_HASH_KEY_STRING, apr_pstrdup(handle->pool, url));
		apr_hash_set(handle->log,  apr_pstrdup(handle->pool, "VERSION"), APR_HASH_KEY_STRING, apr_pstrdup(handle->pool, version));
		apr_hash_set(handle->log,  apr_pstrdup(handle->pool, "DATA_LENGTH"), APR_HASH_KEY_STRING, apr_pmemdup (handle->pool, upload_data_size, sizeof(size_t)));
		apr_hash_set(handle->log,  apr_pstrdup(handle->pool, "DATA"), APR_HASH_KEY_STRING, apr_pmemdup (handle->pool, upload_data, *upload_data_size));
	}
	*c_count = *c_count+1;
	if(*upload_data_size == 0) {
		//cause a timeout...
		apr_sleep(handle->sleep_duration);
		response = MHD_create_response_from_data(strlen(handle->response),
						   (void*) handle->response,
						   MHD_NO,
						   MHD_NO);
		ret = MHD_queue_response(connection,
				   handle->response_code,
				   response);
		MHD_destroy_response(response);
		return ret;
	} else {
		*upload_data_size = 0; //we processed everything?
		return MHD_YES;
	}

}

int test_http_server_send_half_response_handler(void *cls, uint64_t pos, char *buf, int max) {
	test_http_server * handle = cls;
	int buf_len = handle->response_length;
	if((buf_len / 2) < pos) { //error time.. we have downloaded more than half...
		if(handle->sleep_duration > 0) {
			apr_sleep(handle->sleep_duration);
			handle->sleep_duration = -1;
		} else if(handle->sleep_duration==0) {
			return -1;
		}
	}
	int to_send = buf_len - pos;
	if(to_send > max) {
		to_send  = max;
	}
	char * src = (handle->response + pos);
	memcpy(buf, src, to_send);
	return to_send;
}

void test_http_server_send_half_response_free_handler(void *cls) {
	//do nothing
}

int test_http_server_send_half_handler(void * cls,
		    struct MHD_Connection * connection,
		    const char * url,
		    const char * method,
            const char * version,
		    const char * upload_data,
		    size_t * upload_data_size, 
    		void ** ptr) {

	test_http_server * handle = cls;
	struct MHD_Response * response;
	int ret;
	int * c_count;
	if (*ptr == NULL) {
		handle->log = apr_hash_make(handle->pool);
	  	/* The first time only the headers are valid,
		 do not respond in the first round... */
	  	*ptr = cls; //just to flag its not null....
	  	int * c_count = apr_palloc(handle->pool, sizeof(int));
		*c_count = 0;
		*ptr = c_count;
	  	return MHD_YES;
	}
	c_count = *ptr;
	if(*c_count == 0) {
		//log the request....
		apr_hash_set(handle->log,  apr_pstrdup(handle->pool, "METHOD"), APR_HASH_KEY_STRING, apr_pstrdup(handle->pool, method));
		apr_hash_set(handle->log,  apr_pstrdup(handle->pool, "URL"), APR_HASH_KEY_STRING, apr_pstrdup(handle->pool, url));
		apr_hash_set(handle->log,  apr_pstrdup(handle->pool, "VERSION"), APR_HASH_KEY_STRING, apr_pstrdup(handle->pool, version));
		apr_hash_set(handle->log,  apr_pstrdup(handle->pool, "DATA_LENGTH"), APR_HASH_KEY_STRING, apr_pmemdup (handle->pool, upload_data_size, sizeof(size_t)));
		apr_hash_set(handle->log,  apr_pstrdup(handle->pool, "DATA"), APR_HASH_KEY_STRING, apr_pmemdup (handle->pool, upload_data, *upload_data_size));
	}
	*c_count = *c_count+1;
	if(*upload_data_size == 0) {


		response = MHD_create_response_from_callback(strlen(handle->response), strlen(handle->response) / 10, test_http_server_send_half_response_handler, 
		                                             handle, test_http_server_send_half_response_free_handler);

		ret = MHD_queue_response(connection,
				   handle->response_code,
				   response);
		MHD_destroy_response(response);
		return ret;
	} else {
		*upload_data_size = 0; //we processed everything?
		return MHD_YES;
	}
}

test_http_server * start_test_http_server(int port, char *response, int response_code, MHD_AccessHandlerCallback handler) {
	apr_pool_t *pool = mfs_test_get_pool();
	test_http_server *handle = apr_pcalloc(pool, sizeof(test_http_server));
	handle->response = response;
	handle->response_length = strlen(response);
	handle->response_code = response_code;
	handle->pool = pool;
  	handle->deamon = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION,
		port,
		NULL,
		NULL,
		handler,
		handle,
		MHD_OPTION_END);
	
  	if(handle->deamon == NULL) return NULL;
	return handle;
}

void stop_test_http_server(test_http_server *handle) {
	MHD_stop_daemon(handle->deamon);
}
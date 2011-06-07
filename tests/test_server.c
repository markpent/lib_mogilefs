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

#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

#include <apr_general.h>
#include <apr_file_io.h>
#include <apr_strings.h>
#include <apr_network_io.h>
#include <apr_thread_proc.h>

#include "test_server.h"


/* default socket backlog number. SOMAXCONN is a system default value */
#define DEF_SOCKET_BACKLOG	SOMAXCONN

/* default buffer size */
#define BUFSIZE			4096


static void* APR_THREAD_FUNC test_server_run(apr_thread_t *thd, void *data);


typedef struct _server_thread_data {
	int port;
	apr_status_t (*request_process_callback)(apr_socket_t *serv_sock, apr_pool_t *mp, struct _server_thread_data *server_data);
	void *callback_data;
	test_server_handle *handle;
} server_thread_data;

static apr_status_t do_listen(apr_socket_t **sock, apr_pool_t *mp, int port);
static apr_status_t basic_response_test(apr_socket_t *serv_sock, apr_pool_t *mp, struct _server_thread_data *server_data);
static apr_status_t looped_response_test(apr_socket_t *sock, apr_pool_t *mp, struct _server_thread_data *server_data);

test_server_handle *  test_start_basic_server(char *response_string, int port, apr_pool_t *mp) {
	server_thread_data *data = apr_palloc(mp, sizeof(server_thread_data));
	data->handle = apr_palloc(mp, sizeof(test_server_handle));
	//apr_thread_mutex_create(&data->handle->mutex, APR_THREAD_MUTEX_UNNESTED, mp);
    //apr_thread_cond_create(&data->handle->cond, mp);

	
	data->port = port;
	data->request_process_callback = basic_response_test;
	data->callback_data = response_string;
	apr_threadattr_t *thd_attr;
	apr_threadattr_create(&thd_attr, mp);
	data->handle->test_server_running = 0;
	apr_status_t rv = apr_thread_create(&data->handle->test_server_thread, thd_attr, test_server_run, (void*)data, mp);
	assert(rv == APR_SUCCESS);
	while(data->handle->test_server_running ==0) {
		apr_sleep(100);
	}
	return data->handle;
}

test_server_handle * test_start_looped_server(char *response_string, int port, apr_pool_t *mp) {
	server_thread_data *data = apr_palloc(mp, sizeof(server_thread_data));
	data->handle = apr_palloc(mp, sizeof(test_server_handle));
	//apr_thread_mutex_create(&data->handle->mutex, APR_THREAD_MUTEX_UNNESTED, mp);
    //apr_thread_cond_create(&data->handle->cond, mp);

	
	data->port = port;
	data->request_process_callback = looped_response_test;
	data->callback_data = response_string;
	apr_threadattr_t *thd_attr;
	apr_threadattr_create(&thd_attr, mp);
	data->handle->test_server_running = 0;
	apr_status_t rv = apr_thread_create(&data->handle->test_server_thread, thd_attr, test_server_run, (void*)data, mp);
	assert(rv == APR_SUCCESS);
	while(data->handle->test_server_running ==0) {
		apr_sleep(100);
	}
	return data->handle;
}

void stop_test_server(test_server_handle * handle) {
	handle->test_server_running = 0;
	
	apr_status_t rv2;
	//apr_thread_mutex_lock(handle->mutex);
    //apr_thread_cond_signal(handle->cond);
    //apr_thread_mutex_unlock(handle->mutex);

	apr_status_t rv = apr_thread_join(&rv2, handle->test_server_thread);
    assert(rv == APR_SUCCESS);
}



static void* APR_THREAD_FUNC test_server_run(apr_thread_t *thd, void *data) {

	server_thread_data * server_data = (server_thread_data*)data;
	
	apr_status_t rv;
    apr_pool_t *mp;
    apr_socket_t *s;/* listening socket */

    apr_pool_create(&mp, NULL);

    rv = do_listen(&s, mp, server_data->port);
    if (rv != APR_SUCCESS) {
        char errbuf[256];
	    apr_strerror(rv, errbuf, sizeof(errbuf));
	    printf("test server error listening: %d, %s\n", rv, errbuf);
		apr_pool_destroy(mp);
		apr_thread_exit(thd, rv);
		return;
    }
	server_data->handle->test_server_running = 1;
    while (server_data->handle->test_server_running == 1) {
        apr_socket_t *ns;/* accepted socket */

		//printf("A");
        rv = apr_socket_accept(&ns, s, mp);
		if(rv == 11) {
			//printf(".");
		} else {
			//printf("B");
		    if (rv != APR_SUCCESS) {
		        char errbuf[256];
				apr_strerror(rv, errbuf, sizeof(errbuf));
				printf("test server error accepting: %d, %s\n", rv, errbuf);
				apr_pool_destroy(mp);
				apr_thread_exit(thd, rv);
				return;
		    }
		    /* it is a good idea to specify socket options for the newly accepted socket explicitly */
		    apr_socket_opt_set(ns, APR_SO_NONBLOCK, 0);
		    apr_socket_timeout_set(ns, 5000);

			rv = server_data->request_process_callback(ns, mp, server_data);
			apr_socket_close(ns);
		    if (rv != APR_SUCCESS) {
		        char errbuf[256];
				apr_strerror(rv, errbuf, sizeof(errbuf));
				printf("test server error processing: %d, %s\n", rv, errbuf);
				apr_pool_destroy(mp);
				apr_thread_exit(thd, rv);
				return;
		    }
		}
    }
	//printf("apr_pool_destroy\n");
    apr_pool_destroy(mp);
	//printf("apr_thread_exit\n");
    apr_thread_exit(thd, APR_SUCCESS);
	//printf("return\n");
	return NULL;
}



/**
 * Create a listening socket, and listen it.
 */
static apr_status_t do_listen(apr_socket_t **sock, apr_pool_t *mp, int port)
{
    apr_status_t rv;
    apr_socket_t *s;
    apr_sockaddr_t *sa;
    
    rv = apr_sockaddr_info_get(&sa, NULL, APR_INET, port, 0, mp);
    if (rv != APR_SUCCESS) {
        return rv;
    }
    
    rv = apr_socket_create(&s, sa->family, SOCK_STREAM, APR_PROTO_TCP, mp);
    if (rv != APR_SUCCESS) {
        return rv;
    }

    /* it is a good idea to specify socket options explicitly.
     * in this case, we make a blocking socket as the listening socket */
    apr_socket_opt_set(s, APR_SO_NONBLOCK, 0);
    apr_socket_timeout_set(s, 100);
    apr_socket_opt_set(s, APR_SO_REUSEADDR, 1);/* this is useful for a server(socket listening) process */

    rv = apr_socket_bind(s, sa);
    if (rv != APR_SUCCESS) {
        return rv;
    }
    rv = apr_socket_listen(s, DEF_SOCKET_BACKLOG);
    if (rv != APR_SUCCESS) {
        return rv;
    }

    *sock = s;
    return rv;
}

/**
 * read the request (and ignore it)
 * send the callback data back..
 */
static apr_status_t basic_response_test(apr_socket_t *sock, apr_pool_t *mp, struct _server_thread_data *server_data) {
	char buf[BUFSIZE];
	apr_size_t len = sizeof(buf) - 1;/* -1 for a null-terminated */
	apr_status_t rv = apr_socket_recv(sock, buf, &len);
	if (rv == APR_EOF || len == 0) {
		if(len==0) {
			printf("Server connection stopped with rv=%d\n", rv);
		}
		return APR_SUCCESS;
	}
	char * response_data = (char *)server_data->callback_data;
	apr_size_t length = strlen(response_data);
	apr_socket_send(sock, response_data, &length);
	return APR_SUCCESS;
}


/**
 * read the request (and ignore it)
 * send the callback data back..
 */
static apr_status_t looped_response_test(apr_socket_t *sock, apr_pool_t *mp, struct _server_thread_data *server_data) {
	char buf[BUFSIZE];
	apr_size_t len = sizeof(buf) - 1;/* -1 for a null-terminated */
	while(server_data->handle->test_server_running == 1) {
		apr_status_t rv = apr_socket_recv(sock, buf, &len);
		if(rv == 70007) {
			//timeout
		} else {
			if (rv == APR_EOF || len == 0) {
				if(len==0) {
					printf("Server connection stopped with rv=%d\n", rv);
				}
				return APR_SUCCESS;
			}
			char * response_data = (char *)server_data->callback_data;
			apr_size_t length = strlen(response_data);
			apr_socket_send(sock, response_data, &length);
		}
	}
	return APR_SUCCESS;
}
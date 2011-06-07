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

#include <apr_general.h>
#include <apr_thread_proc.h>
#include <apr_thread_cond.h>


typedef struct {
	volatile int test_server_running;
	apr_thread_t *test_server_thread;
	//apr_thread_mutex_t *mutex;
    //apr_thread_cond_t  *cond;
} test_server_handle;

//start a basic server that always just responds with response_string
test_server_handle * test_start_basic_server(char *response_string, int port, apr_pool_t *mp);
test_server_handle * test_start_looped_server(char *response_string, int port, apr_pool_t *mp);
void stop_test_server(test_server_handle *handle);
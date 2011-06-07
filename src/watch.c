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

/*
typedef struct {
	tracker_info *tracker;
	int tracker_index;
	tracker_pool *trackers;
	tracker_connection_pool_entry *connection;
	char read_buf[5000];
	char *buf_pointer;
	int len;
} watch_data;
*/

#include "mogile_fs.h"
#include "logger.h"

bool allow_watching = true;

void stop_watching() {
	allow_watching = false;
}

void enable_watching() {
	allow_watching = true;
}

watch_data * init_watch(int tracker_index, tracker_pool *trackers, apr_pool_t *pool, char *client_id) {
	watch_data * watch = apr_pcalloc(pool, sizeof(watch_data));
	watch->tracker = &trackers->trackers[tracker_index];
	watch->tracker_index = tracker_index;
	watch->trackers = trackers;
	watch->buf_pointer = watch->read_buf;
	watch->client_id = client_id;
	if(client_id != NULL) {
		watch->client_id_length = strlen(client_id);
	} else {
		watch->client_id_length = 0;
	}
	return watch;
}

apr_status_t sock_readline(char *bufptr, size_t len, watch_data * watch) {
	char *bufx = bufptr;
	char c;
	apr_status_t rv;
	int size=0;
	while((allow_watching)&&(--len > 0)) {
		if(watch->cnt == 0) {
			//need to refill buffer from socket....
			watch->cnt = sizeof( watch->read_buf );
			if((rv = apr_socket_recv(watch->connection->connection->socket, watch->read_buf, &watch->cnt)) != APR_SUCCESS) {
				if(APR_STATUS_IS_TIMEUP(rv)) {
					//try again...
					len++;		/* the while will decrement...*/
					continue;
				} else if(watch->cnt == 0) {
					return rv; //only return if we have no data to process... next time we try and fill the buffer we will probbaly get the same error but with no data...
				}
			}
			if(watch->cnt == 0)
				return APR_EOF;
			watch->buf_pointer = watch->read_buf;
		} else {
			watch->cnt--;
		}
		c = *watch->buf_pointer++;
		size++;
		*bufptr++ = c;
		if(c == '\n'){
			*bufptr = '\0';
			if(size > 1) {
				*bufptr--;
				*bufptr--;
				if(*bufptr == '\r') {
					*bufptr = '\0';
				}
			}
			//somehow there is a nul-terminator at the end of the payload....
			while((watch->cnt > 0)&&(watch->buf_pointer[0] == '\0')) {
				watch->cnt--;
				*watch->buf_pointer++;
			}
			return APR_SUCCESS;
		}
	}
	return APR_ENOMEM;
}

apr_status_t get_next_watch_line(watch_data *watch, char *buf, int len) {
	apr_status_t rv;
	while(allow_watching) {
		if(watch->connection == NULL) {
			watch->connection = mfs_pool_get_connection(watch->trackers, watch->tracker_index, NULL, true, apr_time_from_sec(1) / 4);
			if(watch->connection != NULL) {
				apr_size_t l = 8;
				if((rv = apr_socket_send(watch->connection->connection->socket, "!watch\r\n", &l)) != APR_SUCCESS) {
					mfs_log_apr(LOG_ERR, rv, NULL, "Failed to send !watch command:");
				}
			}
		}
		if(watch->connection != NULL) {
			rv = sock_readline(buf, len, watch);
			if(rv != APR_SUCCESS) {
				mfs_log_apr(LOG_ERR, rv, NULL, "Failed to readline for watch:");
				//destroy the failed connection...
				mfs_pool_destroy_connection(watch->connection);
				watch->connection = NULL;
			} else {
				return APR_SUCCESS;
			}
		}
		//failure.. try again in a second....
		if(allow_watching) {
			apr_sleep(apr_time_from_sec(1));
		}
	}
	return APR_EGENERAL;
}

apr_status_t get_next_watch_cache_line(watch_data *watch, char *buf, int len) {
	apr_status_t rv;
	char * c_start;
	while(allow_watching) {
		if((rv = get_next_watch_line(watch, buf, len)) == APR_SUCCESS) {
			c_start = strstr(buf, "[cache][");
			if(c_start != NULL) {
				c_start = c_start + 8;
				int pos = strcspn(c_start, "] ");
				if((watch->client_id == NULL)||(pos != watch->client_id_length)||(memcmp(watch->client_id,c_start, pos) != 0)) {
					c_start = c_start + pos + 2;
					memmove(buf, c_start, strlen(c_start) + 1);
					return APR_SUCCESS;
				}
			}
		} else {
			return rv;
		}
	}
	return APR_EGENERAL;

}
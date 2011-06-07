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
 
#include "mogile_fs.h"
#include "logger.h"
#include <apr_strings.h>
#include <apr_errno.h>
#include <ctype.h>


#define MFS_CONNECTION_TIMEOUT 1
#define MFS_READ_BUFFER_SIZE 4096

apr_status_t mfs_tracker_init(char *address, int port, apr_pool_t *pool, tracker_info **tracker) {
	tracker_info *t;
	t = apr_palloc(pool, sizeof(tracker_info));
	*tracker = t;
	return mfs_tracker_init2(address, port, pool, t);
}

apr_status_t mfs_tracker_init2(char *address, int port, apr_pool_t *pool, tracker_info *tracker) {
	//lets try and make sure the address is ok first...
	apr_sockaddr_t * sa = NULL;
	apr_status_t rv = apr_sockaddr_info_get(&sa, address,APR_INET, port, 0, pool);
	if(rv != APR_SUCCESS) {
		mfs_log(LOG_CRIT, "Unable to create connection information from address '%s' and port %d", address, port);
		return rv;
	}
	tracker->address = apr_pstrdup(pool, address);
	tracker->port = port;
	tracker->sa = sa;
	return rv;
}

apr_status_t mfs_tracker_connect(tracker_info *tracker, tracker_connection ** connection, apr_pool_t *pool, apr_interval_time_t timeout) {
	mfs_log(LOG_DEBUG, "connecting to tracker %s:%d", tracker->address, tracker->port);

	apr_socket_t *s;
	apr_status_t rv = apr_socket_create(&s, APR_INET, SOCK_STREAM, APR_PROTO_TCP, pool);
	if(rv != APR_SUCCESS) {
		char err[100];
		apr_strerror(rv,err,100); 	
		mfs_log(LOG_CRIT, "Unable to create socket: %s", err);
		return rv;
	}
	apr_socket_timeout_set(s, timeout);

	rv = apr_socket_connect(s, tracker->sa);
	if(rv != APR_SUCCESS) {
		char err[100];
		apr_strerror(rv,err,100); 
		mfs_log(LOG_ERR, "Unable to connect to %s: %s", tracker->address, err);
		return rv;
	}
	tracker_connection *tc = apr_palloc(pool, sizeof(tracker_connection));
	tc->tracker = tracker;
	tc->connected = true;
	tc->socket = s;
	tc->pool = pool;
	*connection = tc;
	return APR_SUCCESS;
}

tracker_request_parameters * mfs_tracker_init_parameters(apr_pool_t *pool) {
	return (tracker_request_parameters*) apr_pcalloc(pool,sizeof(tracker_request_parameters));
}
void mfs_tracker_add_parameter(tracker_request_parameters *parameters, const char *key,  const char *value, apr_pool_t *pool) {
	int key_length, value_length;
	char *encoded_key = mfs_tracker_url_encode(key, pool, &key_length);
	char *encoded_value = mfs_tracker_url_encode(value, pool, &value_length);
	mfs_tracker_add_parameter_pointers(parameters, encoded_key , key_length, encoded_value, value_length, pool);
}
//add key/value parameters: uses values (dont deallocate them).
//they should already be url_encoded
void mfs_tracker_add_parameter_pointers(tracker_request_parameters *parameters, char *key, int key_length, char *value, int value_length, apr_pool_t *pool) {
	tracker_request_parameter * parameter = (tracker_request_parameter *)apr_pcalloc(pool,sizeof(tracker_request_parameter));
	parameters->count ++;
	parameters->strlen += (key_length + value_length + 1); //+1 for '='
	parameter->value = value;
	parameter->value_length = value_length;
	parameter->key = key;
	parameter->key_length = key_length;
	if(parameters->head == NULL) {
		parameters->head = parameter;
	} else {
		//append to linked list...
		tracker_request_parameter * tail = parameters->head;
		while(tail->next != NULL) {
			tail = tail->next;
		}
		tail->next = parameter;
	}	
}

void mfs_tracker_add_meta_data(tracker_request_parameters *parameters, const char *key,  const char *value, bool prepare, apr_pool_t *pool) {
	if(prepare) {
		char *tmp_key = apr_psprintf(pool, "plugin.meta.key%d",parameters->meta_count);
		mfs_tracker_add_parameter(parameters, tmp_key, key, pool);
		tmp_key = apr_psprintf(pool, "plugin.meta.value%d",parameters->meta_count);
		mfs_tracker_add_parameter(parameters, tmp_key, value, pool);
		parameters->meta_count++;
	} else {
		tracker_request_parameter * parameter = (tracker_request_parameter *)apr_pcalloc(pool,sizeof(tracker_request_parameter));
		parameters->count ++;
		//parameters->strlen += (key_length + value_length + 1); if !prepare, this parameters list will not be used directly...
		parameter->value = apr_pstrdup(pool, value);
		parameter->value_length = 0;
		parameter->key = apr_pstrdup(pool, key);
		parameter->key_length = 0;
		parameter->is_metadata = true;
		if(parameters->head == NULL) {
			parameters->head = parameter;
		} else {
			//append to linked list...
			tracker_request_parameter * tail = parameters->head;
			while(tail->next != NULL) {
				tail = tail->next;
			}
			tail->next = parameter;
		}
		parameters->meta_count++;
	}
}

void mfs_tracker_copy_parameter_pointers(tracker_request_parameters *src, tracker_request_parameters *dest, apr_pool_t *pool) {
	tracker_request_parameter * param = src->head;
	while(param != NULL) {
		if(param->is_metadata) {
			mfs_tracker_add_meta_data(dest, param->key, param->value, true, pool);
		} else {
			mfs_tracker_add_parameter_pointers(dest, param->key, param->key_length, param->value, param->value_length, pool);
		}
		param = param->next;
	}
}

apr_status_t mfs_tracker_request(tracker_connection *connection, char * cmd, tracker_request_parameters * parameters, bool *ok, apr_hash_t *result, apr_pool_t *pool, apr_interval_time_t timeout) {
	apr_size_t request_size;
	apr_socket_timeout_set(connection->socket, timeout);
	char *request = mfs_tracker_build_request(cmd, parameters, pool, &request_size);
	apr_status_t rv = apr_socket_send(connection->socket,request, &request_size);
	if(rv != APR_SUCCESS) {
		char err[100];
		apr_strerror(rv,err,100); 
		mfs_log(LOG_ERR, "Unable to send %s request to %s:%d: %s", cmd, connection->tracker->address, connection->tracker->port, err);
		return rv;
	}
	//we now want to read until \r\n
	//use a 1000 byte stack buffer ro read...
	char buf[MFS_READ_BUFFER_SIZE];
	char * final_buffer = NULL; //used when more > MFS_READ_BUFFER_SIZE bytes
	int final_buffer_size = 0;
	apr_size_t response_size;
	
	bool finished = false;
	while(!finished) {
		response_size = MFS_READ_BUFFER_SIZE;
		rv = apr_socket_recv(connection->socket,buf, &response_size);
		if(rv != APR_SUCCESS) {
			char err[100];
			apr_strerror(rv,err,100); 
			mfs_log(LOG_ERR, "Unable to receive %s response to %s:%d: %s", cmd, connection->tracker->address, connection->tracker->port, err);
			return rv;
		}
		if((response_size > 0)&&(buf[response_size-1] == '\n') && (final_buffer == NULL)) { //finished
			final_buffer = buf; //we can use the stack memory!
			final_buffer_size = response_size;
			finished = true;
		} else if(response_size==0) {
			//something went wrong!
			mfs_log(LOG_ERR, "Unable to receive %s response to %s: 0 sized reply. multi_buffer_size=%d", cmd, connection->tracker->address, final_buffer_size);
			return APR_EOF;
		} else {
			mfs_log(LOG_DEBUG, "Response to %s is over MFS_READ_BUFFER_SIZE. multi_buffer_size=%d, response_size=%d", cmd, final_buffer_size, response_size);
			//we now have to dynamically allocate buffers...damn!
			if(final_buffer == NULL) {
				final_buffer =  apr_pmemdup(pool,buf, response_size);
				final_buffer_size = response_size;
			} else {
				char *multi_buffer = (char *)apr_palloc(pool,response_size + final_buffer_size);
				memcpy(multi_buffer, final_buffer, final_buffer_size);
				final_buffer = multi_buffer; //we can abandon  final_buffer because of pools
				multi_buffer += final_buffer_size;
				memcpy(multi_buffer, buf, response_size);
				final_buffer_size += response_size;
			}
			if(final_buffer[final_buffer_size-1] == '\n') {
				finished = true;
			}
		}
	}
	//lets replace the terminating \r\n with a \0
	if((final_buffer_size > 1)&&(final_buffer[final_buffer_size-2]=='\r')) {
		final_buffer[final_buffer_size-2] = '\0';
	} else {
		mfs_log(LOG_ERR, "Invalid reponse terminator");
		return APR_EFTYPE;
	}
	//we now have a buffer for the line... lets parse it!
	return mfs_tracker_parse_response(final_buffer, final_buffer_size, ok, result, pool);
}

/* Converts a hex character to its integer value */
char from_hex(char ch) {
  return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

/* Converts an integer value to its hex character*/
char to_hex(char code) {
  static char hex[] = "0123456789abcdef";
  return hex[code & 15];
}

char * mfs_tracker_url_encode(const char *raw, apr_pool_t *pool, int *length) {
	char *pstr = (char *)raw, *buf = apr_palloc(pool, strlen(raw) * 3 + 1), *pbuf = buf;
	int l=0;
	while (*pstr) {
		if (isalnum(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~') {
			*pbuf++ = *pstr;
			l++;
		} else if (*pstr == ' ') {
			*pbuf++ = '+';
			l++;
		} else {
			*pbuf++ = '%', *pbuf++ = to_hex(*pstr >> 4), *pbuf++ = to_hex(*pstr & 15);
			l+= 3;
		}
		pstr++;
	}
	*pbuf = '\0';
	*length = l;
	return buf;
}

char * mfs_tracker_url_decode(const char *encoded, apr_pool_t *pool) {
	char *pstr = (char *)encoded, *buf = apr_palloc(pool, strlen(encoded) + 1), *pbuf = buf;
	while (*pstr) {
		if (*pstr == '%') {
			if (pstr[1] && pstr[2]) {
				*pbuf++ = from_hex(pstr[1]) << 4 | from_hex(pstr[2]);
				pstr += 2;
			}
		} else if (*pstr == '+') { 
			*pbuf++ = ' ';
		} else {
			*pbuf++ = *pstr;
		}
		pstr++;
	}
	*pbuf = '\0';
	return buf;
}

char * mfs_tracker_build_request(char * cmd, tracker_request_parameters * parameters, apr_pool_t *pool, apr_size_t *size) {
	int cmd_length = strlen(cmd);
	//{cmd} {param1}&{param2}&{paramn}\r\n
	
	int total_length = cmd_length + 3 + parameters->strlen + parameters->count -1; //+3 for space and \r\n , -1 because & is between params
	char *meta_count_param;
	int meta_count_param_length;
	if(parameters->meta_count > 0) {
		meta_count_param = apr_psprintf(pool, "plugin.meta.keys=%d&",parameters->meta_count);
		meta_count_param_length = strlen(meta_count_param);
		total_length += meta_count_param_length;
	}
	
	*size = total_length;
	char* request = apr_palloc(pool, total_length + 1); // + 1 for null terminator
	request[0] = '\0';
	char *cat_pos = request;
	strcpy(request, cmd);
	cat_pos += cmd_length;
	*cat_pos ++ = ' ';
	if(parameters->meta_count > 0) {
		strcpy(cat_pos, meta_count_param);
		cat_pos += meta_count_param_length;
	}
	tracker_request_parameter * param = parameters->head;
	while(param != NULL) {
		strcpy(cat_pos, param->key);
		cat_pos += param->key_length;
		*cat_pos ++ = '=';
		strcpy(cat_pos, param->value);
		cat_pos += param->value_length;
		if(param->next != NULL)
			*cat_pos ++ = '&';
		param = param->next;
	}
	*cat_pos ++ = '\r';
	*cat_pos ++ = '\n';
	*cat_pos ++ = '\0';
	
	return request;
}

int skip_char(char *buf, int buf_size, char c) {
	int buf_count = 0;
	while(buf_count < buf_size && buf[buf_count] == c) buf_count++;
	if(buf_count == buf_size) return -1;
	return buf_count;
}

int skip_space_or_number(char *buf, int buf_size) {
	int buf_count = 0;
	while(buf_count < buf_size && (buf[buf_count] == ' ' || (buf[buf_count] >= '0' && buf[buf_count] <='9')) ) buf_count++;
	if(buf_count == buf_size) return -1;
	return buf_count;
}

int skip_not_char(char *buf, int buf_size, char c) {
	int buf_count = 0;
	while(buf_count < buf_size && buf[buf_count] != c) buf_count++;
	if(buf_count == buf_size) return -1;
	return buf_count;
}

int skip_not_char_in_str(char *buf, char c) {
	int buf_count = 0;
	while(buf[buf_count] != c && buf[buf_count] != '\0') buf_count++;
	if(buf[buf_count] == '\0') return -1;
	return buf_count;
}

apr_status_t mfs_tracker_parse_response(char *final_buffer, int final_buffer_size, bool *ok, apr_hash_t *result, apr_pool_t *pool) {
	//format is either OK FID URL_ENCODED_PARAMETERS
	// or ERR ERROR_CODE ERROR_STRING
	if(final_buffer_size < 4) {
		mfs_log(LOG_ERR, "Invalid reponse length (%d)", final_buffer_size);
		return APR_EFTYPE;
	}
	apr_pool_t *result_pool = apr_hash_pool_get(result);
	if(memcmp("OK ", final_buffer, 3) == 0) { //OK!
		char *buf = (char *)final_buffer + 3;
		int buf_left = final_buffer_size - 3;
		int next_pos = skip_space_or_number(buf, buf_left);
		/*if(next_pos == -1) {
			mfs_log(LOG_ERR, "Invalid OK reponse: no space/number returned after OK");
			return APR_EFTYPE;
		}
		buf += next_pos;
		buf_left -= next_pos;
		next_pos = skip_not_char(buf, buf_left, ' ');
		if(next_pos == -1) {
			mfs_log(LOG_ERR, "Invalid OK reponse: no space returned after FID: %s", apr_pstrndup (pool, final_buffer, final_buffer_size));
			return APR_EFTYPE;
		}
		buf += next_pos;
		buf_left -= next_pos;
		next_pos = skip_char(buf, buf_left, ' ');*/
		if(next_pos == -1) {
			mfs_log(LOG_ERR, "Invalid OK reponse: no URL_ENCODED_PARAMETERS");
			return APR_EFTYPE;
		}
		buf += next_pos;
		buf_left -= next_pos;
		//the rest is x=y&a=b (URL_ENCODED_PARAMETERS)
		char *tok_state=NULL;
		char *pair = apr_strtok(buf, "&", &tok_state);
		char *val_buf;
		while(pair != NULL) {
			next_pos = skip_not_char_in_str(pair, '=');
			if(next_pos == -1) {
				mfs_log(LOG_ERR, "Invalid OK reponse: URL_ENCODED_PARAMETER missing '='");
				return APR_EFTYPE;
			}
			val_buf = pair + next_pos + 1;
			//pair[next_pos] = '\0';
			apr_hash_set(result,apr_pmemdup(result_pool, pair, next_pos),next_pos, mfs_tracker_url_decode(val_buf, result_pool)); //we use the pool of the result hash to make sure the values stay in same scope
			pair = apr_strtok(NULL, "&", &tok_state);
		}
		*ok = true;	
	} else if((final_buffer_size > 4) && (memcmp("ERR ", final_buffer, 4) == 0)) { //ERROR!
		char *buf = (char *)final_buffer + 4;
		int buf_left = final_buffer_size - 4;
		int next_pos = skip_char(buf, buf_left, ' ');
		if(next_pos == -1) {
			mfs_log(LOG_ERR, "Invalid ERR reponse (only spaces after ERR)");
			return APR_EFTYPE;
		}
		buf += next_pos;
		buf_left -= next_pos;
		char *error_code = buf;

		next_pos = skip_not_char(buf, buf_left, ' ');
		if(next_pos == -1) {
			mfs_log(LOG_ERR, "Invalid ERR reponse: no space returned after ERROR_CODE");
			return APR_EFTYPE;
		}
		buf += (next_pos + 1); 
		buf_left -= (next_pos + 1);
		error_code[next_pos] = '\0';

		next_pos = skip_char(buf, buf_left, ' ');
		if(next_pos == -1) {
			mfs_log(LOG_ERR, "Invalid ERR reponse (only spaces after ERROR_CODE)");
			return APR_EFTYPE;
		}
		buf += next_pos;
		buf_left -= next_pos;

		//the rest of the string is the error message...

		apr_hash_set(result,MFS_TRACKER_ERROR_CODE,10, mfs_tracker_url_decode(error_code, apr_hash_pool_get(result)));
		apr_hash_set(result,MFS_TRACKER_ERROR_DESC,10, mfs_tracker_url_decode(buf, apr_hash_pool_get(result)));
		
		*ok = false;
	} else {
		final_buffer[3] = '\0';
		mfs_log(LOG_ERR, "Invalid reponse start (%s)", final_buffer);
		return APR_EFTYPE;
	}
	return APR_SUCCESS;
}


void mfs_tracker_destroy_connection(tracker_connection *connection) {
	if(connection->socket != NULL) {
		apr_socket_close(connection->socket); //dont care about the result..
	}
	apr_pool_destroy(connection->pool); 
}
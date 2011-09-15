/*
 * Copyright (C) Mark Pentland 2010 <mark.pent@gmail.com>
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
#include <stdlib.h>
#include <stdio.h>
#include <curl/curl.h>
#include <sys/stat.h>
#include <fcntl.h>

//cURL does not do 'last successfully transfered' type timeouts... its timed based on total transfer time (even if everything is going ok)... odd
//this will track the last time a successful transfer occured and timeout if that time is too long ago...
typedef struct {
	apr_time_t start_time;
	apr_time_t last_transferred_at;
	apr_interval_time_t timeout;
	double last_transfer_total;
} mfs_timeout_tracker;

int mfs_curl_upload_timeout_checker(void *clientp,
                                      double dltotal,
                                      double dlnow,
                                      double ultotal,
                                      double ulnow) 
{
	mfs_timeout_tracker *tt = (mfs_timeout_tracker*)clientp;
	double amount = ultotal - tt->last_transfer_total;
	tt->last_transfer_total = ultotal;
	apr_time_t now =  apr_time_now();
	if(amount > 0) { //something was transferred since last test... all good nothing to see here...
		tt->last_transferred_at = now;
	} else {//nothing transferred... have we timeout out?	

		if(tt->last_transferred_at == 0) { //double the time because it will include connection times...
			apr_interval_time_t elapsed = now - tt->start_time;
			if(elapsed >= (tt->timeout * 2)) {
				mfs_log(LOG_ERR, "Timeout reached on upload: inactive %d ms from connection start", (apr_int32_t)apr_time_as_msec(elapsed));
				return -1;
			}
		} else {
			apr_interval_time_t elapsed = now - tt->start_time;
			if(elapsed >= tt->timeout) {
				mfs_log(LOG_ERR, "Timeout reached on upload: inactive %d ms from last upload of data", (apr_int32_t)apr_time_as_msec(elapsed));
				return -1;
			}
		}
	}
	return 0;
}

typedef struct {
	char *buffer;
	size_t length;
	size_t pos;
} mfs_read_buffer;

size_t mfs_buffer_put_read_callback(void *ptr, size_t size, size_t nmemb, void *stream) {
	
	mfs_read_buffer *rbuf = (mfs_read_buffer*)stream;
	size_t retcode;
	size_t rem_size = rbuf->length - rbuf->pos;
	//mfs_log(LOG_ERR, "%d: mfs_buffer_put_read_callback: size=%d, nmemb=%d, rbuf->length=%d, rbuf->pos=%d, rem_size=%d", 
	//        (apr_int32_t)apr_time_as_msec(apr_time_now()), size, nmemb, rbuf->length, rbuf->pos, rem_size);

	if(rem_size == 0) {
		retcode = 0; //EOF
	} else {
		size_t req_size = size * nmemb;
		if(rem_size > req_size) {
			retcode = req_size;
		} else {
			//do we need to transfer aligned to size? (for now no)
//			retcode = (rem_size / size) * size; //this will round it to size
			retcode = rem_size;
		}
		memcpy(ptr, rbuf->buffer + rbuf->pos, retcode);
		rbuf->pos += retcode;
	}
	//mfs_log(LOG_ERR, "retcode=%d", retcode);
	return retcode;
}

size_t mfs_file_put_read_callback(void *ptr, size_t size, size_t nmemb, void *stream) {
	apr_file_t *file = (apr_file_t *)stream;

	apr_status_t rv;
	apr_size_t nbytes = size * nmemb;
	
	if((rv = apr_file_read(file, ptr, &nbytes)) != APR_SUCCESS) {
		if(rv == APR_EOF) {
			return 0;
		} else {
			mfs_log_apr(LOG_ERR, rv, NULL, "Error reading file when sending to server:");
			return CURL_READFUNC_ABORT;
		}
	}
	return nbytes;
}

size_t mfs_buffer_put_write_callback( void *ptr, size_t size, size_t nmemb, void *stream) {
	//just swallow the response...we dont want it going to stdout...
	return size * nmemb;
}

int mfs_buffer_put_read_seek(void *instream, curl_off_t offset, int origin) {
	mfs_log(LOG_ERR, "Seek called on curl callback (upload)");
	return CURL_SEEKFUNC_CANTSEEK;
}

apr_status_t mfs_file_server_put(mfs_file_system *file_system, apr_uri_t *uri, char *original_uri, void *bytes, long *total_bytes, apr_file_t *file, apr_pool_t *pool) {
	struct curl_slist *headerlist=NULL;
  	static const char expect_buf[] = "Expect:";
	
	
	apr_status_t rv;
	mfs_file_server *file_server;
	if((rv = mfs_get_file_server(file_system, uri, &file_server) != APR_SUCCESS)) {
		mfs_log_apr(LOG_ERR, rv, pool, "Unable to get file server for %s:", uri->hostinfo);
		return rv;
	}
	mfs_http_connection *conn; 
	if((rv = apr_reslist_acquire(file_server->connections, (void**)&conn)) != APR_SUCCESS) {
		mfs_log_apr(LOG_ERR, rv, pool, "Unable to get file server connection for %s:", uri->hostinfo);
		return rv;
	}
	//curl_easy_setopt(conn->curl, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(conn->curl, CURLOPT_URL,original_uri);
	/* tell it to "upload" to the URL */
    curl_easy_setopt(conn->curl, CURLOPT_UPLOAD, 1L);
	curl_easy_setopt(conn->curl, CURLOPT_PUT, 1L);
	curl_easy_setopt(conn->curl, CURLOPT_WRITEFUNCTION, mfs_buffer_put_write_callback);
	curl_easy_setopt(conn->curl, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(conn->curl, CURLOPT_SEEKFUNCTION, mfs_buffer_put_read_seek);
	curl_easy_setopt(conn->curl, CURLOPT_SEEKDATA, NULL);
	
	curl_easy_setopt(conn->curl, CURLOPT_CONNECTTIMEOUT_MS, apr_time_as_msec(file_system->file_server_timeout));

	//curl_easy_setopt(conn->curl, CURLOPT_PROXY, "127.0.0.1:8888");
	
	//curl_easy_setopt(conn->curl, CURLOPT_TIMEOUT_MS, apr_time_as_msec(file_system->file_server_timeout));

	mfs_timeout_tracker *tt = apr_palloc(pool, sizeof(mfs_timeout_tracker));
	tt->start_time = apr_time_now();
	tt->last_transferred_at = 0;
	tt->timeout = file_system->file_server_timeout;
	tt->last_transfer_total=0;
	curl_easy_setopt(conn->curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(conn->curl, CURLOPT_PROGRESSFUNCTION, mfs_curl_upload_timeout_checker);
	curl_easy_setopt(conn->curl, CURLOPT_PROGRESSDATA, tt);

		
	curl_easy_setopt(conn->curl, CURLOPT_NOSIGNAL, 1L);
	
	
	//we now have a curl handle...time to put the file/buffer....
	if(bytes != NULL) {
		mfs_read_buffer * rbuf = apr_palloc(pool, sizeof(mfs_read_buffer));
		rbuf->buffer = bytes;
		rbuf->length = *total_bytes;
		rbuf->pos = 0;
		curl_easy_setopt(conn->curl, CURLOPT_READFUNCTION, mfs_buffer_put_read_callback);
		curl_easy_setopt(conn->curl, CURLOPT_READDATA, rbuf);
	} else {
		if(*total_bytes == -1) { //figure it out from the FILE handle...
			apr_finfo_t finfo;
			if((rv = apr_file_info_get(&finfo, APR_FINFO_SIZE ,file)) != APR_SUCCESS) {
				mfs_log_apr(LOG_ERR, rv, NULL, "Failed to get file size of file %s:", original_uri);
				return APR_ENOSTAT;
			}
			*total_bytes = finfo.size;
		}
		curl_easy_setopt(conn->curl, CURLOPT_READFUNCTION, mfs_file_put_read_callback);
		curl_easy_setopt(conn->curl, CURLOPT_READDATA, file);
	}
	//turn off expect-100 header as this causes a timeout on some backend servers (perlbal for instance)
	headerlist = curl_slist_append(headerlist, expect_buf);
	curl_easy_setopt(conn->curl, CURLOPT_HTTPHEADER, headerlist);
	curl_easy_setopt(conn->curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)*total_bytes);
	//mfs_log(LOG_ERR, "curl_easy_perform(%s, %d)", original_uri, (apr_int32_t)apr_time_as_msec(apr_time_now()));
	CURLcode res = curl_easy_perform(conn->curl);
	//mfs_log(LOG_ERR, "DONE curl_easy_perform(%s, %d)", original_uri, (apr_int32_t)apr_time_as_msec(apr_time_now()));
	curl_slist_free_all (headerlist);
	curl_easy_setopt(conn->curl, CURLOPT_HTTPHEADER, NULL); //we euse connections... clean this up!
	if(res != CURLE_OK) {
		mfs_log(LOG_ERR, "Error uploading to %s:%s (%d)", original_uri, curl_easy_strerror(res), res);
		if((rv = apr_reslist_invalidate(file_server->connections, conn)) != APR_SUCCESS) {
			mfs_log_apr(LOG_ERR, rv, pool, "Unable to invalidate file server connection for %s:", uri->hostinfo);
			return rv;
		}
		rv = APR_EGENERAL;
	} else {
		//mfs_log(LOG_ERR, "curl_easy_perform finished OK");
		if((rv = apr_reslist_release(file_server->connections, conn)) != APR_SUCCESS) {
			mfs_log_apr(LOG_ERR, rv, pool, "Unable to release file server connection for %s:", uri->hostinfo);
			//return rv; we still succeeded... lets continue...
		}
		rv = APR_SUCCESS;
	}
	return rv;
}

//internal method...
apr_status_t mfs_store_file_or_bytes(mfs_file_system *file_system, const char *domain, const char *key, const char *storage_class, apr_pool_t *pool, void *bytes, long total_bytes, apr_file_t *file, tracker_request_parameters * extra_open_parameters, tracker_request_parameters * extra_close_parameters) {
	bool manage_pool;
	apr_status_t rv = APR_SUCCESS;
	if(pool == NULL) {
		manage_pool = true;
		if((rv = apr_pool_create(&pool,NULL)) != APR_SUCCESS) {
			mfs_log(LOG_CRIT, "Unable to create apr_pool");
			return rv;
		}
	} else {
		manage_pool = false;
	}
	bool ok;

	tracker_request_parameters * params = mfs_tracker_init_parameters(pool);
	if(extra_open_parameters != NULL) {
		mfs_tracker_copy_parameter_pointers(extra_open_parameters, params, pool);
	}
	mfs_tracker_add_parameter(params, "domain",  domain, pool);
	mfs_tracker_add_parameter(params, "class",  storage_class, pool);
	mfs_tracker_add_parameter(params, "key",  key, pool);
	
	apr_hash_t *result = apr_hash_make(pool);
	
	int attempt_count=0;
	int put_count = 0;
	bool call_tracker = true; //we will recall the tracker every 2 attempts...
	char *put_url, *fid, *devid;
	apr_uri_t uri;
	rv = APR_EGENERAL;
	while((attempt_count < file_system->max_retries)&&(rv != APR_SUCCESS)) {
		rv = APR_SUCCESS;
		if(call_tracker) {
			rv = mfs_request_do(file_system->trackers, "create_open", params, &ok, result, pool, file_system->tracker_timeout);
			if(rv == APR_SUCCESS) {
				if(ok) {
					put_url = apr_hash_get(result, "path", APR_HASH_KEY_STRING);
					if(put_url == NULL) {
						mfs_log(LOG_ERR, "Sucessful create.open did not return a path");
						rv = APR_EGENERAL;
					} else {
						fid = apr_hash_get(result, "fid", APR_HASH_KEY_STRING);
						if(fid == NULL) {
							mfs_log(LOG_ERR, "Sucessful create.open did not return a fid");
							rv = APR_EGENERAL;
						} else {
							devid = apr_hash_get(result, "devid", APR_HASH_KEY_STRING);
							if(devid == NULL) {
								mfs_log(LOG_ERR, "Sucessful create.open did not return a devid");
								rv = APR_EGENERAL;
							} else {
								call_tracker = false; //if we have to try again we wont call the tracker...
							}
						}
					}
				} else {
					//we cant create files?.. lets retry for now...
					mfs_log(LOG_ERR, "Tracker returned error %s (%s) when calling create.open for key %s", apr_hash_get(result, MFS_TRACKER_ERROR_CODE, APR_HASH_KEY_STRING), apr_hash_get(result, MFS_TRACKER_ERROR_DESC, APR_HASH_KEY_STRING), key );
					rv = APR_EGENERAL;
				}
			}
		} else {
			call_tracker = true; //if we have to try again we will call the tracker...
		}
		if(rv == APR_SUCCESS) {
			//ok, we have the path etc in result, lets try and put the file...
			if((rv = apr_uri_parse(pool, put_url, &uri)) != APR_SUCCESS) {
				mfs_log_apr(LOG_ERR, rv, pool, "Unable to parse put_url %s:", put_url);
				rv = APR_EGENERAL;
			} else if((uri.hostinfo == NULL)||(uri.scheme == NULL)||(uri.path==NULL)) {
				mfs_log(LOG_ERR, "Unable to parse put_url %s:", put_url);
				rv = APR_EGENERAL;
			} else {
				if(/*(put_count>0)&&*/(file != NULL)) { //need to rewind the file to the start...(cant assume it will be at start...)
					apr_off_t off;
					off=0;
					if((rv = apr_file_seek(file,APR_SET, &off)) != APR_SUCCESS) {
						mfs_log_apr(LOG_ERR, rv, NULL, "Error rewinding in file %s:", key);
						return rv;
					}
				}
				if((rv = mfs_file_server_put(file_system, &uri, put_url, bytes, &total_bytes, file, pool)) != APR_SUCCESS) {
					mfs_log(LOG_ERR, "Failed to put file to %s. Attempt count = %d", put_url, attempt_count);
				}
				put_count++;
			}
		}
		if(rv == APR_SUCCESS) { //we have successfully upload the file to the fileserver... lets let the tracker know this...
			tracker_request_parameters * close_params = mfs_tracker_init_parameters(pool);
			if(extra_close_parameters != NULL) {
				mfs_tracker_copy_parameter_pointers(extra_close_parameters, close_params, pool);
			}
			mfs_tracker_add_parameter(close_params, "fid",  fid, pool);
			mfs_tracker_add_parameter(close_params, "devid",  devid, pool);
			mfs_tracker_add_parameter(close_params, "size", apr_ltoa(pool, total_bytes), pool);
			mfs_tracker_add_parameter(close_params, "domain",  domain, pool);
			mfs_tracker_add_parameter(close_params, "path",  put_url, pool);
			mfs_tracker_add_parameter(close_params, "key",  key, pool);
			if(file_system->client_id != NULL) {
				mfs_tracker_add_parameter(close_params, "client_id",  file_system->client_id, pool);
			}
			rv = mfs_request_do(file_system->trackers, "create_close", close_params, &ok, result, pool, file_system->tracker_timeout);
			if(rv == APR_SUCCESS) {
				if(!ok) {
					mfs_log(LOG_ERR, "Tracker returned error %s (%s) when calling create_close for key %s", apr_hash_get(result, MFS_TRACKER_ERROR_CODE, APR_HASH_KEY_STRING), apr_hash_get(result, MFS_TRACKER_ERROR_DESC, APR_HASH_KEY_STRING), key );
					//we cant commit created files?.. lets retry for now...
					rv = APR_EGENERAL;
				}
			}
		}
		attempt_count ++;
		if((attempt_count < file_system->max_retries)&&(rv != APR_SUCCESS)) {
			apr_sleep(file_system->retry_timeout);
		}
	}
	
	if(manage_pool) {
		apr_pool_destroy(pool); 
	}
	return rv;
}

apr_status_t mfs_store_bytes_ex(mfs_file_system *file_system, const char *domain, const char *key, const char *storage_class, apr_pool_t *pool, void *bytes, long total_bytes, tracker_request_parameters * extra_open_parameters, tracker_request_parameters * extra_close_parameters) {
	return mfs_store_file_or_bytes(file_system, domain, key, storage_class, pool, bytes, total_bytes, NULL, extra_open_parameters, extra_close_parameters);
}

apr_status_t mfs_store_bytes_filepath(mfs_file_system *file_system, const char *domain, const char *key, const char *storage_class, apr_pool_t *pool, void *bytes, long total_bytes, apr_time_t mtime) {
	//we need to add a mtime to the request
	tracker_request_parameters * extra_close_parameters = mfs_tracker_init_parameters(pool);
	//not using meta data for mtime anymore
//	mfs_tracker_add_meta_data(extra_close_parameters, "mtime", apr_psprintf(pool, "%" APR_TIME_T_FMT,apr_time_sec(mtime)) , false, pool);
	mfs_tracker_add_parameter(extra_close_parameters, "mtime", apr_psprintf(pool, "%" APR_TIME_T_FMT,apr_time_sec(mtime)) , pool);
	return mfs_store_file_or_bytes(file_system, domain, key, storage_class, pool, bytes, total_bytes, NULL, NULL, extra_close_parameters);
}

apr_status_t mfs_store_bytes(mfs_file_system *file_system, const char *domain, const char *key, const char *storage_class, apr_pool_t *pool, void *bytes, long total_bytes) {
	return mfs_store_file_or_bytes(file_system, domain, key, storage_class, pool, bytes, total_bytes, NULL, NULL, NULL);
}

apr_status_t mfs_store_file_ex(mfs_file_system *file_system, const char *domain, const char *key, const char *storage_class, apr_pool_t *pool, apr_file_t *file, tracker_request_parameters * extra_open_parameters, tracker_request_parameters * extra_close_parameters) {
	return mfs_store_file_or_bytes(file_system, domain, key, storage_class, pool, NULL, -1, file, extra_open_parameters, extra_close_parameters);
}

apr_status_t mfs_store_file_filepath(mfs_file_system *file_system, const char *domain, const char *key, const char *storage_class, apr_pool_t *pool, apr_file_t *file, apr_time_t mtime) {
	//we need to add a mtime to the request
	tracker_request_parameters * extra_close_parameters = mfs_tracker_init_parameters(pool);
	//not using meta data for mtime anymore
	//mfs_tracker_add_meta_data(extra_close_parameters, "mtime", apr_psprintf(pool, "%" APR_TIME_T_FMT,apr_time_sec(mtime)) , false, pool);
	mfs_tracker_add_parameter(extra_close_parameters, "mtime", apr_psprintf(pool, "%" APR_TIME_T_FMT,apr_time_sec(mtime)) , pool);
	return mfs_store_file_or_bytes(file_system, domain, key, storage_class, pool, NULL, -1, file, NULL, extra_close_parameters);
}

apr_status_t mfs_store_file(mfs_file_system *file_system, const char *domain, const char *key, const char *storage_class, apr_pool_t *pool, apr_file_t *file) {
	return mfs_store_file_or_bytes(file_system, domain, key, storage_class, pool, NULL, -1, file, NULL, NULL);
}
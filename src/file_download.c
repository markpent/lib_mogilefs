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


typedef struct {
	apr_bucket_brigade *brigade;
	bool dont_want_brigade; //if set to true and amount read exceeds mfs_file_system.max_buffer_size, put already read data into the file so it can be used as a file...
	apr_size_t current_size;
	apr_size_t file_size; //for when the file will be turned into a bucket if !dont_want_brigade
	apr_file_t *file; //if not null, we will stream the results to the FILE instead of the brigade
	mfs_file_system *file_system;
	apr_pool_t *pool;
	char *destination_file_path; //if not NULL, store the file here if above memory threshold
} mfs_write_buffer;


//cURL does not do 'last successfully transfered' type timeouts... its timed based on total transfer time (even if everything is going ok)... odd
//this will track the last time a successful transfer occured and timeout if that time is too long ago...
typedef struct {
	apr_time_t start_time;
	apr_time_t last_transferred_at;
	apr_interval_time_t timeout;
	double last_transfer_total;
} mfs_timeout_tracker;

int mfs_curl_download_timeout_checker(void *clientp,
                                      double dltotal,
                                      double dlnow,
                                      double ultotal,
                                      double ulnow) 
{
	mfs_timeout_tracker *tt = (mfs_timeout_tracker*)clientp;
	double amount = dltotal - tt->last_transfer_total;
	tt->last_transfer_total = dltotal;
	apr_time_t now =  apr_time_now();
	if(amount > 0) { //something was transferred since last test... all good nothing to see here...
		tt->last_transferred_at = now;
	} else {//nothing transferred... have we timeout out?	

		if(tt->last_transferred_at == 0) { //double the time because it will include connection times...
			apr_interval_time_t elapsed = now - tt->start_time;
			if(elapsed >= (tt->timeout * 2)) {
				mfs_log(LOG_ERR, "Timeout reached on download: inactive %d ms from connection start", (apr_int32_t)apr_time_as_msec(elapsed));
				return -1;
			}
		} else {
			apr_interval_time_t elapsed = now - tt->start_time;
			if(elapsed >= tt->timeout) {
				mfs_log(LOG_ERR, "Timeout reached on download: inactive %d ms from last download of data", (apr_int32_t)apr_time_as_msec(elapsed));
				return -1;
			}
		}
	}
	return 0;
}



//called by cURL when data is ready to read
size_t mfs_buffer_get_write_callback( void *ptr, size_t size, size_t nmemb, void *stream) {
	mfs_write_buffer *buf = (mfs_write_buffer*)stream;
	apr_status_t rv;
	apr_size_t bytes_written;
	apr_size_t total_size = (size * nmemb);
	if(buf->file != NULL) { //we are using a file to store the data
		if((rv = apr_file_write_full(buf->file, ptr, total_size, &bytes_written))!= APR_SUCCESS) {
			mfs_log_apr(LOG_ERR, rv, buf->pool, "Error writing to wbuf->file when streaming download:");
		}
		buf->file_size += total_size;
		buf->current_size += total_size;
	} else if(total_size + buf->current_size > buf->file_system->max_buffer_size) { //if we get here then file is NULL so this is the first time...
		if(buf->destination_file_path == NULL) {
			char filename[] = "mogile_fs_XXXXXX";
			if((rv = apr_file_mktemp(&buf->file, filename, APR_CREATE | APR_READ | APR_WRITE, buf->pool)) != APR_SUCCESS) {
				mfs_log_apr(LOG_ERR, rv, buf->pool, "Error opening tmp file when streaming download:");
				return 0;
			}
		} else {
			//we have to create the parent directory....
			char *last_slash = NULL;
			int i;
			for(i = strlen(buf->destination_file_path); ((i > 0) && (last_slash == NULL)); i--) {
				if(buf->destination_file_path[i] == '/') {
				    last_slash = buf->destination_file_path + i;
				    last_slash[0] = '\0';
				}
			}
			//we now have the parent directory...
			if((rv = apr_dir_make_recursive(buf->destination_file_path, 0x0777, buf->pool)) != APR_SUCCESS) {
				mfs_log_apr(LOG_ERR, rv, NULL, "Unable to create tmp file parent directory %s:",buf->destination_file_path);
			}
			//still try even if above fails..... it may not be fatal....
			last_slash[0] = '/';
			//create the file....
			if((rv = apr_file_open(&buf->file, buf->destination_file_path, APR_READ | APR_WRITE | APR_CREATE | APR_XTHREAD , 0x777, buf->pool)) != APR_SUCCESS) {
				mfs_log_apr(LOG_ERR, rv, buf->pool, "Error opening tmp file %s when streaming download:", buf->destination_file_path);
				return 0;
			}
		}
		//do we put the existing data into the file first?
		if(buf->dont_want_brigade) { //yes, otherwise the start of the file would only be in the brigade...
			apr_bucket *b;
			apr_size_t len;
			const char *str;
			for ( b = APR_BRIGADE_FIRST(buf->brigade);  b != APR_BRIGADE_SENTINEL(buf->brigade); b = APR_BUCKET_NEXT(b) ) {
				if((rv = apr_bucket_read(b, &str,&len,APR_BLOCK_READ)) !=APR_SUCCESS) {
					mfs_log_apr(LOG_ERR, rv, buf->pool, "Error reading from bucket to init tmp file:");
					return 0;
				}
				if((rv = apr_file_write_full(buf->file, str,len, NULL))!= APR_SUCCESS) {
					mfs_log_apr(LOG_ERR, rv, buf->pool, "Error writing to wbuf->file when init temp file:");
					return 0;
				}
				buf->file_size += len;
			}
		} else {
			//if we do want to return the brigade, we will add the file bucket at the end whe we know the size of the file..
		}
		//now append the current data...
		if((rv = apr_file_write_full(buf->file, ptr, size * nmemb, &bytes_written))!= APR_SUCCESS) {
			mfs_log_apr(LOG_ERR, rv, buf->pool, "Error writing to wbuf->file when streaming download:");
			return 0;
		}
		buf->file_size += total_size;
		buf->current_size += total_size;
	} else { //we are still in memory... add a memory bucket to the brigade...
		apr_bucket *b = apr_bucket_pool_create (apr_pmemdup(buf->pool, ptr, total_size), total_size, buf->pool, buf->brigade->bucket_alloc);
		APR_BUCKET_INSERT_AFTER(APR_BRIGADE_LAST(buf->brigade), b); //append the new bucket....
		bytes_written = total_size;
		buf->current_size += total_size;
	}
	return bytes_written; //if this does not == (size*nmemb) then something when wrong and cURL will abort the download...
}

int mfs_buffer_get_write_seek(void *instream, curl_off_t offset, int origin) {
	mfs_log(LOG_ERR, "Seek called on curl callback (download)");
	return CURL_SEEKFUNC_CANTSEEK;
}

//if the *file pointer is not NULL, it is assumed we want to store the data in the passed in file
//if the brigade is not NULL, it is assumed that is how the data will be returned
apr_status_t mfs_file_server_get(mfs_file_system *file_system, apr_uri_t *uri, char *original_uri, void **bytes, apr_size_t *total_bytes, apr_file_t **file, apr_bucket_brigade *brigade, apr_pool_t *pool, char *destination_file_path) {
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

	mfs_write_buffer *wbuf = apr_pcalloc(pool, sizeof(mfs_write_buffer));
	wbuf->file_system = file_system;
	wbuf->pool = pool;
	wbuf->destination_file_path = destination_file_path;
	apr_bucket *start_bucket; //used for cleanup of passed in brigade
	if(*file != NULL) { //caller wants the result in the file
		wbuf->brigade = NULL;
		wbuf->dont_want_brigade = true;
		wbuf->file = *file;
	} else if(brigade != NULL) { //a brigade was passed in so thats the output vector (httpd)
		wbuf->brigade = brigade;
		wbuf->dont_want_brigade = false;
		start_bucket = APR_BRIGADE_LAST(wbuf->brigade); //if we fail, everything after this will be removed
	} else {
		//we will use a temp brigade...
		wbuf->dont_want_brigade = true;
		wbuf->brigade = apr_brigade_create(pool, apr_bucket_alloc_create(pool));
	}
	
	
	
	curl_easy_setopt(conn->curl, CURLOPT_URL,original_uri);
	curl_easy_setopt(conn->curl, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(conn->curl, CURLOPT_WRITEFUNCTION, mfs_buffer_get_write_callback);
	curl_easy_setopt(conn->curl, CURLOPT_WRITEDATA, wbuf);

	curl_easy_setopt(conn->curl, CURLOPT_SEEKFUNCTION, mfs_buffer_get_write_seek);
	curl_easy_setopt(conn->curl, CURLOPT_SEEKDATA, wbuf);
	
	curl_easy_setopt(conn->curl, CURLOPT_CONNECTTIMEOUT_MS, apr_time_as_msec(file_system->file_server_timeout));
	curl_easy_setopt(conn->curl, CURLOPT_NOSIGNAL, 1L);

	mfs_timeout_tracker *tt = apr_palloc(pool, sizeof(mfs_timeout_tracker));
	tt->start_time = apr_time_now();
	tt->last_transferred_at = 0;
	tt->timeout = file_system->file_server_timeout;
	tt->last_transfer_total=0;
	curl_easy_setopt(conn->curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(conn->curl, CURLOPT_PROGRESSFUNCTION, mfs_curl_download_timeout_checker);
	curl_easy_setopt(conn->curl, CURLOPT_PROGRESSDATA, tt);

	//need to clear these values in case connection used for 
	curl_easy_setopt(conn->curl, CURLOPT_READFUNCTION, NULL);
	curl_easy_setopt(conn->curl, CURLOPT_READDATA, NULL);
	//curl_easy_setopt(conn->curl, CURLOPT_INFILESIZE_LARGE, 0);
	
	CURLcode res = curl_easy_perform(conn->curl);
	if(res != CURLE_OK) {
		mfs_log(LOG_ERR, "Error downloading from %s:%s (%d)", original_uri, curl_easy_strerror(res), res);
		if((rv = apr_reslist_invalidate(file_server->connections, conn)) != APR_SUCCESS) {
			mfs_log_apr(LOG_ERR, rv, pool, "Unable to invalidate file server connection for %s:", uri->hostinfo);
		} else {
			rv = APR_EGENERAL;
		}
	} else {
		if((rv = apr_reslist_release(file_server->connections, conn)) != APR_SUCCESS) {
			mfs_log_apr(LOG_ERR, rv, pool, "Unable to release file server connection for %s:", uri->hostinfo);
			//return rv; we still succeeded... lets continue...
		}
		rv = APR_SUCCESS;
	}
	if(rv == APR_SUCCESS) { //set the result size...
		*total_bytes = wbuf->current_size;
	}
	//cleanup...
	if(*file != NULL) { //caller wants the result in the file
		if(wbuf->current_size > 0) { //we wrote to the file... lets rewind it to be nice
			apr_off_t start_pos = 0;
			apr_status_t rv2 = apr_file_seek(wbuf->file,APR_SET, &start_pos);
			if(rv2 != APR_SUCCESS) {
				mfs_log_apr(LOG_ERR, rv2, pool, "Unable to rewind file after read of %s:", original_uri);
			}
		}
		//dont do anything of SUCCESS: the data is in the file
	} else if(brigade != NULL) { //a brigade was passed in so thats the output vector (httpd)
		if((rv != APR_SUCCESS)&&(wbuf->current_size > 0)) { //we wrote to the bucket... lets clean it up for the next attempt
			if(start_bucket == APR_BRIGADE_SENTINEL(brigade)) { //it was an empty brigade....
				apr_brigade_cleanup(brigade);   	
			} else {
				start_bucket = APR_BUCKET_NEXT(start_bucket); //we want to start deleting after start_bucket
				apr_bucket *del_bucket;
				while(start_bucket != APR_BRIGADE_SENTINEL(brigade)) {
					del_bucket = start_bucket;
					start_bucket = APR_BUCKET_NEXT(start_bucket);
					apr_bucket_delete(del_bucket);
				}
			}
		} else if(rv == APR_SUCCESS) {
			if(wbuf->file != NULL) {//we need to add the file as a file bucket....
				apr_bucket * b = apr_bucket_file_create(wbuf->file, 0, wbuf->file_size, pool, wbuf->brigade->bucket_alloc);
				APR_BUCKET_INSERT_AFTER(APR_BRIGADE_LAST(wbuf->brigade), b); //append the new bucket....
			}
		}
	} else { //we will use a temp brigade... 
		if(rv == APR_SUCCESS) {
			//did the response fit in memory?
			if(wbuf->file != NULL) { //a file was used to buffer data... 
				if(wbuf->current_size > 0) { //we wrote to the file... lets rewind it to be nice
					apr_off_t start_pos = 0;
					apr_status_t rv2 = apr_file_seek(wbuf->file,APR_SET, &start_pos);
					if(rv2 != APR_SUCCESS) {
						mfs_log_apr(LOG_ERR, rv2, pool, "Unable to rewind file after read of %s:", original_uri);
					}
				}
				*file = wbuf->file; //return the file pointer...
			} else {
				//we need to get the data out of the bucket brigade...
				rv = apr_brigade_pflatten(wbuf->brigade, (char**)bytes, total_bytes, pool);
				if(rv != APR_SUCCESS) {
					mfs_log_apr(LOG_ERR, rv, pool, "Unable to get response from bucket brigade for get %s:", original_uri);
				}
			}
		}
		apr_status_t rv2 = apr_brigade_destroy(wbuf->brigade);
		if(rv2 != APR_SUCCESS) {
			mfs_log_apr(LOG_ERR, rv2, pool, "Failed to destroy tmp bucket brigade after reading %s:", original_uri);
		}
	}
	return rv;
}

//internal method.. that the api calls that looks after tracker calling...
apr_status_t mfs_file_system_get(mfs_file_system *file_system, char *domain, char *key, void **bytes, apr_size_t *total_bytes, apr_file_t **file, apr_bucket_brigade *brigade, apr_pool_t *pool, char *destination_file_path, long requiredLength) {

	char **paths;
	int path_count;
	apr_status_t rv;
	if((rv = mfs_get_paths(file_system, domain, key, true, &paths, &path_count, pool)) != APR_SUCCESS) {
		mfs_log_apr(LOG_DEBUG, rv, pool, "Unable to get paths for %s.%s:", domain, key);
		return rv;
	}
	int i=0;
	for(i = 0; i < path_count; i++) {
		char *path = paths[i];
		apr_uri_t uri;
		if((rv = apr_uri_parse(pool, path, &uri)) != APR_SUCCESS) {
			mfs_log_apr(LOG_ERR, rv, pool, "%s: Unable to parse get_url %s:", key, path);
		} else if((uri.hostinfo == NULL)||(uri.scheme == NULL)||(uri.path==NULL)) {
			mfs_log(LOG_ERR, "%s: Unable to parse get_url %s:", key, path);
			rv = APR_EGENERAL;
		} else {
			if((rv = mfs_file_server_get(file_system, &uri, path, bytes, total_bytes, file, brigade, pool, destination_file_path)) != APR_SUCCESS) {
				mfs_log(LOG_ERR, "%s: Failed to get file from %s. Attempt count = %d/%d", key, path, i+1, path_count);
			} else {
				//we succeeded!.. lets make sure its the correct length (the file server can return a 0 length file..)
				if(requiredLength >= 0) {
					if((*total_bytes) != requiredLength) {
						mfs_log(LOG_ERR, "Failed to get file %s from %s because returned length (%d) does not match the required length (%d). Attempt count = %d/%d", key, path, (*total_bytes), requiredLength, i+1, path_count);
						rv = APR_EGENERAL;
					} else {
						if(i != 0) {
							mfs_log(LOG_ERR, "Fetched %s from %s Attempt count = %d", key, path, i+1);
						}
						return APR_SUCCESS;
					}
				} else {
					if(i != 0) {
						mfs_log(LOG_ERR, "Fetched %s from %s Attempt count = %d", key, path, i+1);
					}
					return APR_SUCCESS;
				}
			}
		}
	}
	return rv; //this will contain the last error code...

}

apr_status_t mfs_get_file(mfs_file_system *file_system, char *domain, char *key, apr_size_t *total_bytes, apr_file_t **file, apr_pool_t *pool, long requiredLength) {
	return mfs_file_system_get(file_system, domain, key, NULL, total_bytes, file, NULL, pool, NULL, requiredLength);
}

//return the file in a byte buffer if the file is small enough (file_system.max_buffer_size)
apr_status_t mfs_get_file_or_bytes(mfs_file_system *file_system, char *domain, char *key, apr_size_t *total_bytes, void **bytes, apr_file_t **file, apr_pool_t *pool, char *destination_file_path, long requiredLength) {
	return mfs_file_system_get(file_system, domain, key, bytes, total_bytes, file, NULL, pool, destination_file_path, requiredLength);
}

//store the file in a bucket brigade
apr_status_t mfs_get_brigade(mfs_file_system *file_system, char *domain, char *key, apr_size_t *total_bytes, apr_bucket_brigade *brigade, apr_pool_t *pool, long requiredLength) {
	apr_file_t *file = NULL;
	return mfs_file_system_get(file_system, domain, key, NULL, total_bytes, &file, brigade, pool, NULL, requiredLength);
}



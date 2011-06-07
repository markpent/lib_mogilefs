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

apr_status_t mfs_file_server_conn_constructor(void **resource, void *params, apr_pool_t *pool);
apr_status_t mfs_file_server_conn_destructor(void *resource, void *params, apr_pool_t *pool);


apr_status_t mfs_init_file_system(mfs_file_system ** file_system, tracker_pool *trackers) {
	curl_global_init(0);
	apr_pool_t *p;
	apr_status_t rv;
	if((rv = apr_pool_create(&p,NULL)) != APR_SUCCESS) {
		mfs_log(LOG_CRIT, "Unable to create apr_pool");
		return rv;
	}
	apr_thread_rwlock_t *lock;
	rv = apr_thread_rwlock_create(&lock,p);
	if(rv != APR_SUCCESS) {
		mfs_log_apr(LOG_CRIT, rv, p, "Unable to create apr_thread_rwlock_t:");
		return rv;
	}
	mfs_file_system * fs = apr_palloc(p, sizeof(mfs_file_system));

	fs->max_retries = DEFAULT_MAX_RETRIES;
	fs->retry_timeout = DEFAULT_RETRY_WAIT;
	fs->tracker_timeout = DEFAULT_TRACKER_TIMEOUT;
	fs->file_server_timeout = DEFAULT_FILE_SERVER_TIMEOUT;
	fs->trackers = trackers;
	fs->pool = p;
	fs->max_buffer_size = DEFAULT_MAX_BUFFER_SIZE;
	fs->lock = lock;
	fs->file_servers = apr_hash_make(p);
	fs->client_id = NULL;
	*file_system = fs;
	mfs_pool_start_maintenance_thread(trackers);
	return APR_SUCCESS;
}

void mfs_close_file_system(mfs_file_system *file_system) {
	if(file_system->trackers != NULL) {
		mfs_pool_stop_maintenance_thread(file_system->trackers);
	}

	apr_hash_index_t *hi;
	mfs_file_server *file_server;
	apr_status_t rv;
	
	for (hi = apr_hash_first(file_system->pool, file_system->file_servers); hi; hi = apr_hash_next(hi)) {
		apr_hash_this(hi, NULL, NULL, (void**)&file_server);
		int count=0;
		while(count < 10) {
	 		if((rv = apr_reslist_destroy(file_server->connections)) != APR_SUCCESS) {
				count ++;
				mfs_log_apr(LOG_ERR, rv, file_system->pool, "Failed to release file server connections (attempt=%d)", count);
				apr_sleep(1000); 
			} else {
				count = 10;
			}
		}
	}
	apr_thread_rwlock_destroy(file_system->lock);
	if(file_system->trackers != NULL) {
		mfs_destroy_pool(file_system->trackers);
		file_system->trackers = NULL;
	}
	apr_pool_destroy(file_system->pool);
}

apr_status_t mfs_get_file_server(mfs_file_system *file_system, apr_uri_t *uri, mfs_file_server **file_server) {
	//servers are keyed by apr_uri_t::hostinfo
	apr_ssize_t klen = strlen(uri->hostinfo);
	mfs_file_server *fs;
	
	apr_status_t rv = apr_thread_rwlock_rdlock(file_system->lock);
	
	if(rv != APR_SUCCESS) {
		mfs_log_apr(LOG_CRIT, rv, file_system->pool, "Unable to read-lock file_system mutex:");
		return rv;
	}
	fs = (mfs_file_server *)apr_hash_get(file_system->file_servers, uri->hostinfo,  klen);
	rv = apr_thread_rwlock_unlock(file_system->lock);
	if(rv != APR_SUCCESS) {
		mfs_log_apr(LOG_ERR, rv, file_system->pool, "Unable to unlock read-lock file_system mutex:");
	}
	if(fs != NULL) {
		*file_server = fs;
		return APR_SUCCESS;
	}
	//no luck... lets add one....
	rv = apr_thread_rwlock_wrlock(file_system->lock);
	if(rv != APR_SUCCESS) {
		mfs_log_apr(LOG_CRIT, rv, file_system->pool, "Unable to write-lock file_system mutex:");
		return rv;
	}
	//first check the file server was not added in the last few ops...
	fs = (mfs_file_server *)apr_hash_get(file_system->file_servers, uri->hostinfo,  klen);
	if(fs == NULL) {
		fs = apr_palloc(file_system->pool, sizeof(mfs_file_server));
		rv = apr_reslist_create(&fs->connections, 0, 3, 50, apr_time_from_sec(60), mfs_file_server_conn_constructor, mfs_file_server_conn_destructor, fs, file_system->pool);
		if(rv != APR_SUCCESS) {
			apr_thread_rwlock_unlock(file_system->lock);
			mfs_log_apr(LOG_CRIT, rv, file_system->pool, "Unable to create file server apr_reslist:");
			return rv;
		}
		fs->url = apr_pstrdup(file_system->pool, uri->hostinfo);
		apr_hash_set(file_system->file_servers, fs->url, klen, fs);
		rv = apr_thread_rwlock_unlock(file_system->lock);
		if(rv != APR_SUCCESS) {
			mfs_log_apr(LOG_ERR, rv, file_system->pool, "Unable to unlock write-lock file system mutex:");
		}
	}
	*file_server = fs;
	return APR_SUCCESS;
}

apr_status_t mfs_file_server_conn_constructor(void **resource, void *params, apr_pool_t *pool) {
	mfs_http_connection *c = malloc(sizeof(mfs_http_connection));
	c->curl = curl_easy_init();
	if(c->curl != NULL) {
		*resource = c;
		return APR_SUCCESS;
	}
	mfs_log(LOG_CRIT, "Unable to curl_easy_init");
	return APR_EGENERAL;
}

apr_status_t mfs_file_server_conn_destructor(void *resource, void *params, apr_pool_t *pool) {
	mfs_http_connection *c = (mfs_http_connection *)resource;
	if(c->curl != NULL) {
		curl_easy_cleanup(c->curl);
	}
	free(c);
	return APR_SUCCESS;
}

apr_status_t mfs_get_paths(mfs_file_system *file_system, const char *domain, const char *key, bool noverify, char ***paths, int *path_count, apr_pool_t *pool) {
	apr_status_t rv = APR_SUCCESS;
	bool ok;

	tracker_request_parameters * params = mfs_tracker_init_parameters(pool);
	mfs_tracker_add_parameter(params, "domain",  domain, pool);
	mfs_tracker_add_parameter(params, "noverify",  noverify ? "1" : "0", pool);
	mfs_tracker_add_parameter(params, "key",  key, pool);
	
	apr_hash_t *result = apr_hash_make(pool);

	rv = mfs_request_do(file_system->trackers, "get_paths", params, &ok, result, pool, file_system->tracker_timeout);
	if(rv == APR_SUCCESS) {
		if(ok) {
			char *path_count_str = apr_hash_get(result, "paths", APR_HASH_KEY_STRING);
			if(path_count_str == NULL) {
				mfs_log(LOG_ERR, "Successful get_paths did not return a paths count");
				rv = APR_EGENERAL;
			} else {
				int pc = atoi(path_count_str);
				if((pc > 0) && (pc < MAX_ALLOWED_PATHS)) {
					char **s_paths = apr_pcalloc(pool,sizeof(char *) * pc);
					int pos;
					for(pos = 1; pos <=pc; pos++) {
						char *key = apr_psprintf(pool, "path%d", pos);
						char *path = apr_hash_get(result, key, APR_HASH_KEY_STRING);
						if(path == NULL) {
							mfs_log(LOG_ERR, "Successful get_paths did not return a path for entry %d", pos);
							rv = APR_EGENERAL;
						}
						s_paths[pos-1] = path;
					}
					if(rv == APR_SUCCESS) { //just in case the path entry was missing....
						*paths = s_paths;
						*path_count = pc;
					}
				} else if((pc == 0)&&(strcmp("0", path_count_str)==0)) { //does this mean not found?.. can it even happen?
					mfs_log(LOG_ERR, "Successful get_paths returned 0 paths count (%s)", path_count_str);
					*path_count = 0;
					rv = APR_EGENERAL;
				} else {
					mfs_log(LOG_ERR, "Successful get_paths returned invalid paths count (%s)", path_count_str);
					rv = APR_EGENERAL;
				}
			}
		} else { //an error occured....
			if(strcmp("unknown_key", apr_hash_get(result, MFS_TRACKER_ERROR_CODE, APR_HASH_KEY_STRING)) == 0) {
				mfs_log(LOG_DEBUG, "Tracker returned error unknown_key when calling get_paths for key %s", key);
				return APR_EBADPATH;
			}
			mfs_log(LOG_ERR, "Tracker returned error %s (%s) when calling get_paths for key %s", apr_hash_get(result, MFS_TRACKER_ERROR_CODE, APR_HASH_KEY_STRING), apr_hash_get(result, MFS_TRACKER_ERROR_DESC, APR_HASH_KEY_STRING), key);
			rv = APR_EGENERAL;
		}
	} 
	return rv;
}


apr_status_t mfs_delete(mfs_file_system *file_system, const char *domain, const char *key, apr_pool_t *pool){
	apr_status_t rv;
	bool ok;

	tracker_request_parameters * params = mfs_tracker_init_parameters(pool);
	mfs_tracker_add_parameter(params, "domain",  domain, pool);
	mfs_tracker_add_parameter(params, "key",  key, pool);
	if(file_system->client_id != NULL) {
		mfs_tracker_add_parameter(params, "client_id",  file_system->client_id, pool);
	}
	
	apr_hash_t *result = apr_hash_make(pool);

	rv = mfs_request_do(file_system->trackers, "delete", params, &ok, result, pool, file_system->tracker_timeout);
	if(rv != APR_SUCCESS) {
		return rv;
	}
	if(!ok) {
		mfs_log(LOG_ERR, "Tracker returned error %s (%s) when calling delete for key %s", apr_hash_get(result, MFS_TRACKER_ERROR_CODE, APR_HASH_KEY_STRING), apr_hash_get(result, MFS_TRACKER_ERROR_DESC, APR_HASH_KEY_STRING), key);
		rv = APR_EGENERAL;
	}
	return rv;
}

apr_status_t mfs_delete_filepath_node(mfs_file_system *file_system, const char *domain, const char *key, apr_pool_t *pool) {
	apr_status_t rv;
	bool ok;

	tracker_request_parameters * params = mfs_tracker_init_parameters(pool);
	mfs_tracker_add_parameter(params, "domain",  domain, pool);
	mfs_tracker_add_parameter(params, "arg1",  key, pool);
	mfs_tracker_add_parameter(params, "argcount",  "1", pool);
	if(file_system->client_id != NULL) {
		mfs_tracker_add_parameter(params, "client_id",  file_system->client_id, pool);
	}
	
	
	apr_hash_t *result = apr_hash_make(pool);

	rv = mfs_request_do(file_system->trackers, "plugin_filepaths_delete_node", params, &ok, result, pool, file_system->tracker_timeout);
	if(rv != APR_SUCCESS) {
		return rv;
	}
	if(!ok) {
		mfs_log(LOG_ERR, "Tracker returned error %s (%s) when calling filepaths_delete_node for key %s", apr_hash_get(result, MFS_TRACKER_ERROR_CODE, APR_HASH_KEY_STRING), apr_hash_get(result, MFS_TRACKER_ERROR_DESC, APR_HASH_KEY_STRING), key );
		rv = APR_EGENERAL;
	}
	return rv;
}

apr_status_t mfs_create_directory(mfs_file_system *file_system, const char *domain, const char *key, apr_pool_t *pool, long long *server_id) {
	apr_status_t rv;
	bool ok;

	tracker_request_parameters * params = mfs_tracker_init_parameters(pool);
	mfs_tracker_add_parameter(params, "domain",  domain, pool);
	mfs_tracker_add_parameter(params, "arg1",  key, pool);
	mfs_tracker_add_parameter(params, "arg2",  "D", pool);
	mfs_tracker_add_parameter(params, "argcount",  "2", pool);
	if(file_system->client_id != NULL) {
		mfs_tracker_add_parameter(params, "client_id",  file_system->client_id, pool);
	}
	
	
	apr_hash_t *result = apr_hash_make(pool);

	rv = mfs_request_do(file_system->trackers, "plugin_filepaths_create_node", params, &ok, result, pool, file_system->tracker_timeout);
	if(rv != APR_SUCCESS) {
		return rv;
	}
	if(!ok) {
		mfs_log(LOG_ERR, "Tracker returned error %s (%s) when calling filepaths_create_node (directory) for key %s", apr_hash_get(result, MFS_TRACKER_ERROR_CODE, APR_HASH_KEY_STRING), apr_hash_get(result, MFS_TRACKER_ERROR_DESC, APR_HASH_KEY_STRING), key );
		rv = APR_EGENERAL;
	}
	if(server_id != NULL) {
		char *s_server_id = apr_hash_get(result, "nid", APR_HASH_KEY_STRING);
		if(s_server_id != NULL) {
			long long tmp = apr_atoi64(s_server_id);
			*server_id = tmp;
		} else {
			mfs_log(LOG_ERR, "Missing nid (server_id) when calling filepaths_create_node (directory) for key %s", key);
		}
	}
	return rv;
}

apr_status_t mfs_create_link(mfs_file_system *file_system, const char *domain, const char *key, const char *link, apr_pool_t *pool, long long *server_id) {
	apr_status_t rv;
	bool ok;

	tracker_request_parameters * params = mfs_tracker_init_parameters(pool);
	mfs_tracker_add_parameter(params, "domain",  domain, pool);
	mfs_tracker_add_parameter(params, "arg1",  key, pool);
	mfs_tracker_add_parameter(params, "arg2",  "L", pool);
	mfs_tracker_add_parameter(params, "arg3",  link, pool);
	mfs_tracker_add_parameter(params, "argcount",  "3", pool);
	if(file_system->client_id != NULL) {
		mfs_tracker_add_parameter(params, "client_id",  file_system->client_id, pool);
	}
	
	
	apr_hash_t *result = apr_hash_make(pool);

	rv = mfs_request_do(file_system->trackers, "plugin_filepaths_create_node", params, &ok, result, pool, file_system->tracker_timeout);
	if(rv != APR_SUCCESS) {
		return rv;
	}
	if(!ok) {
		mfs_log(LOG_ERR, "Tracker returned error %s (%s) when calling filepaths_create_node (link) for key %s", apr_hash_get(result, MFS_TRACKER_ERROR_CODE, APR_HASH_KEY_STRING), apr_hash_get(result, MFS_TRACKER_ERROR_DESC, APR_HASH_KEY_STRING), key );
		rv = APR_EGENERAL;
	}
	if(server_id != NULL) {
		char *s_server_id = apr_hash_get(result, "nid", APR_HASH_KEY_STRING);
		if(s_server_id != NULL) {
			long long tmp = apr_atoi64(s_server_id);
			*server_id = tmp;
		} else {
			mfs_log(LOG_ERR, "Missing nid (server_id) when calling filepaths_create_node (link) for key %s", key);
		}
	}
	return rv;
}

apr_status_t mfs_sleep(mfs_file_system *file_system, int duration, apr_pool_t *pool) {
	apr_status_t rv;
	bool ok;

	tracker_request_parameters * params = mfs_tracker_init_parameters(pool);
	mfs_tracker_add_parameter(params, "duration",  apr_itoa(pool, duration), pool);
	
	apr_hash_t *result = apr_hash_make(pool);

	rv = mfs_request_do(file_system->trackers, "sleep", params, &ok, result, pool, file_system->tracker_timeout);
	if(rv != APR_SUCCESS) {
		return rv;
	}
	if(!ok) {
		mfs_log(LOG_ERR, "Tracker returned error %s (%s) when calling sleep", apr_hash_get(result, MFS_TRACKER_ERROR_CODE, APR_HASH_KEY_STRING), apr_hash_get(result, MFS_TRACKER_ERROR_DESC, APR_HASH_KEY_STRING));
		rv = APR_EGENERAL;
	}
	return rv;
}
apr_status_t mfs_rename(mfs_file_system *file_system, const char *domain, const char *from_key, const char *to_key, apr_pool_t *pool) {
	apr_status_t rv;
	bool ok;

	tracker_request_parameters * params = mfs_tracker_init_parameters(pool);
	mfs_tracker_add_parameter(params, "domain",  domain, pool);
	mfs_tracker_add_parameter(params, "from_key",  from_key, pool);
	mfs_tracker_add_parameter(params, "to_key",  to_key, pool);
	if(file_system->client_id != NULL) {
		mfs_tracker_add_parameter(params, "client_id",  file_system->client_id, pool);
	}
	
	apr_hash_t *result = apr_hash_make(pool);

	rv = mfs_request_do(file_system->trackers, "rename", params, &ok, result, pool, file_system->tracker_timeout);
	if(rv != APR_SUCCESS) {
		return rv;
	}
	if(!ok) {
		mfs_log(LOG_ERR, "Tracker returned error %s (%s) when calling rename from key %s to %s", apr_hash_get(result, MFS_TRACKER_ERROR_CODE, APR_HASH_KEY_STRING), apr_hash_get(result, MFS_TRACKER_ERROR_DESC, APR_HASH_KEY_STRING), from_key,to_key );
		rv = APR_EGENERAL;
	}
	return rv;
}

apr_status_t mfs_rename_filepath(mfs_file_system *file_system, const char *domain, const char *from_key, const char *to_key, apr_pool_t *pool) {
	apr_status_t rv;
	bool ok;

	tracker_request_parameters * params = mfs_tracker_init_parameters(pool);
	mfs_tracker_add_parameter(params, "domain",  domain, pool);
	mfs_tracker_add_parameter(params, "arg1",  from_key, pool);
	mfs_tracker_add_parameter(params, "arg2",  to_key, pool);
	mfs_tracker_add_parameter(params, "argcount",  "2", pool);
	if(file_system->client_id != NULL) {
		mfs_tracker_add_parameter(params, "client_id",  file_system->client_id, pool);
	}
	
	
	apr_hash_t *result = apr_hash_make(pool);

	rv = mfs_request_do(file_system->trackers, "plugin_filepaths_rename", params, &ok, result, pool, file_system->tracker_timeout);
	if(rv != APR_SUCCESS) {
		return rv;
	}
	if(!ok) {
		mfs_log(LOG_ERR, "Tracker returned error %s (%s) when calling mfs_rename_filepath from key %s to %s", apr_hash_get(result, MFS_TRACKER_ERROR_CODE, APR_HASH_KEY_STRING), apr_hash_get(result, MFS_TRACKER_ERROR_DESC, APR_HASH_KEY_STRING), from_key,to_key );
		rv = APR_EGENERAL;
	}
	return rv;
}


apr_status_t mfs_set_mtime(mfs_file_system *file_system, const char *domain, const char *key, apr_time_t mtime, apr_pool_t *pool) {
	apr_status_t rv;
	bool ok;

	tracker_request_parameters * params = mfs_tracker_init_parameters(pool);
	mfs_tracker_add_parameter(params, "domain",  domain, pool);
	mfs_tracker_add_parameter(params, "arg1",  key, pool);
	mfs_tracker_add_parameter(params, "arg2",  apr_psprintf(pool, "%" APR_TIME_T_FMT,apr_time_sec(mtime)), pool);
	mfs_tracker_add_parameter(params, "argcount",  "2", pool);
	if(file_system->client_id != NULL) {
		mfs_tracker_add_parameter(params, "client_id",  file_system->client_id, pool);
	}
	
	
	apr_hash_t *result = apr_hash_make(pool);

	rv = mfs_request_do(file_system->trackers, "plugin_filepaths_set_mtime", params, &ok, result, pool, file_system->tracker_timeout);
	if(rv != APR_SUCCESS) {
		return rv;
	}
	if(!ok) {
		mfs_log(LOG_ERR, "Tracker returned error %s (%s) when calling mfs_set_mtime for key %s", apr_hash_get(result, MFS_TRACKER_ERROR_CODE, APR_HASH_KEY_STRING), apr_hash_get(result, MFS_TRACKER_ERROR_DESC, APR_HASH_KEY_STRING), key );
		rv = APR_EGENERAL;
	}
	return rv;

}

//FilePaths plugin function
apr_status_t mfs_list_directory(mfs_file_system *file_system, const char *domain, const char *directory, mfs_filepath_entry **filepath_entries, int *child_count, apr_pool_t *pool) {
	apr_status_t rv = APR_SUCCESS;
	bool ok;

	tracker_request_parameters * params = mfs_tracker_init_parameters(pool);
	mfs_tracker_add_parameter(params, "domain",  domain, pool);
	mfs_tracker_add_parameter(params, "arg1",  directory, pool);
	mfs_tracker_add_parameter(params, "argcount",  "1", pool);
	
	apr_hash_t *result = apr_hash_make(pool);

	rv = mfs_request_do(file_system->trackers, "plugin_filepaths_list_directory", params, &ok, result, pool, file_system->tracker_timeout);
	if(rv == APR_SUCCESS) {
		if(ok) {
			char *path_count_str = apr_hash_get(result, "files", APR_HASH_KEY_STRING);
			if(path_count_str == NULL) {
				mfs_log(LOG_ERR, "Successful filepaths_list_directory did not return a files count");
				rv = APR_EGENERAL;
			} else {
				int pc = atoi(path_count_str);
				if((pc > 0) && (pc < MAX_ALLOWED_DIRECTORY_ENTRIES)) {
					mfs_filepath_entry *entries = apr_pcalloc(pool,sizeof(mfs_filepath_entry) * pc);
					int pos;
					char key[100];
					for(pos = 0; pos <pc; pos++) {
						sprintf(key, "file%d", pos);
						entries[pos].name = apr_hash_get(result, key, APR_HASH_KEY_STRING);
						if(entries[pos].name == NULL) {
							mfs_log(LOG_ERR, "Successful plugin_filepaths_list_directory did not return a name for entry %d", pos);
							return APR_EGENERAL;
						}

						sprintf(key, "file%d.mtime", pos);
						char *mtime = apr_hash_get(result, key, APR_HASH_KEY_STRING);
						if(mtime != NULL) {
							apr_int64_t mtime_n = apr_atoi64(mtime);
							entries[pos].mtime = apr_time_from_sec(mtime_n);
						} else {
							entries[pos].mtime = 0;
						}

						sprintf(key, "file%d.nid", pos);
						char *nid = apr_hash_get(result, key, APR_HASH_KEY_STRING);
						if(nid != NULL) {
							entries[pos].server_id = apr_atoi64(nid);
						} else {
							entries[pos].server_id = -1;
							mfs_log(LOG_ERR, "Successful plugin_filepaths_list_directory did not return a server id for entry %d", pos);
						}

						
						sprintf(key, "file%d.type", pos);
						char *type = apr_hash_get(result, key, APR_HASH_KEY_STRING);
						if(type == NULL) {
							mfs_log(LOG_ERR, "Successful plugin_filepaths_list_directory did not return a type for entry %d", pos);
							return APR_EGENERAL;
						}
						if(type[0] == 'D') {
							entries[pos].type = TYPE_DIRECTORY;
							
						} else if(type[0] == 'L') {
							entries[pos].type = TYPE_SYMLINK;
							sprintf(key, "file%d.link", pos);
							
							entries[pos].link = apr_hash_get(result, key, APR_HASH_KEY_STRING);
							if(entries[pos].link == NULL) {
								mfs_log(LOG_ERR, "Successful plugin_filepaths_list_directory did not return a link for entry %d", pos);
								return APR_EGENERAL;
							}
						} else {
							entries[pos].type = TYPE_FILE;
							
							sprintf(key, "file%d.size", pos);
							char *size = apr_hash_get(result, key, APR_HASH_KEY_STRING);
							if(size != NULL) {
								entries[pos].size = apr_atoi64(size);
							} else {
								entries[pos].size = 0;
							}
						}
						
					}
					if(rv == APR_SUCCESS) { //just in case the path entry was missing....
						*filepath_entries = entries;
						*child_count = pc;
					}
				} else if((pc == 0)&&(strcmp("0", path_count_str)==0)) { //no files in directory...
					*child_count= 0;
				} else {
					mfs_log(LOG_ERR, "Successful plugin_filepaths_list_directory returned invalid paths count (%s)", path_count_str);
					rv = APR_EGENERAL;
				}
			}
		} else { //an error occured....
			if(strcmp("unknown_key", apr_hash_get(result, MFS_TRACKER_ERROR_CODE, APR_HASH_KEY_STRING)) == 0) {
				mfs_log(LOG_DEBUG, "Tracker returned error unknown_key when calling plugin_filepaths_list_directory for directory %s", directory);
				return APR_EBADPATH;
			}
			mfs_log(LOG_ERR, "Tracker returned error %s (%s) when calling plugin_filepaths_list_directory for directory %s", apr_hash_get(result, MFS_TRACKER_ERROR_CODE, APR_HASH_KEY_STRING), apr_hash_get(result, MFS_TRACKER_ERROR_DESC, APR_HASH_KEY_STRING), directory);
			rv = APR_EGENERAL;
		}
	} 
	return rv;
}

//FilePaths plugin function
apr_status_t mfs_path_info(mfs_file_system *file_system, const char *domain, const char *path, mfs_filepath_entry *filepath_entry, apr_pool_t *pool) {
	apr_status_t rv = APR_SUCCESS;
	bool ok;

	tracker_request_parameters * params = mfs_tracker_init_parameters(pool);
	mfs_tracker_add_parameter(params, "domain",  domain, pool);
	mfs_tracker_add_parameter(params, "arg1",  path, pool);
	mfs_tracker_add_parameter(params, "argcount",  "1", pool);
	
	apr_hash_t *result = apr_hash_make(pool);

	rv = mfs_request_do(file_system->trackers, "plugin_filepaths_path_info", params, &ok, result, pool, file_system->tracker_timeout);
	if(rv == APR_SUCCESS) {
		if(ok) {
			filepath_entry->name = NULL; //we dont set this ATM..
			char *mtime = apr_hash_get(result, "mtime", APR_HASH_KEY_STRING);
			if(mtime != NULL) {
				apr_int64_t mtime_n = apr_atoi64(mtime);
				filepath_entry->mtime = apr_time_from_sec(mtime_n);
			} else {
				filepath_entry->mtime = 0;
			}

			char *nid = apr_hash_get(result, "nid", APR_HASH_KEY_STRING);
			if(nid != NULL) {
				filepath_entry->server_id = apr_atoi64(nid);
			} else {
				filepath_entry->server_id = -1;
				mfs_log(LOG_ERR, "Successful plugin_filepaths_path_info did not return a server id for entry");
			}
			
			char *type = apr_hash_get(result, "type", APR_HASH_KEY_STRING);
			if(type == NULL) {
				mfs_log(LOG_ERR, "Successful plugin_filepaths_path_info did not return a type for entry");
				return APR_EGENERAL;
			}
			if(type[0] == 'D') {
				filepath_entry->type = TYPE_DIRECTORY;
			} else if(type[0] == 'L') {
				filepath_entry->type = TYPE_SYMLINK;
				filepath_entry->link = apr_hash_get(result, "link", APR_HASH_KEY_STRING);
				if(filepath_entry->link == NULL) {
					mfs_log(LOG_ERR, "Successful plugin_filepaths_path_info did not return a link for entry");
					return APR_EGENERAL;
				}
			} else {
				filepath_entry->type = TYPE_FILE;
				char *size = apr_hash_get(result, "size", APR_HASH_KEY_STRING);
				if(size != NULL) {
					filepath_entry->size = apr_atoi64(size);
				} else {
					filepath_entry->size = 0;
				}
			}
		} else { //an error occured....
			if(strcmp("path_not_found", apr_hash_get(result, MFS_TRACKER_ERROR_CODE, APR_HASH_KEY_STRING)) == 0) {
				mfs_log(LOG_DEBUG, "Tracker returned error path_not_found when calling plugin_filepaths_path_info for path %s", path);
				return APR_EBADPATH;
			}
			if(strcmp("unknown_key", apr_hash_get(result, MFS_TRACKER_ERROR_CODE, APR_HASH_KEY_STRING)) == 0) {
				mfs_log(LOG_DEBUG, "Tracker returned error unknown_key when calling plugin_filepaths_path_info for path %s", path);
				return APR_EBADPATH;
			}
			mfs_log(LOG_ERR, "Tracker returned error %s (%s) when calling plugin_filepaths_path_info for path %s", apr_hash_get(result, MFS_TRACKER_ERROR_CODE, APR_HASH_KEY_STRING), apr_hash_get(result, MFS_TRACKER_ERROR_DESC, APR_HASH_KEY_STRING), path);
			rv = APR_EGENERAL;
		}
	} 
	return rv;
}


apr_status_t mfs_stats_filepath(mfs_file_system *file_system, const char *domain, mfs_filepath_stats *stats, apr_pool_t *pool) {
	apr_status_t rv;
	bool ok;

	tracker_request_parameters * params = mfs_tracker_init_parameters(pool);
	mfs_tracker_add_parameter(params, "domain",  domain, pool);
	mfs_tracker_add_parameter(params, "argcount",  "0", pool);
	if(file_system->client_id != NULL) {
		mfs_tracker_add_parameter(params, "client_id",  file_system->client_id, pool);
	}
	apr_hash_t *result = apr_hash_make(pool);

	rv = mfs_request_do(file_system->trackers, "plugin_filepaths_stats", params, &ok, result, pool, file_system->tracker_timeout);
	if(rv != APR_SUCCESS) {
		return rv;
	}
	if(!ok) {
		mfs_log(LOG_ERR, "Tracker returned error %s (%s) when calling mfs_stats_filepath", apr_hash_get(result, MFS_TRACKER_ERROR_CODE, APR_HASH_KEY_STRING), apr_hash_get(result, MFS_TRACKER_ERROR_DESC, APR_HASH_KEY_STRING));
		rv = APR_EGENERAL;
	}

	char *tmp = apr_hash_get(result, "mb_total", APR_HASH_KEY_STRING);
	if(tmp != NULL) {
		stats->total_mb = apr_atoi64(tmp);
	} else {
		stats->total_mb = 0;
		mfs_log(LOG_ERR, "Successful plugin_filepaths_stats did not return a mb_total");
	}
	tmp = apr_hash_get(result, "mb_used", APR_HASH_KEY_STRING);
	if(tmp != NULL) {
		stats->used_mb = apr_atoi64(tmp);
	} else {
		stats->used_mb = 0;
		mfs_log(LOG_ERR, "Successful plugin_filepaths_stats did not return a mb_used");
	}
	return rv;
}


apr_status_t mfs_checkfs_filepath(mfs_file_system *file_system, const char *domain, bool get_total, mfs_check_fs_result *stats, apr_pool_t *pool) {
	apr_status_t rv;
	bool ok;

	tracker_request_parameters * params = mfs_tracker_init_parameters(pool);
	mfs_tracker_add_parameter(params, "domain",  domain, pool);
	mfs_tracker_add_parameter(params, "argcount",  "1", pool);
	if(get_total) {
		mfs_tracker_add_parameter(params, "arg1",  "1", pool);
	} else {
		mfs_tracker_add_parameter(params, "arg1",  "0", pool);
	}
	if(file_system->client_id != NULL) {
		mfs_tracker_add_parameter(params, "client_id",  file_system->client_id, pool);
	}
	apr_hash_t *result = apr_hash_make(pool);

	rv = mfs_request_do(file_system->trackers, "plugin_filepaths_check_fs", params, &ok, result, pool, file_system->tracker_timeout);
	if(rv != APR_SUCCESS) {
		return rv;
	}
	if(!ok) {
		mfs_log(LOG_ERR, "Tracker returned error %s (%s) when calling mfs_check_fs_filepath", apr_hash_get(result, MFS_TRACKER_ERROR_CODE, APR_HASH_KEY_STRING), apr_hash_get(result, MFS_TRACKER_ERROR_DESC, APR_HASH_KEY_STRING));
		rv = APR_EGENERAL;
	}

	char *tmp = apr_hash_get(result, "fixed", APR_HASH_KEY_STRING);
	if(tmp != NULL) {
		stats->fixed = apr_atoi64(tmp);
	} else {
		stats->fixed = -1;
		mfs_log(LOG_ERR, "Successful plugin_filepaths_filepath did not return a fixed");
	}
	tmp = apr_hash_get(result, "total", APR_HASH_KEY_STRING);
	if(tmp != NULL) {
		stats->total = apr_atoi64(tmp);
	} else {
		stats->total = -1;
	}
	return rv;
}



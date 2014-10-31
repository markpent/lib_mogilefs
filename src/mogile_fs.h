/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * Copyright (C) Mark Pentland 2011 <mark.pent@gmail.com>
 * 
 * mogile_fs is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * mogile_fs is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <apr_general.h>
#include <apr_network_io.h>
#include <apr_hash.h>
#include <apr_pools.h>
#include <apr_thread_rwlock.h>
#include <apr_thread_mutex.h>
#include <apr_thread_proc.h>
#include <apr_thread_cond.h>
#include <apr_ring.h>
#include <stdbool.h>
#include <curl/curl.h>
#include <apr_reslist.h>
#include <apr_uri.h>
#include <apr_buckets.h>
#include <apr_file_io.h>
/*
===================================================================
TRACKER STUFF
===================================================================
*/
#ifndef _MFS_HEADER
#define _MFS_HEADER 1

#define MFS_TRACKER_ERROR_CODE "ERROR_CODE"
#define MFS_TRACKER_ERROR_DESC "ERROR_DESC"

void mfs_logging_init(int level, int facility);
void mfs_logging_init_ex(bool to_syslog, const char *level, const char *identifier, apr_file_t *file);
void mfs_logging_set_file_ptr(apr_file_t *file);

typedef struct {
	char *address;
	int port;
	apr_sockaddr_t * sa;
} tracker_info;

typedef struct {
	tracker_info *tracker;
	bool connected;
	int last_error;
	apr_socket_t * socket;
	apr_pool_t *pool;
} tracker_connection;

typedef struct _tracker_request_parameter {
	char *key;
	int key_length;
	char *value;
	int value_length;
	bool is_metadata; //is this a meta data?
	struct _tracker_request_parameter * next;
} tracker_request_parameter;

typedef struct {
	tracker_request_parameter *head;
	int count;
	int strlen;
	int meta_count; //track number of meta data params added
} tracker_request_parameters;

apr_status_t mfs_tracker_init(char *address, int port, apr_pool_t *pool, tracker_info **tracker);
//same as above but an already allocated tracker is used
apr_status_t mfs_tracker_init2(char *address, int port, apr_pool_t *pool, tracker_info *tracker);

//connect to a tracker
//the pool is what will be stored against the connection
apr_status_t mfs_tracker_connect(tracker_info *tracker, tracker_connection ** connection, apr_pool_t *pool, apr_interval_time_t timeout);

//init the tracker_request_parameters struct
tracker_request_parameters * mfs_tracker_init_parameters(apr_pool_t *pool);
//add key/value parameters: copies values (deallocate them).
void mfs_tracker_add_parameter(tracker_request_parameters *parameters, const char *key,  const char *value, apr_pool_t *pool);
//add key/value parameters: uses values (dont deallocate them).
void mfs_tracker_add_parameter_pointers(tracker_request_parameters *parameters, char *key, int key_length, char *value, int value_length, apr_pool_t *pool);
//add meta data to parameters.. will look after naming/encoding if prepare (and will not be flagged as meta data), otherwise will just flag as meta data
void mfs_tracker_add_meta_data(tracker_request_parameters *parameters, const char *key, const char *value, bool prepare, apr_pool_t *pool);

//copy parameters from one to another: just appends pointers so dont deallocate src until no longer needed in dest
void mfs_tracker_copy_parameter_pointers(tracker_request_parameters *src, tracker_request_parameters *dest, apr_pool_t *pool);


//try a request against the tracker
//the pool will be used for any memory allocations
//returns APR_SUCCESS unless there was an error communicating with the tracker 
apr_status_t mfs_tracker_request(tracker_connection *connection, char * request, tracker_request_parameters * parameters, bool *ok, apr_hash_t *result, apr_pool_t *pool, apr_interval_time_t timeout);

char * mfs_tracker_url_encode(const char *raw, apr_pool_t *pool, int *length);
char * mfs_tracker_url_decode(const char *encoded, apr_pool_t *pool);

//build a request to send to the tracker
char * mfs_tracker_build_request(char * cmd, tracker_request_parameters * parameters, apr_pool_t *pool, apr_size_t *size);

//parse the response from the tracker.
//ok will be set to true if OK, false if ERR.
//result will have key/values of response.
//if ERR, result will have MFS_TRACKER_ERROR_CODE and MFS_TRACKER_ERROR_DESC keys
apr_status_t mfs_tracker_parse_response(char *final_buffer, int final_buffer_size, bool *ok, apr_hash_t *result, apr_pool_t *pool);

//deallocate any memory assocated with connection. Disconnect the socket if its connected.
void mfs_tracker_destroy_connection(tracker_connection *connection);



/*
===================================================================
TRACKER POOLING
===================================================================
*/

#define MFS_CONNECTION_EXPIRE_TIME 60 //seconds
#define MFS_POOL_MAINTENANCE_POLL_TIME 2 //seconds

//get a list of trackers so we can iterate over them.
typedef struct {
	int * tracker_indexes;
	int tracker_count;
	int current_position; //index into list for iteration
	int start_postion; //where we started.. so we know where to stop...
} tracker_list;

typedef struct _tracker_connection_pool_entry {
	APR_RING_ENTRY(_tracker_connection_pool_entry) link;
	tracker_connection * connection;
	apr_time_t last_used;
} tracker_connection_pool_entry;

typedef struct _tracker_connection_stack tracker_connection_stack;
APR_RING_HEAD(_tracker_connection_stack, _tracker_connection_pool_entry);

typedef struct {
	apr_thread_mutex_t * lock; //push/pop lock
	int connection_count;
	tracker_connection_stack *connection_stack;
} tracker_connection_pool;

typedef struct {
	tracker_info *trackers; //array of trackers
	int tracker_count;
	int * active_trackers; //indexes of active trackers
	int active_tracker_count;
	int * inactive_trackers; //indexes of inactive trackers
	int inactive_tracker_count; 
	int max_tracker_count;
	tracker_connection_pool * connection_pools; //array of collection pools whose index matches trackers array
	apr_thread_rwlock_t *lock; //used to lock when changing active trackers
	apr_pool_t *pool;
	apr_thread_mutex_t *maintenance_mutex; //used to stop/start thread quickly
	apr_thread_cond_t  *maintenance_cond; //used to stop/start thread quickly
	apr_thread_t *maintenance_thread; //this thread will start when the last tracker is registered
	volatile bool pool_running; //used to let maintenance_thread shutdown
	volatile bool maintenance_thread_running; //used to tell if maintenance thread is running
	unsigned int maintenance_thread_check_count; //used to test if a check has occured...
} tracker_pool;

//init the tracker pool
//the quick way: a comma separated list of trackers in the form address:port
tracker_pool * mfs_pool_init_quick(char *tracker_list);
//tracker_count: max number of trackers that can be registered...
tracker_pool * mfs_pool_init(int tracker_count);
void mfs_destroy_pool(tracker_pool * trackers);
//add a tracker
void mfs_pool_register_tracker(tracker_pool * trackers, char *address, int port);
//get a list of active trackers - returns NULL if no active trackers
//pool is used to allocate the list so its at request scope
tracker_list * mfs_pool_list_active_trackers(tracker_pool * trackers, apr_pool_t *pool);
tracker_list * mfs_pool_list_inactive_trackers(tracker_pool * trackers, apr_pool_t *pool);
//iterate over that list. returns NULL when start position is reached
tracker_info * mfs_pool_next_tracker(tracker_list * list, tracker_pool *trackers);
//get the current tracker index of the list: used to mark tracker as active/inactive
int mfs_pool_current_tracker_index(tracker_list * list);
//mark tracker active
//pool is used when logging errors so its at request scope
void mfs_pool_activate(tracker_pool * trackers, int tracker_index, apr_pool_t *pool);
//mark tracker inactive
//pool is used when logging errors so its at request scope
void mfs_pool_deactivate(tracker_pool * trackers, int tracker_index, apr_pool_t *pool);

//get a tracker connection (wrapped in a tracker_connection_pool_entry) for a tracker at tracker_index
//will return NULL if we cant get a connection: this will internally call mfs_pool_deactivate
//pool is in request scope
//create_new means create a new connection if none available
tracker_connection_pool_entry * mfs_pool_get_connection(tracker_pool *trackers, int tracker_index, apr_pool_t *pool, bool create_new, apr_interval_time_t timeout);
tracker_connection_pool_entry * mfs_pool_get_connection_ex(tracker_pool *trackers, int tracker_index, apr_pool_t *pool, bool *create_new, apr_interval_time_t timeout);


//return a conection that was successful
//pool is at request scope
void mfs_pool_return_connection(tracker_pool *trackers, int tracker_index, tracker_connection_pool_entry * connection_entry, apr_pool_t *pool);

//destroy a connection entry. This will destroy the associated tracker connection
void mfs_pool_destroy_connection(tracker_connection_pool_entry * connection_entry);

//used by tests to stop maintenance thread starting up
void mfs_pool_disable_maintenance(); 
void mfs_pool_enable_maintenance(); 
void mfs_pool_start_maintenance_thread(tracker_pool *trackers);
void mfs_pool_stop_maintenance_thread(tracker_pool *trackers);
//pool maintenance function
//check deactivated connections and try and activate them
//poll active trackers
//expire old connections that have not been used for CONNECTION_EXPIRE_TIME
void* APR_THREAD_FUNC mfs_pool_maintenance(apr_thread_t *thd, void *data);

void mfs_pool_test_inactive_trackers(tracker_pool *trackers, apr_pool_t *pool);
void mfs_pool_expire_active_trackers(tracker_pool *trackers, apr_pool_t *pool);
tracker_connection_pool_entry * mfs_pool_get_expired_trackers(tracker_connection_pool * cp, apr_pool_t *pool);

/*
===================================================================
WATCH
===================================================================
*/

typedef struct {
	tracker_info *tracker;
	int tracker_index;
	tracker_connection_pool_entry *connection;
	tracker_pool *trackers;
	char read_buf[5000];
	char *buf_pointer;
	apr_size_t cnt;
	char *client_id;
	int client_id_length;
} watch_data;

void stop_watching();
void enable_watching();

watch_data * init_watch(int tracket_index, tracker_pool *trackers, apr_pool_t *pool, char *client_id);
apr_status_t get_next_watch_line(watch_data *watch, char *buf, int len);
apr_status_t get_next_watch_cache_line(watch_data *watch, char *buf, int len);

/*
===================================================================
REQUEST STUFF
===================================================================
*/

#define MFS_NO_TRACKERS_AVAILABLE (APR_OS_START_USERERR + 1000)

//call tracker from pool to service a request
//result must already be allocated
//pool is optional
apr_status_t mfs_request_do(tracker_pool *trackers, char *action, tracker_request_parameters *parameters, bool *ok, apr_hash_t *result, apr_pool_t *pool, apr_interval_time_t timeout);

/*
===================================================================
FS Client
===================================================================
*/
#define DEFAULT_MAX_RETRIES 2
#define DEFAULT_RETRY_WAIT apr_time_from_sec(1)
#define DEFAULT_TRACKER_TIMEOUT apr_time_from_sec(2)
#define DEFAULT_FILE_SERVER_TIMEOUT apr_time_from_sec(2)
#define DEFAULT_MAX_BUFFER_SIZE (100 * 1024)
#define MAX_ALLOWED_PATHS 100
#define MAX_ALLOWED_DIRECTORY_ENTRIES 32000

//a connection to a http server
typedef struct _mfs_http_connection {
	CURL *curl; //the Curl handle that manages the http stuff
} mfs_http_connection;

//used to pool http connections for keep-alive
typedef struct {
	apr_reslist_t *connections;
	char *url; //i.e http://some.local.address:port
} mfs_file_server;

//threadsafe handle to the file system
typedef struct {
	volatile int max_retries;
	volatile apr_interval_time_t retry_timeout; //in microseconds - wait between upload retries
	volatile apr_interval_time_t tracker_timeout; //microseconds
	volatile apr_interval_time_t file_server_timeout; //microseconds
	tracker_pool *trackers;
	apr_pool_t *pool;
	apr_size_t max_buffer_size; //if file transfer goes over this size then use a FILE. size is in bytes
	apr_thread_rwlock_t *lock; //file server lock
	apr_hash_t *file_servers; //hash of mfs_http_server to cache connections so we can use keep-alive
	//client_id is sent with requests that cause cache invalidations so we can safely ignore cache invalidations caused by out own requests
	char *client_id;
} mfs_file_system;

//init the file system
apr_status_t mfs_init_file_system(mfs_file_system **file_system, tracker_pool *trackers);
void mfs_close_file_system(mfs_file_system *file_system);

//get the file server for a uri
apr_status_t mfs_get_file_server(mfs_file_system *file_system, apr_uri_t *uri, mfs_file_server **file_server);

//the paths are allocated from the pool passed in
apr_status_t mfs_get_paths(mfs_file_system *file_system, const char *domain, const char *key, bool noverify, char ***paths, int *path_count, apr_pool_t *pool);

apr_status_t mfs_delete(mfs_file_system *file_system, const char *domain, const char *key, apr_pool_t *pool);
apr_status_t mfs_sleep(mfs_file_system *file_system, int duration, apr_pool_t *pool);
apr_status_t mfs_rename(mfs_file_system *file_system, const char *domain, const char *from_key, const char *to_key, apr_pool_t *pool);
//filepaths version
apr_status_t mfs_rename_filepath(mfs_file_system *file_system, const char *domain, const char *from_key, const char *to_key, apr_pool_t *pool);
//delete a directory/symlink (not file)
apr_status_t mfs_delete_filepath_node(mfs_file_system *file_system, const char *domain, const char *key, apr_pool_t *pool);
//create a directory (filepaths plugin)
apr_status_t mfs_create_directory(mfs_file_system *file_system, const char *domain, const char *key, apr_pool_t *pool, long long *server_id);
//create a symlink (filepaths plugin)
apr_status_t mfs_create_link(mfs_file_system *file_system, const char *domain, const char *key, const char *link, apr_pool_t *pool, long long *server_id);
//set the modification time (filepaths plugin)
apr_status_t mfs_set_mtime(mfs_file_system *file_system, const char *domain, const char *key, apr_time_t mtime, apr_pool_t *pool);


//FilePaths Plugin Calls
typedef struct {
	char *name;
	unsigned char type;
	apr_time_t mtime;
	apr_size_t size;
	long long server_id;
	char *link;
} mfs_filepath_entry;

#define TYPE_FILE 0
#define TYPE_DIRECTORY 1
#define TYPE_SYMLINK 2

apr_status_t mfs_list_directory(mfs_file_system *file_system, const char *domain, const char *directory, mfs_filepath_entry **filepath_entries, int *child_count, apr_pool_t *pool);
apr_status_t mfs_path_info(mfs_file_system *file_system, const char *domain, const char *path, mfs_filepath_entry *filepath_entry, apr_pool_t *pool);

typedef struct {
	long total_mb;
	long used_mb;
} mfs_filepath_stats;
apr_status_t mfs_stats_filepath(mfs_file_system *file_system, const char *domain, mfs_filepath_stats *stats, apr_pool_t *pool);

typedef struct {
	long total;
	long fixed;
} mfs_check_fs_result;
apr_status_t mfs_checkfs_filepath(mfs_file_system *file_system, const char *domain, bool get_total, mfs_check_fs_result *result, apr_pool_t *pool);
/*
 in file_upload.c
*/
//using CURL upload a file/buffer
apr_status_t mfs_file_server_put(mfs_file_system *file_system, apr_uri_t *uri, char *original_uri, void *bytes, long *total_bytes, apr_file_t *file, apr_pool_t *pool);

//upload from a memory buffer
apr_status_t mfs_store_bytes(mfs_file_system *file_system, const char *domain, const char *key, const char *storage_class, apr_pool_t *pool, void *bytes, long total_bytes);
apr_status_t mfs_store_bytes_ex(mfs_file_system *file_system, const char *domain, const char *key, const char *storage_class, apr_pool_t *pool, void *bytes, long total_bytes, tracker_request_parameters * extra_open_parameters, tracker_request_parameters * extra_close_parameters);
//FilePath plugin support
apr_status_t mfs_store_bytes_filepath(mfs_file_system *file_system, const char *domain, const char *key, const char *storage_class, apr_pool_t *pool, void *bytes, long total_bytes, apr_time_t mtime);

//upload from a file
apr_status_t mfs_store_file(mfs_file_system *file_system, const char *domain, const char *key, const char *storage_class, apr_pool_t *pool, apr_file_t *file);
apr_status_t mfs_store_file_ex(mfs_file_system *file_system, const char *domain, const char *key, const char *storage_class, apr_pool_t *pool, apr_file_t *file, tracker_request_parameters * extra_open_parameters, tracker_request_parameters * extra_close_parameters);
//FilePath plugin support
apr_status_t mfs_store_file_filepath(mfs_file_system *file_system, const char *domain, const char *key, const char *storage_class, apr_pool_t *pool, apr_file_t *file, apr_time_t mtime);



/*
 in file_download.c
*/

//read a file, based on mfs_file_system.max_buffer_size, return a FILE or bytes
apr_status_t mfs_file_server_get(mfs_file_system *file_system, apr_uri_t *uri, char *original_uri, void **bytes, apr_size_t *total_bytes, apr_file_t **file, apr_bucket_brigade *brigade, apr_pool_t *pool, char *destination_file_path);

//get a file and store it in the file parameter
//if the file pointer is not NULL then use the file, otherwise allocate a file pointer on the fly
apr_status_t mfs_get_file(mfs_file_system *file_system, char *domain, char *key, apr_size_t *total_bytes, apr_file_t **file, apr_pool_t *pool, long requiredLength);

//return the file in a byte buffer if the file is small enough (file_system.max_buffer_size)
apr_status_t mfs_get_file_or_bytes(mfs_file_system *file_system, char *domain, char *key, apr_size_t *total_bytes, void **bytes, apr_file_t **file, apr_pool_t *pool, char *destination_file_path, long requiredLength);

//store the file in a bucket brigade
apr_status_t mfs_get_brigade(mfs_file_system *file_system, char *domain, char *key, apr_size_t *total_bytes, apr_bucket_brigade *brigade, apr_pool_t *pool, long requiredLength);

#endif

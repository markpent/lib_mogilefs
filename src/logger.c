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

#ifndef SYSLOG_NAMES
	#define SYSLOG_NAMES 1
#endif

#include "logger.h"
#include <stdarg.h>
#include <stdbool.h>
#include <apr_strings.h>
#include <stdio.h>
#include <time.h>

#include <apr_file_io.h>

int mfs_logging_level = LOG_WARNING;
int mfs_logging_facility = LOG_USER;
//FILE *mfs_log_file_h = NULL;

apr_file_t *mfs_log_file_h = NULL;
apr_pool_t *mfs_log_file_pool = NULL;


void mfs_logging_init(int level, int facility) {
	mfs_logging_level = level;
	mfs_logging_facility = facility;
	openlog("mogile_fs_fuse", 0, mfs_logging_facility);
}

void mfs_logging_init_ex(bool to_syslog, const char *level, const char *identifier, apr_file_t *file) {
	int i;
	int log_level = -1;
	for(i=0; prioritynames[i].c_name != NULL; i++) {
		if(strcmp(level, prioritynames[i].c_name) == 0) {
			log_level = prioritynames[i].c_val;
			break;
		}
	}
	if(log_level == -1) {
		mfs_log(LOG_ERR, "Unable to find syslog_level '%s'", level);
		log_level = LOG_WARNING;
	}
	if(to_syslog) {
		int log_facility = -1;
		for(i=0; facilitynames[i].c_name != NULL; i++) {
			if(strcmp(identifier, facilitynames[i].c_name) == 0) {
				log_facility = facilitynames[i].c_val;
				break;
			}
		}
		if(log_facility == -1) {
			mfs_log(LOG_ERR, "Unable to find syslog_facility '%s'", identifier);
			log_facility = LOG_USER;
		}
		mfs_logging_init(log_level, log_facility);
	} else {
		mfs_logging_level = log_level;
		//identifier is a path...
		

		if(apr_pool_create(&mfs_log_file_pool,NULL) != APR_SUCCESS) {
			mfs_log(LOG_ERR, "UNABLE TO CREATE APR POOL");
			return;
		}
		
		if(file != NULL) {
			mfs_log_file_h = file;
		} else {
			apr_status_t rv;
			if((rv = apr_file_open(&mfs_log_file_h, identifier, APR_WRITE | APR_CREATE | APR_XTHREAD , 0x777, mfs_log_file_pool)) != APR_SUCCESS) {
				mfs_log_apr(LOG_ERR, rv, NULL, "unable to open log file %s:",identifier);
				return;
			}
		}
		
		//if(mfs_log_file_h == NULL) {
		//	mfs_log(LOG_ERR, "Unable to open log '%s'", identifier);
		//}
	}
}

void mfs_logging_set_file_ptr(apr_file_t *file) {
	mfs_log_file_h = file;
}

char * cur_time_string(char *buf) {
	struct tm tm;
	time_t t;
	time(&t);
	localtime_r(&t, &tm);
	strftime(buf, 26, "%F %H:%M:%S", &tm);
	return buf;
}

void mfs_log(int level, char *format, ...) {
	if(level <= mfs_logging_level) {
		va_list arglist;
		va_start(arglist, format);

		char tmp[255];
		vsnprintf(tmp, 254, format, arglist); 

		apr_file_t *ftmp = mfs_log_file_h;
		
		if(ftmp != NULL) {
			char stime[26];
			//fprintf(ftmp, "[mfs] %s: %s\n", cur_time_string(stime), tmp);
			apr_file_printf(ftmp, "[mfs] %s: %s\n", cur_time_string(stime), tmp);
		} else {
			syslog(LOG_MAKEPRI(mfs_logging_facility, LOG_INFO), "%s", tmp);
		}
	}
}

void mfs_log_apr(int level, apr_status_t error, apr_pool_t *pool, char *format, ...) {
	if(level <= mfs_logging_level) {
		char err[255];
		char tmp[255];
		va_list arglist;
		va_start(arglist, format);
		vsnprintf(tmp, 254, format, arglist); 
		
		apr_strerror(error,err,255); 
		apr_file_t *ftmp = mfs_log_file_h;
		if(ftmp != NULL) {
			char stime[26];
			//fprintf(ftmp, "%s: %s%s\n", cur_time_string(stime), tmp, err);
			apr_file_printf(ftmp, "[mfs] %s: %s%s\n", cur_time_string(stime), tmp, err);
		} else {
			syslog(LOG_MAKEPRI(mfs_logging_facility, LOG_INFO), "%s%s", tmp, err);
		}
	}
}

bool will_log(int level) {
	return (level <= mfs_logging_level);
}
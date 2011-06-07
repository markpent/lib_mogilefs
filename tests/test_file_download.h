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
 
void test_file_get_ok_bytes();
void test_file_get_ok_file();
void test_file_get_ok_either();
void test_file_get_ok_either_large();
void test_file_get_ok_brigade_large();
void test_file_get_fail_brigade_large();
void test_file_get_fail_brigade_large_with_data();
void test_file_get_timeout_bytes();
void test_file_get_timeout_brigade_large_with_data();
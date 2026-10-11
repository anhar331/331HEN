/* Copyright (C) 2025 OnionHEN / LightningMods

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING. If not, see
<http://www.gnu.org/licenses/>.  */

#pragma once

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "freebsd-helper.h"
#include "ps5/payload.h"
#include "ps5/kernel.h"

#include <ps5/kernel.h>
#include <sys/sysctl.h>
#include <machine/param.h>
#include <sys/types.h>

#define MiB(x) ((x) / (1024.0 * 1024))


#define SYS_dynlib_get_info_ex 608
#define SYS_dl_get_list 0x217
#define SYS_dl_get_info_2 0x2cd

#include <onion/module_info.h>




struct proc* find_proc_by_name(const char* process_name);
struct proc* get_proc_by_pid(pid_t pid);
struct proc* get_proc_by_title_id(const char* title_id);
module_info_t* get_module_info(pid_t pid, const char* module_name);
int get_module_handle(pid_t pid, const char* module_name);
void list_all_proc_and_pid();

void list_proc_modules(struct proc* proc);
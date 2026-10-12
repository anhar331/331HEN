/* Independently authored local-file logger. GPL-3.0-or-later. No upstream logger code. */
#pragma once
#include <stdarg.h>
enum { PS5LOG_INFO=1,PS5LOG_MARK=2,PS5LOG_ERR=3 };
void ps5log_printf(int level,const char* format,...) __attribute__((format(printf,2,3)));
void ps5log_line(int level,const char* line);
void ps5log_close(const char* reason);
int fg_log_open(void);

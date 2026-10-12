/* Copyright (C) 2026 ANHAR / 331HEN. GPL-3.0-or-later. */
#include "ps5log.h"
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>
static FILE *logfile;
int fg_log_open(void){mkdir("/data/331FrameGen",0777);logfile=fopen("/data/331FrameGen/diagnostic.log","w");return logfile?0:-1;}
void ps5log_printf(int level,const char* fmt,...){if(!logfile)return;struct timespec t={0};clock_gettime(CLOCK_MONOTONIC,&t);fprintf(logfile,"%lld.%09ld level=%d ",(long long)t.tv_sec,t.tv_nsec,level);va_list a;va_start(a,fmt);vfprintf(logfile,fmt,a);va_end(a);fputc('\n',logfile);fflush(logfile);}
void ps5log_line(int level,const char* s){ps5log_printf(level,"%s",s);}
void ps5log_close(const char* reason){if(!logfile)return;ps5log_line(PS5LOG_MARK,reason);fclose(logfile);logfile=0;}

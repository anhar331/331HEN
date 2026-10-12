#define _DEFAULT_SOURCE
#include "native_file_probe.h"
#include "control.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int32_t open_result=7,stat_result,close_result;static int closes,stats;static mode_t file_mode=S_IFREG;static off_t file_size;
int32_t sceKernelOpen(const char*p,int32_t flags,mode_t mode){assert(p&&flags==(O_RDONLY|O_NOFOLLOW|O_NONBLOCK)&&mode==0);return open_result;}
int32_t sceKernelFstat(int32_t fd,struct stat*st){assert(fd==7);stats++;if(!stat_result){memset(st,0,sizeof(*st));st->st_mode=file_mode;st->st_size=file_size;}return stat_result;}
int32_t sceKernelClose(int32_t fd){assert(fd==7);closes++;return close_result;}
static int probe(const char*p,struct stat*st){return fg_native_file_probe(p,st,0);}
static int missing_stop(const char*p,struct stat*st){if(!strcmp(p,"STOP")){errno=ENOENT;return -1;}return probe(p,st);}
int main(void){struct stat st;struct fg_native_probe_trace trace;assert(fg_native_file_probe("marker",&st,&trace)==0&&closes==1&&stats==1&&S_ISREG(st.st_mode)&&st.st_size==0&&trace.open_rc==7);
 open_result=(int32_t)0x80020002u;assert(probe("marker",&st)==-1&&errno==ENOENT&&closes==1);open_result=(int32_t)0x80020001u;assert(probe("marker",&st)==-1&&errno==EPERM);open_result=-1;assert(probe("marker",&st)==-1&&errno==EIO);
 open_result=7;stat_result=(int32_t)0x8002000du;assert(probe("marker",&st)==-1&&errno==EACCES&&closes==2);stat_result=0;close_result=(int32_t)0x80020005u;assert(probe("marker",&st)==-1&&errno==EIO&&closes==3);close_result=0;
 assert(fg_control_check("STOP","mounted","installed",probe).state==FG_CONTROL_STOPPED);
 assert(fg_control_check("STOP","mounted","installed",missing_stop).state==FG_CONTROL_ENABLED);
 file_mode=S_IFDIR;assert(fg_control_check("STOP","mounted","installed",missing_stop).state==FG_CONTROL_DISABLED);file_mode=S_IFREG;file_size=1;assert(fg_control_check("STOP","mounted","installed",missing_stop).state==FG_CONTROL_DISABLED);file_size=0;
 open_result=(int32_t)(0x80020000u|ELOOP);assert(fg_control_check("STOP","mounted","installed",probe).state==FG_CONTROL_STOP_ERROR);
 puts("PASS: native filesystem flags, SCE errno mapping, descriptor closure, unknown errors, STOP precedence, empty-file activation, directory/nonempty denial and no-follow errors (11 scenarios; mocks, no PS5)");return 0;}

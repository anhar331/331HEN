#define _DEFAULT_SOURCE
#include "control.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
static const char *stop_target;static int injected_error;
static int probe(const char *p,struct stat *st){if(injected_error&&!strcmp(p,stop_target)){errno=injected_error;return -1;}return lstat(p,st);}
static void empty(const char*p){FILE*f=fopen(p,"w");assert(f);assert(fclose(f)==0);}
int main(void){char root[]="/tmp/331fg-controls-XXXXXX";assert(mkdtemp(root));char stop[256],mount[256],fallback[256],target[256];snprintf(stop,sizeof(stop),"%s/STOP",root);snprintf(mount,sizeof(mount),"%s/app0-marker",root);snprintf(fallback,sizeof(fallback),"%s/install-marker",root);snprintf(target,sizeof(target),"%s/target",root);stop_target=stop;
 struct fg_control_result r=fg_control_check(stop,mount,fallback,probe);assert(r.state==FG_CONTROL_DISABLED&&r.markers[0].error==ENOENT);
 empty(mount);r=fg_control_check(stop,mount,fallback,probe);assert(r.state==FG_CONTROL_ENABLED&&!strcmp(r.selected_path,mount));
 empty(stop);assert(fg_control_check(stop,mount,fallback,probe).state==FG_CONTROL_STOPPED);unlink(stop);
 mkdir(stop,0700);assert(fg_control_check(stop,mount,fallback,probe).state==FG_CONTROL_STOPPED);rmdir(stop);
 unlink(mount);empty(fallback);r=fg_control_check(stop,mount,fallback,probe);assert(r.state==FG_CONTROL_ENABLED&&!strcmp(r.selected_path,fallback));
 FILE*f=fopen(fallback,"w");assert(f);fputc('x',f);fclose(f);assert(fg_control_check(stop,mount,fallback,probe).state==FG_CONTROL_DISABLED);unlink(fallback);
 mkdir(fallback,0700);assert(fg_control_check(stop,mount,fallback,probe).state==FG_CONTROL_DISABLED);rmdir(fallback);
 empty(target);assert(!symlink(target,fallback));assert(fg_control_check(stop,mount,fallback,probe).state==FG_CONTROL_DISABLED);unlink(fallback);
 empty(fallback);injected_error=EACCES;assert(fg_control_check(stop,mount,fallback,probe).state==FG_CONTROL_STOP_ERROR);injected_error=EIO;assert(fg_control_check(stop,mount,fallback,probe).state==FG_CONTROL_STOP_ERROR);injected_error=0;
 assert(!symlink("/nonexistent-331fg-target",stop));assert(fg_control_check(stop,mount,fallback,probe).state==FG_CONTROL_STOPPED);unlink(stop);
 empty(mount);r=fg_control_check(stop,mount,fallback,probe);assert(r.state==FG_CONTROL_ENABLED&&!strcmp(r.selected_path,mount));
 unlink(mount);unlink(fallback);unlink(target);rmdir(root);puts("PASS: missing/mounted/installed markers; STOP precedence; wrong types/nonempty/symlink; STOP read failures; mounted preference (12 scenarios, no GPU)");return 0;}

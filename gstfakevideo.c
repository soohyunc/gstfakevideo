/* gstfakevideo 0.1 
 * (c) Mariusz Krynski, 2007 
 * email, jid: mrk(at)sed.pl
 * work based on:
 * skype_dsp_hijacker 0.8 - redirect the open system call to a second dsp device.
 *                        - patch for separate devices
 *                        - workaround a bug introduced in Skype 1.2.0.11
 *                        - support for swapping ALSA device
 *
 * Copyright (C) 2004 <snow-x@web.de>
 * Copyright (C) 2005 <jan@nsgroup.net>
 * Copyright (C) 2005-2006 Jan Slupski <jslupski@juljas.net>
 * Copyright (C) 2005 _26oo_ user of Skype Forum
 * Copyright (C) 2006 Romano Giannetti <romano@dea.icai.upcomillas.es>
 *
 * Special thanks to the KDE Project for arts. This program is based on artsdsp.
 * Copyright (C) 1998 Manish Singh <yosh@gimp.org>
 * Copyright (C) 2000 Stefan Westerfeld <stefan@space.twc.de> (aRts port)
 * 
 * Resources:
 *  [http://suedpol.dyndns.org/skype/]
 *  http://195.38.3.142:6502/skype/
 *  http://juljas.net/linux/skype/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#define HIJACKER_VERSION "0.8"

#define _GNU_SOURCE

#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#define HIJACKVIDEO "/dev/video0"

//debug
// 0 - no messages
// 1 - only very basic messages (recommended)
// 2 - more output on opening/closing sound devices
// 3 - lots of messages on open/close actions
#define DEBUG_HIJACKER 2

/* original C Library functions */
typedef int (*orig_open_ptr)(const char *pathname, int flags, ...);
typedef int (*orig_close_ptr)(int fd);
typedef int (*orig_write_ptr)(int fd, const void *buf, size_t count);
typedef int (*orig_read_ptr)(int fd, const void *buf, size_t count);
typedef int (*orig_ioctl_ptr)(int d, int request, char *argp);

static orig_open_ptr orig_open;
static orig_close_ptr orig_close;
static orig_write_ptr orig_write;
static orig_read_ptr orig_read;
static orig_ioctl_ptr orig_ioctl;

static int lib_init = 0;
static int videofd=-1;
int videopipe[2]={-1,-1};

static char *video=0;
static void skype_video_doinit(void);
static void names_init(void);

extern int shim_ioctl(unsigned long int request, char *argp);
extern int shim_open();

#define CHECK_INIT() if(!lib_init) { lib_init=1; skype_video_doinit(); names_init(); }

ssize_t write(int fd, const void *buf, size_t count)
{
	CHECK_INIT();

	if(fd==videofd);

	return orig_write(fd, buf, count);
}

ssize_t read(int fd, void *buf, size_t count)
{
	CHECK_INIT();
	if(fd==videofd) {
		return orig_read(videopipe[0],buf,count);
	}

	return orig_read(fd, buf, count);
}


int ioctl (int d, int request, char *argp)
{
	CHECK_INIT();

	if(DEBUG_HIJACKER>2)
		fprintf(stderr,"hijacker: ioctl called with fd %d\n", d);

	//clone ioctl of microphone to speakers as well
	if(d==videofd)
		return shim_ioctl(request,argp);

	return orig_ioctl(d, request, argp);
}

int close(int fd)
{
	CHECK_INIT();

	if(DEBUG_HIJACKER>2)
		fprintf(stderr,"hijacker: close called with fd %d\n", fd);

	if(fd==videofd) {
		orig_close(videopipe[0]);
		orig_close(videopipe[1]);
		videopipe[0]=-1;
		videopipe[1]=-1;
		videofd=-1;
	} 

	return orig_close(fd);
}

int open (const char *pathname, int flags, ...)
{
	va_list args;
	mode_t mode = 0;

	CHECK_INIT();

	va_start(args,flags);
	if(flags & O_CREAT) {
		if (sizeof(int) >= sizeof(mode_t)) {
			mode = va_arg(args, int);
		} else {
			mode = va_arg(args, mode_t);
		}
	}
	va_end(args);

	if(strcmp(pathname,video) == 0) {
		int result=shim_open();
		if(result<0) {
			if(DEBUG_HIJACKER>2)
				fprintf(stderr,"shim_open error\n");
			return result;
		}
		videofd=orig_open("/dev/null",O_RDONLY);
		pipe(videopipe);
		if(DEBUG_HIJACKER>2)
			fprintf(stderr,"videofd:%d, path:%s\n",videofd,pathname);
		return videofd;
	}

	/* call the original open command */

	return orig_open (pathname, flags, mode);
}

/* Save the original functions */

static void skype_video_doinit()
{
	orig_open = (orig_open_ptr)dlsym(RTLD_NEXT,"open");
	orig_close = (orig_close_ptr)dlsym(RTLD_NEXT,"close");
	orig_write = (orig_write_ptr)dlsym(RTLD_NEXT,"write");
	orig_read = (orig_write_ptr)dlsym(RTLD_NEXT,"read");
	orig_ioctl = (orig_ioctl_ptr)dlsym(RTLD_NEXT,"ioctl");
}

static void names_init(){

	video=getenv("HIJACKVIDEO");
	if(!video) video=HIJACKVIDEO;
	if(DEBUG_HIJACKER>2)
		fprintf(stderr,"%s hijacking\n",video);	
}


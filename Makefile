CC=gcc
CFLAGS=-O2 -Wall -m32 
CFLAGS += `pkg-config gstreamer-0.10 --cflags`
LIBADD_DL=-ldl `pkg-config gstreamer-0.10 --libs`
OPTIONS=-shared -fpic

PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
LIBDIR=$(PREFIX)/lib


libgstfakevideo.so:	gst.c gstfakevideo.c
	$(CC) $(CFLAGS) $(LIBADD_DL) $(OPTIONS) gst.c gstfakevideo.c -o libgstfakevideo.so

install:
	cp libgstfakevideo.so $(LIBDIR)
	chmod 0755 $(LIBDIR)/libgstfakevideo.so
	cp gstfakevideo $(BINDIR)
	chmod 0755 $(BINDIR)/gstfakevideo
	
clean:
	rm -f *.o *~ libgstfakevideo.so

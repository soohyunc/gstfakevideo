/*
(c) Mariusz Krynski, 2007 
    email, jid: mrk(at)sed.pl
work based on:
filedummy: A dummy user space video4linux driver.
(c) Harmen van der Wal, 2006.
$Id: filedummy.c,v 1.1 2006/08/16 22:24:41 harmen Exp $
License: GPLv2
See http://www.harmwal.nl/pccam880/
*/

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <unistd.h>
#include <linux/ioctl.h>
#include <linux/videodev.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <gst/gst.h>

#define ENOIOCTLCMD 515

//#define DBG(fmt,args...)
#define DBG(fmt,args...) g_print("%s %s (%d): "fmt, __FILE__, __FUNCTION__, __LINE__,##args)
#define WARN(fmt,args...) g_print("%s %s (%d): "fmt, __FILE__, __FUNCTION__, __LINE__,##args)

/* v4l */

#define CAPS_WIDTH 320
#define CAPS_HEIGHT 240

struct video_capability cap = {
  .name = "GStreamer fake video",
  .channels = 1,
  .type = VID_TYPE_CAPTURE,
  .maxwidth = CAPS_WIDTH,
  .maxheight = CAPS_HEIGHT,
  .minwidth = CAPS_WIDTH,
  .minheight = CAPS_HEIGHT,      
};

struct video_picture pic = {
  .brightness = 0xffff,
  .hue = 0xffff,
  .colour = 0xffff,
  .contrast = 0xffff,
  .whiteness = 0xffff,
  .depth = 24,
  .palette = VIDEO_PALETTE_YUV420P,
};

struct video_window window = {
  .width = CAPS_WIDTH,
  .height = CAPS_HEIGHT,
};

struct video_channel channel = {
  .name = __FILE__,
  .channel = 0,
  .type = VIDEO_TYPE_CAMERA,
};

/* file io */

extern int videopipe[2];

static GstElement *pipeline=0;

void play(GstElement *);
void stop(GstElement *);

static void
cb_handoff (GstElement *fakesrc,
	    GstBuffer  *buffer,
	    GstPad     *pad,
	    gpointer    user_data)
{
  if(videopipe[1]>=0)
    write(videopipe[1],GST_BUFFER_DATA(buffer),GST_BUFFER_SIZE(buffer));
}

void on_alarm(int sig) {
  play(pipeline);  
}

static gboolean
bus_callback (GstBus     *bus,
		 GstMessage *message,
		 gpointer    data)
{
//  DBG ("Got %s message\n", GST_MESSAGE_TYPE_NAME (message));

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR: {
      GError *err;
      gchar *debug;

      gst_message_parse_error (message, &err, &debug);
      DBG ("Error: %s\n", err->message);
      g_error_free (err);
      g_free (debug);
      break;
    }
    case GST_MESSAGE_EOS:
      /* end-of-stream */
      stop(pipeline);
      signal(SIGALRM,on_alarm);
      alarm(1);
      break;
    default:
      /* unhandled message */
      break;
  }
  return TRUE;
}

GstElement *create_pipeline () 
{
  GstElement *bin, *sink, *pipeline;
  GError *err=0;
  GstBus *bus;
  
  gst_init (0,0);

  GstCaps *caps=gst_caps_new_simple("video/x-raw-yuv",
  "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('Y', 'V', '1', '2'),
  "width",G_TYPE_INT,CAPS_WIDTH,
  "height",G_TYPE_INT,CAPS_HEIGHT,
//  "framerate",GST_TYPE_FRACTION,25,1,
  NULL);

  char *pipe_descr=getenv("GST_PIPE");
  if(!pipe_descr) {
    pipe_descr="videotestsrc is-live=true";
  }
    

  pipeline = gst_element_factory_make ("pipeline", "pipeline");
  g_assert (pipeline != NULL);

  sink = gst_element_factory_make ("fakesink", "videosink");
  g_assert (sink != NULL);
  
  bin=gst_parse_bin_from_description(pipe_descr,TRUE,&err);
  if(!bin) {
    DBG("parse bin error: %s\n",err ? err->message : "unknown error");
    return 0;
  } else {
    DBG("pipeline created\n");
  }
  gst_bin_add_many(GST_BIN(pipeline),bin,sink,NULL);
  if(gst_element_link_filtered(bin,sink,caps)) {
    DBG("pipeline linked\n");
  }
  gst_caps_unref (caps);

  g_signal_connect (sink, "handoff", G_CALLBACK (cb_handoff), NULL);

  g_object_set(G_OBJECT(sink),"signal-handoffs",TRUE,NULL);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_watch (bus, bus_callback, NULL);
  gst_object_unref (bus);

  return pipeline;
}

void play(GstElement *p) {
  gst_element_set_state(GST_ELEMENT(p),GST_STATE_PLAYING);
}

void stop(GstElement *p) {
  gst_element_set_state(GST_ELEMENT(p),GST_STATE_READY);
}


int shim_open()
{
  if(!pipeline)
    pipeline=create_pipeline();

  if(pipeline)
    play(pipeline);

  if(!pipeline)
    DBG("Can't create pipeline, fake video not available\n");
  return pipeline ? 0 : -1;
}

int shim_ioctl(unsigned long int request, char *argp){

  int result = 0;
  errno=0;

  DBG("request=%x nr %d\n", (int)request, (int)_IOC_NR(request));

  switch(request) {

  case VIDIOCGCAP:
    /* Get capabilities */
    {
      DBG("VIDIOCGCAP\n");
      memcpy(argp, &cap, sizeof(struct video_capability));
      break;
    }
  case VIDIOCGCHAN:
      /* Get channel info (sources) */
      DBG("VIDIOCGCHAN\n");
      memcpy(argp, &channel, sizeof(struct video_channel));
      break;
  case VIDIOCSCHAN:
    /* Set channel  */
    DBG("VIDIOCSCHAN\n");
    if (((struct video_channel *)argp)->channel != 0) {
      errno = EINVAL;
      result = -1;
    }
    break;
  case VIDIOCGTUNER:
    DBG("VIDIOCGTUNER\n");
    /* Get tuner abilities */
    break;
  case VIDIOCSTUNER:
    DBG("VIDIOCSTUNER\n");
    /* Tune the tuner for the current channel */
    break;
  case VIDIOCGPICT:
    /* Get picture properties */
    {
      DBG("VIDIOCGPICT\n");
      memcpy(argp, &pic, sizeof(struct video_picture));
      break;
    }
  case VIDIOCSPICT:{
    /* Set picture properties */
    struct video_picture *p=(struct video_picture *)argp;
    DBG("VIDIOCSPICT depth:%d, colorspace:%d\n",p->depth,p->palette);
    if (memcmp(argp, &pic, sizeof(struct video_picture))) {
      errno = EINVAL;
      result = -1;
    }}
    break;
  case VIDIOCCAPTURE:
    /* Start, end capture */
    DBG("VIDIOCCAPTURE\n");
    break;
  case VIDIOCGWIN:
    /* Get the video overlay window */
    DBG("VIDIOCGWIN\n");
    memcpy(argp, &window, sizeof(struct video_window));
    break;
  case VIDIOCSWIN:
    DBG("VIDIOCSWIN\n");
    /* Set the video overlay window - passes clip list for hardware smarts , chromakey etc */
    if (memcmp(argp, &window, sizeof(struct video_window))) {
      errno = EINVAL;
      result = -1;
    }
    break;
  case VIDIOCGFBUF:
    /* Get frame buffer */
  case VIDIOCSFBUF:
     /* Set frame buffer - root only */
  case VIDIOCKEY:
    /* Video key event - to dev 255 is to all - cuts capture on all DMA windows with this key (0xFFFFFFFF == all) */
  case VIDIOCGFREQ:
    /* Set tuner */
  case VIDIOCSFREQ:
    /* Set tuner */
  case VIDIOCGAUDIO:
    /* Get audio info */
  case VIDIOCSAUDIO:
    /* Audio source, mute etc */
  case VIDIOCSYNC:
      /* Sync with mmap grabbing */
  case VIDIOCMCAPTURE:
    /* Grab frames */
  case VIDIOCGMBUF:
    /* Memory map buffer info */
  case VIDIOCGUNIT:
    /* Get attached units */
  case VIDIOCGCAPTURE:
    /* Get subcapture */
  case VIDIOCSCAPTURE:
    /* Set subcapture */
  case VIDIOCSPLAYMODE:
    /* Set output video mode/feature */
  case VIDIOCSWRITEMODE:
    /* Set write mode */
  case VIDIOCGPLAYINFO:
    /* Get current playback info from hardware */
  case VIDIOCSMICROCODE:
    /* Load microcode into hardware */
  case VIDIOCGVBIFMT:
    /* Get VBI information */
  case VIDIOCSVBIFMT:
    /* Set VBI information */
    errno = EINVAL;
    result = -1;
    break;

  default:
    errno = ENOIOCTLCMD;
    result = -1;
 }

  DBG("result=%d error=%d %s\n\n", result, errno, strerror(errno));

  return result;
}


/*
 * @video_stage.c
 * @author marc-olivier.dzeukou@parrot.com
 * @date 2007/07/27
 *
 * ihm vision thread implementation
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <sys/time.h>
#include <time.h>

#include <VP_Api/vp_api.h>
#include <VP_Api/vp_api_error.h>
#include <VP_Api/vp_api_stage.h>
#include <VP_Api/vp_api_picture.h>
#include <VP_Stages/vp_stages_io_file.h>
#include <VP_Stages/vp_stages_i_camif.h>

#include <config.h>
#include <VP_Os/vp_os_print.h>
#include <VP_Os/vp_os_malloc.h>
#include <VP_Os/vp_os_delay.h>
#include <VP_Stages/vp_stages_yuv2rgb.h>
#include <VP_Stages/vp_stages_buffer_to_picture.h>
#include <VLIB/Stages/vlib_stage_decode.h>

#include <ardrone_tool/ardrone_tool.h>
#include <ardrone_tool/Com/config_com.h>

#ifndef RECORD_VIDEO
#define RECORD_VIDEO
#endif
#ifdef RECORD_VIDEO
#    include <ardrone_tool/Video/video_stage_recorder.h>
#endif

#include <ardrone_tool/Video/video_com_stage.h>

#include "Video/video_stage.h"
#include "UI/gui.h"
#include "cv.h"
#include "highgui.h" // if you want to display images with OpenCV functions
#include "Video/camshifting.h"
#include <time.h>

int64_t timespecDiff(struct timespec *timeA_p, struct timespec *timeB_p)
{
return ((timeA_p->tv_sec * 1000000000) + timeA_p->tv_nsec) - ((timeB_p->tv_sec * 1000000000) + timeB_p->tv_nsec);
}

#define NB_STAGES 10

#define OPENCV_DATA "/home/michael/Desktop/camshift/" // FIXME: Change this to the location of your cascade xml files
char *classifer = OPENCV_DATA "haarcascade_frontalface_default.xml";

PIPELINE_HANDLE pipeline_handle;

static uint8_t*  pixbuf_data       = NULL;
static vp_os_mutex_t  video_update_lock = PTHREAD_MUTEX_INITIALIZER;
static vp_os_mutex_t  face_lock = PTHREAD_MUTEX_INITIALIZER;

static CvHaarClassifierCascade* cascade = 0;
static CvMemStorage* storage = 0;
static TrackedObj* tracked_obj = 0;
static int camshiftcount = 0;

typedef struct FaceLocation
{
	int isDetected; //Boolean if face is detected
	float x; //Center x location
	float y; //Center y location
	float radius; //Average of width and height
}
FaceLocation;

static FaceLocation face_loc;

void set_face_location( float x, float y, float radius, int isDetected )
{
	vp_os_mutex_lock(&face_lock);
	face_loc.x = x;
	face_loc.y = y;
	face_loc.radius = radius;
	face_loc.isDetected = isDetected;
	vp_os_mutex_unlock(&face_lock);
}

float get_face_x()
{
	float x = -1;
	vp_os_mutex_lock(&face_lock);
	if(face_loc.isDetected)
		x = face_loc.x;
	vp_os_mutex_unlock(&face_lock);
	return x;
}

float get_face_y()
{
	float y = -1;
	vp_os_mutex_lock(&face_lock);
	if(face_loc.isDetected)
		y = face_loc.y;
	vp_os_mutex_unlock(&face_lock);
	return y;
}

float get_face_radius()
{
	float radius = -1;
	vp_os_mutex_lock(&face_lock);
	if(face_loc.isDetected)
		radius = face_loc.radius;
	vp_os_mutex_unlock(&face_lock);
	return radius;
}

C_RESULT output_gtk_stage_open( void *cfg, vp_api_io_data_t *in, vp_api_io_data_t *out)
{
  // initialize cascade classifier and storage
  cascade = (CvHaarClassifierCascade*) cvLoad(classifer, 0, 0, 0 );
  storage = cvCreateMemStorage(0);
  //validate
  assert(cascade && storage);
  return (SUCCESS);
}

IplImage *ipl_image_from_data(uint8_t* data, int reduced_image)
{
  IplImage *currframe;
  IplImage *dst;
 
  if (!reduced_image)
  {
    currframe = cvCreateImage(cvSize(320,240), IPL_DEPTH_8U, 3);
    dst = cvCreateImage(cvSize(320,240), IPL_DEPTH_8U, 3);
  }
  else
  {
    currframe = cvCreateImage(cvSize(176, 144), IPL_DEPTH_8U, 3);
    dst = cvCreateImage(cvSize(176,144), IPL_DEPTH_8U, 3);
    currframe->widthStep = 320*3;
  }
 
  currframe->imageData = data;
  cvCvtColor(currframe, dst, CV_BGR2RGB);
  cvReleaseImage(&currframe);
  return dst;
}

GdkPixbuf* pixbuf_from_opencv(IplImage *img, int resize)
{
  IplImage* converted = cvCreateImage(cvSize(img->width, img->height), IPL_DEPTH_8U, 3);
  cvCvtColor(img, converted, CV_BGR2RGB);
 
  GdkPixbuf* res = gdk_pixbuf_new_from_data(converted->imageData,
                                           GDK_COLORSPACE_RGB,
                                           FALSE,
                                           8,
                                           converted->width,
                                           converted->height,
                                           converted->widthStep,
                                           NULL,
                                           NULL);
  if (resize)
    res = gdk_pixbuf_scale_simple(res, 320, 240, GDK_INTERP_BILINEAR);
 
  return res;
}

/* Given an image and a classider, detect and return region. */
CvRect* detect_face (IplImage* image,
                     CvHaarClassifierCascade* cascade,
                     CvMemStorage* storage) {

  CvRect* rect = 0;
  
  //get a sequence of faces in image
  CvSeq *faces = cvHaarDetectObjects(image, cascade, storage,
     1.1,                       //increase search scale by 10% each pass
     6,                         //require 6 neighbors
     CV_HAAR_DO_CANNY_PRUNING,  //skip regions unlikely to contain a face
     cvSize(0, 0));             //use default face size from xml

  //if one or more faces are detected, return the first one
  if(faces && faces->total)
    printf("Num faces: %i", faces->total);
    rect = (CvRect*) cvGetSeqElem(faces, 0);

  return rect;
}

C_RESULT output_gtk_stage_transform( void *cfg, vp_api_io_data_t *in, vp_api_io_data_t *out)
{
  vp_os_mutex_lock(&video_update_lock);
 
  /* Get a reference to the last decoded picture */
  pixbuf_data      = (uint8_t*)in->buffers[0];

  vp_os_mutex_unlock(&video_update_lock);

  gdk_threads_enter();
  // GdkPixbuf structure to store the displayed picture
  static GdkPixbuf *pixbuf = NULL;
 
  if(pixbuf!=NULL)
    {
      g_object_unref(pixbuf);
      pixbuf=NULL;
    }
 
  // Convert raw data to IplImage format for OpenCV
  IplImage *img = ipl_image_from_data((uint8_t*)in->buffers[0], 0);

  // Image processing goes here
  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);
  int detected = 0;
  if (!tracked_obj || camshiftcount > 100) // If no tracked object yet, look for a face to follow
  {
    CvRect* face_rect = detect_face(img, cascade, storage);
    if (face_rect)
    {
      if (tracked_obj) destroy_tracked_object(tracked_obj); // Destroy previous, if exists
      tracked_obj = create_tracked_object(img, face_rect);
      cvRectangle(img, cvPoint(face_rect->x, face_rect->y), cvPoint(face_rect->x+face_rect->width, face_rect->y+face_rect->height), CV_RGB(255,0,0), 3, CV_AA, 0);
			cvCircle(img, cvPoint((face_rect->x + (face_rect->width)/2), (face_rect->y + (face_rect->height)/2)), 5, CV_RGB(0,0,255), -1, CV_AA, 0);
      camshiftcount = 0;
      detected = 1;
    }
  }
  if (tracked_obj && !detected)
  {
    CvBox2D face_box = camshift_track_face(img, tracked_obj);
		if(face_box.size.height != -1)
		{
    	cvEllipseBox(img, face_box, CV_RGB(255,0,0), 3, CV_AA, 0);
			cvCircle(img, cvPoint(face_box.center.x, face_box.center.y), 5, CV_RGB(0,0,255), -1, CV_AA, 0);
			set_face_location(face_box.center.x, face_box.center.y, (face_box.size.height+face_box.size.width)/2, 1);
    	camshiftcount++;
		}
		else
		{
			set_face_location(-1, -1, -1, 0);
			destroy_tracked_object(tracked_obj);
			tracked_obj = NULL;
		}
  }
  clock_gettime(CLOCK_MONOTONIC, &end);
  uint64_t timeElapsed = timespecDiff(&end, &start)/1000000;
  //printf("Face tracking time (ms): %i\n", timeElapsed);

  // Convert IplImage to GdkPixbuf to display in Gtk
  pixbuf = pixbuf_from_opencv(img, 1);

  gui_t *gui = get_gui();
  if (gui && gui->cam) // Displaying the image
    gtk_image_set_from_pixbuf(GTK_IMAGE(gui->cam), pixbuf);
  gdk_threads_leave();

  return (SUCCESS);
}

C_RESULT output_gtk_stage_close( void *cfg, vp_api_io_data_t *in, vp_api_io_data_t *out)
{
  if (cascade) cvReleaseHaarClassifierCascade(&cascade);
  if (storage) cvReleaseMemStorage(&storage);
  if (tracked_obj) destroy_tracked_object(tracked_obj);
  return (SUCCESS);
}


const vp_api_stage_funcs_t vp_stages_output_gtk_funcs =
{
  NULL,
  (vp_api_stage_open_t)output_gtk_stage_open,
  (vp_api_stage_transform_t)output_gtk_stage_transform,
  (vp_api_stage_close_t)output_gtk_stage_close
};

DEFINE_THREAD_ROUTINE(video_stage, data)
{
  C_RESULT res;

  vp_api_io_pipeline_t    pipeline;
  vp_api_io_data_t        out;
  vp_api_io_stage_t       stages[NB_STAGES];

  vp_api_picture_t picture;

  video_com_config_t              icc;
  vlib_stage_decoding_config_t    vec;
  vp_stages_yuv2rgb_config_t      yuv2rgbconf;
#ifdef RECORD_VIDEO
  video_stage_recorder_config_t   vrc;
#endif
  /// Picture configuration
  picture.format        = PIX_FMT_YUV420P;

  picture.width         = QVGA_WIDTH;
  picture.height        = QVGA_HEIGHT;
  picture.framerate     = 30;

  picture.y_buf   = vp_os_malloc( QVGA_WIDTH * QVGA_HEIGHT     );
  picture.cr_buf  = vp_os_malloc( QVGA_WIDTH * QVGA_HEIGHT / 4 );
  picture.cb_buf  = vp_os_malloc( QVGA_WIDTH * QVGA_HEIGHT / 4 );

  picture.y_line_size   = QVGA_WIDTH;
  picture.cb_line_size  = QVGA_WIDTH / 2;
  picture.cr_line_size  = QVGA_WIDTH / 2;

  vp_os_memset(&icc,          0, sizeof( icc ));
  vp_os_memset(&vec,          0, sizeof( vec ));
  vp_os_memset(&yuv2rgbconf,  0, sizeof( yuv2rgbconf ));

  icc.com                 = COM_VIDEO();
  icc.buffer_size         = 100000;
  icc.protocol            = VP_COM_UDP;
  COM_CONFIG_SOCKET_VIDEO(&icc.socket, VP_COM_CLIENT, VIDEO_PORT, wifi_ardrone_ip);

  vec.width               = QVGA_WIDTH;
  vec.height              = QVGA_HEIGHT;
  vec.picture             = &picture;
  vec.block_mode_enable   = TRUE;
  vec.luma_only           = FALSE;

  yuv2rgbconf.rgb_format = VP_STAGES_RGB_FORMAT_RGB24;
#ifdef RECORD_VIDEO
  vrc.fp = NULL;
#endif

  pipeline.nb_stages = 0;

  stages[pipeline.nb_stages].type    = VP_API_INPUT_SOCKET;
  stages[pipeline.nb_stages].cfg     = (void *)&icc;
  stages[pipeline.nb_stages].funcs   = video_com_funcs;

  pipeline.nb_stages++;

#ifdef RECORD_VIDEO
  stages[pipeline.nb_stages].type    = VP_API_FILTER_DECODER;
  stages[pipeline.nb_stages].cfg     = (void*)&vrc;
  stages[pipeline.nb_stages].funcs   = video_recorder_funcs;

  pipeline.nb_stages++;
#endif // RECORD_VIDEO
  stages[pipeline.nb_stages].type    = VP_API_FILTER_DECODER;
  stages[pipeline.nb_stages].cfg     = (void*)&vec;
  stages[pipeline.nb_stages].funcs   = vlib_decoding_funcs;

  pipeline.nb_stages++;

  stages[pipeline.nb_stages].type    = VP_API_FILTER_YUV2RGB;
  stages[pipeline.nb_stages].cfg     = (void*)&yuv2rgbconf;
  stages[pipeline.nb_stages].funcs   = vp_stages_yuv2rgb_funcs;

  pipeline.nb_stages++;

  stages[pipeline.nb_stages].type    = VP_API_OUTPUT_SDL;
  stages[pipeline.nb_stages].cfg     = NULL;
  stages[pipeline.nb_stages].funcs   = vp_stages_output_gtk_funcs;

  pipeline.nb_stages++;

  pipeline.stages = &stages[0];
 
  /* Processing of a pipeline */
  if( !ardrone_tool_exit() )
  {
    PRINT("\n   Video stage thread initialisation\n\n");

    res = vp_api_open(&pipeline, &pipeline_handle);

    if( SUCCEED(res) )
    {
      int loop = SUCCESS;
      out.status = VP_API_STATUS_PROCESSING;

      while( !ardrone_tool_exit() && (loop == SUCCESS) )
      {
          if( SUCCEED(vp_api_run(&pipeline, &out)) ) {
            if( (out.status == VP_API_STATUS_PROCESSING || out.status == VP_API_STATUS_STILL_RUNNING) ) {
              loop = SUCCESS;
            }
          }
          else loop = -1; // Finish this thread
      }

      vp_api_close(&pipeline, &pipeline_handle);
    }
  }

  PRINT("   Video stage thread ended\n\n");

  return (THREAD_RET)0;
}


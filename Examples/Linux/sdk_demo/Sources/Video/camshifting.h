#ifndef INC_OPENCV
#define INC_OPENCV
#include <stdio.h>
#include <cv.h>
#include <highgui.h>
#endif

#ifndef INC_CAMSHIFTING
#define INC_CAMSHIFTING

typedef struct {
  IplImage* hsv;     //input image converted to HSV
  IplImage* hue;     //hue channel of HSV image
  IplImage* mask;    //image for masking pixels
  IplImage* prob;    //face probability estimates for each pixel

  CvHistogram* hist; //histogram of hue in original face image

  CvRect prev_rect;  //location of face in previous frame
  CvBox2D curr_box;  //current face location estimate
} TrackedObj;

TrackedObj* create_tracked_object (IplImage* image, CvRect* face_rect);
void destroy_tracked_object (TrackedObj* tracked_obj);
CvBox2D camshift_track_face (IplImage* image, TrackedObj* imgs);
void update_hue_image (const IplImage* image, TrackedObj* imgs);

#endif
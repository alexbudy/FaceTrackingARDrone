#include "camshifting.h"

/* Create a camshift tracked object from a region in image. */
TrackedObj* create_tracked_object (IplImage* image, CvRect* region) {
  TrackedObj* obj;
  
  //allocate memory for tracked object struct
  if((obj = malloc(sizeof *obj)) != NULL) {
    //create-image: size(w,h), bit depth, channels
    obj->hsv  = cvCreateImage(cvGetSize(image), 8, 3);
    obj->mask = cvCreateImage(cvGetSize(image), 8, 1);
    obj->hue  = cvCreateImage(cvGetSize(image), 8, 1);
    obj->prob = cvCreateImage(cvGetSize(image), 8, 1);

    int hist_bins = 30;           //number of histogram bins
    float hist_range[] = {0,180}; //histogram range
    float* range = hist_range;
    obj->hist = cvCreateHist(1,             //number of hist dimensions
                             &hist_bins,    //array of dimension sizes
                             CV_HIST_ARRAY, //representation format
                             &range,        //array of ranges for bins
                             1);            //uniformity flag
  }
  
  //create a new hue image
  update_hue_image(image, obj);

  float max_val = 0.f;
  
  //create a histogram representation for the face
  cvSetImageROI(obj->hue, *region);
  cvSetImageROI(obj->mask, *region);
  cvCalcHist(&obj->hue, obj->hist, 0, obj->mask);
  cvGetMinMaxHistValue(obj->hist, 0, &max_val, 0, 0 );
  cvConvertScale(obj->hist->bins, obj->hist->bins,
                 max_val ? 255.0/max_val : 0, 0);
  cvResetImageROI(obj->hue);
  cvResetImageROI(obj->mask);
  
  //store the previous face location
  obj->prev_rect = *region;

  return obj;
}

/* Release resources from tracked object. */
void destroy_tracked_object (TrackedObj* obj) {
  cvReleaseImage(&obj->hsv);
  cvReleaseImage(&obj->hue);
  cvReleaseImage(&obj->mask);
  cvReleaseImage(&obj->prob);
  cvReleaseHist(&obj->hist);

  free(obj);
}

/* Given an image and tracked object, return box position. */
CvBox2D camshift_track_face (IplImage* image, TrackedObj* obj) {
  CvConnectedComp components;

  //create a new hue image
  update_hue_image(image, obj);

  //create a probability image based on the face histogram
  cvCalcBackProject(&obj->hue, obj->prob, obj->hist);
  cvAnd(obj->prob, obj->mask, obj->prob, 0);

  //use CamShift to find the center of the new face probability
  cvCamShift(obj->prob, obj->prev_rect,
             cvTermCriteria(CV_TERMCRIT_EPS | CV_TERMCRIT_ITER, 10, 1),
             &components, &obj->curr_box);

  //update face location and angle
  obj->prev_rect = components.rect;
  obj->curr_box.angle = -obj->curr_box.angle;

  return obj->curr_box;
}

void update_hue_image (const IplImage* image, TrackedObj* obj) {
  //limits for calculating hue
  int vmin = 65, vmax = 256, smin = 55;
  
  //convert to HSV color model
  cvCvtColor(image, obj->hsv, CV_BGR2HSV);
  
  //mask out-of-range values
  cvInRangeS(obj->hsv,                               //source
             cvScalar(0, smin, MIN(vmin, vmax), 0),  //lower bound
             cvScalar(180, 256, MAX(vmin, vmax) ,0), //upper bound
             obj->mask);                             //destination
  
  //extract the hue channel, split: src, dest channels
  cvSplit(obj->hsv, obj->hue, 0, 0, 0 );
}
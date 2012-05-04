#ifndef _IHM_STAGES_O_GTK_H
#define _IHM_STAGES_O_GTK_H

#include <config.h>
#include <VP_Api/vp_api_thread_helper.h>

float get_face_x();
float get_face_y();
float get_face_radius();

PROTO_THREAD_ROUTINE(video_stage, data);

#endif // _IHM_STAGES_O_GTK_H

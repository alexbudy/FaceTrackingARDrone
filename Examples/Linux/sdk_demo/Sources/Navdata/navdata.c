#include <ardrone_tool/Navdata/ardrone_navdata_client.h>

#include <Navdata/navdata.h>
#include <Video/video_stage.h>
#include <stdio.h>

#define LIFTOFF 0
#define FOLLOW 1
#define LAND 2

/* Initialization local variables before event loop  */
inline C_RESULT demo_navdata_client_init( void* data )
{
  return C_OK;
}

/* Receving navdata during the event loop */
inline C_RESULT demo_navdata_client_process( const navdata_unpacked_t* const navdata )
{
	const navdata_demo_t*nd = &navdata->navdata_demo;
	
	static int stage = 0;
	static float32_t init_psi;
	float x;
	float y;
	float radius;
	float gaz = 0;
	float yaw = 0;
	float theta = 0;
	//ardrone_tool_set_ui_pad_start(0);
	
    // other values
    int height0 = 1500; // approximately eye-level for 5'7", in mm or pt

	printf("STAGE: %i \n", stage);
	switch(stage) {
	case LIFTOFF:
		//ardrone_tool_set_ui_pad_start(1);
		if (nd->altitude > 300) {
			stage = 1;
			init_psi = nd->psi;		
		}
		break;
	case FOLLOW:
		x = get_face_x();
		y = get_face_y();
		if (x != -1)
			yaw = (x-160)/160;
		if (y != -1)
			gaz = (120-y)/120;
		ardrone_at_set_progress_cmd(0, 0, theta, gaz, yaw);
		break;
	case LAND:
		ardrone_tool_set_ui_pad_start(0);
		break;
	}
	
	//ardrone_tool_set_ui_pad_start(1);
	/*
	static int stage = 0;
	static float32_t goal_psi, initial_psi;
	printf("STAGE: %i \n", stage);
	if (nd->altitude < 8) { //liftoff
		ardrone_tool_set_ui_pad_start(1);
		stage++;	
		initial_psi = nd->psi;
	} else if (stage == 1 || nd->psi > initial_psi + 50 && nd->psi < initial_psi - 50 ) { //turn
		ardrone_at_set_progress_cmd(0, 0, 0, 0, -.5);
		stage++;
	} else {
		ardrone_tool_set_ui_pad_start(0);	
	}
	*/

	//	
                                
	/*printf("=====================\nNavdata for flight demonstrations =====================\n\n");

	printf("Control state : %i\n",nd->ctrl_state);*/
	printf("Battery level : %i mV\n",nd->vbat_flying_percentage);
	/*printf("Orientation   : [Theta] %4.3f  [Phi] %4.3f  [Psi] %4.3f\n",nd->theta,nd->phi,nd->psi);
	printf("Altitude      : %i\n",nd->altitude);
	printf("Speed         : [vX] %4.3f  [vY] %4.3f  [vZPsi] %4.3f\n",nd->theta,nd->phi,nd->psi);*/
	printf("Radius        : %4.3f\n", get_face_radius());
	//printf("\033[8A");

  return C_OK;
}

/* Relinquish the local resources after the event loop exit */
inline C_RESULT demo_navdata_client_release( void )
{
  return C_OK;
}

/* Registering to navdata client */
BEGIN_NAVDATA_HANDLER_TABLE
  NAVDATA_HANDLER_TABLE_ENTRY(demo_navdata_client_init, demo_navdata_client_process, demo_navdata_client_release, NULL)
END_NAVDATA_HANDLER_TABLE


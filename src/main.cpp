#include <stdio.h>
#include <string.h>
#ifdef WIN32
#include "windows/serial.h"
#else
#include "serial.h"
#endif
#include "crc32.h"
#include "protocal_uart_sdk.h"
#include "DJI_guidance.h"

#include <ros/ros.h>

#define UART 1
#define CAMERA_PAIR_NUM 5
#ifdef WIN32
#define UART_PORT "COM5"
#else
#define UART_PORT 3 
#endif

#include <stdio.h>
#include <string.h>
#include <iostream>
#include <ros/ros.h>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/Image.h>
#include <sensor_msgs/image_encodings.h>

#include <opencv2/opencv.hpp>

#include "DJI_guidance.h"
#include "DJI_utility.h"

#include <geometry_msgs/TransformStamped.h> //IMU
#include <geometry_msgs/Vector3Stamped.h> //velocity
#include <sensor_msgs/LaserScan.h> //obstacle distance & ultrasonic



using namespace cv;

int main()
{
	if ( connect_serial( UART_PORT ) < 0 )
	{
        printf( "connect serial error\n" );
        return 0;
	}
	printf("serial connected (main)\n");

	stereo_cali cali[CAMERA_PAIR_NUM];
	int err_code = get_stereo_cali(cali);
	printf("err code %d \n",err_code);

	for ( int i = 0; i < 1000; ++i )
	{
		//printf("+++++++++++++++++++++++++++++++NEW DATA+++++++++++++++++++++++++++++++++\n");
		unsigned char data[1000] = {0};
		int max_size = (int)sizeof(data);
		int timeout = 1000;
		int n = read_serial( data, max_size, timeout);
		
		if( n <= 0 )
		{
			continue;
		}

		push( data, sizeof(data) );
		for ( ; ; )
		{
			unsigned int len = 0;
			int has_packet = pop( data, len );
			if ( has_packet )
			{
				if ( len )
				{
					unsigned char cmd_id = data[1];
					
					//printf("event id %u\n",data[1]);
/*					if(cmd_id==e_imu){
						printf("imu data received\n");
					}
					else if(cmd_id==e_ultrasonic){
						printf("ultrasonic data received\n");
					}
					else if(cmd_id==e_velocity){
						printf("velocity data received\n");
					}
					else if(cmd_id==e_obstacle_distance){
						printf("obstacle data received\n");
					}
					else if(cmd_id==e_motion){
						printf("motion data received\n");
					}*/
							
					if ( e_imu == cmd_id )
					{
						imu imu_data;
						memcpy( &imu_data, data + 2, sizeof(imu_data) );
/*						printf( "imu:%f %f %f,%f %f %f %f\n", imu_data.acc_x, imu_data.acc_y, imu_data.acc_z, 
							     imu_data.q[0], imu_data.q[1], imu_data.q[2], imu_data.q[3] );*/
						/*printf( "frame index:%d,stamp:%d\n", imu_data.frame_index, imu_data.time_stamp );
						printf( "\n" );*/
					}
					
					if ( e_ultrasonic == cmd_id )
					{
						ultrasonic_data ultrasonic;
						memcpy( &ultrasonic, data + 2, sizeof(ultrasonic) );
/*						for ( int d = 0; d < CAMERA_PAIR_NUM; ++d )
						{
							printf( "distance:%f,reliability:%d\n", ultrasonic.ultrasonic[d] * 0.001f, (int)ultrasonic.reliability[d] );
						}*/
						/*printf( "frame index:%d,stamp:%d\n", ultrasonic.frame_index, ultrasonic.time_stamp );
						printf( "\n" );*/
					}
					if ( e_velocity == cmd_id )
					{
						velocity vo;
						soc2pc_vo_can_output output;
						memcpy( &output, data + 2, sizeof(vo) );
						vo.vx = output.m_vo_output.vx;
						vo.vy = output.m_vo_output.vy;
						vo.vz = output.m_vo_output.vz;
/*						printf( "vx:%f vy:%f vz:%f\n", 0.001f * vo.vx, 0.001f * vo.vy, 0.001f * vo.vz );
						printf( "frame index:%d,stamp:%d\n", vo.frame_index, vo.time_stamp );
						printf( "\n" );*/
					}
					if ( e_obstacle_distance == cmd_id )
					{
						obstacle_distance oa;
						memcpy( &oa, data + 2, sizeof(oa) );
/*						printf( "obstacle distance:" );
						for ( int direction = 0; direction < CAMERA_PAIR_NUM; ++direction )
						{
							printf( " %f ", 0.01f * oa.distance[direction] );
						}*/
						/*printf( "\n" );
						printf( "frame index:%d,stamp:%d\n", oa.frame_index, oa.time_stamp );
						printf( "\n" );*/
					}
					
					if( e_motion == cmd_id )
					{
						motion mo;
						memcpy( &mo, data + 2, sizeof(mo));mo
						printf("new motion data ");
						
						
						printf("corresponding_imu_index %d\n",mo.corresponding_imu_index);
						printf("q0 %f q1 %f q2 %f q3 %f \n",mo.q0,mo.q1,mo.q2,mo.q3);
						printf("attitude status %d \n",mo.attitude_status);
						printf("positions in global %f %f %f \n",mo.position_in_global_x,mo.position_in_global_y,mo.position_in_global_z);
						printf("position status %d\n",mo.position_status);
						printf("velocity_in_global_x  %f %f %f \n",mo.velocity_in_global_x,mo.velocity_in_global_y,mo.velocity_in_global_z);
						printf("velocity_status  %d\n",mo.velocity_status);

						printf("reserve_int %d %d %d %d\n ",mo.reserve_int[0],mo.reserve_int[1],mo.reserve_int[2],mo.reserve_int[3]);
						printf("uncertainty_location %f %f %f %f \n", mo.uncertainty_location[0],mo.uncertainty_location[1],mo.uncertainty_location[2],mo.uncertainty_location[3]);
						printf("uncertainty_velocity %f %f %f %f \n", mo.uncertainty_velocity[0],mo.uncertainty_velocity[1],mo.uncertainty_velocity[2],mo.uncertainty_velocity[3]);


								
								
						
						
						
					}
					
					
					
				}
				else
				{
					printf( "err\n" );
				}
			}
			else
			{
				break;
			}
		}
	}

	disconnect_serial();

	return 0;
}

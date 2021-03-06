#include <stdio.h>
#include <string.h>
#include "serial.h"
#include "crc32.h"
#include "protocal_uart_sdk.h"
#include "DJI_guidance.h"
#include "DJI_utility.h"

#include <ros/ros.h>
#include <sensor_msgs/LaserScan.h>
#include <sensor_msgs/Imu.h>
#include <geometry_msgs/Vector3Stamped.h>
#include <guidance_uart/Motion.h>
#include <iostream>

#define CAMERA_PAIR_NUM 5

// PLEASE change this port parameter depending on your connection.
// my connection is on port UART3 ~ "ttyTHS2"
char UART_PORT[50] = "/dev/ttyTHS2";

ros::Publisher motion_pub;
ros::Publisher obstacle_distance_pub;
ros::Publisher velocity_pub;
ros::Publisher ultrasonic_pub;
ros::Publisher imu_pub;

int callback ()
{
	printf("Connecting on serial port \"%s\" ...\n",UART_PORT);
	
	if ( connect_serial( UART_PORT ) < 0 )
	{
		printf( "Cannot serial connect on the port.\n" );
		return 0;
	}
	printf("Creating topics and posting ...\n");
	for ( int i = 0; i < 1000; ++i )
	{
		unsigned char data[1000] = {0};
		int max_size = (int)sizeof(data);
		int timeout = 1000;
		int n = read_serial( data, max_size, timeout);
		
		if( n <= 0 )
		{
			continue;
		}

		push( data, sizeof(data) );
		//printf("######### NEW DATA ############\n");
		for ( ; ; )
		{
			unsigned int len = 0;
			int has_packet = pop( data, len );			
			if ( has_packet )
			{
				if ( len )
				{
					unsigned char cmd_id = data[1];
					
					//printf("event id %u\n",cmd_id);
					
					//no image data posted via UART
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
						sensor_msgs::Imu imu_tmp;			
																
						imu_tmp.header.frame_id  = "guidance_uart";
						imu_tmp.header.stamp	 = ros::Time::now();
						imu_tmp.linear_acceleration.x=imu_data.acc_x;
						imu_tmp.linear_acceleration.y=imu_data.acc_y;
						imu_tmp.linear_acceleration.z=imu_data.acc_z;
						//< quaternion: [w,x,y,z], info from DJI_guidance.h
						imu_tmp.orientation.w=imu_data.q[0];
						imu_tmp.orientation.x=imu_data.q[1];
						imu_tmp.orientation.y=imu_data.q[2];
						imu_tmp.orientation.z=imu_data.q[3];
						imu_pub.publish(imu_tmp);
						
/*						printf( "imu:%f %f %f,%f %f %f %f\n", imu_data.acc_x, imu_data.acc_y, imu_data.acc_z, 
							     imu_data.q[0], imu_data.q[1], imu_data.q[2], imu_data.q[3] );*/
						/*printf( "frame index:%d,stamp:%d\n", imu_data.frame_index, imu_data.time_stamp );
						printf( "\n" );*/
					}
					
					if ( e_ultrasonic == cmd_id )
					{
						ultrasonic_data ultrasonic;
						memcpy( &ultrasonic, data + 2, sizeof(ultrasonic) );

						sensor_msgs::LaserScan ultrasonic_dists_tmp;
						ultrasonic_dists_tmp.ranges.resize(CAMERA_PAIR_NUM);
						ultrasonic_dists_tmp.intensities.resize(CAMERA_PAIR_NUM);						
						ultrasonic_dists_tmp.header.frame_id  = "guidance_uart";
						ultrasonic_dists_tmp.header.stamp	 = ros::Time::now();					
						
						for ( int d = 0; d < CAMERA_PAIR_NUM; ++d )
						{
							ultrasonic_dists_tmp.ranges[d] = 0.001f * ultrasonic.ultrasonic[d];
							ultrasonic_dists_tmp.intensities[d] = 1.0 * ultrasonic.reliability[d];
							//printf( "distance:%f,reliability:%d\n", ultrasonic.ultrasonic[d] * 0.001f, (int)ultrasonic.reliability[d] );
						}
						ultrasonic_pub.publish(ultrasonic_dists_tmp);						
						
						/*printf( "frame index:%d,stamp:%d\n", ultrasonic.frame_index, ultrasonic.time_stamp );
						printf( "\n" );*/
					}
					
					if ( e_velocity == cmd_id )
					{
						velocity vo;
						geometry_msgs::Vector3Stamped vec_tmp;
						vec_tmp.header.frame_id  = "guidance_uart";
						vec_tmp.header.stamp	 = ros::Time::now();

						soc2pc_vo_can_output output;
						memcpy( &output, data + 2, sizeof(vo) );
						vo.vx = output.m_vo_output.vx ;
						vo.vy = output.m_vo_output.vy ;
						vo.vz = output.m_vo_output.vz ;
						vec_tmp.vector.x = 0.001f * vo.vx ;
						vec_tmp.vector.y = 0.001f * vo.vy ;
						vec_tmp.vector.z = 0.001f * vo.vz ;
						velocity_pub.publish(vec_tmp);							
						
						//printf( "Velocities vx:%f vy:%f vz:%f\n", 0.001f * vo.vx, 0.001f * vo.vy, 0.001f * vo.vz );
/*						printf( "frame index:%d,stamp:%d\n", vo.frame_index, vo.time_stamp );
						printf( "\n" );*/
					}
					
					if ( e_obstacle_distance == cmd_id )
					{
						obstacle_distance oa;
						memcpy( &oa, data + 2, sizeof(oa) );
						sensor_msgs::LaserScan obstacle_dists_tmp;
						obstacle_dists_tmp.ranges.resize(CAMERA_PAIR_NUM);
						obstacle_dists_tmp.header.frame_id  = "guidance_uart";
						obstacle_dists_tmp.header.stamp	 = ros::Time::now();
						
						/**
						* obstacle_distance
						* Define obstacle distance calculated by fusing vision and ultrasonic sensors. Unit is `cm`.
						*/
						
						//printf( "obstacle distance:" );
						for ( int direction = 0; direction < CAMERA_PAIR_NUM; ++direction )
						{
							obstacle_dists_tmp.ranges[direction] = 0.01f * oa.distance[direction] ;
							//printf( " %f ", 0.01f * oa.distance[direction] );
						}
						obstacle_distance_pub.publish(obstacle_dists_tmp);
						
						/*printf( "\n" );
						printf( "frame index:%d,stamp:%d\n", oa.frame_index, oa.time_stamp );
						printf( "\n" );*/
					}
					
					if( e_motion == cmd_id )
					{
						motion mo;
						memcpy( &mo, data + 2, sizeof(mo));
						guidance_uart::Motion motion_tmp;
						
						motion_tmp.frame_id  = "guidance_uart";
						motion_tmp.stamp	 = ros::Time::now();			
				
						motion_tmp.corresponding_imu_index = mo.corresponding_imu_index;
						motion_tmp.q0=mo.q0; motion_tmp.q1=mo.q1; motion_tmp.q2=mo.q2; motion_tmp.q3=mo.q3;
						motion_tmp.attitude_status = mo.attitude_status;
						motion_tmp.position_in_global_x = mo.position_in_global_x;
						motion_tmp.position_in_global_y = mo.position_in_global_y;
						motion_tmp.position_in_global_z = mo.position_in_global_z;
						motion_tmp.position_status = mo.position_status;
						motion_tmp.velocity_in_global_x = mo.velocity_in_global_x;
						motion_tmp.velocity_in_global_y = mo.velocity_in_global_y;
						motion_tmp.velocity_in_global_z = mo.velocity_in_global_z;
						motion_tmp.velocity_status = mo.velocity_status;
						
						int size_ind=sizeof(mo.reserve_float)/sizeof(mo.reserve_float[0]);
						for(int i=0;i<size_ind;i++){
							motion_tmp.reserve_float.push_back(mo.reserve_float[i]);
						}
						
						size_ind=sizeof(mo.reserve_int)/sizeof(mo.reserve_int[0]);
						for(int i=0;i<size_ind;i++){
							motion_tmp.reserve_int.push_back(mo.reserve_int[i]);
						}
						
						size_ind=sizeof(mo.uncertainty_location)/sizeof(mo.uncertainty_location[0]);
						for(int i=0;i<size_ind;i++){
							motion_tmp.uncertainty_location.push_back(mo.uncertainty_location[i]);
						}
						
						size_ind=sizeof(mo.uncertainty_velocity)/sizeof(mo.uncertainty_velocity[0]);
						for(int i=0;i<size_ind;i++){
							motion_tmp.uncertainty_velocity.push_back(mo.uncertainty_velocity[i]);
						}
						
						//posting motion data ..
						motion_pub.publish(motion_tmp);

						
/*						printf("corresponding_imu_index %d\n",mo.corresponding_imu_index);
						printf("q0 %f q1 %f q2 %f q3 %f \n",mo.q0,mo.q1,mo.q2,mo.q3);
						printf("attitude status %d \n",mo.attitude_status);
						printf("positions in global %f %f %f \n",mo.position_in_global_x,mo.position_in_global_y,mo.position_in_global_z);
						printf("position status %d\n",mo.position_status);
						printf("velocity_in_global_x  %f %f %f \n",mo.velocity_in_global_x,mo.velocity_in_global_y,mo.velocity_in_global_z);
						printf("velocity_status  %d\n",mo.velocity_status);

						printf("reserve_int %d %d %d %d\n ",mo.reserve_int[0],mo.reserve_int[1],mo.reserve_int[2],mo.reserve_int[3]);
						printf("uncertainty_location %f %f %f %f \n", mo.uncertainty_location[0],mo.uncertainty_location[1],mo.uncertainty_location[2],mo.uncertainty_location[3]);
						printf("uncertainty_velocity %f %f %f %f \n", mo.uncertainty_velocity[0],mo.uncertainty_velocity[1],mo.uncertainty_velocity[2],mo.uncertainty_velocity[3]);
							*/	
						
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
	
	return -1;
}

int main(int argc, char **argv)
{
	ros::init(argc, argv, "guidance_uart");
	ros::NodeHandle n("~");

	std::string str;
	n.param<std::string>("uart_port", str, "/dev/ttyTHS2");
	
	if(str!="/dev/ttyTHS2"){ //if not default value, read value from launch file..
		std::copy(str.begin(), str.end(), UART_PORT);
		UART_PORT[str.size()] = '\0';
	}
	
	ros::Rate loop_rate(10);
	
	motion_pub		= n.advertise<guidance_uart::Motion>("/guidance_uart/motion",1);
	imu_pub			= n.advertise<sensor_msgs::Imu>("/guidance_uart/imu",1);
	velocity_pub  		= n.advertise<geometry_msgs::Vector3Stamped>("/guidance_uart/velocity",1);
	obstacle_distance_pub	= n.advertise<sensor_msgs::LaserScan>("/guidance_uart/obstacle_distance",1);
	ultrasonic_pub		= n.advertise<sensor_msgs::LaserScan>("/guidance_uart/ultrasonic",1);
	
	while(ros::ok){
		callback();
		ros::spinOnce();
	}

	return 0;
}


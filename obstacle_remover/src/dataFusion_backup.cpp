#include <math.h>
#include <stdlib.h>
#include <iostream>

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include <ros/ros.h>
#include <image_transport/image_transport.h>
#include <cv_bridge/cv_bridge.h>

#include <sensor_msgs/LaserScan.h>

using namespace cv;
using namespace std;

void imageCallback(const sensor_msgs::ImageConstPtr& msg);
void laserdata_recieved(const sensor_msgs::LaserScan &LaserScan); //change calibration and transformation here
void obstacleEliminate(Mat temp); //this blacks out stuff
void blackOut(Mat image, float x, float y); //change colour of objects here
void blackOut_big(Mat image, float y, float x);

cv::Mat frame = Mat::zeros( 200, 200,CV_8UC3);
cv::Mat black;

float angle_min;
float angle_max;
float angle_increment;

float ranges[360];  // change this with sensor //calculated as (angle_max - angle_min) / angle_increment // 
       			   //stores the angle in order from angle_min -> angle_max
			   //the value represents the distance of objects

float lidar[360][2]; //0th element is z and other is x 
float camera[360][4];
float image_x[360];
float image_y[360];


//following represents intrinsic matrix of the camera
float k[3][3] = {{ 476.7030836014194, 0.0, 400.5},
	            { 0.0, 476.7030836014194, 400.5},
	            { 0.0, 0.0, 1.0}};

double c_pos[6] = {0, -0.2, 0, 0.3 ,0, 0};

double tmp1[4][4] = {    {cos(c_pos[5]), sin(c_pos[5]),        0,    0},
		        {-sin(c_pos[5]), cos(c_pos[5]),      0,    0},
		        {                 0,                     0,      1,    0},
		        {                 0,                     0,      0,    1}};

Mat Rz = Mat(4, 4, CV_64FC1, &tmp1);


double tmp2[4][4] = {    {  cos(c_pos[4]),                    0 ,    -sin(c_pos[4]),     0},
		{                    0,                     1,                       0,     0},
		{ -sin(c_pos[4]),                      0,    cos(c_pos[4]),     0},
		{                    0,                      0,                      0,     1}};

Mat Ry = Mat(4, 4, CV_64FC1, &tmp2);


double tmp3[4][4] = {    {                    1,                    0 ,                       0,      0},
		{                    0,    cos(c_pos[3]),   -sin(c_pos[3]),      0},
		{                    0,   -sin(c_pos[3]),    cos(c_pos[3]),      0},
		{                    0,                      0,                      0,      1}};

Mat Rx = Mat(4, 4, CV_64FC1, &tmp3);

Mat R = Rz*Ry*Rx;

double tmp[4][4] = {    {                    0,                    0 ,                      0,      c_pos[0] },
			{                    0,                     0,                      0,      c_pos[1] },
			{                    0,                     0,                      0,      c_pos[2] },
			{                    0,                     0,                      0,                 0} };

Mat trans = Mat(4, 4, CV_64FC1, &tmp);

Mat tsf = trans;


int main( int argc, char **argv )
{
	tsf =  tsf + R;


	for(int i =0; i < 4; i++)
	{
		for(int j =0; j < 4; j++)
			cout << tsf.at<double>(i , j)<< ", ";
		cout << endl;
	}
	
	ros::init(argc, argv, "dataFusion");
	ros::NodeHandle n;
	
	image_transport::ImageTransport it(n);   
	image_transport::Publisher pub = it.advertise("obstacle_free_image", 1);
	
	//scaning
	image_transport::Subscriber sub = it.subscribe("/image", 20, imageCallback);
	ros::Subscriber sub_laser = n.subscribe("scan", 3, laserdata_recieved);

	//publishing
	sensor_msgs::ImagePtr msg;
	msg = cv_bridge::CvImage(std_msgs::Header(), "bgr8", black).toImageMsg();
	pub.publish(msg);
	
	ros::spin();

	return 0;
}

void obstacleEliminate(Mat image)
{	
	cv::Mat black =  image;

	int positive[360];

	for(int i = 0; i < 360; i++) 
		positive[i] = 0;

	
	Mat pose, c_tmp;

	for(int i = 0; i != 359 ; i = (i + 1) % 360) //only project what is in front of the lidar
	{
		int iy = floor( image_y[i] );
		int ix = floor( image_x[i] );

		if(ranges[i] < 20 && ix >= 2 && ix< black.cols - 2 && iy >= 2 && iy < black.rows - 2)
		{	
			
			
			double tmp[4][1]= {     { lidar[i][1] },
							{         0.2     },
							{ lidar[i][0] },
							{         1     }  };


				pose = Mat(4, 1, CV_64FC1, &tmp);

				c_tmp = tsf * pose;

				double s = c_tmp.at<double>(3,0);
				double x = c_tmp.at<double>(0,0)  / s;
				double y = c_tmp.at<double>(1,0)  / s;
				double z = c_tmp.at<double>(2,0)  / s;

				int image_y_temp  = ( k[1][0] * x + k[1][1] * y + k[1][2] * z )/ z;  //scaling factor lidar[i][0]  is constant
				
				double tmp2[4][1]= {     { lidar[i][1] },
							{        -0.2     },
							{ lidar[i][0] },
							{         1     }  };


				pose = Mat(4, 1, CV_64FC1, &tmp2);

				c_tmp = tsf * pose;

				 s = c_tmp.at<double>(3,0);
				 x = c_tmp.at<double>(0,0)  / s;
				 y = c_tmp.at<double>(1,0)  / s;
				 z = c_tmp.at<double>(2,0)  / s;

				int image_y_temp2  = ( k[1][0] * x + k[1][1] * y + k[1][2] * z )/ z;  //scaling factor lidar[i][0]  is constant


			positive[i] = 1; //something here

			for(int j = image_y_temp; j < image_y_temp2; j ++)
				for(int k = ix - 7; k < ix + 7; k++) //width of the angle
					if( k >= 2 && k < black.cols - 2 && j >= 2 && j < black.rows - 2){
						
						blackOut(black, j, k);
				}		
		}
	}
	

	//filling gaps in obstacles
	for(int i = 0; i != 359; i = (i + 1) % 360) //only project what is in front of the lidar
	{
		if( positive[i] == 0 && positive[ i - 1]  == 1 && positive[ i + 1 ] == 1)
		{	
			i = i - 1; //project the previous point

			int iy = floor( image_y[i] );
			int ix = floor( image_x[i] );

			if(ix >= 2 && ix< black.cols - 2 && iy >= 2 && iy < black.rows - 2)
			{

				double tmp[4][1]= {     { lidar[i][1] },
							{         0.2     },
							{ lidar[i][0] },
							{         1     }  };


				pose = Mat(4, 1, CV_64FC1, &tmp);

				c_tmp = tsf * pose;

				double s = c_tmp.at<double>(3,0);
				double x = c_tmp.at<double>(0,0)  / s;
				double y = c_tmp.at<double>(1,0)  / s;
				double z = c_tmp.at<double>(2,0)  / s;

				int image_y_temp  = ( k[1][0] * x + k[1][1] * y + k[1][2] * z )/ z;  //scaling factor lidar[i][0]  is constant
				
				double tmp2[4][1]= {     { lidar[i][1] },
							{        -0.2     },
							{ lidar[i][0] },
							{         1     }  };


				pose = Mat(4, 1, CV_64FC1, &tmp2);

				c_tmp = tsf * pose;

				 s = c_tmp.at<double>(3,0);
				 x = c_tmp.at<double>(0,0)  / s;
				 y = c_tmp.at<double>(1,0)  / s;
				 z = c_tmp.at<double>(2,0)  / s;

				int image_y_temp2  = ( k[1][0] * x + k[1][1] * y + k[1][2] * z )/ z;  //scaling factor lidar[i][0]  is constant





				for(int j = image_y_temp; j < image_y_temp2; j ++)
				{
					for(int k = ix  - 22; k < ix - 7; k++) //width of the angle
						if( k >= 2 && k < black.cols - 2 && j >= 2 && j < black.rows - 2)
							blackOut(black, j, k);
				}
			}

			i = i + 1; //switching back original angle
		}
	}
	imshow("With objects",black);
}

void blackOut(Mat black, float iy, float ix) //prints 3x3 boxes
{
	blackOut_big(black, iy, ix);
	blackOut_big(black, iy, ix-1);
	blackOut_big(black, iy, ix+1);
	blackOut_big(black, iy - 1, ix);
	blackOut_big(black, iy + 1, ix);
	blackOut_big(black, iy - 1, ix-1);
	blackOut_big(black, iy + 1, ix+1);
	blackOut_big(black, iy - 1, ix+1);
	blackOut_big(black, iy + 1, ix-1);
}

void blackOut_big(Mat image, float y, float x) //prints individual pixels
{
	image.at<Vec3b>( y, x )[0]  = 0; //color
	image.at<Vec3b>( y, x)[1]  = 0;
	image.at<Vec3b>( y, x)[2]  = 0;
}

void laserdata_recieved(const sensor_msgs::LaserScan &LaserScan) //and processed
{
	angle_min = LaserScan.angle_min;
	angle_max = LaserScan.angle_max;
	angle_increment = LaserScan.angle_increment;

	// cout << "nglemin " << angle_min << "angle max" << angle_max << "angle incre" << angle_increment << endl << endl;


	Mat pose; 
	// double tmp[4][1];
	Mat c_tmp;
	double x, y, z, s;

	for(int i = 0; i < 360; i++)
	{
		ranges[i] = LaserScan.ranges[i];

		//to cartesian coordinates
		//z
		lidar[i][0] = ranges[i] * cos(  (i+180)* angle_increment + angle_min);  // 2 is there to rotate frames (camera and lidar not at the same angle)
		//x
		lidar[i][1] = - ranges[i] * sin( (i+ 180)* angle_increment + angle_min ); //x and y coordinates in m



		double tmp[4][1]= {     { lidar[i][1] },
					{         0     },
					{ lidar[i][0] },
					{         1     }  };


		pose = Mat(4, 1, CV_64FC1, &tmp);

		c_tmp = tsf * pose;

		s = c_tmp.at<double>(3,0);
		x = c_tmp.at<double>(0,0)  / s;
		y = c_tmp.at<double>(1,0)  / s;
		z = c_tmp.at<double>(2,0)  / s;

		// local frame to image coordinates
		image_x[i] = ( k[0][0] * x + k[0][1] * y + k[0][2] * z ) / z;
		image_y[i]  = ( k[1][0] * x + k[1][1] * y + k[1][2] * z )/ z;  //scaling factor lidar[i][0]  is a constant
	}

	obstacleEliminate(frame);
}

void imageCallback(const sensor_msgs::ImageConstPtr& msg)
{
	  try 
	  {
	  	frame  = cv_bridge::toCvShare(msg, "bgr8")->image;
	    	cv::waitKey(30);
	  }
	  catch (cv_bridge::Exception& e)
	  {
	    	ROS_ERROR("Could not convert from '%s' to 'bgr8'.", msg->encoding.c_str());
	  }
}

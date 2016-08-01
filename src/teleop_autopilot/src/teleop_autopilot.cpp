/*********************************************************************
* Software License Agreement (BSD License)
* 
*  Copyright (c) 2008, Willow Garage, Inc.
*  All rights reserved.
* 
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
* 
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Willow Garage nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
* 
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

/**
 * Connect the image saving node to cmd_vel control node so control is written away into name of the image file: [-x +x -y +y -z +z cw ccw stop].
    $ rosrun autopilot save_labelled_images generated_set=”mydata”
    Saves images in /home/dell/data/mydata while listening to control of teleop node:
    $ rosrun teleop_twist_keyboard teleop_twist_keyboard.py
    
    Scaling depth image from range 0.4m till 3m into jpg of values between 0..255 (alpha = 80)
*/

#include <opencv2/highgui/highgui.hpp>

#include <ros/ros.h>
#include <cv_bridge/cv_bridge.h>
#include <image_transport/image_transport.h>
#include <camera_calibration_parsers/parse.h>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <string>
#include <geometry_msgs/Twist.h>

#include <std_srvs/Empty.h>
#include <algorithm>

using namespace std;

int STEER_DIR = 0;
const int DEPTH_SCALE_FACTOR = 40;

boost::format g_format;
bool save_all_image, save_image_service;
std::string encoding;
bool service(std_srvs::Empty::Request &req, std_srvs::Empty::Response &res) {
  save_image_service = true;
  return true;
}


/** Class to deal with which callback to call whether we have CameraInfo or not
 */
class Callbacks {
public://initialize fields of callbacks
  string path;
  //string path="/home/dell/remote_images";
  string path_depth;
  Callbacks() : is_first_image_(true), is_first_image_depth_(true), has_camera_info_(false), count_(0){}
  
  void depthToCV8UC1(const cv::Mat& float_img, cv::Mat& mono8_img){
    //Process images
    if(mono8_img.rows != float_img.rows || mono8_img.cols != float_img.cols){
      mono8_img = cv::Mat(float_img.size(), CV_8UC1);}
    //cv::convertScaleAbs(float_img, mono8_img, 80, 0.0);
    cv::convertScaleAbs(float_img, mono8_img, DEPTH_SCALE_FACTOR, 0.0);
    //The following doesn't work due to NaNs
    /*double min, max;
    cv::minMaxLoc(your_mat, &min, &max);	
    std::cout << min << "; " << max << */
    //double minVal, maxVal; 
    //minMaxLoc(float_img, &minVal, &maxVal);
    //ROS_DEBUG("Minimum/Maximum Depth in current image: %f/%f", minVal, maxVal);
    //mono8_img = cv::Scalar(0);
    //cv::line( mono8_img, cv::Point2i(10,10),cv::Point2i(200,100), cv::Scalar(255), 3, 8);
  }
  
  void callbackWithoutCameraInfo(const sensor_msgs::ImageConstPtr& image_msg)
  {
    if (is_first_image_) {
      is_first_image_ = false;

      // Wait a tiny bit to see whether callbackWithCameraInfo is called
      ros::Duration(0.001).sleep();
    }

    if (has_camera_info_)
      return;

    // save the image
    cv::Mat image;
    try
    {
      image = cv_bridge::toCvShare(image_msg, encoding)->image;
    } catch(cv_bridge::Exception)
    {
      ROS_ERROR("Unable to convert %s image to bgr8", image_msg->encoding.c_str());
      return;
    }
    std::string filename;
    /*if (!saveImage(image, filename))
      return;
*/
    count_++;
  }

  void getDirectionSteer(cv::Mat depth_mono8_img)
  {
    int DEPTH_MAX = 255;
    int depth_scaled = 0;
    int image_height = depth_mono8_img.rows;
    int image_width  = depth_mono8_img.cols;
    int num_depth_bins = 10;
    int stride_length    = floor(image_width/num_depth_bins);
    cv::Mat depth_vals(num_depth_bins,1,CV_32F);
    int image_height_depth_val = floor(image_height/2);
    int image_col_val = floor(stride_length/2);
    for(int i = 0; i < num_depth_bins; ++i)
    {
      float depth_this = 0;
      if( image_col_val <= image_width)
      {
	depth_this = (int)(depth_mono8_img.at<char>(image_height_depth_val, image_col_val));  //(image_height_depth_val, image_col_val);
      }
      
      if (depth_this <=-1) // EXCEEDS MAX RANGE
      {
	depth_scaled = DEPTH_MAX;
      }
      else
      {
	depth_scaled = depth_this;
      }
      
      depth_vals.at<float>(i) = depth_scaled;
      cout << depth_this << " "  << depth_scaled << endl;
      
      image_col_val += stride_length;
    }
    int depths_left = 0, depths_right = 0, depths_centre = 0;
    
    for(int i = 0; i < 4; ++i)
    {
      depths_left += depth_vals.at<float>(i);
    }
    depths_left /= 4;
    
    for(int i = 4; i < 6; ++i)
    {
      depths_centre += depth_vals.at<float>(i);
    }
    depths_centre /= 2;
    
    for(int i = 6; i < 10; ++i)
    {
      depths_right += depth_vals.at<float>(i);
    }
    depths_right /= 4;
    
    cout << "left: " << depths_left << endl;
    cout << "centre: " << depths_centre << endl;
    cout << "right: " << depths_right << endl;
    
    if (depths_centre == 0 && depths_left == 0 && depths_right == 0)
    {
      STEER_DIR = 0; //TURN LEFT
    }
    else
    {
      if (depths_centre >= depths_left && depths_centre >= depths_right)
      {
	STEER_DIR = 1; //GO STRAIGHT
	//cout << "GO STRAIGHT" << endl;
      }
      else if (depths_left >= depths_centre && depths_left >= depths_right)
      {
	STEER_DIR = 0; //TURN LEFT
	//cout << "TURN LEFT" << endl;
      }
      else
      {
	STEER_DIR = 2; //TURN RIGHT
	//cout << "TURN RIGHT" << endl;
      }
    }
      
    
    
    
    
  }
  void callbackWithoutCameraInfoWithDepth(const sensor_msgs::ImageConstPtr& original_image)
  {
    if (is_first_image_) {
      is_first_image_depth_ = false;
      // Wait a tiny bit to see whether callbackWithCameraInfo is called
      ros::Duration(0.001).sleep();
    }

    if (has_camera_info_)
      return;

    cv_bridge::CvImagePtr cv_ptr;
    //Convert from the ROS image message to a CvImage suitable for working with OpenCV for processing
    try
    {
        //Always copy, returning a mutable CvImage
        //OpenCV expects color images to use BGR channel order.
        cv_ptr = cv_bridge::toCvCopy(original_image);
    }
    catch (cv_bridge::Exception& e)
    {
        //if there is an error during conversion, display it
        ROS_ERROR("save_labelled_images_depth::main.cpp::cv_bridge exception: %s", e.what());
        return;
    }

    //Copy the image.data to imageBuf. Depth image is uint16 with depths in mm.
    cv::Mat depth_float_img = cv_ptr->image;
    cv::Mat depth_mono8_img;
    depthToCV8UC1(depth_float_img, depth_mono8_img);
    //cout << "Here" << endl;
    cv::imshow("test_window", depth_mono8_img);
    cv::waitKey(25);
    
    getDirectionSteer(depth_mono8_img);
    
    
//     double min, max;
//     cv::minMaxLoc(depth_float_img, &min, &max);	
//     std::cout << "minmax float: " << min << "; " << max << "\n";
//     cv::minMaxLoc(depth_mono8_img, &min, &max);	
//     std::cout << "minmax mono: " << min << "; " << max << "\n";
//     
    // save the image
    std::string filename;
    if (!saveImage(depth_mono8_img, filename, true))
      return;
    
    count_++;
  }
  
  void callbackWithCameraInfo(const sensor_msgs::ImageConstPtr& image_msg, const sensor_msgs::CameraInfoConstPtr& info)
  {
    has_camera_info_ = true;

    // save the image
    cv::Mat image;
    try
    {
      image = cv_bridge::toCvShare(image_msg, encoding)->image;
    } catch(cv_bridge::Exception)
    {
      ROS_ERROR("Unable to convert %s image to bgr8", image_msg->encoding.c_str());
      return;
    }
    std::string filename;
    if (!saveImage(image, filename))
      return;

    // save the CameraInfo
    if (info) {
      filename = filename.replace(filename.rfind("."), filename.length(), ".ini");
      camera_calibration_parsers::writeCalibration(filename, "camera", *info);
    }

    count_++;
  }
  
  void callbackCmd(const geometry_msgs::Twist& msg)
  {
    geometry_msgs::Vector3 lin = msg.linear;
    geometry_msgs::Vector3 ang = msg.angular;
    //std::cout << "twist message: "<< msg << std::endl;
    //std::cout << "twist message ang z: "<< ang.z << std::endl;
    
    std::string cmd;
    //check stop signal if quadrotor has to hover
    if( !(lin.x || lin.y || lin.z || ang.z)){
      cmd = "100000000";//hover
    }else{
      if(lin.y < 0){
	cmd = "000100000";//-y aka right
      }else if(lin.y > 0){
	cmd = "000010000";//+y aka left
      }else{
	if(lin.x < 0){
	  cmd = "010000000";//-x aka back
	}else if(lin.x > 0){
	  cmd = "001000000";//+x aka forward
	}else{
	  if(lin.z < 0){
	    cmd =  "000001000";//-z aka down
	  }else if(lin.z > 0){
	    cmd =  "000000100";//+z aka up
	  }else{
	    if(ang.z < 0){
	      cmd =  "000000010";//cw
	    }else if(ang.z > 0){
	      cmd =  "000000001";//ccw
	    }else{
	      cmd = "x";
	    }
	  }
	}
      }
    }
         
    control = cmd;
    std::cout << "control message: "<< cmd << std::endl;
  }
  
private: //private methods of callback
  bool saveImage(cv::Mat image, std::string &filename, bool depth = false) {
    
    //cv::imshow("test_window", image);
    //cv::waitKey(0.05);
    if (!image.empty()) {
      
      try {
        filename = (g_format).str();
      } catch (...) { g_format.clear(); }
      try {
        filename = (g_format % path).str();
      } catch (...) { g_format.clear(); }
      try {
        filename = (g_format % path % count_).str();
      } catch (...) { g_format.clear(); }
      try { 
        filename = (g_format % path % count_ % "jpg").str();
      } catch (...) { g_format.clear(); }
      try {
	if(!depth){
	  //RELEVANT for rgb
	  filename = (g_format % path % count_ % control % "jpg").str();
	}else{
	  //RELEVANT FOR DEPTH
	  filename = (g_format % path_depth % count_ % control % "jpg").str();
	}
      } catch (...) { g_format.clear(); }
      
      if ( save_all_image || save_image_service ) {
        try{
	  cv::imwrite(filename, image);
	  ROS_INFO("Saved image %s", filename.c_str());

	  save_image_service = false;
	}catch(runtime_error& ex){
	  fprintf(stderr, "Exception converting image to PNG format: %s\n", ex.what());
	  return false;
	}
      } else {
        return false;
      }
    } else {
      ROS_WARN("Couldn't save image, no data!");
      return false;
    }
    return true;
  }

private: //private fields of callback
  bool is_first_image_;
  bool is_first_image_depth_;
  bool has_camera_info_;
  size_t count_;
  string control;
};

int main(int argc, char** argv)
{
  ros::init(argc, argv, "teleop_autopilot", ros::init_options::AnonymousName);
  ros::NodeHandle nh;
  image_transport::ImageTransport it(nh);
  //std::string topic = nh.resolveName("image");
  std::string topic = "/ardrone/image_raw";
  std::string topic_depth = "/ardrone/kinect/depth/image_raw";

  Callbacks callbacks;
  
  // Useful when CameraInfo is not being published
  image_transport::Subscriber sub_image = it.subscribe(
      topic, 1, boost::bind(&Callbacks::callbackWithoutCameraInfo, &callbacks, _1));
  //depth
  image_transport::Subscriber sub_image_depth = it.subscribe(
      topic_depth, 1, boost::bind(&Callbacks::callbackWithoutCameraInfoWithDepth, &callbacks, _1));

  // Make subscriber to cmd_vel in order to set the name.
  ros::Publisher pubControl = nh.advertise<geometry_msgs::Twist>("/cmd_vel", 1000);
  ros::Rate loop_rate(10);

  geometry_msgs::Twist twist;
  
  twist.linear.x = 0.5;//straight
  twist.linear.y = 0.0;//left
  twist.linear.z = 0.0;//right
  
  twist.angular.x = 0.05;
  twist.angular.y = 0.0;
  twist.angular.z = 0.0;
  //while(nh.ok()){
  while(ros::ok()){
    
    if (STEER_DIR == 1)
    {
      twist.linear.x = 0.2;//straight
      twist.linear.y = 0.0;
      twist.linear.z = 0.0;
      
      twist.angular.x = 0.0;
      twist.angular.y = 0.0;
      twist.angular.z = 0.0;
      
    }
    else if (STEER_DIR == 0)
    {
      twist.linear.x = 0.5;
      twist.linear.y = 0.0;
      twist.linear.z = 0.0;
      
      twist.angular.x = 0.0;
      twist.angular.y = 0.0;
      twist.angular.z = +1.0;//turn left
    }
    else
    {
      twist.linear.x = 0.5;
      twist.linear.y = 0.0;
      twist.linear.z = 0.0;
      
      twist.angular.x = 0.0;
      twist.angular.y = 0.0;
      twist.angular.z = -1.0;//turn right
    }
    cout << "lin x: " << twist.linear.x << endl;
    cout << "lin y: " << twist.linear.y << endl;
    cout << "lin z: " << twist.linear.z << endl;
    
    cout << "theta x: " << twist.angular.x << endl;
    cout << "theta y: " << twist.angular.y << endl;
    cout << "theta z: " << twist.angular.z << endl;
    
    cout << "STEER DIR" << STEER_DIR << endl;
    pubControl.publish(twist);
  
  // [hover, back, forward, turn right, turn left, down, up, clockwise, ccw]
  // Adapt name instead of left0000.jpg it should be 00000-gt1.jpg when receiving control 1 ~ straight
  ros::NodeHandle local_nh("~");
  std::string format_string;
  local_nh.param("filename_format", format_string, std::string("%s/%05i-gt%s.%s"));
  local_nh.param("encoding", encoding, std::string("bgr8"));
  local_nh.param("save_all_image", save_all_image, true);
  g_format.parse(format_string);
  ros::ServiceServer save = local_nh.advertiseService ("save", service);
  loop_rate.sleep();
  ros::spinOnce();
  }
} 

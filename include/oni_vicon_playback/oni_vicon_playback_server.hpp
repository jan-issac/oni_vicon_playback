/*
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2014 Max-Planck-Institute for Intelligent Systems,
 *                     University of Southern California,
 *                     Karlsruhe Institute of Technology
 *    Jan Issac (jan.issac@gmail.com)
 *
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
 *   * Neither the name of Willow Garage, Inc. nor the names of its
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
 *
 */

/**
 * @date 05/14/2014
 * @author Jan Issac (jan.issac@gmail.com)
 * Max-Planck-Institute for Intelligent Systems, University of Southern California (USC),
 *   Karlsruhe Institute of Technology (KIT)
 */

#ifndef oni_vicon_playback_ONI_VICON_PLAYBACK_SERVER_HPP
#define oni_vicon_playback_ONI_VICON_PLAYBACK_SERVER_HPP

// boost
#include <boost/thread/mutex.hpp>

// ros
#include <actionlib/server/simple_action_server.h>
#include <visualization_msgs/Marker.h>
#include <tf/transform_broadcaster.h>

// actions
#include <oni_vicon_playback/OpenAction.h>
#include <oni_vicon_playback/PlayAction.h>

// services
#include <oni_vicon_playback/Pause.h>
#include <oni_vicon_playback/SeekFrame.h>
#include <oni_vicon_playback/SetPlaybackSpeed.h>
#include <oni_vicon_playback/SetTimeOffset.h>

#include <oni_vicon_common/type_conversion.hpp>
#include <oni_vicon_common/exceptions.hpp>

#include "oni_vicon_playback/oni_vicon_player.hpp"

namespace oni_vicon_playback
{
    class OniViconPlaybackServer
    {
    public:
        OniViconPlaybackServer(ros::NodeHandle& node_handle,
                       OniViconPlayer::Ptr playback,
                       const std::string &depth_frame_id,
                       const std::string &camera_info_topic,
                       const std::string &point_cloud_topic);
        void run();

    public: /* action callbacks */
        void playCb(const PlayGoalConstPtr& goal);
        void openCb(const OpenGoalConstPtr& goal);

    public: /* service callbacks */
        bool pauseCb(Pause::Request& request,
                     Pause::Response& response);
        bool seekFrameCb(SeekFrame::Request& request,
                         SeekFrame::Response& response);
        bool setPlaybackSpeedCb(SetPlaybackSpeed::Request& request,
                                SetPlaybackSpeed::Response& response);

        bool setTimeOffset(SetTimeOffset::Request& request,
                           SetTimeOffset::Response& response);
    public:

        void loadUpdateCb(uint32_t total_frames, uint32_t frames_loaded);

        void publish(sensor_msgs::ImagePtr image);

        void publish(const tf::Pose& pose,
                     sensor_msgs::ImagePtr corresponding_image,
                     const std::string& object_display,
                     const std::string &suffix = "",
                     double r = 1., double g = 0., double b = 0.);

        sensor_msgs::ImagePtr depthFrameAsMsgImage();
        sensor_msgs::PointCloud2Ptr depthFrameAsMsgPointCloud();


        void toMsgImage(const xn::DepthMetaData& depth_meta_data,
                        sensor_msgs::ImagePtr image,
                        oni_vicon::Unit unit = oni_vicon::Meter);

        float toMeter(const XnDepthPixel& depth_pixel) const;
        float toMillimeter(const XnDepthPixel& depth_pixel) const;

    private:
        boost::mutex player_lock_;

        oni_vicon_playback::OniViconPlayer::Ptr playback_;

        image_transport::ImageTransport image_transport_;
        image_transport::Publisher pub_depth_image_;
        ros::Publisher pub_point_cloud_;
        ros::Publisher pub_depth_info_;
        ros::Publisher vicon_object_pose_publisher_;
        tf::TransformBroadcaster tf_broadcaster_;

        /* published data */
        std::string depth_frame_id_;
        std::string camera_info_topic_;
        std::string point_cloud_topic_;
        sensor_msgs::CameraInfo depth_cam_info_;

        actionlib::SimpleActionServer<OpenAction> open_as_;
        actionlib::SimpleActionServer<PlayAction> play_as_;

        OpenFeedback feedback_;

        ros::ServiceServer pause_srv_;
        ros::ServiceServer seek_frame_srv_;
        ros::ServiceServer set_playback_speed_srv_;
        ros::ServiceServer set_time_offset_srv_;

        bool paused_;
        XnUInt32 seeking_frame_;

        sensor_msgs::PointCloud2Ptr msg_pointcloud_;
        sensor_msgs::ImagePtr msg_image_;

        bool msg_image_dirty_;
        bool msg_pointcloud_dirty_;
    };
}

#endif

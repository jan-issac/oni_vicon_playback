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
 * @date 05/04/2014
 * @author Jan Issac (jan.issac@gmail.com)
 * Max-Planck-Institute for Intelligent Systems, University of Southern California (USC),
 *   Karlsruhe Institute of Technology (KIT)
 */

#include "oni_vicon_playback/vicon_player.hpp"

#include <iostream>
#include <fstream>

using namespace oni_vicon;
using namespace oni_vicon_playback;

ViconPlayer::ViconPlayer():
    start_frame_offset_(0),
    start_time_offset_(0),
    vicon_camera_offset_(0)
{

}

ViconPlayer::~ViconPlayer()
{

}

bool ViconPlayer::load(const std::string& source_file,
                       const Transformer &calibration_transform,
                       LoadUpdateCallback update_cb)
{    
    std::ifstream data_file(source_file.c_str());

    if (!data_file.is_open())
    {
        return false;
    }

    raw_data_.clear();
    data_.clear();

    RawRecord previous_record;
    RawRecord record;
    RawRecord selected_record;

    data_file >> vicon_camera_offset_;
    vicon_camera_offset_ = -0.04; // TODO FIX THIS

    while (data_file.peek() != EOF)
    {
        previous_record = record;

        data_file >> record.vicon_time;
        data_file >> record.vicon_frame;
        data_file >> record.depth_sensor_time;
        data_file >> record.depth_sensor_frame;
        data_file >> record.vicon_frame_id;
        data_file >> record.translation_x;
        data_file >> record.translation_y;
        data_file >> record.translation_z;
        data_file >> record.orientation_w;
        data_file >> record.orientation_x;
        data_file >> record.orientation_y;
        data_file >> record.orientation_z;

        raw_data_.push_back(record);

        if (record.depth_sensor_frame == 0)
        {
            previous_record = record;
            continue;
        }

        if (previous_record.depth_sensor_frame == 0
            && record.depth_sensor_frame != 0)
        {
            start_frame_offset_ = record.depth_sensor_frame - 1;
            start_time_offset_ = record.vicon_time;
        }

        raw_data_.back().vicon_file_time = raw_data_.back().vicon_time;
        raw_data_.back().vicon_time -= start_time_offset_;

        if (record.depth_sensor_frame == previous_record.depth_sensor_frame)
        {
            continue;
        }

        record.depth_sensor_frame -= start_frame_offset_;

        PoseRecord pose_record;
        tf::Vector3 translation;
        tf::Quaternion orientation;

        selected_record = closestViconFrame(record);

        translation.setX(selected_record.translation_x);
        translation.setY(selected_record.translation_y);
        translation.setZ(selected_record.translation_z);

        orientation.setW(selected_record.orientation_w);
        orientation.setX(selected_record.orientation_x);
        orientation.setY(selected_record.orientation_y);
        orientation.setZ(selected_record.orientation_z);

        pose_record.stamp.fromSec(record.depth_sensor_time);
        pose_record.pose.setOrigin(translation);
        pose_record.pose.setRotation(orientation);

        // transform into depth sensor frame
        pose_record.pose = calibration_transform.viconPoseToCameraPose(pose_record.pose);

        pose_record.vicon_frame = record.vicon_frame;
        data_[record.depth_sensor_frame] = pose_record;

        if (!update_cb.empty())
        {
            update_cb(0, data_.size());
        }

//        if (countDepthSensorFrames() != record.depth_sensor_frame)
//        {
//            ROS_WARN("%ud != %ud", countDepthSensorFrames(), record.depth_sensor_frame);
//        }
    }

    data_file.close();

    return true;
}

const ViconPlayer::PoseRecord ViconPlayer::pose(uint32_t frame)
{        
    return data_[frame];
}

uint32_t ViconPlayer::countViconFrames() const
{
    return raw_data_.size();
}

uint32_t ViconPlayer::countDepthSensorFrames() const
{
    return data_.size();
}

ViconPlayer::RawRecord ViconPlayer::closestViconFrame(const ViconPlayer::RawRecord& oni_frame)
{
    std::vector<RawRecord>::reverse_iterator rit;// = raw_data_.rbegin();

    RawRecord selected_record;

    for (rit = raw_data_.rbegin(); rit != raw_data_.rend(); ++rit)
    {
        if (rit == raw_data_.rbegin())
        {
            selected_record = *rit;
        }
        else if (std::abs(oni_frame.depth_sensor_time - rit->vicon_time) >=
                 std::abs(oni_frame.depth_sensor_time - selected_record.vicon_time))
        {
            break;
        }
        else
        {
            selected_record = *rit;
        }
    }

    return selected_record;
}

ViconPlayer::RawRecord ViconPlayer::closestViconFrame(const ViconPlayer::PoseRecord& oni_frame,
                                                      double timestamp)
{
    std::vector<RawRecord>::reverse_iterator rit;

    RawRecord selected_record;

    int64_t positive_shift = std::max(vicon_camera_offset_ * 100., 0.);
    int64_t r_offset = int64_t(oni_frame.vicon_frame) + positive_shift;

    // reverse the index offset
    r_offset = std::max(int64_t(raw_data_.size()) - r_offset, int64_t(0));

    for (rit = raw_data_.rbegin() + r_offset; rit != raw_data_.rend(); ++rit)
    {
        if (rit == raw_data_.rbegin() + r_offset)
        {
            selected_record = *rit;
        }
        else if (std::fabs(timestamp + vicon_camera_offset_ - rit->vicon_time) >=
                 std::fabs(timestamp + vicon_camera_offset_ - selected_record.vicon_time))
        {
            break;
        }
        else
        {
            selected_record = *rit;
        }
    }

    return selected_record;
}

void ViconPlayer::viconCameraTimeOffset(double vicon_camera_offset)
{
    vicon_camera_offset_ = vicon_camera_offset;
}

double ViconPlayer::viconCameraTimeOffset() const
{
    return vicon_camera_offset_;
}

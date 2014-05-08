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
 * @date 04/26/2014
 * @author Jan Issac (jan.issac@gmail.com)
 * Max-Planck-Institute for Intelligent Systems, University of Southern California (USC),
 *   Karlsruhe Institute of Technology (KIT)
 */

#include "depth_sensor_vicon_calibration/calibration.hpp"

#include <boost/bind.hpp>
#include <algorithm>
#include <iterator>
#include <iostream>
#include <fstream>

#include <oni_vicon_recorder/ViconObjectPose.h>

using namespace depth_sensor_vicon_calibration;
using namespace visualization_msgs;
using namespace simple_object_tracker;
using namespace oni_vicon_recorder;

Calibration::Calibration(ros::NodeHandle& node_handle,
                         int global_calibration_iterations,
                         int local_calibration_iterations,
                         std::string global_calibration_object_name,
                         std::string global_calibration_object,
                         std::string global_calibration_object_display):
    node_handle_(node_handle),
    global_calibration_iterations_(global_calibration_iterations),
    local_calibration_iterations_(local_calibration_iterations),
    global_calibration_object_name_(global_calibration_object_name),
    global_calibration_object_(global_calibration_object),
    global_calibration_object_display_(global_calibration_object_display),
    global_pose_set_(false),
    local_pose_set_(false),
    test_pose_set_(false),
    global_calibration_as_(node_handle,
                           GlobalCalibrationGoal::ACTION_NAME,
                           boost::bind(&Calibration::globalCalibrationCB, this, _1),
                           false),
    /*
    continue_global_calibration_as_(node_handle,
                                    ContinueGlobalCalibrationGoal::ACTION_NAME,
                                    boost::bind(&Calibration::continueGlobalCalibrationCB, this,_1),
                                    false),
    complete_global_calibration_as_(node_handle,
                                    CompleteGlobalCalibrationGoal::ACTION_NAME,
                                    boost::bind(&Calibration::completeGlobalCalibrationCB, this,_1),
                                    false),
    */
    local_calibration_as_(node_handle,
                         LocalCalibrationGoal::ACTION_NAME,
                         boost::bind(&Calibration::localCalibrationCB, this, _1),
                         false),
    /*
    continue_local_calibration_as_(node_handle,
                                  ContinueLocalCalibrationGoal::ACTION_NAME,
                                  boost::bind(&Calibration::continueLocalCalibrationCB, this, _1),
                                  false),
    complete_local_calibration_as_(node_handle,
                                  CompleteLocalCalibrationGoal::ACTION_NAME,
                                  boost::bind(&Calibration::completeLocalCalibrationCB, this, _1),
                                  false),
    */
    test_calibration_as_(node_handle,
                         TestCalibrationGoal::ACTION_NAME,
                         boost::bind(&Calibration::testCalibrationCB, this, _1),
                         false),
    global_calibration_complete_(false),
    local_calibration_complete_(false)
{
    // advertise calibration markers
    object_publisher_ =
            node_handle_.advertise<visualization_msgs::Marker>("calibration_object_pose", 0);

    // start action servers
    global_calibration_as_.start();
    //continue_global_calibration_as_.start();
    //complete_global_calibration_as_.start();

    local_calibration_as_.start();
    //continue_local_calibration_as_.start();
    //complete_local_calibration_as_.start();

    // advertise services
    save_global_calib_srv_ = node_handle.advertiseService(
                SaveGlobalCalibration::Request::SERVICE_NAME,
                &Calibration::saveGlobalCalibrationCB,
                this);

    load_global_calib_srv_ = node_handle.advertiseService(
                LoadGlobalCalibration::Request::SERVICE_NAME,
                &Calibration::loadGlobalCalibrationCB,
                this);

    save_local_calib_srv_ = node_handle.advertiseService(
                SaveLocalCalibration::Request::SERVICE_NAME,
                &Calibration::saveLocalCalibrationCB,
                this);

    load_local_calib_srv_ = node_handle.advertiseService(
                LoadLocalCalibration::Request::SERVICE_NAME,
                &Calibration::loadLocalCalibrationCB,
                this);

    continue_global_calibration_srv_= node_handle.advertiseService(
                ContinueGlobalCalibration::Request::SERVICE_NAME,
                &Calibration::continueGlobalCalibrationCB,
                this);

    complete_global_calibration_srv_= node_handle.advertiseService(
                CompleteGlobalCalibration::Request::SERVICE_NAME,
                &Calibration::completeGlobalCalibrationCB,
                this);

    continue_local_calibration_srv_= node_handle.advertiseService(
                ContinueLocalCalibration::Request::SERVICE_NAME,
                &Calibration::continueLocalCalibrationCB,
                this);

    complete_local_calibration_srv_= node_handle.advertiseService(
                CompleteLocalCalibration::Request::SERVICE_NAME,
                &Calibration::completeLocalCalibrationCB,
                this);

    continue_test_calibration_srv_= node_handle.advertiseService(
                ContinueTestCalibration::Request::SERVICE_NAME,
                &Calibration::continueTestCalibrationCB,
                this);

    global_calibration_transform_.setIdentity();
    local_calibration_transform_.setIdentity();
}

Calibration::~Calibration()
{
}

// ============================================================================================== //
// == Global Calibration ======================================================================== //
// ============================================================================================== //

void Calibration::globalCalibrationCB(const GlobalCalibrationGoalConstPtr& goal)
{
    boost::unique_lock<boost::mutex> lock(global_calib_mutex_);

    GlobalCalibrationFeedback feedback;

    ROS_INFO("Global calibration started.");

    // ========================================================================================== //
    // == Display interactive marker to set initial pose and wait to continue =================== //
    // ========================================================================================== //
    interactive_markers::InteractiveMarkerServer server("calibration_object_marker");
    InteractiveMarker global_calib_maker = makeObjectMarker(global_calibration_object_display_,
                                                            "global_calibration_marker");
    if (global_pose_set_)
    {
        global_calib_maker.pose = current_global_marker_pose_;
    }
    else
    {
        loadPoseFromCache("global_pose", global_calib_maker.pose);
        current_global_marker_pose_ = global_calib_maker.pose;
        global_pose_set_ = true;
    }
    server.insert(global_calib_maker,
                  boost::bind(&Calibration::processGlobalCalibrationFeedback, this, _1));
    server.applyChanges();

    GlobalCalibrationResult result;    

    int iterations = global_calibration_iterations_;

    feedback.finished = false;
    feedback.max_progress = iterations + 4;
    feedback.progress = 0;
    publishGlobalStatus("Waiting for calibration object alignment", feedback);

    // wait to continue
    while (!global_calib_cond_.timed_wait(lock, boost::posix_time::milliseconds(50))
           && !global_calibration_as_.isPreemptRequested())
    {
    }

    // ========================================================================================== //
    // == Coninued. Check for abort and continue otherwise. Start pose estimation of           == //
    // == calibration object.                                                                  == //
    // ========================================================================================== //
    if (global_calibration_as_.isPreemptRequested())
    {
        publishGlobalStatus("Global calibration Aborted.", feedback);
        global_calibration_as_.setAborted();
        return;
    }

    // remove marker
    server.clear();
    server.applyChanges();

    // cache set pose for the next calibration approach. small feature to make things easier
    cachePose(current_global_marker_pose_, "global_pose");

    ROS_INFO("Calibrating ...");

    publishGlobalStatus("Creating object state estimator ...", feedback);
    SpkfObjectTracker object_tracker(node_handle_, "/oni_vicon_recorder");
    feedback.progress = 1;

    publishGlobalStatus("Setup estimator parameters ...", feedback);
    object_tracker.setupParameters();
    feedback.progress = 2;

    publishGlobalStatus("Setup filter ...", feedback);
    object_tracker.setupFilter(current_global_marker_pose_,
                               global_calibration_object_,
                               global_calibration_object_display_);
    feedback.progress = 3;

    publishGlobalStatus("Running filter ...", feedback);
    object_tracker.run();
    feedback.progress = 4;

    // track for a shot period
    while (object_tracker.iteration() <= iterations)
    {
        if (global_calibration_as_.isPreemptRequested() || !ros::ok())
        {
            publishGlobalStatus("Global calibration Aborted.", feedback);
            global_calibration_as_.setAborted();
            object_tracker.shutdown();
            return;
        }

        if (feedback.progress != object_tracker.iteration())
        {
            feedback.progress = object_tracker.iteration() + 4;
            publishGlobalStatus("Tracking ... ", feedback);
        }

        ros::spinOnce();
    }

    feedback.progress = feedback.max_progress;
    publishGlobalStatus("Ready ", feedback);

    // ========================================================================================== //
    // == Ready to complete. Wait for complete and display result in the meanwhile. ============= //
    // ========================================================================================== //
    ViconObjectPose vicon_object_pose;
    vicon_object_pose.request.object_name = global_calibration_object_name_;
    if (ros::service::call(ViconObjectPose::Request::SERVICE_NAME, vicon_object_pose))
    {
        // calculate global transformation matrix
        calibraion_transform_.calibrateGlobally(
                    Transform::toTfPose(vicon_object_pose.response.object_pose),
                    Transform::toTfPose(object_tracker.getCurrentPose()));

        feedback.finished = true;
        publishGlobalStatus("Global calibration ready.", feedback);

        // wait to complete or abort
        global_calibration_complete_ = false;
        while (!global_calib_cond_.timed_wait(lock, boost::posix_time::milliseconds(1000./30.))
               && !global_calibration_as_.isPreemptRequested())
        {
            // while waiting to complete publish calibrated pose
            if (ros::service::call(ViconObjectPose::Request::SERVICE_NAME, vicon_object_pose))
            {
                // vicon pose to depth sensor pose and publish marker
                tf::Pose vicon_obj_pose =
                        calibraion_transform_.viconToDepthSensor(
                            Transform::toTfPose(vicon_object_pose.response.object_pose));

                publishMarker(Transform::toMsgPose(vicon_obj_pose),
                              global_calibration_object_display_,
                              object_publisher_,
                              0, 1, 0);

                // publish frames
                tf_broadcaster_.sendTransform(
                            tf::StampedTransform(
                                Transform::toTfTransform(object_tracker.getCurrentPose()),
                                ros::Time::now(),
                                "XTION_RGB",
                                "ds_object_frame"));

                tf_broadcaster_.sendTransform(
                            tf::StampedTransform(
                                vicon_obj_pose,
                                ros::Time::now(),
                                "XTION_RGB",
                                "vicon_object_frame"));

                tf_broadcaster_.sendTransform(
                            tf::StampedTransform(
                                calibraion_transform_.getViconGlobalFrame(),
                                ros::Time::now(),
                                "XTION_RGB",
                                "vicon_wcs"));
            }

            ros::spinOnce();
        }

        if (global_calibration_as_.isPreemptRequested() && !global_calibration_complete_)
        {
            publishGlobalStatus("Global calibration Aborted.", feedback);
            global_calibration_as_.setAborted();
            return;
        }
    }
    else
    {
        publishGlobalStatus("Aborted. Retrieving '"
                      + global_calibration_object_name_
                      + "' Vicon frame failed", feedback);
        global_calibration_as_.setAborted();
        object_tracker.shutdown();
        return;
    }

    object_tracker.shutdown();
    ROS_INFO("Global calibration done.");
    global_calibration_as_.setSucceeded(result);
}

/*
void Calibration::continueGlobalCalibrationCB(const ContinueGlobalCalibrationGoalConstPtr& goal)
{     
    boost::unique_lock<boost::mutex> lock(global_calib_mutex_);

    ROS_INFO("Global calibration continued");

    global_calib_cond_.notify_all();

    ContinueGlobalCalibrationResult result;
    continue_global_calibration_as_.setSucceeded(result);
}


void Calibration::completeGlobalCalibrationCB(const CompleteGlobalCalibrationGoalConstPtr& goal)
{
    boost::unique_lock<boost::mutex> lock(global_calib_mutex_);

    ROS_INFO("Completing global calibration ...");

    global_calibration_complete_ = true;

    global_calib_cond_.notify_all();

    CompleteGlobalCalibrationResult result;
    complete_global_calibration_as_.setSucceeded(result);
}
*/

void Calibration::processGlobalCalibrationFeedback(
        const visualization_msgs::InteractiveMarkerFeedbackConstPtr &feedback)
{
    current_global_marker_pose_ = feedback->pose;
}

// ============================================================================================== //
// == Local Calibration ========================================================================= //
// ============================================================================================== //

void Calibration::localCalibrationCB(const LocalCalibrationGoalConstPtr& goal)
{
    boost::unique_lock<boost::mutex> lock(local_calib_mutex_);
    LocalCalibrationFeedback feedback;

    ROS_INFO("Local calibration started.");

    ROS_INFO("local_calibration_object = %s", goal->calibration_object.c_str());
    ROS_INFO("local_calibration_object_display = %s", goal->calibration_object_display.c_str());

    // ========================================================================================== //
    // == Display interactive marker to set initial pose and wait to continue =================== //
    // ========================================================================================== //
    interactive_markers::InteractiveMarkerServer server("calibration_object_marker");
    InteractiveMarker local_calib_maker = makeObjectMarker(goal->calibration_object_display,
                                                           "local_calibration_marker");
    if (local_pose_set_)
    {
        local_calib_maker.pose = current_local_marker_pose_;
    }
    else
    {
        loadPoseFromCache("local_pose", local_calib_maker.pose);
        current_local_marker_pose_ = local_calib_maker.pose;
        local_pose_set_ = true;
    }
    server.insert(local_calib_maker,
                  boost::bind(&Calibration::processLocalCalibrationFeedback, this, _1));
    server.applyChanges();

    LocalCalibrationResult result;

    int iterations = local_calibration_iterations_;

    feedback.finished = false;
    feedback.max_progress = iterations + 4;
    feedback.progress = 0;
    publishLocalStatus("Waiting for calibration object alignment", feedback);

    // wait to continue
    while (!local_calib_cond_.timed_wait(lock, boost::posix_time::milliseconds(10))
           && !local_calibration_as_.isPreemptRequested())
    {
    }

    // ========================================================================================== //
    // == Coninued. Check for abort and continue otherwise. Start pose estimation of           == //
    // == object.                                                                              == //
    // ========================================================================================== //
    if (local_calibration_as_.isPreemptRequested())
    {
        publishLocalStatus("Local calibration Aborted.", feedback);
        local_calibration_as_.setAborted();
        return;
    }

    server.clear();
    server.applyChanges();

    cachePose(current_local_marker_pose_, "local_pose");

    ROS_INFO("Calibrating ...");

    publishLocalStatus("Creating object state estimator ...", feedback);
    SpkfObjectTracker object_tracker(node_handle_, "/oni_vicon_recorder");
    feedback.progress = 1;

    publishLocalStatus("Setup estimator parameters ...", feedback);
    object_tracker.setupParameters();
    feedback.progress = 2;

    publishLocalStatus("Setup filter ...", feedback);
    object_tracker.setupFilter(current_local_marker_pose_,
                               goal->calibration_object,
                               goal->calibration_object_display);
    feedback.progress = 3;

    publishLocalStatus("Running filter ...", feedback);
    object_tracker.run();
    feedback.progress = 4;

    while (object_tracker.iteration() <= iterations)
    {
        if (local_calibration_as_.isPreemptRequested() || !ros::ok())
        {
            publishLocalStatus("Local calibration Aborted.", feedback);
            local_calibration_as_.setAborted();
            object_tracker.shutdown();
            return;
        }

        if (feedback.progress != object_tracker.iteration())
        {
            feedback.progress = object_tracker.iteration() + 4;
            publishLocalStatus("Tracking ... ", feedback);
        }

        ros::spinOnce();
    }

    // ========================================================================================== //
    // == Ready to complete. Wait for complete and display result in the meanwhile. ============= //
    // ========================================================================================== //
    ViconObjectPose vicon_object_pose;
    vicon_object_pose.request.object_name = goal->calibration_object_name;
    if (ros::service::call(ViconObjectPose::Request::SERVICE_NAME, vicon_object_pose))
    {
        // calculate local transformation
        calibraion_transform_.calibrateLocally(
                    Transform::toTfPose(vicon_object_pose.response.object_pose),
                    Transform::toTfPose(object_tracker.getCurrentPose()));

        feedback.finished = true;
        publishLocalStatus("Local calibration ready.", feedback);

        // wait to complete or abort
        local_calibration_complete_ = false;
        while (!local_calib_cond_.timed_wait(lock, boost::posix_time::milliseconds(1000./30.))
               && !local_calibration_as_.isPreemptRequested())
        {
            // while waiting to complete publish calibrated pose
            if (ros::service::call(ViconObjectPose::Request::SERVICE_NAME, vicon_object_pose))
            {
                // vicon pose to depth sensor pose and publish marker
                tf::Pose vicon_obj_pose =
                        calibraion_transform_.viconToDepthSensor(
                            Transform::toTfPose(vicon_object_pose.response.object_pose));

                publishMarker(Transform::toMsgPose(vicon_obj_pose),
                              goal->calibration_object_display,
                              object_publisher_,
                              0, 1, 0);

                // set coordinates
                tf_broadcaster_.sendTransform(
                            tf::StampedTransform(
                                Transform::toTfTransform(object_tracker.getCurrentPose()),
                                ros::Time::now(),
                                "XTION_RGB",
                                "ds_object_frame"));

                tf_broadcaster_.sendTransform(
                            tf::StampedTransform(
                                vicon_obj_pose,
                                ros::Time::now(),
                                "XTION_RGB",
                                "vicon_ds_object_frame"));

                tf_broadcaster_.sendTransform(
                            tf::StampedTransform(
                                calibraion_transform_.getViconGlobalFrame()
                                    * Transform::toTfPose(vicon_object_pose.response.object_pose),
                                ros::Time::now(),
                                "XTION_RGB",
                                "vicon_object_frame"));

                tf_broadcaster_.sendTransform(
                            tf::StampedTransform(
                                calibraion_transform_.getViconGlobalFrame(),
                                ros::Time::now(),
                                "XTION_RGB",
                                "vicon_wcs"));
            }

            ros::spinOnce();
        }

        if (local_calibration_as_.isPreemptRequested() && !local_calibration_complete_)
        {
            publishLocalStatus("Local calibration Aborted.", feedback);
            local_calibration_as_.setAborted();
            return;
        }
    }
    else
    {
        publishLocalStatus("Aborted. Retrieving '"
                      + goal->calibration_object_name
                      + "' Vicon frame failed", feedback);
        local_calibration_as_.setAborted();
        object_tracker.shutdown();
        return;
    }

    object_tracker.shutdown();
    ROS_INFO("Local calibration done.");
    local_calibration_as_.setSucceeded(result);
}

/*
void Calibration::continueLocalCalibrationCB(const ContinueLocalCalibrationGoalConstPtr& goal)
{
    boost::unique_lock<boost::mutex> lock(local_calib_mutex_);

    ROS_INFO("Local calibration continued");

    local_calib_cond_.notify_all();

    ContinueLocalCalibrationResult result;
    continue_local_calibration_as_.setSucceeded(result);
}

void Calibration::completeLocalCalibrationCB(const CompleteLocalCalibrationGoalConstPtr& goal)
{
    boost::unique_lock<boost::mutex> lock(local_calib_mutex_);

    ROS_INFO("Completing local calibration ...");

    local_calibration_complete_ = true;

    local_calib_cond_.notify_all();

    CompleteLocalCalibrationResult result;
    complete_local_calibration_as_.setSucceeded(result);
}
*/

void Calibration::processLocalCalibrationFeedback(const InteractiveMarkerFeedbackConstPtr& feedback)
{
    current_local_marker_pose_ = feedback->pose;
}

// ============================================================================================== //
// == Testing Calibration ======================================================================= //
// ============================================================================================== //

void Calibration::testCalibration(const std::string& vicon_object_name,
                                  const std::string& object,
                                  const std::string& object_display)
{
    boost::unique_lock<boost::mutex> lock(test_calib_mutex_);

    ROS_INFO("Calibration test started.");

    // ========================================================================================== //
    // == Display interactive marker to set initial pose and wait to continue =================== //
    // ========================================================================================== //
    interactive_markers::InteractiveMarkerServer server("object_marker");
    InteractiveMarker test_object_marker = makeObjectMarker(object_display,
                                                            "object_marker");
    if (test_pose_set_)
    {
        test_object_marker.pose = current_test_marker_pose_;
    }
    else
    {
        loadPoseFromCache("test_pose", test_object_marker.pose);
        current_test_marker_pose_ = test_object_marker.pose;
        test_pose_set_ = true;
    }
    server.insert(test_object_marker,
                  boost::bind(&Calibration::processTestCalibrationFeedback, this, _1));
    server.applyChanges();

    // wait to continue
    while (!test_calib_cond_.timed_wait(lock, boost::posix_time::milliseconds(50))
           && !test_calibration_as_.isPreemptRequested())
    {
    }

    // ========================================================================================== //
    // == Coninued. Check for abort and continue otherwise. Start pose estimation =============== //                                                                == //
    // ========================================================================================== //
    if (test_calibration_as_.isPreemptRequested())
    {
        ROS_INFO("Test calibration Aborted.");
        test_calibration_as_.setAborted();
        return;
    }

    // remove marker
    server.clear();
    server.applyChanges();

    // cache set pose for the next calibration approach. small feature to make things easier
    cachePose(current_test_marker_pose_, "test_pose");

    ROS_INFO("Tracking ...");
    SpkfObjectTracker object_tracker(node_handle_, "/oni_vicon_recorder");
    object_tracker.setupParameters();
    object_tracker.setupFilter(current_test_marker_pose_, object, object_display);
    object_tracker.run();

    ViconObjectPose vicon_object_pose;
    vicon_object_pose.request.object_name = vicon_object_name;
    if (ros::service::call(ViconObjectPose::Request::SERVICE_NAME, vicon_object_pose))
    {
        // track and publish vicon pose and frames until stop testing
        while (!test_calib_cond_.timed_wait(lock, boost::posix_time::milliseconds(1000./30.))
               && !test_calibration_as_.isPreemptRequested())
        {
            if (ros::service::call(ViconObjectPose::Request::SERVICE_NAME, vicon_object_pose))
            {
                // vicon pose to depth sensor pose and publish marker
                tf::Pose vicon_obj_pose =
                        calibraion_transform_.viconToDepthSensor(
                            Transform::toTfPose(vicon_object_pose.response.object_pose));

                publishMarker(Transform::toMsgPose(vicon_obj_pose),
                              object_display,
                              object_publisher_,
                              0, 1, 0);

                // set coordinates
                tf_broadcaster_.sendTransform(
                            tf::StampedTransform(
                                Transform::toTfTransform(object_tracker.getCurrentPose()),
                                ros::Time::now(),
                                "XTION_RGB",
                                "ds_object_frame"));

                tf_broadcaster_.sendTransform(
                            tf::StampedTransform(
                                vicon_obj_pose,
                                ros::Time::now(),
                                "XTION_RGB",
                                "vicon_ds_object_frame"));

                tf_broadcaster_.sendTransform(
                            tf::StampedTransform(
                                calibraion_transform_.getViconGlobalFrame()
                                    * Transform::toTfPose(vicon_object_pose.response.object_pose),
                                ros::Time::now(),
                                "XTION_RGB",
                                "vicon_object_frame"));

                tf_broadcaster_.sendTransform(
                            tf::StampedTransform(
                                calibraion_transform_.getViconGlobalFrame(),
                                ros::Time::now(),
                                "XTION_RGB",
                                "vicon_wcs"));
            }

            ros::spinOnce();
        }
    }
    else
    {
        ROS_WARN("Testing aborted. Requesting vicon pose failed");
        test_calibration_as_.setAborted();
        object_tracker.shutdown();
    }

    ROS_INFO("Calibration test stopped.");
}

void Calibration::testCalibrationCB(const TestCalibrationGoalConstPtr& goal)
{
    switch (goal->test_what)
    {
    case TestCalibrationGoal::GLOBAL_CALIBRATION:
        testCalibration(global_calibration_object_name_,
                        global_calibration_object_,
                        global_calibration_object_display_);
        break;
    case TestCalibrationGoal::LOCAL_CALIBRATION:
        testCalibration(goal->calibration_object_name,
                        goal->calibration_object,
                        goal->calibration_object_display);
        break;
    default:
        ROS_WARN("Testing aborted. Unknown Testing source.");
        break;
    }
}

void Calibration::processTestCalibrationFeedback(const InteractiveMarkerFeedbackConstPtr &feedback)
{
    current_test_marker_pose_ = feedback->pose;
}

// ============================================================================================== //
// == Service callbacks ========================================================================= //
// ============================================================================================== //

bool Calibration::continueGlobalCalibrationCB(ContinueGlobalCalibration::Request &request,
                                              ContinueGlobalCalibration::Response &response)
{
    boost::unique_lock<boost::mutex> lock(global_calib_mutex_);

    ROS_INFO("Global calibration continued");

    global_calib_cond_.notify_all();

    return true;
}

bool Calibration::completeGlobalCalibrationCB(CompleteGlobalCalibration::Request &request,
                                              CompleteGlobalCalibration::Response &response)
{
    boost::unique_lock<boost::mutex> lock(global_calib_mutex_);

    ROS_INFO("Completing global calibration ...");

    global_calibration_complete_ = true;

    global_calib_cond_.notify_all();

    return true;
}

bool Calibration::continueLocalCalibrationCB(ContinueLocalCalibration::Request &request,
                                             ContinueLocalCalibration::Response &response)
{
    boost::unique_lock<boost::mutex> lock(local_calib_mutex_);

    ROS_INFO("Local calibration continued");

    local_calib_cond_.notify_all();

    return true;
}

bool Calibration::completeLocalCalibrationCB(CompleteLocalCalibration::Request &request,
                                             CompleteLocalCalibration::Response &response)
{
    boost::unique_lock<boost::mutex> lock(local_calib_mutex_);

    ROS_INFO("Completing local calibration ...");

    local_calibration_complete_ = true;

    local_calib_cond_.notify_all();

    return true;
}

bool Calibration::continueTestCalibrationCB(ContinueLocalCalibration::Request& request,
                                            ContinueLocalCalibration::Response& response)
{
    boost::unique_lock<boost::mutex> lock(test_calib_mutex_);

    ROS_INFO("Testing calibration continued");

    test_calib_cond_.notify_all();

    return true;
}

bool Calibration::saveGlobalCalibrationCB(SaveGlobalCalibration::Request& request,
                                          SaveGlobalCalibration::Response& response)
{
    if (calibraion_transform_.saveGlobalCalibration(request.destination))
    {
        ROS_INFO("Global calibration saved to %s", request.destination.c_str());
        return true;
    }

    ROS_ERROR("Failed to save global calibration to %s", request.destination.c_str());
    return false;
}

bool Calibration::loadGlobalCalibrationCB(LoadGlobalCalibration::Request& request,
                                          LoadGlobalCalibration::Response& response)
{
    if (calibraion_transform_.loadGlobalCalibration(request.source))
    {
        ROS_INFO("Global calibration %s loaded", request.source.c_str());
        return true;
    }

    ROS_ERROR("Failed to load global calibration from %s", request.source.c_str());
    return false;
}

bool Calibration::saveLocalCalibrationCB(SaveLocalCalibration::Request& request,
                                         SaveLocalCalibration::Response& response)
{
    if (calibraion_transform_.saveLocalCalibration(request.destination))
    {
        ROS_INFO("Local calibration saved to %s", request.destination.c_str());
        return true;
    }

    ROS_ERROR("Failed to save local calibration to %s", request.destination.c_str());
    return false;
}

bool Calibration::loadLocalCalibrationCB(LoadLocalCalibration::Request& request,
                                         LoadLocalCalibration::Response& response)
{
    if (calibraion_transform_.loadLocalCalibration(request.source))
    {
        ROS_INFO("Local calibration %s loaded", request.source.c_str());
        return true;
    }

    ROS_ERROR("Failed to load local calibration from %s", request.source.c_str());
    return false;
}


// ============================================================================================== //
// == Helper ==================================================================================== //
// ============================================================================================== //

void Calibration::publishGlobalStatus(const std::string &status,
                                      GlobalCalibrationFeedback& feedback)
{
    feedback.status = status;
    global_calibration_as_.publishFeedback(feedback);
    ROS_INFO("%s", status.c_str());
}

void Calibration::publishLocalStatus(const std::string &status,
                                     LocalCalibrationFeedback &feedback)
{
    feedback.status = status;
    local_calibration_as_.publishFeedback(feedback);
    ROS_INFO("%s", status.c_str());
}

InteractiveMarker Calibration::makeObjectMarker(const std::string& mesh_resource,
                                                const std::string& name)
{
    // create an interactive marker for our server
    InteractiveMarker int_marker;
    int_marker.header.frame_id = "/XTION_RGB";
    int_marker.name = name;
    int_marker.description = int_marker.name;

    // create a grey box marker
    Marker calibration_object_marker;
    calibration_object_marker.type = Marker::MESH_RESOURCE;
    calibration_object_marker.mesh_resource = mesh_resource;
    calibration_object_marker.scale.x = 1.0;
    calibration_object_marker.scale.y = 1.0;
    calibration_object_marker.scale.z = 1.0;
    calibration_object_marker.color.r = 0.0;
    calibration_object_marker.color.g = 1.0;
    calibration_object_marker.color.b = 0.0;
    calibration_object_marker.color.a = 1.0;

    InteractiveMarkerControl marker_control_obj;
    marker_control_obj.always_visible = true;
    marker_control_obj.orientation_mode = InteractiveMarkerControl::VIEW_FACING;
    marker_control_obj.interaction_mode = InteractiveMarkerControl::MOVE_PLANE;
    marker_control_obj.independent_marker_orientation = true;
    marker_control_obj.markers.push_back(calibration_object_marker);
    int_marker.controls.push_back(marker_control_obj);

    InteractiveMarkerControl marker_control;

    marker_control.orientation.w = 1;
    marker_control.orientation.x = 1;
    marker_control.orientation.y = 0;
    marker_control.orientation.z = 0;
    marker_control.name = "rotate_x";
    marker_control.interaction_mode = InteractiveMarkerControl::ROTATE_AXIS;
    int_marker.controls.push_back(marker_control);
    marker_control.name = "move_x";
    marker_control.interaction_mode = InteractiveMarkerControl::MOVE_AXIS;
    int_marker.controls.push_back(marker_control);

    marker_control.orientation.w = 1;
    marker_control.orientation.x = 0;
    marker_control.orientation.y = 1;
    marker_control.orientation.z = 0;
    marker_control.name = "rotate_z";
    marker_control.interaction_mode = InteractiveMarkerControl::ROTATE_AXIS;
    int_marker.controls.push_back(marker_control);
    marker_control.name = "move_z";
    marker_control.interaction_mode = InteractiveMarkerControl::MOVE_AXIS;
    int_marker.controls.push_back(marker_control);

    marker_control.orientation.w = 1;
    marker_control.orientation.x = 0;
    marker_control.orientation.y = 0;
    marker_control.orientation.z = 1;
    marker_control.name = "rotate_y";
    marker_control.interaction_mode = InteractiveMarkerControl::ROTATE_AXIS;
    int_marker.controls.push_back(marker_control);
    marker_control.name = "move_y";
    marker_control.interaction_mode = InteractiveMarkerControl::MOVE_AXIS;
    int_marker.controls.push_back(marker_control);

    return int_marker;
}

void Calibration::publishMarker(const geometry_msgs::Pose& pose,
                                const std::string& mesh_resource,
                                const ros::Publisher& pub,
                                int marker_id,
                                float r,
                                float g,
                                float b,
                                float a)
{
    visualization_msgs::Marker marker;
    marker.header.frame_id =  "/XTION_RGB";
    marker.header.stamp =  ros::Time::now();
    marker.ns = "global_calibration_object";
    marker.id = marker_id;
    marker.scale.x = 1.0;
    marker.scale.y = 1.0;
    marker.scale.z = 1.0;
    marker.color.r = r;
    marker.color.g = g;
    marker.color.b = b;
    marker.color.a = a;

    marker.type = visualization_msgs::Marker::MESH_RESOURCE;
    marker.action = visualization_msgs::Marker::ADD;
    marker.pose = pose;
    marker.mesh_resource = mesh_resource;

    pub.publish(marker);
}

void Calibration::cachePose(const geometry_msgs::Pose& pose, const std::string dest)
{
    std::ofstream pose_tmp_file;
    std::string cache_file = "/tmp/pose_cache_";    
    cache_file += dest;
    ROS_INFO("Caching pose %s", dest.c_str());
    pose_tmp_file.open (cache_file.c_str());
    pose_tmp_file << pose.position.x  << " " << pose.position.y << " " << pose.position.z << " ";
    pose_tmp_file << pose.orientation.w << " "
                  << pose.orientation.x << " "
                  << pose.orientation.y << " "
                  << pose.orientation.z;
    pose_tmp_file.close();
}

void Calibration::loadPoseFromCache(const std::string src, geometry_msgs::Pose& pose)
{
    std::ifstream pose_tmp_file;
    std::string cache_file = "/tmp/pose_cache_";
    cache_file += src;
    pose_tmp_file.open (cache_file.c_str());
    if (pose_tmp_file.is_open())
    {
        ROS_INFO("Loading pose %s from cache", cache_file.c_str());

        pose_tmp_file >> pose.position.x;
        pose_tmp_file >> pose.position.y;
        pose_tmp_file >> pose.position.z;
        pose_tmp_file >> pose.orientation.w;
        pose_tmp_file >> pose.orientation.x;
        pose_tmp_file >> pose.orientation.y;
        pose_tmp_file >> pose.orientation.z;
        pose_tmp_file.close();
    }
}
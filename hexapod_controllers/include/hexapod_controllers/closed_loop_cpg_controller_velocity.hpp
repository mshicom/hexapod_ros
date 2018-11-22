
/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2008, Willow Garage, Inc.
 *  Copyright (c) 2012, hiDOF, Inc.
 *  Copyright (c) 2013, PAL Robotics, S.L.
 *  Copyright (c) 2014, Fraunhofer IPA
 *  Copyright (c) 2018, INRIA
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

#ifndef CLOSED_LOOP_CPG_CONTROLLER_VELOCITY_H
#define CLOSED_LOOP_CPG_CONTROLLER_VELOCITY_H
#define TF_EULER_DEFAULT_ZYX
#include "cpg.hpp"
#include <boost/date_time.hpp>
#include <chrono>
#include <cmath>
#include <controller_interface/controller.h>
#include <geometry_msgs/Transform.h>
#include <geometry_msgs/TransformStamped.h>
#include <hardware_interface/joint_command_interface.h>
#include <iostream>
#include <kdl/chainiksolverpos_nr_jl.hpp>
#include <math.h>
#include <realtime_tools/realtime_buffer.h>
#include <ros/node_handle.h>
#include <ros/ros.h>
#include <sensor_msgs/Imu.h>
#include <std_msgs/Float64MultiArray.h>
#include <stdio.h>
#include <string>
#include <tf/transform_listener.h>
#include <tf2_msgs/TFMessage.h>
#include <tf_conversions/tf_eigen.h>
#include <trac_ik/trac_ik.hpp>
#include <vector>
// Trac_ik and KDL solver
#include <kdl/chainfksolverpos_recursive.hpp>
#include <kdl/chainiksolverpos_nr_jl.hpp>
#include <trac_ik/trac_ik.hpp>
// Generate a trajectory using KDL
#include <Eigen/Dense>
#include <kdl/path_line.hpp>
#include <kdl/rotational_interpolation_sa.hpp>
#include <kdl/trajectory_composite.hpp>
#include <kdl/trajectory_segment.hpp>
#include <kdl/velocityprofile_trap.hpp>
#include <tf/transform_broadcaster.h>

namespace closed_loop_cpg_controller_velocity {

    struct NoSafetyConstraints;

    /**
     * \brief Speed command controller for hexapod servos
     *
     * This class forwards the command signal down to a set of joints, if they
     * do not infringe some user-defined safety constraints.
     *
     * This controller is from the first part of the paper :
     *  Guillaume Sartoretti, Samuel Shaw, Katie Lam, Naixin Fan, Matthew J. Travers, Howie Choset:
     *  Central Pattern Generator With Inertial Feedback for Stable Locomotion and Climbing in Unstructured Terrain. ICRA 2018: 1-5$
     *
     * \tparam T class implementing the safety constraints
     *
     * \section ROS interface
     *
     * \param type hardware interface type.
     * \param joints Names of the joints to control.
     *
     * Subscribes to:
     * - \b command (std_msgs::Float64MultiArray) : The joint commands to apply.
     */
    template <class SafetyConstraint = NoSafetyConstraints, int NLegs = 6>
    class ClCpgControllerV : public controller_interface::Controller<hardware_interface::VelocityJointInterface> {
    public:
        ClCpgControllerV();
        ~ClCpgControllerV(){};

        /**
         * \brief Tries to recover n_joints JointHandles
         */
        bool init(hardware_interface::VelocityJointInterface* hw, ros::NodeHandle& nh);
        /**
         * \brief Starting is called one time after init and switch to update. It is enough  to read data but not to set servos position.
         * This needs to be done in update
         */
        void starting(const ros::Time& time){};
        /**
         * \brief This function is the control loop itself, called periodically by the controller_manager
         */
        void update(const ros::Time& /*time*/, const ros::Duration& period);

        /** Get joint angles from end-effector pose.

              @param q_init joint angles used to initialise the inverse kinematics
              @param frame target poses
              @param q_out resulting joint angles as found by inverse kinematics
              @param stop_on_failure if true, the function will return as soon as
                  the inverse kinematic computation fails for one leg (we walk
                  through them in ascending leg number).

              @return status of the computation of each leg; failed if < 0
          **/
        std::array<int, NLegs> cartesian_to_joint(const std::array<KDL::JntArray, NLegs>& q_init, const std::array<KDL::Frame, NLegs>& frame, std::array<KDL::JntArray, NLegs>& q_out, bool stop_on_failure = false);

        /*!  Name of the joints defined in urdf and .yaml config file. Make the correspondance with ros_control JointHandles  */
        std::vector<std::string> joint_names;
        /*!  JointHandle objects recovered thanks to joint_names*/
        std::vector<std::shared_ptr<hardware_interface::JointHandle>> joints;

        /*!  number of joints */
        unsigned int n_joints;
        /*!  has_init will be set to true when all joint positions are at 0 +- e*/
        bool has_init;
        /*! e is the error accepted at init */
        float e;
        /*!  cpg object go to cpg.hpp for more details*/
        cpg::CPG cpg_;

        /*!    angles of the soulder joints in the axial plane at init*/
        std::vector<float> X, Xprev, Xcommand, XcommandSmoothed;
        /*!    angles of the soulder joints in the sagittal plane at init*/
        std::vector<float> Y, Yprev, Ycommand, YcommandSmoothed;
        /*!    imu buffer  to get imu data from a topic ros*/
        realtime_tools::RealtimeBuffer<std::vector<double>> imu_buffer;
        /*!    tf buffer  to get tf data from a topic ros*/
        realtime_tools::RealtimeBuffer<std::vector<double>> tf_buffer;
        /* quat is used to recover the data from the imu as quaternions */
        tf::Quaternion quat;
        /* P is the rotation matrix associated tp quat */
        tf::Matrix3x3 P;
        /* R is the rotation that brings the robot orientation from its current orientation P to the desired orientation (keeping the body level)*/
        tf::Matrix3x3 R;
        /*_R_eigen the conversion of R in eigen format */
        Eigen::Matrix<float, 3, 3> _R_eigen;
        /*_P_eigen the conversion of R in eigen format */
        Eigen::Matrix<float, 3, 3> _P_eigen;
        /* rpy : current roll pitch and yaw, extracted from raw P */
        geometry_msgs::Vector3 rpy;
        /*end_effectors_transform are the transformations between the base_link and the end effectors (tip of the legs)*/
        std::vector<tf::Transform> end_effectors_transform;
        /*leg_end_effector_static_transforms are the end transforms between the last servo and the tip of the leg. Those are constant*/
        tf::Transform leg_end_effector_static_transforms;
        /*_tracik_solvers inverse kinematic solvers from trac_ik. There are NlLegs solvers, from base_link to the tip of the leg */
        std::array<std::shared_ptr<TRAC_IK::TRAC_IK>, NLegs> _tracik_solvers;
        std::vector<KDL::ChainFkSolverPos_recursive> _kdl_fk_solvers;
        /*_bounds for _tracik_solvers init */
        KDL::Twist _bounds;
        /*_q_init are the current angles of each servos, for each leg */
        std::array<KDL::JntArray, NLegs> _q_init;
        /*_q_out are the desired angle position of each servos, for each leg */
        std::array<KDL::JntArray, NLegs> _q_out;
        /*_q_out are the desired angle position of each servos, for each leg */
        std::array<KDL::JntArray, NLegs> _q_out2;
        /*_frame are the desired end effector position and orientation to keep the body level */
        std::array<KDL::Frame, NLegs> _frame;
        /*_leg_map_to_paper is mapping the numbering of the legs from the cpg paper to the servo order taht we have on our platform*/
        std::vector<int> _leg_map_to_paper;
        /*end effector position for each leg, in the base_link frame (robot body frame) */
        Eigen::Matrix<float, 3, NLegs> _r;
        /* desired end effector position for each leg, in the base_link frame (robot body frame) */
        Eigen::Matrix<float, 3, NLegs> _r_tilde;
        /* _kp is the proportional gain to go to the desired joint angle position */
        float _kp;
        bool init2;
        //For debug only for the imu feedback convention
        //tf::TransformBroadcaster br;
        Eigen::Matrix<float, 1, NLegs> integrate_delta_thetax;
        Eigen::Matrix<float, 1, NLegs> integrate_delta_thetay;
        Eigen::Matrix<float, 3, NLegs> _delta_theta_e;

    private:
        SafetyConstraint _constraint;

        /**
         * \brief Put the servos at the position 0 at init
         */
        void initJointPosition();
        /*!  Subscriber to recover imu data*/
        ros::Subscriber _sub_imu;
        /*!  Subscriber to recover tf data*/
        ros::Subscriber _sub_tf;
        /**
         * \brief callback to recover imu data
         */
        void imuCB(const sensor_msgs::ImuConstPtr& msg);
        /**
         * \brief callback to recover end effector position
         */
        void tfCB(const tf2_msgs::TFMessageConstPtr& msg);
    }; // class ClCpgControllerV

    /**
     * \brief Constructor
     */
    template <class SafetyConstraint, int NLegs>
    ClCpgControllerV<SafetyConstraint, NLegs>::ClCpgControllerV()
    {
        e = 0.05;
        has_init = false;
        X.resize(NLegs, 0.0);
        Xprev.resize(NLegs, 0.0);
        Xcommand.resize(NLegs, 0.01);
        Y.resize(NLegs, 0.0);
        Yprev.resize(NLegs, 0.0);
        Ycommand.resize(NLegs, 0.1);
        XcommandSmoothed.resize(NLegs, 0.0);
        YcommandSmoothed.resize(NLegs, 0.0);
        init2 = false;
        end_effectors_transform.resize(NLegs, tf::Transform());
        leg_end_effector_static_transforms = tf::Transform(tf::Quaternion(0.707106781188, 0.707106781185, -7.31230107717e-14, -7.3123010772e-14), tf::Vector3(0.0, 0.03825, -0.115));
        _leg_map_to_paper = {0, 2, 4, 5, 3, 1};
        integrate_delta_thetax = Eigen::Matrix<float, 1, NLegs>::Zero();
        integrate_delta_thetay = Eigen::Matrix<float, 1, NLegs>::Zero();
        // Init the IK solver
        std::string chain_start
            = "base_link";
        std::vector<std::string> chain_ends;
        std::vector<std::string> chain_ends_fk;
        float timeout = 0.005;
        float eps = 1e-5;
        std::string urdf = "robot_description"; // our urdf is loaded in the /robot_description topic

        // Construct the objects doing inverse kinematic computations
        for (size_t leg = 0; leg < NLegs; ++leg) {
            chain_ends.push_back("force_sensor_" + std::to_string(leg));
            chain_ends_fk.push_back("leg_" + std::to_string(leg) + "_3");
            std::cout << "force_sensor_" + std::to_string(leg) << std::endl;
            // This constructor parses the URDF loaded in rosparm urdf_param into the
            // needed KDL structures.
            _tracik_solvers[leg] = std::make_shared<TRAC_IK::TRAC_IK>(
                chain_start, chain_ends_fk[leg], urdf, timeout, eps);

            KDL::Chain chain;
            _tracik_solvers[leg]->getKDLChain(chain);
            std::cout << "numb seg : " << chain.getNrOfSegments() << " num joints " << chain.getNrOfJoints();
            for (int i = 0; i < chain.getNrOfSegments(); i++) {
                std::cout << " seg name " << chain.getSegment(i).getName() << std::endl;
            }
            // for (int i = 0; i < chain.getNrOfJoints(); i++) {
            //     std::cout << " joint name " << chain.getJoint(i).getName() << std::endl;
            // }
            _kdl_fk_solvers.push_back(KDL::ChainFkSolverPos_recursive(chain));

            _tracik_solvers[leg] = std::make_shared<TRAC_IK::TRAC_IK>(
                chain_start, chain_ends[leg], urdf, timeout, eps);
        }

        // Set rotational bounds to max, so that the kinematic ignores the rotation
        // part of the target pose
        _bounds = KDL::Twist::Zero();
        _bounds.rot.x(std::numeric_limits<float>::max());
        _bounds.rot.y(std::numeric_limits<float>::max());
        _bounds.rot.z(std::numeric_limits<float>::max());
        _kp = 3;

    } // namespace closed_loop_cpg_controller_velocity

    /**
     * \brief Tries to recover n_joints JointHandles
     */
    template <class SafetyConstraint, int NLegs>
    bool ClCpgControllerV<SafetyConstraint, NLegs>::init(hardware_interface::VelocityJointInterface* hw, ros::NodeHandle& nh)
    {
        // List of controlled joints
        std::string param_name = "joints";
        if (!nh.getParam(param_name, joint_names)) {
            ROS_ERROR_STREAM("Failed to getParam '" << param_name << "' (namespace: " << nh.getNamespace() << ").");
            return false;
        }
        n_joints = joint_names.size();

        if (n_joints == 0) {
            ROS_ERROR_STREAM("List of joint names is empty.");
            return false;
        }
        for (unsigned int i = 0; i < n_joints; i++) {
            try {
                joints.push_back(
                    std::make_shared<hardware_interface::JointHandle>(
                        hw->getHandle(joint_names[i])));
                ROS_DEBUG_STREAM("Joint number " << i << ": " << joint_names[i]);
            }
            catch (const hardware_interface::HardwareInterfaceException& e) {
                ROS_ERROR_STREAM("Exception thrown: " << e.what());
                return false;
            }
        }
        // Safety Constraint
        if (!_constraint.init(joints, nh)) {
            ROS_ERROR_STREAM("Initialisation of the safety contraint failed");
            return false;
        }
        // ros subscriber to recover imu data
        _sub_imu = nh.subscribe<sensor_msgs::Imu>("/imu/data", 1, &ClCpgControllerV<SafetyConstraint, NLegs>::imuCB, this);
        // ros subscriber to recover end effector positions (tip of the legs positions)
        _sub_tf = nh.subscribe<tf2_msgs::TFMessage>("/tf", 1, &ClCpgControllerV<SafetyConstraint, NLegs>::tfCB, this);
        return true;
    }

    /**
     * \brief This function is the control loop itself, called periodically by the controller_manager
     */
    template <class SafetyConstraint, int NLegs>
    void ClCpgControllerV<SafetyConstraint, NLegs>::update(const ros::Time& /*time*/, const ros::Duration& period)
    {
        //Read the imu buffer to recover imu data
        std::vector<double>& imu_quat = *imu_buffer.readFromRT();

        if (imu_quat.size() == 4) {
            //Recover current orientation
            quat = tf::Quaternion(imu_quat[0], imu_quat[1], imu_quat[2], imu_quat[3]);
            P = tf::Matrix3x3(quat);
            P.getRPY(rpy.x, rpy.y, rpy.z);
            //Here we use the TF_EULER_DEFAULT_ZYX convention : the yaw is around z, the pitch is around y and the rooll is around x
            //Doing so we have a discrapency between the feedback from the imu and the TF_EULER_DEFAULT_ZYX convention
            //The roll is actually -pitch
            //The pitch is the roll
            //We create a new P which will follow the TF_EULER_DEFAULT_ZYX from ros convention
            tf::Quaternion tmp(rpy.z, rpy.x, -rpy.y);
            P = tf::Matrix3x3(tmp);
            //Create R, the rotation that will bring the hexapod body level
            //We only want to keep the yaw direction from P and bring the roll and pitch to 0
            R = tf::Matrix3x3(tf::Quaternion(tf::Vector3(0, 0, 1), rpy.z)) * P.transpose();
            //Other way to create R :
            // tf::Quaternion tmp0(0, -rpy.x, rpy.y);
            // R = tf::Matrix3x3(tmp0);
            // tf::Quaternion tmp0(0, -rpy.x, rpy.y);
            // R = tf::Matrix3x3(tmp0);

            //Uncomment to debug imu conventions
            //tf::Transform transformimu(P, tf::Vector3(0.0, 0.0, 0.0));
            //br.sendTransform(tf::StampedTransform(transformimu, ros::Time::now(), "base_link", "imu"));

            for (unsigned int i = 0; i < 3; i++) {
                for (unsigned int j = 0; j < 3; j++) {
                    _P_eigen(i, j) = P[i].m_floats[j];
                    _R_eigen(i, j) = R[i].m_floats[j];
                }
            }
        }

        //Set the servo positions to 0 at the beginning because the init and starting function are not called long enough to do that
        if (has_init == false) {
            initJointPosition();
            Xcommand = {0.1, 0.1, 0.1, 0.1, 0.1, 0.1};
            Ycommand = {0.1, 0.1, 0.1, 0.1, 0.1, 0.1};
        }
        //Then carry on the cpg control
        else {

            /* Read the position values from the servos*/
            KDL::JntArray array(3); //Contains the 3 joint angles for one leg
            int sign = 1;
            for (unsigned int i = 0; i < NLegs; i++) {
                sign = (i < 3) ? 1 : -1;
                X[_leg_map_to_paper[i]] = sign * joints[i]->getPosition();
                Y[_leg_map_to_paper[i]] = joints[6 + i]->getPosition();
                array(0) = joints[i]->getPosition();
                array(1) = joints[6 + i]->getPosition();
                array(2) = joints[12 + i]->getPosition();
                _q_init.at(i) = array; // angles for the first leg
                _r(0, i) = end_effectors_transform[i].getOrigin().m_floats[0]; //x position of the tip of the first leg in the robot body frame
                _r(1, i) = end_effectors_transform[i].getOrigin().m_floats[1]; //y position of the tip of the first leg in the robot body frame
                _r(2, i) = end_effectors_transform[i].getOrigin().m_floats[2]; //z position of the tip of the first leg in the robot body frame
            }
            _r_tilde = _P_eigen.transpose() * _R_eigen * _P_eigen * _r;
            std::array<KDL::Frame, NLegs> _r_tilde_frame;

            for (size_t leg = 0; leg < 6; leg++) {
                // _r_tilde(2, leg) = _r(2, leg);
                // _r_tilde(2, leg) = -std::sqrt(std::abs(_r(0, leg) * _r(0, leg) + _r(1, leg) * _r(1, leg) + _r(2, leg) * _r(2, leg) - _r_tilde(0, leg) * _r_tilde(0, leg) - _r_tilde(1, leg) * _r_tilde(1, leg)));
                _r_tilde_frame.at(leg) = KDL::Frame(KDL::Vector(_r_tilde(0, leg), _r_tilde(1, leg), _r_tilde(2, leg)));
            }

            std::array<int, NLegs> status = ClCpgControllerV<SafetyConstraint, NLegs>::cartesian_to_joint(_q_init, _r_tilde_frame, _q_out, false);

            for (unsigned int j = 0; j < NLegs; j++) {
                sign = (j < 3) ? 1 : -1;

                //qout is actually the  qinit - q_desired
                _delta_theta_e(0, _leg_map_to_paper[j]) = -_q_out.at(j)(0);
                _delta_theta_e(1, _leg_map_to_paper[j]) = -_q_out.at(j)(1);
                _delta_theta_e(2, _leg_map_to_paper[j]) = -_q_out.at(j)(2);

                // integrate_delta_thetax[_leg_map_to_paper[j]] += 0 * (-sign * _q_out.at(j)(0) - 0.05 * integrate_delta_thetax[_leg_map_to_paper[j]]);
                integrate_delta_thetay[_leg_map_to_paper[j]] += -_q_out.at(j)(1) - 0.05 * integrate_delta_thetay[_leg_map_to_paper[j]];

                // Here a threshold on the integrator is used to prevent a too high xdot and ydot output. (It leads to an unstable servos motion)
                if (integrate_delta_thetay[_leg_map_to_paper[j]] > 10)
                    integrate_delta_thetay[_leg_map_to_paper[j]] = 10;
                if (integrate_delta_thetay[_leg_map_to_paper[j]] < -10)
                    integrate_delta_thetay[_leg_map_to_paper[j]] = -10;
            }

            std::vector<std::pair<float, float>> XYdot = cpg_.computeXYdot(X, Y, integrate_delta_thetax, integrate_delta_thetay, _delta_theta_e);
            /* Send velocity command */
            // std::vector<float> cy = cpg_.get_cy();
            // int longt = 0;
            // for (unsigned int i = 0; i < NLegs; i++) {
            //     if ((Y[_leg_map_to_paper[i]]) > (cy[_leg_map_to_paper[i]] + 0.3)) {
            //         longt++;
            //     }
            // }

            for (unsigned int i = 0; i < NLegs; i++) {
                sign = (i < 3) ? 1 : -1;
                // XcommandSmoothed[_leg_map_to_paper[i]] = (sign * XYdot[_leg_map_to_paper[i]].first + XcommandSmoothed[_leg_map_to_paper[i]]) / 2;
                // YcommandSmoothed[_leg_map_to_paper[i]] = (XYdot[_leg_map_to_paper[i]].second + YcommandSmoothed[_leg_map_to_paper[i]]) / 2;
                joints[i]->setCommand(sign * 1 * XYdot[_leg_map_to_paper[i]].first);
                joints[6 + i]->setCommand(1 * XYdot[_leg_map_to_paper[i]].second);
                joints[12 + i]->setCommand(1 * XYdot[_leg_map_to_paper[i]].second);
            }
        }
        _constraint.enforce(period);
    } // namespace closed_loop_cpg_controller_velocity
    /**
    * \brief Put the servos at the position 0 at init
    */
    template <class SafetyConstraint, int NLegs>
    void ClCpgControllerV<SafetyConstraint, NLegs>::initJointPosition()
    {
        unsigned int count = 0;
        for (unsigned int i = 0; i < n_joints; i++) {
            joints[i]->setCommand(-_kp * (joints[i]->getPosition()));
            if (std::abs(joints[i]->getPosition()) < e) {
                count++; //when the position 0 is reached do count++
            }
            if (count == n_joints) {
                has_init = true;
                //When every servo is around the 0 position stop the init phase and go on with cpg control
            }
        }
    }
    template <class SafetyConstraint, int NLegs>
    void ClCpgControllerV<SafetyConstraint, NLegs>::imuCB(const sensor_msgs::ImuConstPtr& msg)
    {
        imu_buffer.writeFromNonRT({msg->orientation.x, msg->orientation.y, msg->orientation.z, msg->orientation.w});
    }

    template <class SafetyConstraint, int NLegs>
    void ClCpgControllerV<SafetyConstraint, NLegs>::tfCB(const tf2_msgs::TFMessageConstPtr& msg)
    {
        std::vector<tf::Transform> transformStamped(msg->transforms.size(), tf::Transform());
        for (unsigned int i = 0; i < msg->transforms.size(); i++) {
            tf::transformMsgToTF(msg->transforms[i].transform, transformStamped[i]);
        }
        for (unsigned int j = 0; j < NLegs; j++) {
            end_effectors_transform[j] = transformStamped[j] * transformStamped[6 + 2 * j] * transformStamped[7 + 2 * j] * leg_end_effector_static_transforms;
        }
    }

    /** Get joint angles from end-effector pose.

          @param q_init joint angles used to initialise the inverse kinematics
          @param frame target poses
          @param q_out resulting joint angles as found by inverse kinematics
          @param stop_on_failure if true, the function will return as soon as
              the inverse kinematic computation fails for one leg (we walk
              through them in ascending leg number).

          @return status of the computation of each leg; failed if < 0
      **/
    template <class SafetyConstraint, int NLegs>
    std::array<int, NLegs> ClCpgControllerV<SafetyConstraint, NLegs>::cartesian_to_joint(
        const std::array<KDL::JntArray, NLegs>& q_init,
        const std::array<KDL::Frame, NLegs>& frame,
        std::array<KDL::JntArray, NLegs>& q_out,
        bool stop_on_failure)
    {
        std::array<int, NLegs> status;

        for (size_t leg = 0; leg < NLegs; ++leg) {
            status[leg] = _tracik_solvers[leg]->CartToJnt(q_init[leg], frame[leg], q_out[leg], _bounds);
            if (stop_on_failure && status[leg] < 0)
                break;
        }

        return status;
    }

    /** \cond HIDDEN_SYMBOLS */
    struct NoSafetyConstraints {
        bool init(const std::vector<std::shared_ptr<hardware_interface::JointHandle>>& joints,
            ros::NodeHandle& nh)
        {
            return true;
        }
        bool enforce(const ros::Duration& period)
        {
            return true;
        }
        double consult(const ros::Duration& period)
        {
            return std::numeric_limits<double>::max();
        }
    };
    /** \endcond */

} // namespace closed_loop_cpg_controller_velocity

#endif

/*
   simrate.hpp : PID-based stabilization for simulator

   XXX should eventually be replaced by Rate PID controller in main Hackflight repository

   Copyright (c) 2018 Simon D. Levy

   This file is part of HackflightSim.

   HackflightSim is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   HackflightSim is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   You should have received a copy of the GNU General Public License
   along with Hackflight.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <algorithm>
#include <limits>
#include <cmath>

#include "receiver.hpp"
#include "filters.hpp"
#include "debug.hpp"
#include "datatypes.hpp"
#include "pidcontroller.hpp"

namespace hf {

    // shared with Hackflight class
    enum {
        AXIS_ROLL = 0,
        AXIS_PITCH,
        AXIS_YAW
    };

    class Rate : public PID_Controller {

        friend class Hackflight;

        private: 

            float degreesToRadians(float deg)
            {
                return M_PI * deg / 180.;
            }

            // Arbitrary constants
            const float GYRO_WINDUP_MAX             = 6.0f;
            const float BIG_GYRO_DEGREES_PER_SECOND = 40.0f; 
            const float BIG_YAW_DEMAND              = 0.1f;
            const float MAX_ARMING_ANGLE_DEGREES    = 25.0f;         

            // For PTerm computation
            float _PTerm[2]; // roll, pitch

            // proportion of cyclic demand compared to its maximum
            float _proportionalCyclicDemand;

            float _bigGyroRate;

            float computeITermGyro(float error, float rateI, float rcCommand, float gyro[3], uint8_t axis)
            {
                // Avoid integral windup
                _errorGyroI[axis] = Filter::constrainAbs(_errorGyroI[axis] + error, GYRO_WINDUP_MAX);

                // Reset integral on quick gyro change or large gyroYaw command
                if ((fabs(gyro[axis]) > _bigGyroRate) || ((axis == AXIS_YAW) && (fabs(rcCommand) > BIG_YAW_DEMAND)))
                    _errorGyroI[axis] = 0;

                return (_errorGyroI[axis] * rateI);
            }

            // PID constants set in constructor
            float _gyroYawP; 
            float _gyroYawI;

        protected:

            float maxArmingAngle;

        private:

            void init(void)
            {
                // Zero-out previous values for D term
                for (uint8_t axis=0; axis<2; ++axis) {
                    _lastError[axis] = 0;
                    _gyroDeltaError1[axis] = 0;
                    _gyroDeltaError2[axis] = 0;
                }

                // Convert degree parameters to radians for use later
                _bigGyroRate = degreesToRadians(BIG_GYRO_DEGREES_PER_SECOND);
                maxArmingAngle = degreesToRadians(MAX_ARMING_ANGLE_DEGREES);

                // Initialize gyro error integral
                resetIntegral();
            }

            float _lastError[2];
            float _gyroDeltaError1[2]; 
            float _gyroDeltaError2[2];
            float _errorGyroI[3];

            void resetIntegral(void)
            {
                _errorGyroI[AXIS_ROLL] = 0;
                _errorGyroI[AXIS_PITCH] = 0;
                _errorGyroI[AXIS_YAW] = 0;
            }

        public:

            bool modifyDemands(state_t & state, demands_t & demands, float currentTime)
            {
                (void)currentTime;
                
                _PTerm[0] = demands.roll;
                _PTerm[1] = demands.pitch;

                // Pitch, roll use leveling based on Euler angles
                demands.roll  = computeCyclicPid(demands.roll,  state.angularVelocities, AXIS_ROLL);
                demands.pitch = computeCyclicPid(demands.pitch, state.angularVelocities, AXIS_PITCH);

                // For gyroYaw, P term comes directly from RC command, and D term is zero
                float ITermGyroYaw = computeITermGyro(_gyroYawP, _gyroYawI, demands.yaw, state.angularVelocities, AXIS_YAW);
                demands.yaw = computePid(_gyroYawP, demands.yaw, ITermGyroYaw, 0, state.angularVelocities, AXIS_YAW);

                // Prevent "gyroYaw jump" during gyroYaw correction
                demands.yaw = Filter::constrainAbs(demands.yaw, 0.1 + fabs(demands.yaw));

                // We've always gotta do this!
                return true;
            }
            // ===================================================

        private:
            
            // PID constants set in constructor
            float _demandsToRate;
            float _gyroCyclicI;
            float _gyroCyclicD; 

            float _demandRoll;
            float _demandPitch;

            float computePid(float rateP, float PTerm, float ITerm, float DTerm, float gyro[3], uint8_t axis)
            {
                PTerm -= gyro[axis] * rateP; 

                return PTerm + ITerm - DTerm;
            }

            // Computes leveling PID for pitch or roll
            void computeCyclicPTerm(float demand, float eulerAngles[3], uint8_t imuAxis, uint8_t auxState)
            {
                if (auxState == 0) { // rate mode
                    _PTerm[imuAxis] = demand; 
                }

                else {

                    _PTerm[imuAxis] = Filter::complementary(demand, 0, _proportionalCyclicDemand); 
                }
            }

            // Computes leveling PID for pitch or roll
            float computeCyclicPid(float rcCommand, float gyro[3], uint8_t imuAxis)
            {
                float error = rcCommand * _demandsToRate - gyro[imuAxis];

                // I
                float ITerm = computeITermGyro(error, _gyroCyclicI, rcCommand, gyro, imuAxis);
                ITerm *= _proportionalCyclicDemand;

                // D
                float gyroDeltaError = gyro[imuAxis] - _lastError[imuAxis];
                _lastError[imuAxis] = gyro[imuAxis];
                float gyroDeltaErrorSum = _gyroDeltaError1[imuAxis] + _gyroDeltaError2[imuAxis] + gyroDeltaError;
                _gyroDeltaError2[imuAxis] = _gyroDeltaError1[imuAxis];
                _gyroDeltaError1[imuAxis] = gyroDeltaError;
                float DTerm = gyroDeltaErrorSum * _gyroCyclicD; 

                return computePid(_demandsToRate, _PTerm[imuAxis], ITerm, DTerm, gyro, imuAxis);
            }

        public:

            Rate(float demandsToRate, float gyroCyclicI, float gyroCyclicD, float gyroYawP, float gyroYawI) :
                _demandsToRate(demandsToRate), 
                _gyroCyclicI(gyroCyclicI), 
                _gyroCyclicD(gyroCyclicD), 
                _gyroYawP(gyroYawP), 
                _gyroYawI(gyroYawI) 
            {                
                init();
            }

            void simUpdate(float eulerAngles[3], uint8_t auxState)
            {
                computeCyclicPTerm(_demandRoll,  eulerAngles, 0, auxState);
                computeCyclicPTerm(_demandPitch, eulerAngles, 1, auxState);
            }

            void updateReceiver(demands_t & demands, bool throttleIsDown)
            {
                _demandRoll  = demands.roll;
                _demandPitch = demands.pitch;

                // Compute proportion of cyclic demand compared to its maximum
                _proportionalCyclicDemand = Filter::max(fabs(demands.roll), fabs(demands.pitch)) / 0.5f;
                
                // When landed, reset integral component of PID
                if (throttleIsDown) {
                    resetIntegral();
                }
            }


    };  // class Rate

} // namespace
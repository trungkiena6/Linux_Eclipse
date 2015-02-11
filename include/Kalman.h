/* Copyright (C) 2012 Kristian Lauszus, TKJ Electronics. All rights reserved.
 
 This software may be distributed and modified under the terms of the GNU
 General Public License version 2 (GPL2) as published by the Free Software
 Foundation and appearing in the file GPL2.TXT included in the packaging of
 this file. Please note that GPL2 Section 2[b] requires that all works based
 on this software must also be made publicly available under the terms of
 the GPL2 ("Copyleft").
 
 Contact information
 -------------------
 
 Kristian Lauszus, TKJ Electronics
 Web      :  http://www.tkjelectronics.com
 e-mail   :  kristianl@tkjelectronics.com
 */

#ifndef _Kalman_h
#define _Kalman_h

typedef struct {
    /* Kalman filter variables */
    double Q_angle; // Process noise variance for the accelerometer
    double Q_bias; // Process noise variance for the gyro bias
    double R_measure; // Measurement noise variance - this is actually the variance of the measurement noise
    
    double angle; // The angle calculated by the Kalman filter - part of the 2x1 state matrix
    double bias; // The gyro bias calculated by the Kalman filter - part of the 2x1 state matrix
    double rate; // Unbiased rate calculated from the rate and the calculated bias - you have to call getAngle to update the rate
    
    double P[2][2]; // Error covariance matrix - This is a 2x2 matrix
    double K[2]; 	// Kalman gain - This is a 2x1 matrix
    double y; 		// Angle difference - 1x1 matrix
    double S; 		// Estimate error - 1x1 matrix
} Kalman_t;

void initKalman(Kalman_t *K);

// The angle should be in degrees and the rate should be in degrees per second and the delta time in seconds
double getAngle(Kalman_t *K,  double newAngle, double newRate, double dt);

#endif

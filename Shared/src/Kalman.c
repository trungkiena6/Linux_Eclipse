/*
 * Kalman.c
 *
 *  Created on: Jul 27, 2014
 *      Author: martin
 */


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

#include "Kalman.h"

void initKalman(Kalman_t *K)
{
	/* We will set the variables like so, these can also be tuned by the user */
	K->Q_angle = 0.001;
	K->Q_bias = 0.003;
	K->R_measure = 0.03;

	K->bias = 0; // Reset bias
	K->P[0][0] = 0; // Since we assume that the bias is 0 and we know the starting angle (use setAngle), the error covariance matrix is set like so - see: http://en.wikipedia.org/wiki/Kalman_filter#Example_application.2C_technical
	K->P[0][1] = 0;
	K->P[1][0] = 0;
	K->P[1][1] = 0;
}

// The angle should be in degrees and the rate should be in degrees per second and the delta time in seconds
double getAngle(Kalman_t *K, double newAngle, double newRate, double dt) {
	// KasBot V2  -  Kalman filter module - http://www.x-firm.com/?page_id=145
	// Modified by Kristian Lauszus
	// See my blog post for more information: http://blog.tkjelectronics.dk/2012/09/a-practical-approach-to-kalman-filter-and-how-to-implement-it

	// Discrete Kalman filter time update equations - Time Update ("Predict")
	// Update xhat - Project the state ahead
	/* Step 1 */
	K->rate = newRate - K->bias;
	K->angle += dt * K->rate;

	// Update estimation error covariance - Project the error covariance ahead
	/* Step 2 */
	K->P[0][0] += dt * (dt*K->P[1][1] - K->P[0][1] - K->P[1][0] + K->Q_angle);
	K->P[0][1] -= dt * K->P[1][1];
	K->P[1][0] -= dt * K->P[1][1];
	K->P[1][1] += K->Q_bias * dt;

	// Discrete Kalman filter measurement update equations - Measurement Update ("Correct")
	// Calculate Kalman gain - Compute the Kalman gain
	/* Step 4 */
	K->S = K->P[0][0] + K->R_measure;
	/* Step 5 */
	K->K[0] = K->P[0][0] / K->S;
	K->K[1] = K->P[1][0] / K->S;

	// Calculate angle and bias - Update estimate with measurement zk (newAngle)
	/* Step 3 */
	K->y = newAngle - K->angle;
	/* Step 6 */
	K->angle += K->K[0] * K->y;
	K->bias += K->K[1] * K->y;

	// Calculate estimation error covariance - Update the error covariance
	/* Step 7 */
	K->P[0][0] -= K->K[0] * K->P[0][0];
	K->P[0][1] -= K->K[0] * K->P[0][1];
	K->P[1][0] -= K->K[1] * K->P[0][0];
	K->P[1][1] -= K->K[1] * K->P[0][1];

	return K->angle;
}

/*
 * IMU.h
 *
 *      Author: martin
 */

#ifndef IMU_H_
#define IMU_H_

pthread_t IMUInit();

#define COMPASS_OFFSET 13.0

extern FILE *imuDebugFile;

#endif

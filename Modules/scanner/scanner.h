/*
 * scanner.h
 *
 *  Created on: Jul 11, 2014
 *      Author: martin
 */

#ifndef SCANNER_H_
#define SCANNER_H_

//scanner task
pthread_t ScannerInit();

void ScannerProcessMessage(psMessage_t *msg);

#endif

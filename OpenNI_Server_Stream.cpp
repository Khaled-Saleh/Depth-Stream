/*
 * main.c
 *
 *  Created on: Jun 28, 2012
 *      Author: khaled
 */

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include "cv.h"
#include "highgui.h"

#include <iostream>
#include <vector>
#include <OpenNI.h>
#include <opencv2/opencv.hpp>

#define PORT 8888

#define RESOLUTION_X_DEPTH 320
#define RESOLUTION_Y_DEPTH 240
#define FPS_DEPTH 60

#define RESOLUTION_X_COLOR 320
#define RESOLUTION_Y_COLOR 240
#define FPS_COLOR 30

#define FOCAL 580
#define BASELINE 0.075

using namespace openni;
using namespace cv;
using namespace std;

IplImage* img0;
IplImage* img1;
IplImage* img2;
int is_data_ready = 0;
int serversock, clientsock;
int x,y;
char str[5];

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void* streamServer(void* arg);
void quit(char* msg, int retval);

int main(int argc, char** argv) {

	OpenNI::initialize();
	Device device;
	bool rc = device.open(ANY_DEVICE);
	if (rc != STATUS_OK) {
		printf("can't open device");
		return -1;
	}

	VideoMode myDepthVideoMode;
	VideoMode myColorVideoMode;

	VideoStream colorStream;
	colorStream.create(device, SENSOR_COLOR);
	myColorVideoMode = colorStream.getVideoMode();
	myColorVideoMode.setResolution(RESOLUTION_X_COLOR, RESOLUTION_Y_COLOR);
	myColorVideoMode.setFps(FPS_COLOR);
	colorStream.setVideoMode(myColorVideoMode);
	colorStream.setMirroringEnabled(false);
	colorStream.start();

	VideoStream depthStream;
	depthStream.create(device, SENSOR_DEPTH);
	myDepthVideoMode = depthStream.getVideoMode();
	myDepthVideoMode.setResolution(RESOLUTION_X_DEPTH, RESOLUTION_Y_DEPTH);
	myDepthVideoMode.setFps(FPS_DEPTH);
	depthStream.setVideoMode(myDepthVideoMode);
	depthStream.setMirroringEnabled(false);

	depthStream.start();

	device.setDepthColorSyncEnabled(true);

	// Align the depth and color images
	if (device.isImageRegistrationModeSupported(
			IMAGE_REGISTRATION_DEPTH_TO_COLOR))
		device.setImageRegistrationMode(IMAGE_REGISTRATION_DEPTH_TO_COLOR);

	Mat colorImage;
	Mat depthImage;
	Mat depthoutputImage;

	VideoFrameRef colorFrame, depthFrame;
	unsigned short* t = new
	unsigned short[RESOLUTION_X_DEPTH * RESOLUTION_Y_DEPTH];; // for openni_get_image

	//nod->RGB_Image = cvCreateImage(cvSize(RESOLUTION_X_COLOR,RESOLUTION_Y_COLOR),IPL_DEPTH_8U,3);
	//nod->Depth_Image = cvCreateImage(cvSize(RESOLUTION_X_DEPTH,RESOLUTION_Y_DEPTH),IPL_DEPTH_16U,1);

	img0 = cvCreateImage(cvSize(RESOLUTION_X_DEPTH, RESOLUTION_Y_DEPTH),
			IPL_DEPTH_16U, 1);

	pthread_t thread_s;
	int key;

	//img1 = cvCreateImage(cvGetSize(img0), IPL_DEPTH_16U, 1);
	img2 = cvCreateImage(cvGetSize(img0), IPL_DEPTH_8U, 1);

	cvZero(img1);
	cvZero(img2);
	

	cvNamedWindow("stream_server1", 0); 
	cvNamedWindow("stream_server2", 0);

	/* print the width and height of the frame, needed by the client */
	fprintf(stdout, "width: %d\nheight: %d\n\n", img0->width, img0->height);
	fprintf(stdout, "Press 'q' to quit.\n\n");

	/* run the streaming server as a separate thread */
	if (pthread_create(&thread_s, NULL, streamServer, NULL)) {
		quit("pthread_create failed.", 1);
	}

	while (key != 'q') {

		/*colorStream.readFrame( &colorFrame );
		 if ( colorFrame.isValid() ) {
		 cvSetData(img0, (char*)colorFrame.getData() , RESOLUTION_X_COLOR*3);
		 }*/

		depthStream.readFrame(&depthFrame);
		if (depthFrame.isValid())
			cvSetData(img0, (unsigned short*) depthFrame.getData(),
					RESOLUTION_X_DEPTH * 2);

		

		

		float tmp = 0;
		for (int j = 0; j < RESOLUTION_Y_DEPTH; j++) {
			for (int i = 0; i < RESOLUTION_X_DEPTH; i++) {
				tmp = ((unsigned short *) img0->imageData)[RESOLUTION_X_DEPTH
						* j + i];
				tmp = (1090 - (8000 * FOCAL * BASELINE / tmp)); // from milli meters to pixels
				if (tmp < 1024 && tmp > 0)
					t[RESOLUTION_X_DEPTH * j + i] = (unsigned short) tmp;
				else
					t[RESOLUTION_X_DEPTH * j + i] = 0;
			}
		}
		cvSetData(img0, t, RESOLUTION_X_DEPTH * 2);

		
		//cvCvtColor(img2, img2, CV_GRAY2BGR);
		//depthImage.convertTo( depthoutputImage, CV_8UC1, 255.0/10000);
		//cvtColor(depthoutputImage, depthoutputImage, CV_GRAY2BGR);
		//imshow( "Depth Camera", depthoutputImage );

//img0->origin = 0;
//cvFlip(img0, img0, -1);

		/**
		 * convert to grayscale
		 * note that the grayscaled image is the image to be sent to the client
		 * so we enclose it with pthread_mutex_lock to make it thread safe
		 */
		pthread_mutex_lock(&mutex);
		cvConvertScale(img0, img2, 255.0 / 10000);
		img1 = cvCloneImage(img0);
		x = ((unsigned short *) img0->imageData)[120*320+160];
		y = ((unsigned short *) img2->imageData)[120*320+160];
		sprintf(str, "%d", x);
		fprintf(stdout, "Mid: %d, %d, %s\n\n", x, y,str);
		is_data_ready = 1;
		pthread_mutex_unlock(&mutex);

		/* also display the video here on server */
		cvShowImage("stream_server1", img1);
		cvShowImage("stream_server2", img2);
		key = cvWaitKey(30);
	}

	/* user has pressed 'q', terminate the streaming server */
		if (pthread_cancel(thread_s)) {
		quit("pthread_cancel failed.", 1);
		}

	/* free memory */
		cvDestroyWindow("stream_server");

		depthStream.stop();
		depthStream.destroy();

		colorStream.stop();
		colorStream.destroy();

		device.close();

		OpenNI
		::shutdown();

		quit(NULL, 0);

		return 0;
}

/**
 * This is the streaming server, run as a separate thread
 * This function waits for a client to connect, and send the grayscaled images
 */
void* streamServer(void* arg) {
	struct sockaddr_in server;

	/* make this thread cancellable using pthread_cancel() */
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	/* open socket */
	if ((serversock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		quit("socket() failed", 1);
	}

	/* setup server's IP and port */
	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(PORT);
	server.sin_addr.s_addr = INADDR_ANY;

	/* bind the socket */
	if (bind(serversock, (const struct sockaddr*) &server, sizeof(server))
			== -1) {
		quit("bind() failed", 1);
	}

	/* wait for connection */
	if (listen(serversock, 10) == -1) {
		quit("listen() failed.", 1);
	}

	/* accept a client */
	if ((clientsock = accept(serversock, NULL, NULL)) == -1) {
		quit("accept() failed", 1);
	}

	/* the size of the data to be sent */
	int imgsize = img1->imageSize;
	int bytes, i;

	/* start sending images */
	while (1) {
		/* send the grayscaled frame, thread safe */
		pthread_mutex_lock(&mutex);
		if (is_data_ready) {
			bytes = send(clientsock, str, strlen(str), 0);
			//bytes = send(clientsock, img1->imageData, imgsize, 0);
			is_data_ready = 0;
		}
		pthread_mutex_unlock(&mutex);

		/* if something went wrong, restart the connection */
		if (bytes != imgsize) {
			fprintf(stderr, "Connection closed.\n");
			close(clientsock);

			if ((clientsock = accept(serversock, NULL, NULL)) == -1) {
				quit("accept() failed", 1);
			}
		}

		/* have we terminated yet? */
		pthread_testcancel();

		/* no, take a rest for a while */
		usleep(1000);
	}
}

/**
 * this function provides a way to exit nicely from the system
 */
void quit(char* msg, int retval) {
	if (retval == 0) {
		fprintf(stdout, (msg == NULL ? "" : msg));
		fprintf(stdout, "\n");
	} else {
		fprintf(stderr, (msg == NULL ? "" : msg));
		fprintf(stderr, "\n");
	}

	if (clientsock)
		close(clientsock);
	if (serversock)
		close(serversock);
	if (img1)
		cvReleaseImage(&img1);

	pthread_mutex_destroy(&mutex);

	exit(retval);
}

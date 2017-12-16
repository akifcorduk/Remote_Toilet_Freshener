#include <opencv2/opencv.hpp>
#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <numeric>
#include <string>
#include <utility>
#include <vector>
#include "Helper.h"
#include "Video.h"
#include <time.h>
#include "Serial.h"


using namespace cv;
using namespace std;
RNG rng(12345);
//enter your serial port here  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1
tstring commPortName(TEXT("COM3"));
Serial serial(commPortName, 9600);

Helper helper;
Video video("output.avi", 0);
bool is_open_input = video.initializeInput();
auto video_capture = video.getCaptureDevice();
Point base_location(8, 24);
Mat frame;      //actual image
Mat skin;		//binary image
Mat drawing;	//contours and convex hull
vector<cv::Point2i> nonZeroPixelLocation;	//vector that holds the location of non-zero pixels
int key;
int movedRightCount = 0;
int noMovementCount = 0;
time_t resetCountTime = time(0);
long int cumulativeNonZeroLocation = 0 ;	//the sum of all non-zero pixel locations
long int previousNonZeroLocation = 0;		//previous sum
bool gesture1Detected,gesture2Detected,sprayOnce,sprayTwice,personMovedToSink;
time_t gesture1StartTime, gesture2StartTime,sprayTriggerTime;

//finds skin color according to log-choromaticity color space
void findSkin() {
	//fill with one
	skin = Mat::ones(frame.size(), CV_8U) * 255;
	double logRG;
	double logBG;
	for (int i = 0; i < frame.rows; i++) {
		for (int j = 0; j < frame.cols; j++) {
			Vec3b color = frame.at<Vec3b>(Point(j, i));
			logRG = log10(((int)color[2]) * 1.0 / ((int)color[1]));
			logBG = log10(((int)color[0]) * 1.0 / ((int)color[1]));
			if (logRG<1.1 && logRG >0.15 && logBG <0.3 && logBG >-4) {
				skin.at<uchar>(Point(j, i)) = 255;
			}
			else {
				skin.at<uchar>(Point(j, i)) = 0;
			}
		}
	}
	//remove the noise
	erode(skin, skin, Mat(), Point(-1, -1), 2);
	dilate(skin, skin, Mat(), Point(-1, -1), 1);
	medianBlur(skin, skin,3);
	namedWindow("skin", CV_WINDOW_FREERATIO);
	imshow("skin", skin);
}

void findGestures() {
	vector<vector<Point> > contours;
	vector<Vec4i> hierarchy;
	int numberOfDefects = 0;
	// Find contours
	findContours(skin, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, Point(0, 0));
	drawing = Mat::zeros(skin.size(), CV_8UC3);
	vector<vector<Vec4i> >defects(contours.size());
	vector<vector<Point> >hull(contours.size());
	vector<vector<int> >hullsI(contours.size());
	double maxArea = 0;
	int maxIndex = 0;
	for (int i = 0; i< contours.size(); i++)
	{

		//find maximum contour area which is a hand, and that contours index
		if (contourArea(contours[i], false)>maxArea) {
			maxArea = contourArea(contours[i]);
			maxIndex = i;
		}

	}

	//find the convex hull and convexity defects in the contour that has largest area
	convexHull(contours[maxIndex], hull[maxIndex], false);
	convexHull(contours[maxIndex], hullsI[maxIndex], false);
	convexityDefects(contours[maxIndex], hullsI[maxIndex], defects[maxIndex]);
	Scalar color = Scalar(255, 255, 255);
	drawContours(drawing, contours, maxIndex, color, 2, 8, hierarchy, 0, Point());
	drawContours(drawing, hull, maxIndex, color, 1, 8, hierarchy, 0, Point());
	//filter contours
	for (int j = 0; j<defects[maxIndex].size(); ++j)
	{
		Vec4i& v = defects[maxIndex][j];
		float depth = v[3] / 256.0;
		if (depth > 50) //  filter defects by depth
		{
			int startidx = v[0];
			int endidx = v[1];
			int faridx = v[2];
			Point ptStart(contours[maxIndex][startidx]);
			Point ptEnd(contours[maxIndex][endidx]);
			Point ptFar(contours[maxIndex][faridx]);
			//finding angle of the lines with respect to x,y axis
			//subtract angles and find angle between two lines
			double angle1 = atan2(ptStart.y - ptFar.y, ptStart.x - ptFar.x) * 180.0 / CV_PI;
			if (angle1<0) angle1 = angle1 + 360;
			double angle2 = atan2(ptEnd.y - ptFar.y, ptEnd.x - ptFar.x) * 180.0 / CV_PI;
			if (angle2<0) angle2 = angle2 + 360;
			//filter defects by angle between lines
			if (abs(angle1 - angle2)< 85 && abs(angle1 - angle2)>5) {
				line(drawing, ptStart, ptFar, Scalar(255, 255, 255), 1);
				line(drawing, ptEnd, ptFar, Scalar(255, 255, 255), 1);
				circle(drawing, ptFar, 4, Scalar(255, 255, 255), 2);
				numberOfDefects++;
			}

		}
	}
	//gesture 1
	if (numberOfDefects == 4) {
		if (!gesture1Detected) {
			gesture1StartTime = time(0);
			gesture1Detected = true;
		}
		gesture2Detected = false;
	}
	//gesture 2
	else if (numberOfDefects == 3) {
		if (!gesture2Detected) {
			gesture2StartTime = time(0);
			gesture2Detected = true;
		}
		gesture1Detected = false;
	}
	else {
		gesture1Detected = false;
		gesture2Detected = false;
	}
	
}
void detectHandWashed() {
	cv::findNonZero(skin, nonZeroPixelLocation); 
	previousNonZeroLocation = cumulativeNonZeroLocation;
	cumulativeNonZeroLocation = 0;
	//calculate cumulative location
	for (int i = 0; i < nonZeroPixelLocation.size(); i++) {
		cumulativeNonZeroLocation += nonZeroPixelLocation[i].x;
	}
	//determine if person has moved right
	if ((sprayOnce || sprayTwice)) {
		if (cumulativeNonZeroLocation - previousNonZeroLocation > 100000) {
			Helper::putPrettyText("moved right", base_location, 1.2, drawing);
			movedRightCount++;
		}
		else {
			Helper::putPrettyText("no movement", base_location, 1.2, drawing);
			noMovementCount++;
		}

		if (time(0) - resetCountTime > 2) {
			if (movedRightCount > noMovementCount) {
				personMovedToSink = true;
			}
			else {
				personMovedToSink = false;
			}
			resetCountTime = time(0);
			movedRightCount = 0;
			noMovementCount = 0;
		}
	}
	
}

void checkSpray() {
	//check the gesture for 1 second, if gesture remained still for 1 second trigger spray
	if (time(0) - gesture1StartTime > 1 && gesture1Detected && !sprayTwice) {
		sprayOnce = false;
		sprayTwice = true;
		sprayTriggerTime = time(0);
	}
	else if (time(0) - gesture2StartTime > 1 && gesture2Detected && !sprayOnce) {

		sprayOnce = true;
		sprayTwice = false;
		sprayTriggerTime = time(0);
	}
	
}
//if person didn't move to sink send hads not washed warning
void warningLCD() {
	if ((sprayOnce || sprayTwice) && time(0) - sprayTriggerTime > 2) {
		if (!personMovedToSink) {
			serial.flush();
			serial.write("4\n");
			Helper::putPrettyText("Hands not washed",Point(350, 300), 3.0, frame);
			cout << "hands not washed" << endl;
		}
		sprayOnce = false;
		sprayTwice = false;
	}
}
int main() {
	CV_Assert(is_open_input);
	while(frame.empty())
		video_capture >> frame;

	namedWindow(WEBCAM_WINDOW, CV_WINDOW_FREERATIO);
	namedWindow(IMAGE_WINDOW, CV_WINDOW_FREERATIO);
	while (key != 27) {
		video_capture >> frame;
		findSkin();
		findGestures();
		detectHandWashed();
		checkSpray();
		warningLCD();
		//send serial signal to arduino to make it spray
		if (sprayOnce) {
			serial.flush();
			serial.write("1\n");
			Helper::putPrettyText("spray once", Point(200, 300), 3.0, frame);
		}
		else if (sprayTwice) {
			serial.flush();
			serial.write("2\n");
			Helper::putPrettyText("spray twice", Point(200, 300), 3.0, frame);
		}
		else {
			Helper::putPrettyText(" ", Point(200, 300), 3.0, frame);
		}
		imshow(WEBCAM_WINDOW, drawing);
		imshow(IMAGE_WINDOW, frame);
		key = waitKey(15);
	}
	//close all windows
	destroyAllWindows();
	destroyWindow(WEBCAM_WINDOW);
	destroyWindow(IMAGE_WINDOW);
}
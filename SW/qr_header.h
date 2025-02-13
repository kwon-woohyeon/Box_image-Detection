//qr_header.h
#ifndef HEADER_H
#define HEADER_H

#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <locale>
#include <codecvt>
#include <set>
#include <chrono>
#include <windows.h>
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/exception.h>

using namespace cv;
using namespace std;
using namespace chrono;

void putSizeText(Mat& img, double area);
bool notInList(Point new_object);
void initializeDatabase(sql::Connection*& con, sql::mysql::MySQL_Driver*& driver);
void detectAndDisplayContours(Mat& img_contours, Mat& frame, Mat& gray, Mat& binary);
void detectQRCode(Mat& frame, QRCodeDetector& detector, set<string>& detected_QRs, set<string>& printed_QRs, sql::Connection* con, sql::PreparedStatement*& pstmt);
void performTemplateMatching(Mat& gray, Mat& img_contours, vector<Point>& detected_objects, bool& display_text, time_point<steady_clock>& start_time);
void showWarning(Mat& img_contours, bool& display_text, time_point<steady_clock>& start_time);
void processVideo();


#endif

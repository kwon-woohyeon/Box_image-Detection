//qr_code.cpp
#include "qr_header.h"

const int font_scale = 1;
const int font_thickness = 2;
vector<Point> detected_objects;

// 박스의 크기를 화면에 표시하는 함수
void putSizeText(Mat& img, double area) {
    String size_label;
    const int x = 50;
    const int y = img.rows - 50;
    const int area_cut_off = 25000;
    const int pixel_size_S = 80000;
    const int pixel_size_M = 110000;
    const int pixel_size_L = 150000;

    Scalar color(0, 255, 0); // 기본 텍스트 색상

    if (area < area_cut_off) {
        return; // 너무 작은 면적의 경우 텍스트를 출력하지 않음
    }
    else if (area < pixel_size_S) {
        size_label = "[ S ] size";
    }
    else if (area < pixel_size_M) {
        size_label = "[ M ] size";
    }
    else if (area < pixel_size_L) {
        size_label = "[ L ]  size";
    }
    else {
        size_label = "[ XL ] size";
    }

    putText(img, size_label, Point(x, y), FONT_HERSHEY_COMPLEX, font_scale, color, font_thickness);
}

// 객체가 리스트에 있는지 확인하는 함수
bool notInList(Point new_object) {
    const double diagonal = 20.0;
    for (const auto& obj : detected_objects) {
        float a = obj.x - new_object.x;
        float b = obj.y - new_object.y;
        if (sqrt(a * a + b * b) < diagonal)
            return false;
    }
    return true;
}

// 데이터베이스 초기화 함수
void initializeDatabase(sql::Connection*& con, sql::mysql::MySQL_Driver*& driver) {
    try {
        driver = sql::mysql::get_mysql_driver_instance();
        con = driver->connect("tcp://127.0.0.1:3306", "root", "1234");
        con->setSchema("qr_data");
    }
    catch (sql::SQLException& e) {
        cerr << "SQL 에러 : " << e.what() << endl;
    }
}

// 컨투어 감지 및 화면에 표시하는 함수
void detectAndDisplayContours(Mat& img_contours, Mat& frame, Mat& gray, Mat& binary) {
    const int total_pixels = binary.rows * binary.cols;
    const double max_percent = 0.9;
    img_contours = frame.clone();
    const int pixel_size_cut_off = 25000;
    vector<vector<Point>> contours;
    cvtColor(frame, gray, COLOR_BGR2GRAY);
    threshold(gray, binary, 100, 255, THRESH_BINARY);
    Mat kernel = getStructuringElement(MORPH_RECT, Size(7, 7));
    morphologyEx(binary, binary, MORPH_CLOSE, kernel);

    findContours(binary, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    for (const auto& contour : contours) {
        double area = contourArea(contour);
        if (area > pixel_size_cut_off && area < (total_pixels * max_percent)) {
            drawContours(img_contours, contours, -1, Scalar(0, 0, 255), 2);
            putSizeText(img_contours, area);
        }
    }
}

// QR 코드 감지 함수
void detectQRCode(Mat& frame, QRCodeDetector& detector, set<string>& detected_QRs, set<string>& printed_QRs, sql::Connection* con, sql::PreparedStatement*& pstmt) {
    const string print_table_1 = "sender_information";
    const string print_table_2 = "recipient_information";
    const string print_table_3 = "recipient_address";
    const double thickness = 2;
    String info = detector.detectAndDecode(frame);
    if (!info.empty() && detected_QRs.find(info) == detected_QRs.end()) {
        detected_QRs.insert(info);
        Beep(750, 1000);

        if (printed_QRs.find(info) == printed_QRs.end()) {
            printed_QRs.insert(info);
            pstmt = con->prepareStatement("SELECT * FROM qr WHERE qr_code_id = ?");
            pstmt->setString(1, info);
            sql::ResultSet* res = pstmt->executeQuery();
            if (res->next()) {
                string sender_info = res->getString(print_table_1);
                string recipient_info = res->getString(print_table_2);
                string recipient_add = res->getString(print_table_3);

                wstring_convert<codecvt_utf8_utf16<wchar_t>> converter;
                wstring wide_sender_info = converter.from_bytes(sender_info);
                wstring wide_reci_info = converter.from_bytes(recipient_info);
                wstring wide_reci_add = converter.from_bytes(recipient_add);
                wcout.imbue(locale(""));
                wcout << L"/////////////////////////////////////////////////////" << endl;
                wcout << L"보내는 사람 : " << wide_sender_info << endl;
                wcout << L"받는 사람 : " << wide_reci_info << endl;
                wcout << L"주소 : " << wide_reci_add << endl;
                wcout << endl;
            }
            delete res;
        }
    }
}

// 템플릿 매칭 함수
void performTemplateMatching(Mat& gray, Mat& img_contours, vector<Point>& detected_objects,
    bool& display_text, time_point<steady_clock>& start_time) {
    vector<string> image_paths = {
        "as1.png", "as1_1.png", "as2.png", "as2_1.png", "as3.png", "as3_1.png"
    };

    vector<cv::Mat> images;

    for (const auto& path : image_paths) {
        cv::Mat frame_temp = cv::imread(path, cv::IMREAD_GRAYSCALE);
        if (frame_temp.empty()) {
            std::cerr << "이미지 파일을 읽을 수 없습니다: " << path << std::endl;
            continue;
        }
        images.push_back(frame_temp);
    }

    Mat result;

    for (size_t i = 0; i < images.size(); i++) {
        matchTemplate(gray, images[i], result, TM_CCOEFF_NORMED);

        int temp_w = images[i].cols;
        int temp_h = images[i].rows;

        for (int x = 0; x < result.cols; x++) {
            for (int y = 0; y < result.rows; y++) {
                if (result.at<float>(y, x) >= 0.65 && notInList(Point(x, y))) {
                    detected_objects.push_back(Point(x, y));
                    rectangle(img_contours, Point(x, y), Point(x + temp_w, y + temp_h), Scalar(255, 0, 255), 3);
                    display_text = true;
                    start_time = steady_clock::now();
                }
            }
        }
    }
}

// 경고 텍스트 표시 함수
void showWarning(Mat& img_contours, bool& display_text, time_point<steady_clock>& start_time) {
    string text = "danger!";
    const int x = img_contours.cols - 200;
    const int y = 30;
    if (display_text) {
        auto current_time = steady_clock::now();
        auto elapsed_seconds = duration_cast<seconds>(current_time - start_time).count();

        if (elapsed_seconds < 1) {
            putText(img_contours, text, Point(x, y), FONT_HERSHEY_COMPLEX, 1.0, Scalar(0, 0, 255), 2);
        }
        else {
            display_text = false;
        }
    }
}

// 영상 처리 메인 함수
void processVideo() {
    utils::logging::setLogLevel(utils::logging::LOG_LEVEL_ERROR);
    const int ESC = 27;
    const int set_width_value = 640;
    const int set_height_value = 480;
    QRCodeDetector detector;
    set<string> detected_QRs;
    set<string> printed_QRs;
    sql::mysql::MySQL_Driver* driver;
    sql::Connection* con;
    sql::PreparedStatement* pstmt;

    VideoCapture cap(0);
    if (!cap.isOpened()) {
        cerr << "Camera open failed!" << endl;
        return;
    }

    // 데이터베이스 초기화
    initializeDatabase(con, driver);

    auto start_time = steady_clock::now();
    bool display_text = false;

    Mat frame, gray, binary, img_contours;
    while (true) {
        cap >> frame;
        if (frame.empty()) {
            cerr << "Frame load failed!" << endl;
            break;
        }

        detectAndDisplayContours(img_contours, frame, gray, binary, detected_objects);
        detectQRCode(frame, detector, detected_QRs, printed_QRs, con, pstmt);

        Mat img_contour;
        // 플래그를 전달하여 템플릿 매칭 수행
        performTemplateMatching(gray, img_contours, detected_objects, display_text, start_time);
        showWarning(img_contours, display_text, start_time);

        imshow("Binary Image", binary);
        imshow("Detected Contours", img_contours);
        if (waitKey(1) == ESC) {
            break;
        }
    }

    cap.release();
    destroyAllWindows();
    delete pstmt;
    delete con;
}

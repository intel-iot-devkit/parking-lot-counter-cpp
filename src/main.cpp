/*
* Copyright (c) 2018 Intel Corporation.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
* NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
* LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
* OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
* WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

// std includes
#include <iostream>
#include <stdio.h>
#include <thread>
#include <queue>
#include <map>
#include <set>
#include <atomic>
#include <csignal>
#include <ctime>
#include <mutex>
#include <string>
#include <syslog.h>
#include <math.h>
#include <float.h>

// OpenCV includes
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/dnn.hpp>

// MQTT
#include "mqtt.h"

using namespace std;
using namespace cv;
using namespace dnn;

// OpenCV-related variables
Mat frame, blob;
VideoCapture cap;
int delay = 5;
Net net;

// application parameters
String model;
String config;
float carconf;
int backendId;
int targetId;
string entrance;
int max_distance;
int max_frames_gone;
int rate;

// flag to control background threads
atomic<bool> keepRunning(true);

// flag to handle UNIX signals
static volatile sig_atomic_t sig_caught = 0;

// mqtt parameters
const string topic = "parking/counter";

// Car contains information about trajectory of tracked car
struct Car {
    int id;
    vector<Point> traject;
    bool counted;
    bool gone;
    int direction;
};
// tracked_cars tracks detected cars by their ids
map<int, Car> tracked_cars;

// id is a counter used to generate ids for tracked centroids
int id = 0;

// Centroid is the center point of detected car rectangle
struct Centroid {
    int id;
    Point p;
    int gone_count;
};
// centroids maps centroids by their ids
map<int, Centroid> centroids;

// total cars in and out of the parking
int total_in = 0;
int total_out = 0;

// ParkingInfo contains information about available parking spaces
struct ParkingInfo
{
    int total_in;
    int total_out;
    map<int, Centroid> centroids;
};
// currentInfo contains the latest ParkingInfo as tracked by the application
ParkingInfo currentInfo;

// nextImage provides queue for captured video frames
queue<Mat> nextImage;
// currentPerf stores the label which contains application performance information
String currentPerf;
// mutexes used in program to control thread access to shared variables
mutex m, m1, m2;

const char* keys =
    "{ help     | | Print help message. }"
    "{ device d    | 0 | camera device number. }"
    "{ input i     | | Path to input image or video file. Skip this argument to capture frames from a camera. }"
    "{ model m     | | Path to .bin file of model containing face recognizer. }"
    "{ config c    | | Path to .xml file of model containing network configuration. }"
    "{ carconf cc  | 0.5 | Confidence factor for car detection required. }"
    "{ backend b   | 0 | Choose one of computation backends: "
                        "0: automatically (by default), "
                        "1: Halide language (http://halide-lang.org/), "
                        "2: Intel's Deep Learning Inference Engine (https://software.intel.com/openvino-toolkit), "
                        "3: OpenCV implementation }"
    "{ target t    | 0 | Choose one of target computation devices: "
                        "0: CPU target (by default), "
                        "1: OpenCL, "
                        "2: OpenCL fp16 (half-float precision), "
                        "3: VPU }"
    "{ entrance e  | b | Plane axis for parking  entrance and exit division mark. Possible options: "
                        "b: Bottom frame, "
                        "t: Top frame, "
                        "l: Left frame, "
                        "r: Right frame }"
    "{ max_distance md  | 200 | Max distance in pixels between two related centroids. }"
    "{ max_frames_gone mg | 25 | Max number of frames to track the centroid which doesnt change. }"
    "{ rate r      | 1 | Number of seconds between data updates to MQTT server. }";

// nextImageAvailable returns the next image from the queue in a thread-safe way
Mat nextImageAvailable() {
    Mat rtn;
    m.lock();
    if (!nextImage.empty()) {
        rtn = nextImage.front();
        nextImage.pop();
    }
    m.unlock();

    return rtn;
}

// addImage adds an image to the queue in a thread-safe way
void addImage(Mat img) {
    m.lock();
    if (nextImage.size() < 300) {
        nextImage.push(img);
    }
    m.unlock();
}

// getCurrentInfo returns the most-recent ParkingInfo for the application.
ParkingInfo getCurrentInfo() {
    m2.lock();
    ParkingInfo info;
    info = currentInfo;
    m2.unlock();

    return info;
}

// updateInfo uppdates the current ParkingInfo for the application to the latest detected values
//void updateInfo(vector<Rect> detections) {
void updateInfo() {
    m2.lock();
    currentInfo.total_in = total_in;
    currentInfo.total_out = total_out;
    currentInfo.centroids = centroids;
    m2.unlock();
}

// resetInfo resets the current ParkingInfo for the application.
void resetInfo() {
    m2.lock();
    currentInfo.total_in = 0;
    currentInfo.total_out = 0;
    currentInfo.centroids = map<int, Centroid>();
    m2.unlock();
}

// getCurrentPerf returns a display string with the most current performance stats for the Inference Engine.
string getCurrentPerf() {
    string perf;
    m1.lock();
    perf = currentPerf;
    m1.unlock();

    return perf;
}

// savePerformanceInfo sets the display string with the most current performance stats for the Inference Engine.
void savePerformanceInfo() {
    m1.lock();

    vector<double> times;
    double freq = getTickFrequency() / 1000;
    double t = net.getPerfProfile(times) / freq;

    string label = format("Car inference time: %.2f ms", t);

    currentPerf = label;

    m1.unlock();
}

// publish MQTT message with a JSON payload
void publishMQTTMessage(const string& topic, const ParkingInfo& info) {
    ostringstream s;
    s << "{\"TOTAL_IN\": \"" << info.total_in << "\" \"TOTAL_OUT\": \"" << info.total_out << "\"}";
    string payload = s.str();

    mqtt_publish(topic, payload);

    string msg = "MQTT message published to topic: " + topic;
    syslog(LOG_INFO, "%s", msg.c_str());
    syslog(LOG_INFO, "%s", payload.c_str());
}

// message handler for the MQTT subscription for the any desired control channel topic
int handleMQTTControlMessages(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    string topic = topicName;
    string msg = "MQTT message received: " + topic;
    syslog(LOG_INFO, "%s", msg.c_str());

    return 1;
}

// closestCentroid finds the id of the tracked centroid which is the closest to the point passed in as parameter.
// The function uses euclidean distance as a measure of the closeness of the points and returns both as a pair
pair<int, double> closestCentroid(const Point p, const map<int, Centroid> centroids) {
    int id = 0;
    double dist = DBL_MAX;

    for (map<int, Centroid>::const_iterator it = centroids.begin(); it != centroids.end(); ++it) {
         int _id = it->second.id;
         Point _p = it->second.p;

         // If the movement is horizontal, only consider centroids with some small Y coordinate fluctuation
         if (entrance.compare("l") == 0 || entrance.compare("r") == 0) {
             if ((_p.y < (p.y-70)) || (_p.y > (p.y+70))){
                 continue;
             }
         }

         // If the movement is vertical, only consider centroids with some small X coordinate fluctuation
         if (entrance.compare("b") == 0 || entrance.compare("t") == 0) {
             if (_p.x < (p.x-50) || _p.x > (p.x+50)){
                 continue;
             }
         }

         double dx = double(p.x - _p.x);
         double dy = double(p.y - _p.y);
         double _dist = sqrt(dx*dx + dy*dy);

         if (_dist < dist) {
             dist = _dist;
             id = _id;
         }
    }

    return make_pair(id, dist);
}

// addCentroid adds a new centroid to the list of tracked centroids and increments id counter
void addCentroid(Point p) {
    Centroid c;
    c.id = id;
    c.p = p;
    c.gone_count = 0;

    centroids[c.id] = c;
    id++;
}

// removeCentroid removes existing centroid from the list of tracked centroids
void removeCentroid(int id) {
    centroids.erase(id);
    // find gone centroid id in tracked cars map and mark it as gone
    if (tracked_cars.find(id) != tracked_cars.end()){
        tracked_cars[id].gone = true;
    }
}

// updateCentroids takes detected centroid points and updates tracked centroids
void updateCentroids(vector<Point> points) {
    if (points.size() == 0) {
        for (map<int, Centroid>::iterator it = centroids.begin(); it != centroids.end(); ++it) {
            it->second.gone_count++;
            if (it->second.gone_count > max_frames_gone) {
                removeCentroid(it->second.id);
            }
        }

        return;
    }

    if (centroids.empty()) {
        for(const auto& p: points) {
            addCentroid(p);
        }
    } else {
        set<int> checked_points;
        set<int> checked_centroids;
        // iterate through all detected points and update tracked centroid positions
        for(vector<Point>::size_type i = 0; i != points.size(); i++) {
            pair<int, double> closest = closestCentroid(points[i], centroids);

            // if the distance from the point to the closest centroid is too large, don't associate them together
            // also avoid associating with centroid which already has a different association
            if (closest.second > max_distance || (checked_points.find(closest.first) != checked_points.end())) {
                continue;
            }
            // update position of the closest centroid
            centroids[closest.first].p = points[i];
            centroids[closest.first].gone_count = 0;
            // add centroids to checked points
            checked_points.insert(i);
            checked_centroids.insert(closest.first);
        }

        // iterate through all *already tracked* centroids and increment their gone frame count
        // if they weren't updated from the list of detected centroid points
        for (map<int, Centroid>::iterator it = centroids.begin(); it != centroids.end(); ++it) {
            // if centroid wasn't updated - we assume it's missing from the frame
            if (checked_centroids.find(it->second.id) == checked_centroids.end()) {
                it->second.gone_count++;
                if (it->second.gone_count > max_frames_gone) {
                    removeCentroid(it->second.id);
                }
            }
        }

        // iterate through *detected* centroids and add the ones which werent associated
        // with any of the tracked centroids and add start tracking them
        for(vector<Point>::size_type i = 0; i != points.size(); i++) {
            // if detected point was not associated with any already tracked centroids we add it in
            if (checked_points.find(i) == checked_points.end()) {
                addCentroid(points[i]);
            }
        }
    }

    return;
}

// carMovement calculates movement of the car along particular movement axis according to the entrance position
// as a mean value of all the previous poisitions of the car centroids and returns it
int carMovement(vector<Point> traject, string entrance) {
    int mean_movement = 0;

    for(vector<Point>::size_type i = 0; i != traject.size(); i++) {
        // when movement is horizontal only consider trajectory along X axis
        if (entrance.compare("l") == 0 || entrance.compare("r") == 0) {
            mean_movement = mean_movement + traject[i].x;
        }
        // when movement is vertical only consider trajectory along Y axis
        if (entrance.compare("b") == 0 || entrance.compare("t") == 0) {
            mean_movement = mean_movement + traject[i].y;
        }
    }

    // calculate average centroid movement
    mean_movement = mean_movement / traject.size();

    return mean_movement;
}

// carDirection calculates the direction of the car movement along particular movement axis based on
// the entrance position as a difference between current car's position and its previous movement
int carDirection(Point p, int movement, string entrance) {
    int direction = 0;

    // when movement is horizontal only consider trajectory along X axis
    if (entrance.compare("l") == 0 || entrance.compare("r") == 0) {
       direction = p.x - movement;
    }
    // when movement is vertical only consider trajectory along Y axis
    if (entrance.compare("b") == 0 || entrance.compare("t") == 0) {
        direction = p.y - movement;
    }

    return direction;
}

// centroids2Cars iterates through all centroids and associates them with tracked_cars
void centroids2Cars() {
    // iterate through updated centroids and update tracked car counts
    for (map<int, Centroid>::iterator it = centroids.begin(); it != centroids.end(); ++it) {
        int id = it->second.id;
        Point p = it->second.p;

        Car car;
        // if the centroid is not tracked yet, add it to tracked_cars
        if (tracked_cars.find(id) == tracked_cars.end()) {
            car.id = id;
            car.traject.push_back(p);
            car.counted = false;
            car.gone = false;
            car.direction = 0;
        } else {
            car = tracked_cars[id];
            // calculate mean movement from car trajectory
            int movement = carMovement(car.traject, entrance);
            // add the centroid to the car trajectory
            car.traject.push_back(p);
            // calculate car direction based on trajectory and current position
            car.direction = carDirection(p, movement, entrance);
        }
        tracked_cars[id] = car;
    }
}

// updateCarTotals iterates through all tracked cars and updates total counts both in and out of the parking
void updateCarTotals() {
    for (map<int, Car>::iterator it = tracked_cars.begin(); it != tracked_cars.end(); ++it) {
        int id        = it->second.id;
        int direction = it->second.direction;
        bool gone     = it->second.gone;
        bool counted  = it->second.counted;

        if (!counted) {
            if (!gone){
                if (entrance.compare("t") == 0 || entrance.compare("l") == 0) {
                    // direction is "positive" i.e. movement along Y/X axis goes up
                    if (direction > 0) {
                        total_in++;
                        it->second.counted = true;
                    }
                }

                if (entrance.compare("b") == 0 || entrance.compare("r") == 0) {
                    // direction is "negative" i.e. movement along Y/X axis goes down
                    if (direction < 0) {
                        total_in++;
                        it->second.counted = true;
                    }
                }
            } else {
                if (entrance.compare("t") == 0 || entrance.compare("l") == 0) {
                    // "negative" direction i.e. car centroid coords along movement axis go down
                    if (direction < 0) {
                        total_out++;
                        tracked_cars.erase(id);
                    }
                }

                if (entrance.compare("b") == 0 || entrance.compare("r") == 0) {
                    // "positive" direction i.e. car centroid coords along movement axis go up
                    if (direction > 0) {
                        total_out++;
                        tracked_cars.erase(id);
                    }
                }
            }
        } else {
            if (gone) {
                tracked_cars.erase(id);
            }
        }
    }
}

// Function called by worker thread to process the next available video frame.
void frameRunner() {
    while (keepRunning.load()) {
        Mat next = nextImageAvailable();
        if (!next.empty()) {
            // convert to 4d vector as required by vehicle detection model and detect cars
            blobFromImage(next, blob, 1.0, Size(672, 384));
            net.setInput(blob);
            Mat result = net.forward();

            // get detected cars and in and out counts
            vector<Rect> frame_cars;
            int count_in = 0;
            int count_out = 0;
            float* data = (float*)result.data;

            for (size_t i = 0; i < result.total(); i += 7)
            {
                int label = (int)data[i + 1];
                float confidence = data[i + 2];
                if (label == 1 && confidence > carconf)
                {
                    int left = (int)(data[i + 3] * frame.cols);
                    int top = (int)(data[i + 4] * frame.rows);
                    int right = (int)(data[i + 5] * frame.cols);
                    int bottom = (int)(data[i + 6] * frame.rows);
                    int width = right - left + 1;
                    int height = bottom - top + 1;

                    frame_cars.push_back(Rect(left, top, width, height));
                }
            }

            vector<Point> frame_centroids;
            vector<Rect>  car_detections;
            for(auto const& fc: frame_cars) {
                // make sure the car rect is completely inside the main Mat
                if ((fc & Rect(0, 0, next.cols, next.rows)) != fc) {
                    continue;
                }

                // detected car rectangle dimensions
                int width = fc.width;
                int height = fc.height;
                // if detected rectangle is too small, skip it
                if (width < 70 || height < 70) {
                    continue;
                }

                // Sometimes detected car rectangle stretches way over the actual car dimensions
                // so we clip the sizes of the rectangle to avoid skewing the centroid positions
                int w_clip = 200;
                if (width > w_clip) {
                    if ((fc.x + w_clip) < frame.cols) {
                        width = w_clip;
                    }
                } else if ((fc.x + width) > frame.cols) {
                    width = frame.cols - fc.x;
                }

                int h_clip = 350;
                if (height > h_clip) {
                    if ((fc.y + h_clip) < frame.rows){
                        height = h_clip;
                    }
                } else if ((fc.y + height) > frame.rows) {
                    height = frame.rows - fc.y;
                }

                // calculate detected car centroid coordinates
                int x = fc.x + static_cast<int>(width/2.0);
                int y = fc.y + static_cast<int>(height/2.0);

                // append detected centroid and draw rectangle
                frame_centroids.push_back(Point(x,y));
                car_detections.push_back(Rect(fc.x, fc.y, width, height));
            }

            // update tracked centroids using the centroids detected in the frame
            updateCentroids(frame_centroids);

            // associate centroids with tracked cars
            centroids2Cars();
            // update tracked cars total counters
            updateCarTotals();
            // update analytics and performance info
            updateInfo();
            savePerformanceInfo();
        }
    }

    cout << "Video processing thread stopped" << endl;
}

// Function called by worker thread to handle MQTT updates. Pauses for rate second(s) between updates.
void messageRunner() {
    while (keepRunning.load()) {
        ParkingInfo info = getCurrentInfo();
        publishMQTTMessage(topic, info);
        this_thread::sleep_for(chrono::seconds(rate));
    }

    cout << "MQTT sender thread stopped" << endl;
}

// signal handler for the main thread
void handle_sigterm(int signum)
{
    /* we only handle SIGTERM and SIGKILL here */
    if (signum == SIGTERM) {
        cout << "Interrupt signal (" << signum << ") received" << endl;
        sig_caught = 1;
    }
}

int main(int argc, char** argv)
{
    // parse command parameters
    CommandLineParser parser(argc, argv, keys);
    parser.about("Use this script to using OpenVINO.");
    if (argc == 1 || parser.has("help"))
    {
        parser.printMessage();

        return 0;
    }

    model = parser.get<String>("model");
    config = parser.get<String>("config");
    carconf = parser.get<float>("carconf");
    backendId = parser.get<int>("backend");
    targetId = parser.get<int>("target");
    entrance = parser.get<string>("entrance");
    rate = parser.get<int>("rate");
    max_distance = parser.get<int>("max_distance");
    max_frames_gone = parser.get<int>("max_frames_gone");

    // connect MQTT messaging
    int result = mqtt_start(handleMQTTControlMessages);
    if (result == 0) {
        syslog(LOG_INFO, "MQTT started.");
    } else {
        syslog(LOG_INFO, "MQTT NOT started: have you set the ENV varables?");
    }

    mqtt_connect();

    // read in car detection model
    net = readNet(model, config);
    net.setPreferableBackend(backendId);
    net.setPreferableTarget(targetId);

    // open video capture source
    if (parser.has("input")) {
        cap.open(parser.get<String>("input"));

        // also adjust delay so video playback matches the number of FPS in the file
        double fps = cap.get(CAP_PROP_FPS);
        delay = 1000/fps;
    }
    else
        cap.open(parser.get<int>("device"));

    if (!cap.isOpened()) {
        cerr << "ERROR! Unable to open video source\n";
        return -1;
    }

    // register SIGTERM signal handler
    signal(SIGTERM, handle_sigterm);

    // start worker threads
    thread t1(frameRunner);
    thread t2(messageRunner);

    // read video input data
    for (;;) {
        cap.read(frame);

        if (frame.empty()) {
            keepRunning = false;
            cerr << "ERROR! blank frame grabbed\n";
            break;
        }

        addImage(frame);

        // print Inference Engine performance info
        string label = getCurrentPerf();
        putText(frame, label, Point(0, 25), FONT_HERSHEY_SIMPLEX, 0.5, CV_RGB(255, 255, 255));

        ParkingInfo info = getCurrentInfo();
        label = format("Cars In: %d Cars Out: %d", info.total_in, info.total_out);
        putText(frame, label, Point(0, 45), FONT_HERSHEY_SIMPLEX, 0.5, CV_RGB(255, 255, 255));
        // draw car centroids
        for (map<int, Centroid>::const_iterator it = info.centroids.begin(); it != info.centroids.end(); ++it) {
            circle(frame, it->second.p, 5.0, CV_RGB(0, 255, 0), 2);
            label = format("[%d, %d]", it->second.p.x, it->second.p.y);
            putText(frame, label, Point(it->second.p.x+5, it->second.p.y),
                            FONT_HERSHEY_SIMPLEX, 0.5, CV_RGB(0, 255, 0));
        }

        imshow("Parking Lot Counter", frame);

        if (waitKey(delay) >= 27 || sig_caught) {
            cout << "Attempting to stop background threads" << endl;
            keepRunning = false;
            break;
        }
    }

    // wait for the threads to finish
    t1.join();
    t2.join();
    cap.release();

    // disconnect MQTT messaging
    mqtt_disconnect();
    mqtt_close();

    return 0;
}

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
string axis;
int bline;
int spaces;
int rate;

// flag to control background threads
atomic<bool> keepRunning(true);

// flag to handle UNIX signals
static volatile sig_atomic_t sig_caught = 0;

// mqtt parameters
const string topic = "parking/counter";

// Car contains information about the trajectory of the car centroid
struct Car {
    int id;
    vector<Point> traject;
    bool counted;
};
// tracked_cars tracks detected car by their ids
map<int, Car> tracked_cars;

// id is a counter used to generate ids for tracked (car) centroids
int id = 0;
// centroids maps car ids to detected centroids
map<int, Point> centroids;
// gone tracks number of frames during which previously tracked car is not detected
map<int, int> gone;
// max_frames_gone and max_distance are thresholds used when marking car as gone
int max_frames_gone = 30;
int max_distance = 50;

// total cars in and out of the parking
int total_in = 0;
int total_out = 0;

// ParkingInfo contains information about available parking spaces
struct ParkingInfo
{
    int count;
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
    "{ axis a      | x | Plane axis for entrance division mark. }"
    "{ bline l     | 0 | Marks a boundary line for the chosen axis. }"
    "{ spaces s    | 1000 | Number of available parking spaces. }"
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
    if (nextImage.empty()) {
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
void updateInfo(int in, int out) {
    m2.lock();
    // TODO: do something clever with out and in and count
    m2.unlock();
}

// resetInfo resets the current ParkingInfo for the application.
void resetInfo() {
    m2.lock();
    currentInfo.count = 0;
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
void publishMQTTMessage(const string& topic, const ParkingInfo& info)
{
    ostringstream s;
    s << "{\"Count\": \"" << info.count << "\"}";
    string payload = s.str();

    mqtt_publish(topic, payload);

    string msg = "MQTT message published to topic: " + topic;
    syslog(LOG_INFO, "%s", msg.c_str());
    syslog(LOG_INFO, "%s", payload.c_str());
}

// message handler for the MQTT subscription for the any desired control channel topic
int handleMQTTControlMessages(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    string topic = topicName;
    string msg = "MQTT message received: " + topic;
    syslog(LOG_INFO, "%s", msg.c_str());

    return 1;
}

// closest_centroid finds the id of the tracked centroid which is the closest to the supplied centroid
// the function uses euclidean distance as a measure of the closeness of the centroids and returns both as a pair
pair<int, double> closest_centroid(const Point p, const map<int, Point> centroids) {
    int id = 0;
    double dist = DBL_MAX;

    for (const auto& c: centroids) {
         int _id = c.first;
         Point _p = c.second;

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
    centroids[id] = p;
    gone[id] = 0;
    id++;
}

// removeCentroid removes existing centroid from the list of tracked centroids
void removeCentroid(int id) {
    centroids.erase(id);
    gone.erase(id);
}

// updateCentroids takes detected centroid points and updates trackeed centroids
void updateCentroids(vector<Point> points) {
    if (points.size() == 0) {
        for (const auto& g : gone) {
            int id = g.first;
            gone[id]++;
            cout << "gone[" << id << "]++" << endl;
            if (gone[id] > max_frames_gone) {
                cout << "removeCentroid(" << id << ")" << endl;
                removeCentroid(id);
            }
        }

        return;
    }

    if (centroids.empty()){
        cout << "No centroids yet" << endl;
        for(const auto& p: points) {
            cout << "addCentroid(" << p << ")" << endl;
            addCentroid(p);
        }
    } else {
        set<int> checked_points;
        set<int> checked_centroids;
        // iterate through all detected points and update tracked centroid positions
        for(vector<Point>::size_type i = 0; i != points.size(); i++) {
            pair<int, double> closest = closest_centroid(points[i], centroids);
            cout << "Closest centroid: " << closest.first << endl;
            if ((checked_points.find(closest.first) != checked_points.end()) || (closest.second > max_distance)) {
                continue;
            }
            // update position of closest centroid
            centroids[closest.first] = points[i];
            gone[closest.second] = 0;
            checked_points.insert(i);
            checked_centroids.insert(closest.first);
        }

        // iterate through all tracked centroids and increment their gone frame count
        // if they werent updated from the list of detected centroid points
        for (auto& c: centroids) {
            int id = c.first;
            // id centroid wasn't updated - we assume it's missing from the frame
            if (checked_centroids.find(id) == checked_centroids.end()) {
                gone[id]++;
                if (gone[id] > max_frames_gone) {
                    removeCentroid(id);
                }
            }
        }

        // iterate through all detected centroids and add the ones which werent associated
        // with any of the tracked centroids and add start tracking them
        for(vector<Point>::size_type i = 0; i != points.size(); i++) {
            // detected point was not associated with any already tracked centroids
            if (checked_points.find(i) == checked_points.end()) {
                addCentroid(points[i]);
            }
        }
    }

    return;
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

            // get detected cars and in and out couts
            vector<Rect> frame_cars;
            int count_in = 0;
            int count_out = 0;
            float* data = (float*)result.data;

            for (size_t i = 0; i < result.total(); i += 7)
            {
                float confidence = data[i + 2];
                if (confidence > carconf)
                {
                    int label = (int)data[i + 1];
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
            for(auto const& c: frame_cars) {
                // make sure the car rect is completely inside the main Mat
                if ((c & Rect(0, 0, next.cols, next.rows)) != c) {
                    continue;
                }

                // detected rectangle dimensions
                int width = c.width;
                int height = c.height;

                // TODO: Sometimes the detected rectangle stretches way over the actual car
                // so we need clip the sizes of the rectangle to avoid skewing the centroid position
                int w_clip = 200;
                if (width > w_clip) {
                    if ((c.x + w_clip) < frame.cols) {
                        width = w_clip;
                    }
                } else if ((c.x + width) > frame.cols) {
                    width = frame.cols - c.x;
                }

                int h_clip = 350;
                if (height > h_clip) {
                    if ((c.y + h_clip) < frame.rows){
                        height = h_clip;
                    }
                } else if ((c.y + height) > frame.rows) {
                    height = frame.rows - c.y;
                }

                // calculate detected car centroid coordinates
                int x = c.x + static_cast<int>(width/2.0);
                int y = c.y + static_cast<int>(height/2.0);

                // append detected centroid and draw rectangle
                frame_centroids.push_back(Point(x,y));
                rectangle(next, Rect(c.x, c.y, width, height), CV_RGB(0, 255, 0), 2);
            }

            cout << "DETECTED CENTROIDS: " << frame_centroids.size() << endl;

            // update tracked centroids using the centroids detected in the frame
            updateCentroids(frame_centroids);

            //cout << "Centroid count: " << centroids.size() << " Gone count:" << gone.size() << endl;
            for (auto& c: centroids) {
                int id = c.first;
                Point p = c.second;

                Car car;
                if (tracked_cars.find(id) == tracked_cars.end()){
                    cout << "Never seen before: " << id << endl;
                    car.id = id;
                    car.traject.push_back(p);
                    car.counted = false;
                } else {
                    car = tracked_cars[id];
                    // calculate mean from car (centroid) trajectory
                    int mean_movement = 0;
                    for(vector<Point>::size_type i = 0; i != car.traject.size(); i++) {
                        if (axis.compare("x") == 0) {
                            mean_movement = mean_movement + car.traject[i].x;
                        }
                        if (axis.compare("y") == 0) {
                            mean_movement = mean_movement + car.traject[i].y;
                        }
                    }
                    mean_movement = mean_movement / car.traject.size();
                    car.traject.push_back(p);

                    int direction = 0;
                    if (axis.compare("x") == 0) {
                        direction = p.x - mean_movement;
                    }
                    if (axis.compare("y") == 0) {
                        direction = p.y - mean_movement;
                    }

                    cout << "DIRECTION: " << direction << endl;
                    cout << "CAR: " << car.id << " COUNTED: " << car.counted << endl;
                    if (!car.counted) {
                        if (axis.compare("x") == 0) {
                            // direction is "positive" (RIGHT) and centroid right of vertical boundary line
                            if (direction > 0 && p.x > bline) {
                                cout << "RIGHT INCREMENT" << endl;
                                total_in++;
                                car.counted = true;
                            } else if (direction < 0 && p.x < bline) {
                                       cout << "RIGHT INCREMENT" << endl;
                                       total_out++;
                                       car.counted = true;
                            } else {
                                cout << "CAR: " << car.id << " NOT COUNTED" << endl;
                            }
                        }
                        if (axis.compare("y") == 0) {
                            // direction is "negative" (UP) and centroid above horizontal boundary line
                            if (direction < 0 && p.y < bline) {
                                cout << "UP INCREMENT" << endl;
                                total_in++;
                                car.counted = true;
                            } else if (direction > 0 && p.y > bline) {
                                       cout << "DOWN INCREMENT" << endl;
                                       total_out++;
                                       car.counted = true;
                            } else {
                                cout << "CAR: " << car.id << " NOT COUNTED" << endl;
                            }
                        }
                    }
                }
                tracked_cars[id] = car;
                cout << "Updated car: " << car.id << " Positions: " << car.traject << endl;
                circle(next, p, 5.0, CV_RGB(0, 255, 0), 2);
            }

            updateInfo(total_in, total_out);
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
    axis = parser.get<string>("axis");
    bline = parser.get<int>("bline");
    rate = parser.get<int>("rate");
    spaces = parser.get<int>("spaces");

    // connect MQTT messaging
    int result = mqtt_start(handleMQTTControlMessages);
    if (result == 0) {
        syslog(LOG_INFO, "MQTT started.");
    } else {
        syslog(LOG_INFO, "MQTT NOT started: have you set the ENV varables?");
    }

    mqtt_connect();

    // open face model
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

    // Initialize parking space info
    currentInfo.count = spaces;

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

        if (axis.compare("x") == 0) {
            line(frame, Point(bline, 0), Point(bline, frame.rows), CV_RGB(255, 0, 0), 2);
        }

        if (axis.compare("y") == 0) {
            line(frame, Point(0, bline), Point(frame.cols, bline), CV_RGB(255, 0, 0), 2);
        }

        addImage(frame);

        // print Inference Engine performance info
        string label = getCurrentPerf();
        putText(frame, label, Point(0, 15), FONT_HERSHEY_SIMPLEX, 0.5, CV_RGB(0, 0, 255));

        ParkingInfo info = getCurrentInfo();
        label = format("Cars In: %d Cars Out: %d", total_in, total_out);
        putText(frame, label, Point(0, 40), FONT_HERSHEY_SIMPLEX, 0.5, CV_RGB(0, 0, 255));

        imshow("Parking Tracker", frame);

        if (waitKey(delay) >= 0 || sig_caught) {
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

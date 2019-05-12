# Parking Lot Counter

| Details            |              |
|-----------------------|---------------|
| Target OS:            |  Ubuntu\* 16.04 LTS   |
| Programming Language: |  C++ |
| Time to Complete:     |  45 min     |

![app image](./images/parking-lot-counter.png)

## Introduction

The parking lot counter is one of a series of reference implementations for Computer Vision (CV) using the Intel® Distribution of OpenVINO™ toolkit. It is designed for a parking space area with mounted camera which monitors available parking space by tracking the count of the vehicles entering and leaving the parking space area.

This example is intended to demonstrate how to use CV to monitor parking spaces in dedicated parking area.

## Requirements

### Hardware
* 6th to 8th Generation Intel® Core™ processors with Intel® Iris® Pro graphics or Intel® HD Graphics

### Software
* [Ubuntu\* 16.04 LTS](http://releases.ubuntu.com/16.04/)<br>
  **Note**: We recommend using a 4.14+ kernel to use this software. Run the following command to determine your kernel version:
  ```
    uname -a
  ```
  
* OpenCL™ Runtime Package
* Intel® Distribution of OpenVINO™ toolkit 2019 R1 Release

## Setup

### Install the Intel® Distribution of OpenVINO™ toolkit
Refer to https://software.intel.com/en-us/articles/OpenVINO-Install-Linux for more information about how to install and setup the Intel® Distribution of OpenVINO™ toolkit.

You will need the OpenCL™ Runtime package if you plan to run inference on the GPU as shown by the
instructions below. It is not mandatory for CPU inference.

## How it Works

The application uses a video source, such as a camera, to grab frames, and then uses a Deep Neural Network (DNNs) to process the data. The network detects vehicles in the frame and then if successful it tracks the vehicles leaving or entering the parking area and adjusts the count of the vehicles in the parking area for providing the information about the available parking spaces.

The data can then optionally be sent to a MQTT machine to machine messaging server, as part of a parking space data analytics system.

![Code organization](./images/arch3.png)

The program creates three threads for concurrency:

- Main thread that performs the video I/O
- Worker thread that processes video frames using DNNs
- Worker thread that publishes any MQTT messages


## Download the model
This application uses the **pedestrian-and-vehicle-detector-adas-0001** Intel® model, that can be downloaded using the model downloader. The model downloader downloads the .xml and .bin files that will be used by the application.

Steps to download .xml and .bin files:
- Go to the **model_downloader** directory using the following command:
    ```
    cd /opt/intel/openvino/deployment_tools/tools/model_downloader
    ```

- Specify which model to download with __--name__: 

    ```
    sudo ./downloader.py --name pedestrian-and-vehicle-detector-adas-0001
    ```
- To download the model for FP16, run the following command:
    ```
    sudo ./downloader.py --name pedestrian-and-vehicle-detector-adas-0001-fp16
    ```
The files will be downloaded inside `/opt/intel/openvino/deployment_tools/tools/model_downloader/Transportation/object_detection/pedestrian/mobilenet-reduced-ssd/dldt` directory.



## Setting the Build Environment

You must configure the environment to use the Intel® Distribution of OpenVINO™ toolkit one time per session by running the following command:
```
source /opt/intel/openvino/bin/setupvars.sh
```

## Build the application

Start by changing the current directory to wherever you have git cloned the application code. For example:
```
cd parking-lot-counter-cpp
```

Run the following commands to build the application:
```
mkdir -p build && cd build
cmake ..
make 
```

## Run the application

To see a list of the various options:
```
./monitor -help
```

To run the application with the required models using webcam, execute the below command:
```
./monitor -m=/opt/intel/openvino/deployment_tools/tools/model_downloader/Transportation/object_detection/pedestrian-and-vehicle/mobilenet-reduced-ssd/dldt/pedestrian-and-vehicle-detector-adas-0001.bin -c=/opt/intel/openvino/deployment_tools/tools/model_downloader/Transportation/object_detection/pedestrian-and-vehicle/mobilenet-reduced-ssd/dldt/pedestrian-and-vehicle-detector-adas-0001.xml
```

To control the position of the parking entrance/exit use the `-entrance, -e` command line flag. For example:
```
./monitor -m=/opt/intel/openvino/deployment_tools/tools/model_downloader/Transportation/object_detection/pedestrian-and-vehicle/mobilenet-reduced-ssd/dldt/pedestrian-and-vehicle-detector-adas-0001.bin -c=/opt/intel/openvino/deployment_tools/tools/model_downloader/Transportation/object_detection/pedestrian-and-vehicle/mobilenet-reduced-ssd/dldt/pedestrian-and-vehicle-detector-adas-0001.xml -e="b"
```

The `-entrance` flag controls which part of the video stream frame has to be used for counting the cars entering or exiting the parking lot:
* `"b"`: bottom
* `"l"`: left
* `"r"`: right
* `"t"`: top

To control the car detection DNN confidence level, use the `-carconf, -cc` flag. For example, `-carconf=0.6` will track all cars whose DNN detection confidence level is higher than `60%`.

The calculations made to track the movement of vehicles using centroids have two parameters that can be set via command line flags. `--max_distance` set the maximum distance in pixels between two related centroids. In other words, how big of a distance of movement between frames show be allowed before assuming that the object is a different vehicle. `--max_frames_gone` is the maximum number of frames to track a centroid which doesn't change, possibly due to being a parked vehicle.

### Hardware Acceleration

This application can take advantage of the hardware acceleration in the Intel® Distribution of OpenVINO™ toolkit by using the `-b` and `-t` parameters.

To run the application on the integrated Intel® GPU in 32-bit mode, use the below command:
```
./monitor -m=/opt/intel/openvino/deployment_tools/tools/model_downloader/Transportation/object_detection/pedestrian-and-vehicle/mobilenet-reduced-ssd/dldt/pedestrian-and-vehicle-detector-adas-0001.bin -c=/opt/intel/openvino/deployment_tools/tools/model_downloader/Transportation/object_detection/pedestrian-and-vehicle/mobilenet-reduced-ssd/dldt/pedestrian-and-vehicle-detector-adas-0001.xml -b=2 -t=1
```

To run the application on the integrated Intel® GPU in 16-bit mode, use the below command:
```
./monitor -m=/opt/intel/openvino/deployment_tools/tools/model_downloader/Transportation/object_detection/pedestrian-and-vehicle/mobilenet-reduced-ssd/dldt/pedestrian-and-vehicle-detector-adas-0001-fp16.bin -c=/opt/intel/openvino/deployment_tools/tools/model_downloader/Transportation/object_detection/pedestrian-and-vehicle/mobilenet-reduced-ssd/dldt/pedestrian-and-vehicle-detector-adas-0001-fp16.xml -b=2 -t=2
```

To run the application using the VPU, use the below command:
```
./monitor -m=/opt/intel/openvino/deployment_tools/tools/model_downloader/Transportation/object_detection/pedestrian-and-vehicle/mobilenet-reduced-ssd/dldt/pedestrian-and-vehicle-detector-adas-0001-fp16.bin -c=/opt/intel/openvino/deployment_tools/tools/model_downloader/Transportation/object_detection/pedestrian-and-vehicle/mobilenet-reduced-ssd/dldt/pedestrian-and-vehicle-detector-adas-0001-fp16.xml -b=2 -t=3
```

## Sample Videos

There are several videos available to use as sample videos to show the capabilities of this application. You can download them by running the following commands from the `parking-lot-counter-cpp` directory:
```
mkdir resources
cd resources
wget https://github.com/intel-iot-devkit/sample-videos/raw/master/car-detection.mp4
cd ..
```

To execute the application using the sample video, run the following commands from the `parking-lot-counter-cpp` directory:
```
cd build
./monitor -m=/opt/intel/openvino/deployment_tools/tools/model_downloader/Transportation/object_detection/pedestrian-and-vehicle/mobilenet-reduced-ssd/dldt/pedestrian-and-vehicle-detector-adas-0001.bin -c=/opt/intel/openvino/deployment_tools/tools/model_downloader/Transportation/object_detection/pedestrian-and-vehicle/mobilenet-reduced-ssd/dldt/pedestrian-and-vehicle-detector-adas-0001.xml -i=../resources/car-detection.mp4 -cc=0.65 -e="b"
```

The above command will use the bottom edge of the video stream frame as parking lot entrance and will count the cars driving up the frame as the cars entering the parking lot and the cars driving down the frame as the cars leaving the parking lot. The application displays in real time how many cars have entered and exited the parking lot.

### Machine to Machine Messaging with MQTT

#### Install Mosquitto Broker
```
sudo apt-get update
sudo apt-get install mosquitto mosquitto-clients
```

If you wish to use a MQTT server to publish data, you should set the following environment variables before running the program:
```
export MQTT_SERVER=localhost:1883
export MQTT_CLIENT_ID=cvservice
```

Change the `MQTT_SERVER` to a value that matches the MQTT server you are connecting to.

You should change the `MQTT_CLIENT_ID` to a unique value for each monitoring station, so you can track the data for individual locations. For example:
```
export MQTT_CLIENT_ID=parkinglot1337
```

If you want to monitor the MQTT messages sent to your local server, and you have the mosquitto client utilities installed, you can run the following command on a new terminal while the application is running:
```
mosquitto_sub -t 'parking/counter'
```

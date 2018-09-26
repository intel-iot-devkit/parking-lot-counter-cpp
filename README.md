# Available Parking Tracker

| Details            |              |
|-----------------------|---------------|
| Target OS:            |  Ubuntu\* 16.04 LTS   |
| Programming Language: |  C++\* |
| Time to Complete:     |  45 min     |

![app image](./images/parking-space-monitor.png)

## Introduction

This parking space monitor is one of a series of reference implementations for Computer Vision (CV) using the OpenVINO™ toolkit. This application is designed for a parking space area mounted camera which monitors available parking space by tracking the counts of the vehicles entering and leaving the parking space area.

This example is intended to demonstrate how to use CV to monitor parking spaces in dedicated parking area.

## Requirements

### Hardware
* 6th Generation Intel® Core™ processor with Intel® Iris® Pro graphics and Intel® HD Graphics

### Software
* [Ubuntu\* 16.04 LTS](http://releases.ubuntu.com/16.04/)
*Note*: You must be running kernel version 4.7+ to use this software. We recommend using a 4.14+ kernel to use this software. Run the following command to determine your kernel version:
```
uname -a
```
* OpenCL™ Runtime Package
* OpenVINO™ toolkit

## Setup

### Install OpenVINO™ Toolkit
Refer to https://software.intel.com/en-us/articles/OpenVINO-Install-Linux for more information about how to install and setup the OpenVINO™ toolkit.

You will need the OpenCL™ Runtime package if you plan to run inference on the GPU as shown by the
instructions below. It is not mandatory for CPU inference.

## How it works

The application uses a video source, such as a camera, to grab frames, and then uses a Deep Neural Network (DNNs) to process the data. The network detects vehicles in the frame, and then if successful it tracks the vehicles leaving and entering the parking area adjusting the counts of the vehicles in the parking area thus providing the information about the available parking spaces.

The data can then optionally be sent to a MQTT machine to machine messaging server, as part of a parking space data analytics system.

The DNN models used in this application are Intel® optimized models that are part of the OpenVINO™ toolkit.

You can find them here:

- `/opt/intel/computer_vision_sdk/deployment_tools/intel_models/vehicle-detection-adas-0002
`

![Code organization](./images/arch3.png)

The program creates three threads for concurrency:

- main thread that performs the video i/o
- worker thread that processes video frames using the deep neural networks
- worker thread that publishes any MQTT messages

## Setting the build environment

You must configure the environment to use the OpenVINO™ toolkit one time per session by running the following command:
```
    source /opt/intel/computer_vision_sdk/bin/setupvars.sh
```

## Building the code

Start by changing the current directory to wherever you have git cloned the application code. For example:
```
    cd available-parking-tracker-cpp
```

If you do not yet have a `build` directory create one:
```
    mkdir build
```

Then change to that directory:
```
    cd build
```

Now run the following commands:
```
    cmake ..
    make
```

Once the commands are finished, you should have built the `monitor` application executable.

## Running the code

To see a list of the various options:
```
    ./monitor -help
```

To run the application with the needed models using the webcam:
```
    ./monitor -m=/opt/intel/computer_vision_sdk/deployment_tools/intel_models/vehicle-detection-adas-0002/FP32/vehicle-detection-adas-0002.bin -c=/opt/intel/computer_vision_sdk/deployment_tools/intel_models/vehicle-detection-adas-0002/FP32/vehicle-detection-adas-0002.xml
```

To control the monitoring line to be used for counting of entry/exit from the parking area, use the `-axis` and `-bline` flags, like this:
```
    ./monitor -m=/opt/intel/computer_vision_sdk/deployment_tools/intel_models/vehicle-detection-adas-0002/FP32/vehicle-detection-adas-0002.bin -c=/opt/intel/computer_vision_sdk/deployment_tools/intel_models/vehicle-detection-adas-0002/FP32/vehicle-detection-adas-0002.xml -axis=y -bline=350
```

The `-axis` flag controls which axis is to be used for counting, either `"x"` for counting going left/right, or `"y"` for counting up/down.

The  `-bline` flag controls the baseline positioning. For example `-bline=100` will place the baseline 100 pixels counting from either the left side if `-axis=x` or the top if `-axis=y`.

### Hardware acceleration

This application can take advantage of the hardware acceleration in the OpenVINO toolkit by using the `-b` and `-t` parameters.

For example, to use the OpenVINO™ toolkit backend with the GPU in 32-bit mode:
```
    ./monitor -m=/opt/intel/computer_vision_sdk/deployment_tools/intel_models/vehicle-detection-adas-0002/FP32/vehicle-detection-adas-0002.bin -c=/opt/intel/computer_vision_sdk/deployment_tools/intel_models/vehicle-detection-adas-0002/FP32/vehicle-detection-adas-0002.xml -b=2 -t=1
```

To run the code using 16-bit floats, you have to both set the `-t` flag to use the GPU in 16-bit mode, as well as use the FP16 version of the Intel® models:
```
    ./monitor -m=/opt/intel/computer_vision_sdk/deployment_tools/intel_models/vehicle-detection-adas-0002/FP16/vehicle-detection-adas-0002.bin -c=/opt/intel/computer_vision_sdk/deployment_tools/intel_models/vehicle-detection-adas-0002/FP16/vehicle-detection-adas-0002.xml -b=2 -t=2
```

## Sample videos

There are several videos available to use as sample videos to show the capabilities of this application. You can download them by running these commands from the `available-parking-tracker-cpp` directory:
```
    mkdir resources
    cd resources
    wget https://github.com/intel-iot-devkit/sample-videos/raw/master/car-detection.mp4
    cd ..
```

To then execute the code using one of these sample videos, run the following commands from the `available-parking-tracker-cpp` directory:
```
    cd build
    ./monitor -m=/opt/intel/computer_vision_sdk/deployment_tools/intel_models/vehicle-detection-adas-0002/FP32/vehicle-detection-adas-0002.bin -c=/opt/intel/computer_vision_sdk/deployment_tools/intel_models/vehicle-detection-adas-0002/FP32/vehicle-detection-adas-0002.xml -i=../resources/car-detection.mp4
```

### Machine to machine messaging with MQTT

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

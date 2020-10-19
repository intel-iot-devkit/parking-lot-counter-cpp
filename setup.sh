: '
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
'

sudo apt-get update
sudo apt-get install mosquitto mosquitto-clients
sudo apt-get install libssl-dev
sudo ssh install cmd

if [ -d "json" ]
then
  sudo rm -r json
fi

git clone https://github.com/nlohmann/json       # Cloning json parser from github

mkdir resources
cd resources
wget -O car-detection.mp4 https://github.com/intel-iot-devkit/sample-videos/raw/master/car-detection.mp4

cd /opt/intel/openvino/deployment_tools/tools/model_downloader
sudo ./downloader.py --name pedestrian-and-vehicle-detector-adas-0001

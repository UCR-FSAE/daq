# daq  
## What are we doing?  
We want to receive live vehicle telemetry over LoRa (long range radio frequency) and visualize it in Foxglove, a platform popular in robotics and autonomous vehicles to view sensor data.  

For example, we want to view telemetry like inverter temperature or car speed in real-time without being physically connected to the car. This is important as it ensures pit crews are prepared to fix issues before the car returns.  

But what exactly is the whole step-by-step process that allows us to go from the data being recorded by the peripherals, e.g. inverter, sensors, etc, to Foxglove?   
1. A peripheral sends a Controller Area Network (CAN) frame. For example, this might be the inverter sending a CAN frame which contains the motor temperature among other stuff.  
2. The DAQ Teensy microcontroller reads these frames and formats them for later parsing.  
3. A Universal Asynchronous Receiver/Transmitter (UART) serial connection connects the Teensy TX to the LoRa transmitter RX.  
4. The LoRa transmitter RX sends telemetry over its radio link to its receiver.  
5. You can link your laptop to this receiver by USB, and your laptop's operating system will expose a serial port, e.g. Windows COM port or macOS serial device. If you're on Windows, you will likely need to install a driver to communicate with the USB interface used by the receiver. For macOS, it often recognizes common serial USB devices automatically, so a separate driver is likely not necessary.
7. If you have `lora_bridge.py` running on your laptop, this python script can read the serial telemetry data that is sent over on the port, parse it, and publish the data to the Foxglove web socket server.  
8. The Foxglove web socket server makes the telemetry data available to connected clients such as the Foxglove browser or app which we will use to create our live plots and dashboards.

## Ok, but how do we get started?  
### Access Foxglove
You can access Foxglove by using your browser or downloading the app. It will tell you to create an account and organization, so feel free to name them as you please. You can find instructions to download Foxglove [here](https://foxglove.dev/download), or you can choose to go to the link, and click `Get started for free`.
### Open terminal
If you are on macOS, open Terminal. If you are on Windows, open Powershell. Make sure you `cd` into the `daq` repository and are specifically on the `Dana/foxglove-working` branch. If you do not already have Git, please follow the instructions linked [here](https://git-scm.com/install/). To get on this specific branch, make sure you fetch all remote branches with `git fetch origin` and change to this specific branch by running `git checkout Dana/foxglove-working`.
### Install `uv`
Do you have `uv` installed already? This is specifically to help install the correct packages and manage the Python environment used by the `lora_bridge.py`. For this bridge, we are using two packages
- `foxglove-sdk` publishes telemetry to Foxglove
- `pyserial` reads data from the LoRa's receiver port
  
Official installation instructions are linked [here](https://docs.astral.sh/uv/getting-started/installation/). To double-check if `uv` is installed, open a new Terminal/Powershell shell and run `uv --version` which should output something like `uv 0.##.#`...  
- If you are on macOS, install `uv` by running `curl -LsSf https://astral.sh/uv/install.sh | sh`  
- If you are on Windows, install `uv` by running `powershell -ExecutionPolicy ByPass -c "irm https://astral.sh/uv/install.ps1 | iex"`  
### Set up the project with `uv sync`  
Since you are already in the `daq` repository on the `Dana/foxglove-working` branch, just run `uv sync`, and this should install `foxglove-sdk` and `pyserial`. If it does not, you can manually do it by running `uv add foxglove-sdk pyserial`.  
### Connect the LoRa receiver
The receiver needs to appear as a serial device on the computer. 
- On macOS, make sure the receiver is connected, and run `ls /dev/cu*`. Several pathways will likely appear, but focus on the one that starts with `/dev/cu.usb-serial-XXXX` with the XXXX being used as placeholder for four numbers, e.g. 0001, which will depend on the adapter and driver.  
- On Windows, you will likely need to install a driver first, so start by downloading Silicon Labs CP210x Universal Windows Driver [here](https://micoair.com/downloads/). Make sure the receiver is connected, and open `Device Manager` on your UI. Expand `Ports (COM & LPT)` and find the receiver or USB serial adaptor which should have a COM port listed next to it e.g. COM3.  
### Run `lora_bridge.py`
Keep the LoRa receiver connected and run `lora_bridge.py` in root i.e. do not `cd` into any subdirectories like `src` or `include`. 
- On macOS, run `uv run python lora_bridge.py --port /dev/cu.usbserial-XXXX --baud 57600` with XXXX being used as placeholder here for four numbers that depend on your adapter and driver. Please replace XXXX before running the command.
- On Windows, run `uv run python lora_bridge.py --port COM3 --baud 57600` with COM3 being used as placeholder here for the specific COM port. If the COM port listed when you expanded `Ports (COM & LPT)` is not COM3, please replace it with appropriate COM port e.g. COM5.
If everything is working, the terminal should report that the Foxglove server started and show messages indicating that packets are being forwarded.
### Connect Foxglove to `lora_bridge.py`
Open Foxglove. Go to `Open a new connection` and click on `Foxglove Websocket`. If you are just hosting this on your personal computer, you can enter either `ws://127.0.0.1:8765` or `ws://localhost:8765`. Connect to the data source. Open the Topics panel and find `/vehicle/telemetry`. Add a Plot panel or another visualization. Select the fields you want to inspect.

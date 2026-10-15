# daq






# Will/Wheel-Speed-Info

folders: ~/include/WheelSpeed.h  ~/src/WheelSpeed.cpp

Design:
The wheel speed module utilizes system interrupts to log digital pulses coming into pins corresponding to each wheel speed sensor. The sensors are hall effect sensors -> signal processing board -> Teensy 4.1 (daq). Do not change anything in WheelSpeed.h. 

Initialization:
call 'WheelSpeed::begin();' in void setup. 
call 'WheelSpeed::update()' as the bool condition in an if statement in void loop. This will only sample data at the sample rate. Can call it multiple times as needed. 

Data Access:
You may access the calculated frequency (Hz) and rpm by declaring 

const auto& ws = WheelSpeed::getData(); 

and then: 

ws.rpm[WheelSpeed::FL]

or 

ws.frequencyHz[WheelSpeed::FL]

Increment through FL, FR, RL, RR for each wheel sensor. 
Data can be accessed at any point, though it is only accurate up to the sample rate: if you access the data multiple times before the next sample, you will get identical values.


Hybrid Measurement:
The function uses a hybrid measurement system with a configurable method threshold. At each sample time, it increments through each sensor, and if the pulse count is below the threshold, it uses the time between pulses to determine wheel speed. If the pulse count is above the threshold, it uses the number of pulses over the sample time to determine wheel speed. There is also a timeout delay, so that past a certain time + pulse detection causes the data to report wheel speed of 0. 

Configs:
In 'WheelSpeed.cpp': 
uint8_t PINS[COUNT]                     declare pinout of the four sensors
float PULSES_PER_REV[COUNT]             declare the number of teeth of each sensor
uint32_t UPDATE_US                      declare the time between each sample
uint32_t MULTI_PULSE_THRESHOLD          number of pulses before averaging wheel speed
uint32_t MIN_TIMEOUT_US                 minimum time before stop detection
uint32_t MAX_TIMEOUT_US                 maximum time before stop detection

Blocking:
update() does call noInterrupts() and interrupts(), temporarily freezing the interrupt scheduler until all pulse counts have been processed, potentially leading to a small time delay if other modules also use interrupts. Estimated worst case error for an update is I / T * 100, where I is the duration of the non-interrupt section and T is the time between measurements of the tachometer.
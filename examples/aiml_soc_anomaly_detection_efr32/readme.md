# Anomaly Detection
This application demonstrates a model trained to recognize anomalous motion patterns
using data from an IMU sensor. The detected state is printed to the serial port.

With the board facing up, and the USB cable pointed towards you, you should be
able to perform one of the normal or anomalous motion patterns and have them
correctly detected and output to the serial port. The LEDs light up to indicate
a detected anomaly.

When the application is running, the IMU sensor is sampled at 25 Hz to read out
acceleration, gyroscope, and orientation values. The samples are buffered and the
application periodically performs inference using a TensorFlow Lite Micro model
with the 90 latest samples. The machine learning model is a CNN that treats the
IMU data as a time–feature map and recognizes patterns corresponding to normal
or anomalous motion. Common non-anomalous patterns include stationary behavior,
smooth 360° yaw rotation, and magnetometer-based motion. Anomalous patterns include
sudden jolts or drops, shaking, vibration, and fast tilting. Use Netron or similar
tools to see the full model architectur

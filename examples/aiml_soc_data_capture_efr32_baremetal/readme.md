# ML Data Capture

This application collects raw motion data from an IMU sensor—including acceleration, gyroscope, and orientation readings—and logs it to a CSV file using a Python script. It's designed for gesture recognition, motion tracking, or building datasets for machine learning.

## Behavior

- The IMU sensor is sampled at **100 Hz**.
- Each sample includes:
  - **Accelerometer**: x, y, z (in m/s² or g)
  - **Gyroscope**: x, y, z (in °/s or rad/s)
  - **Orientation**: pitch, roll, yaw (in degrees or radians)
- Data is streamed to the serial port in real time.
- A Python script reads the serial stream and writes the data to a CSV file.

## Usage

1. Flash the firmware to your board.
2. Connect the board via USB and open the serial port.
3. Run your Python script to begin logging data to a CSV file.
4. Perform gestures or movements while data is being collected.

## Notes

- No machine learning inference is performed on-device.
- This setup is focused on **data collection only**.
- Ensure consistent movement patterns and sampling conditions for clean datasets.
- You can later use this data to train models for gesture recognition, activity classification, or motion analysis.
# ML Data Capture

This application collects raw motion data from an IMU sensor—including accelerometer, gyroscope, and orientation readings—and logs the data to a CSV file using a Python script. It is designed for gesture recognition, motion tracking, and building datasets for machine learning applications.

## Behavior

- The IMU sensor is sampled at **100 Hz**.
- Each sample includes:
  - **Accelerometer**: x, y, z (m/s² or g)
  - **Gyroscope**: x, y, z (°/s or rad/s)
  - **Orientation**: pitch, roll, yaw (degrees or radians)
- Data is streamed to the serial port in real time.
- A Python script `main.py` which is available at `tool\data-capture` reads the serial stream and writes the data to a CSV file. For the script to run there are detailed steps on how to install venv for the python script to run. Follow the `readme.md` at `tool\data-capture` and proceed accordingly.

### Locating the AI/ML SDK

1. In **Simplicity Studio**, open **Settings** → **SDKs**
2. Select the **Simplicity SDK Suite** version you use. The suite expands to list its installed extension SDKs.
3. Right-click **Silicon Labs AI/ML SDK** in that list → **Browse to Location**
4. In the opened folder, go to `tool\data-capture`

## Usage

1. Flash the firmware to the board.
2. Connect the board via USB and open the serial port.
3. Run the Python script to begin logging data to a CSV file.
4. Perform gestures or movements while data is being collected.

## Notes

- No machine learning inference is performed on-device.
- This setup is intended for **data collection only**.
- Ensure consistent movement patterns and sampling conditions for cleaner datasets.
- The collected data can later be used to train models for gesture recognition, activity classification, or motion analysis.

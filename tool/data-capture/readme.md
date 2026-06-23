## Description
This tool streams **IMU sensor data** from an **EFR32XG26** device via the **J-Link debug interface** and logs it into a CSV file.  

## Environment Setup
1. Create virtual python environment using following command.

```console
python -m venv .venv
```
2. Activate virtual environment.
```console
.venv\Scripts\activate
```
3. Install required python packages from requirement.txt.
```console
pip install -r requirements.txt
```
4. Install [SEGGER J-Link](https://www.segger.com/downloads/jlink/) or [Simplicity Commander](https://www.silabs.com/software-and-tools/simplicity-studio/simplicity-commander?tab=getting-started), then add the folder that contains the J-Link tools or Simplicity Commander to your system path so this tool can find them at runtime.

   - **Windows:** Add path to directory containing `JLink_x64.dll` to your "Path" environment variable. On Windows this is usually `C:\Program Files\SEGGER\JLink_V<XYZ>` where `<XYZ>` is the version number installed.
   - **Linux:** Add the install directory to `PATH` (for example in `~/.bashrc` or `~/.profile`: `export PATH="<install-dir>:$PATH"`), then open a new shell or run `source ~/.bashrc`.
   - **macOS:** Same as Linux — add the install directory to `PATH` in `~/.zshrc` or `~/.bash_profile` (for example `export PATH="<install-dir>:$PATH"`), then restart the terminal or `source` that file.

   Use the folder that contains `JLink_x64.dll` (Windows), `libjlink*.so` (Linux), or `libjlink*.dylib` (macOS), or the Commander install root if that is where those binaries live. Either tool is sufficient; you do not need both.

## Usage:
```console
python main.py --save --out-dir <path/to/save/csv> --target-freq <Hz>
```

## Example:
Stream IMU data and log at 50 Hz :
```console
python main.py  --target-freq 50 
```

## Note
Press Ctrl + C to stop streaming and exit cleanly.
The output CSV file is saved at:
```console
<out-dir>/imu_log.csv
```
CSV columns:
```console
timestamp, acc0, acc1, acc2, gyro0, gyro1, gyro2, ovec0, ovec1, ovec2
```

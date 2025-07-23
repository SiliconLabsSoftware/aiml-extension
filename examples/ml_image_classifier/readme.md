# Image Classifier

This application uses TensorFlow Lite for Microcontrollers to run image
classification machine learning models to classify hand gestures from image data
captured from external camera. The detection is visualized using the LED's on the
board and the classification results are written to the VCOM serial port.

## Behavior

The application executes the following loop:

1. Read image from [ArduCAM](https://www.arducam.com/product/arducam-2mp-spi-camera-b0067-arduino/) camera
2. Optionally "normalize" the based on the parameters embedded in the classification `.tflite` model file (see the "Model Input Normalization" section below)
3. Give normalized image [Tensorflow-Lite Micro](https://github.com/tensorflow/tflite-micro) Interpreter
4. Process interpreter results with optional averaging and thresholding.
5. Turn on or off LED based on detection results.

The application is using two LEDs to show detection and activity and it is
printing detection results and debug log output on the VCOM serial port. In the
application configuration file called image_classifier_config.h the user can
select which LED to use for activity and which LED to use for detection. By
default the detection LED is green/led1 and the activity LED is red/led0.

At a regular interval the application will perform an inference and the result
will be processed to find the average score for each class in the current
window. If the top result score is higher than a detection threshold then a
detection is triggered and the detection LED (green) will light up for about 750
ms.

Once the detection LED turns off the application goes back to responding to the
input data. If the change in model output is greater than a configurable
sensitivity threshold, then the activity LED (red) will blink for about 500 ms.

The activity LED indicates that image has been detected on the input and the
model output is changing, but no clear classification was made.

In image classification it is common to have some results that map to unknown.
These results are something that we usually want to ignore. This is
being filtered out in the image classifier application based on the label text.
By default any labels that start with an underscore are ignored when processing
results. This behavior can be disabled in the application configuration file.

Following figure shows classes used for this application.

![image](rock_paper_scissors_overview.png)

## Hardware Setup

To run this application, an [ArduCAM](https://www.arducam.com/product/arducam-2mp-spi-camera-b0067-arduino/) module is required.
This camera must be connected to the SPI and I2C peripheral of the embedded target.

The following default pin mappings are used by:

- **BRD2601**
- **BRD2608A**

| ArduCAM Pin | Board Expansion Header Pin |
| ----------- | -------------------------- |
| GND         | 1                          |
| VCC         | 18                         |
| CS          | 10                         |
| MOSI        | 4                          |
| MISO        | 6                          |
| SCK         | 8                          |
| SDA         | 16                         |
| SCL         | 15                         |

## Dumping images to PC

This application uses JLink to stream image data to python script available at tools/image-visualization/main.py. Once application binary is flashed in development board start using visualization tool to display or save images to local PC.

## Note

1. This application uses image captured from arducam camera.
2. Model used in this application trained on images which has sufficient brightness, So experiment in good lighting condition. Use SL_ML_IMAGE_MEAN_THRESHOLD from .slcp to set minimum mean intensity to process images.
3. Keep 0.5-meter distance from camera to hand for experimentation.
4. Maintain plain background (White/black) to get good detection results.

## References

- Image classifier [suds](https://suds-web-prod-scratch-internal.silabs.net/machine-learning/2.1.1-mlsw-10587-update-image-classification-documentation-in-suds-repository/aiml-sample-apps/).
- [Machine Learning (AI/ML) Documentation](https://docs.silabs.com/machine-learning/latest/aiml-developing-with)
- [TensorFlow Lite Micro](https://www.tensorflow.org/lite/microcontrollers)

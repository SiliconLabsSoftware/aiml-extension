################################################################################
# @file
# @brief Silicon Labs Feature Generation Initialization with image source
###############################################################################
# # License
# <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
###############################################################################
# @file
###############################################################################
#
# The licensor of this software is Silicon Laboratories Inc. Your use of this
# software is governed by the terms of Silicon Labs Master Software License
# Agreement (MSLA) available at
# www.silabs.com/about-us/legal/master-software-license-agreement. This
# software is distributed to you in Source Code format and is governed by the
# sections of the MSLA applicable to Source Code.
#
################################################################################

import os
import argparse
import re
import time
import threading
import logging
import atexit
import shutil
import math

import numpy as np
from utils.serial_reader import SerialReader
from utils.jlink_stream import JlinkStream, JLinkDataStream, JlinkStreamOptions
from utils.logger import get_logger
import cv2

dump_images: bool = True


# JLink visualization Object
class JlinkVisu:
    def __init__(self, params: argparse.Namespace):
        self.get_shape_info(params)

    def get_shape_info(self, params: argparse.Namespace):
        """Computes image dimentions used for target region.

        Args:
            params (Namespace): Input parameters.

        Raises:
            ValueError: If camera resolution format if invalid
            ValueError: If target shape format if invalid
        """

        try:
            sensor_data_width, sensor_data_height = (
                params.camera_resolution.lower().split("x")
            )
        except:
            raise ValueError("Invalid camera/sensor resolution")

        try:
            target_data_width, target_data_height = params.target_shape.lower().split(
                "x"
            )
        except:
            raise ValueError("Invalid target shape")

        self.sensor_data_width = int(sensor_data_width)
        self.sensor_data_height = int(sensor_data_height)
        self.target_data_width = int(target_data_width)
        self.target_data_height = int(target_data_height)

        if self.target_data_width > self.sensor_data_width:
            self.target_data_width = self.sensor_data_width
        if self.target_data_height > self.sensor_data_height:
            self.target_data_height = self.sensor_data_height

        sensor_data_center_x = int(self.sensor_data_width / 2)
        sensor_data_center_y = int(self.sensor_data_height / 2)

        # Left, top coordinate
        self.target_left_top_cood = (
            sensor_data_center_x - int(self.target_data_width / 2),
            sensor_data_center_y - int(self.target_data_height / 2),
        )
        # Right , bottom coordinate
        self.target_right_bottom_coord = (
            sensor_data_center_x + int(self.target_data_width / 2),
            sensor_data_center_y + int(self.target_data_height / 2),
        )

        dia_length = math.sqrt(
            self.sensor_data_width * self.sensor_data_width
            + self.sensor_data_height * self.sensor_data_height
        )

        self.resize_height = self.sensor_data_height
        self.resize_width = self.sensor_data_width
        if dia_length < 300:
            self.resize_height = self.sensor_data_height * 3
            self.resize_width = self.sensor_data_width * 3

    def _start_jlink_processor(self, dump_image_dir: str) -> threading.Event:
        """Start the JLink stream inferface.
        This allows for reading binary data from the embedded device via debug interface.

        Args:
            dump_image_dir (str): Directory path to save captured images.

        Returns:
            threading.Event: Thread event object.
        """

        jlink_logger = get_logger("jlink", console=False)  # , parent=logger)
        print("Opening device data stream ...")
        opts = JlinkStreamOptions()
        opts.polling_period = 0.10

        jlink_stream = JlinkStream(opts)
        jlink_stream.connect()
        print("Device data stream opened")

        stop_event = threading.Event()
        atexit.register(stop_event.set)
        t = threading.Thread(
            name="JLink Processing loop",
            target=self._jlink_processing_loop,
            daemon=True,
            kwargs=dict(
                jlink_stream=jlink_stream,
                stop_event=stop_event,
                logger=jlink_logger,
                dump_image_dir=dump_image_dir,
            ),
        )
        t.start()
        return stop_event

    def _jlink_processing_loop(
        self,
        jlink_stream: JlinkStream,
        stop_event: threading.Event,
        dump_image_dir: str,
        logger: logging.Logger,
    ) -> None:
        """Read binary data from embedded device via JLink interface.

        This runs in a separate thread and visualizes images.

        Args:
            jlink_stream (JlinkStream): jlink stream object.
            stop_event (threading.Event): Thread event object.
            dump_image_dir (str): Path to save images.
            logger (logging.Logger): Logger object.
        """
        image_stream: JLinkDataStream = None
        image_length = self.sensor_data_height * self.sensor_data_width

        image_data = bytearray()
        img_count = 0
        while True:
            if stop_event.wait(0.010):
                jlink_stream.disconnect()
                break

            if image_stream is None:
                try:
                    image_stream = jlink_stream.open("image", mode="r")
                    logger.debug("Device image stream ready")
                except Exception as e:
                    logger.debug(f"Failed to open device image stream, err: {e}")
                    continue

            remaining_length = image_length - len(image_data)
            img_bytes = image_stream.read_all(
                remaining_length, timeout=0.700, throw_exception=False
            )
            if img_bytes:
                image_data.extend(img_bytes)

            if len(image_data) != image_length:
                continue

            img_buffer = np.frombuffer(image_data, dtype=np.uint8)
            image_data = bytearray()
            img = np.reshape(
                img_buffer, (self.sensor_data_height, self.sensor_data_width)
            )

            # latest_image_q.append(img)
            if dump_image_dir:
                fname = "{:06}".format(img_count)
                # Only dump the image if it is unique
                image_path = f"{dump_image_dir}/{fname}.jpg"
                cv2.imwrite(image_path, img)
                img_count += 1

            img = cv2.rectangle(
                img, self.target_left_top_cood, self.target_right_bottom_coord, 255, 1
            )
            img = cv2.resize(img, (self.resize_width, self.resize_height))
            cv2.imshow("Development Board Samples", img)
            cv2.waitKey(1)


def parse_arguments() -> argparse.Namespace:
    """Input argument parser.

    Returns:
        argparse.Namespace: Input parameters.
    """
    parser = argparse.ArgumentParser(description="Image Visualization Tool")
    parser.add_argument(
        "--camera-resolution",
        type=str,
        help="Input camera resolution Width x Height.",
        default="160x120",
    )
    parser.add_argument(
        "--target-shape",
        type=str,
        help="Input target image resolution used for model WidthxHeight.",
        default="84x84",
    )
    parser.add_argument(
        "--save", action="store_true", help="Flag to store images to --out-dir."
    )
    parser.add_argument(
        "--out-dir",
        type=str,
        help="Output path to save/dump images.",
        default="./dump_images",
    )
    parser.add_argument(
        "--port",
        type=str,
        help="Serial port to connect to the device.",
        default="regex:JLink CDC UART Port",
    )
    return parser.parse_args()


if __name__ == "__main__":
    params = parse_arguments()
    jvisu = JlinkVisu(params)
    # params.save = True
    if params.save:
        if os.path.exists(params.out_dir) and os.path.isdir(params.out_dir):
            shutil.rmtree(params.out_dir)
        os.makedirs(params.out_dir, exist_ok=True)
        print(f"Dumping images to {params.out_dir}")
        # clean_directory(params.out_dir)
    else:
        params.out_dir = None

    # If no serial COM port is provided,
    # then attempt to resolve it based on common Silabs board COM port description
    port = params.port or "regex:JLink CDC UART Port"

    # Start the serial COM port reader
    with SerialReader(
        port=port,
        baud=115200,
        # outfile=logger,
        start_regex=re.compile(r".*Image Classifier.*", re.IGNORECASE),
        fail_regex=[
            re.compile(r".*hardfault.*", re.IGNORECASE),
            re.compile(r".*assert.*", re.IGNORECASE),
            re.compile(r".*error.*", re.IGNORECASE),
        ],
    ) as reader:
        stop_jlink_event = None
        stop_jlink_event = jvisu._start_jlink_processor(dump_image_dir=params.out_dir)

        try:
            while not reader.read(timeout=0.010):
                time.sleep(0.100)

            if reader.error_message:
                if stop_jlink_event:
                    stop_jlink_event.set()
                raise RuntimeError(f"Device error: {reader.error_message}")

        except KeyboardInterrupt:
            if stop_jlink_event:
                stop_jlink_event.set()

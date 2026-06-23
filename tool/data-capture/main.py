################################################################################
# @file
# @brief Silicon Labs IMU Dumping tool from JLink stream
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
import csv
from datetime import datetime, timedelta
import time
import numpy as np
import argparse
import re
import time
import threading
import logging
import atexit
import shutil
import signal
from utils.serial_reader import SerialReader
from utils.jlink_stream import JlinkStream, JLinkDataStream, JlinkStreamOptions
from utils.logger import get_logger

dump_images: bool = True


# JLink visualization Object
class JlinkVisu:
    def __init__(self, params: argparse.Namespace):
        print("Started JLINK Streaming \n")

    def _start_jlink_processor(self, dump_csv_dir: str,target_hz: int) -> threading.Event:
        """Start the JLink stream inferface.
        This allows for reading binary data from the embedded device via debug interface.

        Args:
            dump_csv_dir (str): Directory path to save captured images.

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
                dump_csv_dir=dump_csv_dir,
                target_hz=target_hz,
            ),
        )
        t.start()
        return stop_event

    def _jlink_processing_loop(
        self,
        jlink_stream: JlinkStream,
        stop_event: threading.Event,
        dump_csv_dir: str,
        logger: logging.Logger,
        target_hz: int = 100,   # <--- NEW ARGUMENT
    ) -> None:
        """Read IMU data via JLink, adaptively downsample to target_hz, and log to CSV.

        CSV columns:
        timestamp, acc0, acc1, acc2, gyro0, gyro1, gyro2, ovec0, ovec1, ovec2
        """

        # --- Config ---
        FIELDS = ["acc0", "acc1", "acc2", "gyro0", "gyro1", "gyro2", "ovec0", "ovec1", "ovec2"]
        DTYPE = np.float32
        ENDIAN = "<"
        BYTES_PER_SAMPLE = np.dtype(DTYPE).itemsize * len(FIELDS)

        READ_CHUNK = 4096
        FLUSH_EVERY_N_ROWS = 100

        # Target frequency (Hz) comes from argument
        TARGET_HZ = float(target_hz)
        TS_PERIOD = timedelta(seconds=1.0 / TARGET_HZ)

        # Input rate estimator
        rate_est_hz = 4250.0
        ema_alpha = 0.25
        min_k, max_k = 10, 200  # guardrails for window size
        # ----------------

        csv_stream: JLinkDataStream = None
        raw_buffer = bytearray()
        carry = None
        next_ts = None

        os.makedirs(dump_csv_dir or ".", exist_ok=True)
        csv_path = os.path.join(dump_csv_dir or ".", "imu_log.csv")
        file_exists = os.path.exists(csv_path)

        basecode = DTYPE().dtype.str[1:]
        sample_dtype = np.dtype(ENDIAN + basecode)

        last_read_t = time.perf_counter()

        with open(csv_path, "a", newline="") as f:
            writer = csv.writer(f)
            if not file_exists:
                writer.writerow(["timestamp"] + FIELDS)

            rows_since_flush = 0

            while True:
                if stop_event.wait(0.010):
                    try:
                        jlink_stream.disconnect()
                    except Exception as e:
                        logger.debug(f"Disconnect error (ignored): {e}")
                    break

                if csv_stream is None:
                    try:
                        csv_stream = jlink_stream.open("imu", mode="r")
                        logger.debug("Device IMU stream ready")
                    except Exception as e:
                        logger.debug(f"Failed to open IMU stream: {e}")
                        continue

                try:
                    chunk = csv_stream.read_all(READ_CHUNK, timeout=0.700, throw_exception=False)
                except Exception as e:
                    logger.debug(f"read_all error (ignored): {e}")
                    chunk = b""

                if chunk:
                    raw_buffer.extend(chunk)

                full_bytes = (len(raw_buffer) // BYTES_PER_SAMPLE) * BYTES_PER_SAMPLE
                if full_bytes == 0:
                    continue

                mv = memoryview(raw_buffer)[:full_bytes]
                try:
                    arr = np.frombuffer(mv, dtype=sample_dtype).reshape(-1, len(FIELDS))
                except Exception as e:
                    logger.debug(f"Reshape error (dropping this batch): {e}")
                    raw_buffer = bytearray(raw_buffer[full_bytes:])
                    continue

                # --- Adaptive rate estimation ---
                now_t = time.perf_counter()
                dt = now_t - last_read_t
                last_read_t = now_t
                if dt > 1e-3:
                    inst_rate = len(arr) / dt
                    inst_rate = float(np.clip(inst_rate, 3000.0, 6000.0))
                    rate_est_hz = ema_alpha * inst_rate + (1.0 - ema_alpha) * rate_est_hz

                # Samples per target frame
                k = int(round(rate_est_hz / TARGET_HZ))
                k = max(min_k, min(max_k, k))

                # Merge carry + current
                work = np.vstack((carry, arr)) if carry is not None and carry.size else arr
                n_full = (len(work) // k) * k
                full, carry = work[:n_full], work[n_full:]

                if n_full > 0:
                    w = full.reshape(-1, k, len(FIELDS))
                    down = np.rint(w.mean(axis=1)).astype(DTYPE)

                    if next_ts is None:
                        next_ts = datetime.now()

                    for row in down:
                        ts = next_ts.isoformat(timespec="microseconds")
                        writer.writerow([ts] + row.tolist())
                        next_ts += TS_PERIOD
                        rows_since_flush += 1
                        if rows_since_flush >= FLUSH_EVERY_N_ROWS:
                            try:
                                f.flush()
                                os.fsync(f.fileno())
                            except Exception as e:
                                logger.debug(f"Flush error (ignored): {e}")
                            rows_since_flush = 0

                raw_buffer = bytearray(raw_buffer[full_bytes:])

            try:
                f.flush()
                os.fsync(f.fileno())
            except Exception:
                pass
            logger.debug(f"IMU CSV logging complete (downsampled to ~{TARGET_HZ:.0f} Hz): {csv_path}")

def parse_arguments() -> argparse.Namespace:
    """Input argument parser.

    Returns:
        argparse.Namespace: Input parameters.
    """
    parser = argparse.ArgumentParser(description="CSV Dumping Tool")
    parser.add_argument(
        "--save", action="store_true", help="Flag to store CSV to --out-dir."
    )
    parser.add_argument(
        "--out-dir",
        type=str,
        help="Output path to save/dump CSV.",
        default="./csv",
    )
    parser.add_argument(
        "--port",
        type=str,
        help="Serial port to connect to the device.",
        default="regex:JLink CDC UART Port",
    )
    parser.add_argument(
        "--target-freq",
        type=int,
        help="Output Freq for different scenarios",
        default=100
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
        print(f"Dumping csv to {params.out_dir}")
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
        start_regex=re.compile(r".*imu.*", re.IGNORECASE),
        fail_regex=[
            re.compile(r".*hardfault.*", re.IGNORECASE),
            re.compile(r".*assert.*", re.IGNORECASE),
            re.compile(r".*error.*", re.IGNORECASE),
        ],
    ) as reader:
        stop_jlink_event = None
        stop_jlink_event = jvisu._start_jlink_processor(dump_csv_dir=params.out_dir,target_hz=params.target_freq)
        
        def _graceful_sigint_handler(signum, frame):
            # Called on Ctrl+C (SIGINT)
            try:
                if stop_jlink_event:
                    stop_jlink_event.set()   # ask the thread to stop
                time.sleep(0.3)
            finally:
                # Exit the process with code 130 
                os._exit(130)
        signal.signal(signal.SIGINT, _graceful_sigint_handler)

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
            time.sleep(0.3)
            os._exit(130)

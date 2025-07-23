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

import logging
import os
import sys
import types
from typing import Union


def get_logger(
    name="mltk",
    level="INFO",
    console=False,
    log_file=None,
    log_file_mode="w",
    parent: logging.Logger = None,
    base_level="DEBUG",
    file_level="DEBUG",
):
    """Get or create a logger, optionally adding a console and/or file handler"""
    logger = logging.getLogger(name)
    if len(logger.handlers) == 0:
        if parent is None:
            logger.propagate = False
        else:
            logger.parent = parent
            logger.propagate = True

        logger.setLevel(base_level)

        if console:
            add_console_logger(logger, level=level)

        if log_file:
            log_dir = os.path.dirname(log_file)
            if log_dir:
                os.makedirs(log_dir, exist_ok=True)

            fh = logging.FileHandler(log_file, mode=log_file_mode)
            fh.setLevel(file_level)
            logger.addHandler(fh)

    if not hasattr(logger, "close"):

        def _close(cls):
            for handler in cls.handlers:
                if isinstance(handler, logging.FileHandler):
                    handler.close()

        logger.close = types.MethodType(_close, logger)

    return logger


def add_console_logger(logger: logging.Logger, level="INFO"):
    """Add a console logger to the given logger"""
    for handler in logger.handlers:
        if isinstance(handler, _ConsoleStreamLogger):
            return

    ch = _ConsoleStreamLogger(sys.stdout)
    ch.setLevel(get_level(level))
    logger.addHandler(ch)


# This is needed to distinguish between a FileHandler and console StreamHandler
class _ConsoleStreamLogger(logging.StreamHandler):
    pass


def get_level(level: Union[str, int]) -> str:
    """Return the logging level as a string"""
    if isinstance(level, str):
        return level.upper()
    return logging.getLevelName(level)

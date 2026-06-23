"""
*****************************************************************************
* @file compiler.py
* @brief Shim layer to adapt sml-compile for Advanced Configurators.
*******************************************************************************
* # License
* <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
*******************************************************************************
*
* The licensor of this software is Silicon Laboratories Inc. Your use of this
* software is governed by the terms of Silicon Labs Master Software License
* Agreement (MSLA) available at
* www.silabs.com/about-us/legal/master-software-license-agreement. This
* software is distributed to you in Source Code format and is governed by the
* sections of the MSLA applicable to Source Code.
*
*****************************************************************************
"""

import argparse
import logging
import platform
import subprocess
import sys
from pathlib import Path

OS_MAPPING = {
    "windows": "win",
    "win32": "win",
    "cygwin": "win",
    "linux": "linux",
    "darwin": "darwin",
}

PLATFORM_MAPPING = {
    "x86": "x86",
    "x86_64": "amd64",
    "amd64": "amd64",
    "arm64": "arm64",
    "aarch64": "arm64",
}


def get_sml_executable() -> str:
    """Return sml executable based on OS and CPU architecture

    Returns:
        str: sml executable string
    """

    os = OS_MAPPING[platform.system().lower()]
    if not os:
        logging.error("Unsupported OS: %s", os)
        sys.exit(1)

    arch = PLATFORM_MAPPING[platform.machine().lower()]
    if not arch:
        logging.error("Unsupported platform arch: %s", arch)
        sys.exit(1)

    sml_executable = f"sml_{os}_{arch}"

    if os == "win":
        sml_executable += ".exe"

    logging.debug("found sml executable: %s", sml_executable)
    return str(Path(sml_executable).resolve())


def run_generate(input_dir: Path, output_dir: Path, part_number: str) -> None:
    """Invoke `sml compile` for SLC `uc_generate`-style generation.

    Inputs:
        sml (Path): Path to the `sml` executable.
        model_path (Path): Input `.tflite` file path passed to `sml compile`.
        output_dir (Path): Generation output directory.
        part_number (str): Board / part OPN used only to derive `--platform` for `sml` (not passed through).

    Returns:
        int: Process exit code from `sml`. Returns `1` when the part cannot be mapped
        to a compiler platform.
    """
    gen_out = output_dir.resolve()
    gen_out.mkdir(parents=True, exist_ok=True)
    logging.info(
        "sml.log created by aiml/tool/compiler/compiler.py (Studio/SLC adapter)."
    )
    logging.info("sml appends below.")

    logging.debug("Input Directory: %s", input_dir)
    logging.info("Output Directory: %s", output_dir)
    logging.info("Part Number: %s", part_number)

    sml = get_sml_executable()

    logging.debug("got sml executable: %s", sml)

    mlconfs = list(input_dir.glob("*.mlconf"))
    if not mlconfs:
        logging.warning("no mlconf files found in input directory: %s", input_dir)

    # iterate over all mlconf files found in the input directory
    for mlconf in mlconfs:
        logging.debug("processing %s file", mlconf)
        cmd = [
            sml,
            "compile",
            "--config",
            str(mlconf.resolve()),
            "--output-dir",
            str(gen_out),
            "--platform",
            part_number,
            "--is-studio",
            "--clean-workspace",
            "--generate-templates",
        ]

        rc = subprocess.run(cmd).returncode

        logging.debug("sml executed with return code %d", rc)

        if rc != 0:
            logging.error("sml compile did not succeed for %s", mlconf)
            sys.exit(1)


def run_upgrade(result_file: Path) -> None:
    """Write placeholder upgrade results.

    Inputs:
        result_file (Path): YAML path Studio expects for upgrade results.
    """
    text = "upgrade_results:\n- message: No upgrade action taken. You may have to regenerate the project.\n  status: nothing\n"
    result_file.parent.mkdir(parents=True, exist_ok=True)
    result_file.write_text(text, encoding="utf-8")


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Shim wrapper to make sml-compile compatible with ACs"
    )
    subparser = parser.add_subparsers(
        dest="command", required=True, help="Adapter command"
    )
    generate_parser = subparser.add_parser(
        "generate", help="Run sml compile for project generation."
    )
    generate_parser.add_argument(
        "input_dir", type=Path, help="Input directory containing .mlconf files"
    )
    generate_parser.add_argument(
        "output_dir", type=Path, help="Output directory for generated content"
    )
    generate_parser.add_argument("part_number", type=str, help="Orderable Part Number")

    upgrade_parser = subparser.add_parser(
        "upgrade", help="Run project upgrade using sml-compile"
    )
    upgrade_parser.add_argument(
        "temp_dir", type=Path, help="Temporary directory for upgrade operation"
    )
    upgrade_parser.add_argument(
        "result_file", type=str, help="YAML file path for upgrade results"
    )

    return parser.parse_args()


def main():
    args = _parse_args()
    log_level = logging.DEBUG
    log_format = "%(asctime)s - [%(levelname)s] (%(filename)s:%(lineno)d): %(message)s"
    log_date_format = "%Y-%m-%d %H:%M:%S"

    if args.command == "generate":
        logging.basicConfig(
            filename=f"{args.output_dir.resolve()}/sml.log",
            level=log_level,
            format=log_format,
            datefmt=log_date_format,
        )
        run_generate(args.input_dir, args.output_dir, args.part_number)

    if args.command == "upgrade":
        logging.basicConfig(
            filename=f"{args.temp_dir.resolve()}/sml.log",
            level=log_level,
            format=log_format,
            datefmt=log_date_format,
        )
        run_upgrade(Path(args.result_file).expanduser().resolve())


if __name__ == "__main__":
    main()

import argparse
import logging
import re
import shutil
import subprocess
import sys
import zipfile
from jinja2 import Environment, FileSystemLoader
from pathlib import Path


def find_first_tflite_file(input_dir: Path) -> Path:
    """
    Return the alphabetically sorted first tflite filename and content
    """
    candidates = sorted(input_dir.glob("**/*.tflite"))
    if not candidates:
        logging.info("No tflite file found - exiting.")
        sys.exit(0)

    tflite_file = candidates[0]
    if len(candidates) > 1:
        listing = "\n- ".join(str(cand) for cand in candidates)
        logging.warning(f"Multiple tflite files found:\n- {listing}")
        logging.debug(f"Defaulting to converting {tflite_file}")

    logging.debug(f"Found tflite file: {tflite_file}")
    return tflite_file


def get_board_platform(part_number: str) -> str:
    """select platform value based on part number"""
    board_platform = None
    _part_no = part_number.lower()  # sanitize input
    if re.search("efr32", _part_no):
        board_platform = "efr32"
    elif re.search("si.*917", _part_no):
        board_platform = "si91x"
    else:
        logging.error(f"Unsupported board platform: {board_platform}")
        sys.exit(1)
    logging.debug(f"Board platform selected: {board_platform}")
    return board_platform


def execute_compiler(
    model_path: Path, board_platform: str, output_dir: Path
) -> Path | None:
    # select NPU toolkit executable based on OS
    mvp_compiler_executable = None
    if sys.platform == "win32" or sys.platform == "cygwin":
        mvp_compiler_executable = "./mvp_compiler.exe"
    elif sys.platform == "linux":
        mvp_compiler_executable = "./mvp_compiler"
    # MLSW-10402: Stop NPU compiler execution on macOS
    # this condition should be updated to call the macOS-compatible binary when it is created
    elif sys.platform == "darwin":
        return
    else:
        logging.error(f"Unsupported OS: {sys.platform}")
        sys.exit(1)

    logging.debug(f"OS: {sys.platform}")
    logging.debug(f"Executable selected: {mvp_compiler_executable}")

    mvp_compiler_version_output = subprocess.run(
        [mvp_compiler_executable, "--version"], capture_output=True, text=True
    )
    logging.debug(f"Running version: {mvp_compiler_version_output.stdout}")

    rc = subprocess.Popen(
        [
            mvp_compiler_executable,
            "--accelerator",
            "mvpv1",
            "--platform",
            board_platform,
            "--weights-paging",
            "--output",
            output_dir,
            "-x",
            "codegen.profiler_enabled=1",
            "-x",
            "codegen.shorten_paths=1",
            model_path,
        ]
    )
    rc.wait()
    logging.debug(f"compiler ran with these arguments: {rc.args}")
    logging.debug(f"compiler executable returned {rc.returncode}")

    # alphabetically sort and return the name of the generated zip archive
    return Path(sorted(output_dir.glob("**/*.zip"))[0])


def extract_zip(zip_dir: Path) -> Path:
    extract_dir = zip_dir.parent / zip_dir.stem
    with zipfile.ZipFile(zip_dir, "r") as zip_ref:
        zip_ref.extractall(extract_dir)
    return extract_dir


def delete_files(paths: list[Path]) -> None:
    for path in paths:
        shutil.rmtree(path) if path.is_dir() else path.unlink(missing_ok=True)
        logging.debug(f"deleted: {path}")


def generate_model_file(
    template_path: Path | str,
    template_name: str,
    model_name: str,
    output_dir: Path | str,
) -> None:
    templates_env = Environment(loader=FileSystemLoader(template_path))
    template = templates_env.get_template(template_name)
    model_name = model_name.lower()
    content = template.render(model_name=model_name)
    suffix = template_name.split(".")[-2]
    filename = f"{output_dir}/sl_ml_model_{model_name}.{suffix}"
    with open(filename, mode="w", encoding="utf-8") as out_file:
        out_file.write(content)
        logging.debug(f"generated {filename} for model: {model_name}")


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Machine Learning Model Compiler")
    parser.add_argument(
        "-i",
        "--input-dir",
        required=True,
        type=Path,
        help="Input directory containing .tflite files",
    )
    parser.add_argument(
        "-o",
        "--output-dir",
        required=True,
        type=Path,
        help="Output directory to populate with serialized content.",
    )
    parser.add_argument("-p", "--part-number", required=True, help="Part number")
    return parser.parse_args()


def setup_logging(output_dir):
    log_file = output_dir / "compiler.log"

    logging.basicConfig(
        filename=log_file,
        encoding="utf-8",
        level=logging.DEBUG,
        format="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
    )


def main():
    # MLSW-10402: Stop execution on macOS
    # This condition should be removed when a macOS NPU compiler binary is created
    if sys.platform == "darwin":
        print(
            "NPU Compiler Adapter Pack: macOS is not supported yet but this will not throw an error."
        )
        return

    args = parse_arguments()
    setup_logging(args.output_dir)
    logging.debug(f"Input directory: {args.input_dir}")
    logging.debug(f"Output directory: {args.output_dir}")
    logging.debug(f"Part Number: {args.part_number}")

    model_path = find_first_tflite_file(args.input_dir)
    board_platform = get_board_platform(args.part_number)

    # Skipping if board_platform is not si91x
    if board_platform != "si91x":
        return

    temp_path = Path(args.input_dir).parent / "temp/"
    temp_path.mkdir(exist_ok=True)
    # args.output_dir = temp_path
    zip_path = execute_compiler(model_path, board_platform, temp_path)
    logging.debug(f"model compiled in archive: {zip_path}")

    zip_extract_dir = extract_zip(zip_path)
    logging.debug(f"archive extracted to {zip_extract_dir}")

    # move generated files to output_dir
    shutil.copytree(
        zip_extract_dir / "codegen", Path(args.output_dir), dirs_exist_ok=True
    )
    logging.debug(f"model files moved to {args.output_dir}")
    delete_files([zip_path, zip_extract_dir])
    temp_path.rmdir()

    model_name = model_path.stem
    generate_model_file(
        "templates/", "sl_ml_model_model_name.h.jinja", model_name, args.output_dir
    )


if __name__ == "__main__":
    main()

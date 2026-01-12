import argparse
import logging
import re
import shutil
import subprocess
import sys
import zipfile
import yaml
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
    # select MVP compiler executable based on OS
    mvp_compiler_executable = None
    if sys.platform == "win32" or sys.platform == "cygwin":
        mvp_compiler_executable = "./mvp_compiler.exe"
    elif sys.platform == "linux":
        mvp_compiler_executable = "./mvp_compiler"
    # MLSW-10402: Stop MVP compiler execution on macOS
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
    process_args = [
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

    if model_path.name == "firmware_model_si91x.tflite":
        process_args.append("-x")
        process_args.append("codegen.model_header.generate_op_resolver=0")

    rc = subprocess.Popen(process_args)

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
    parser = argparse.ArgumentParser(
        description="Machine Learning Model Compiler for MVP"
    )

    subparsers = parser.add_subparsers(
        dest="command", help="Available commands", required=True
    )

    # parser for uc_generate
    # https://confluence.silabs.com/spaces/UC/pages/109688909/Adapter+Pack+Advanced+Configurators#AdapterPackAdvancedConfigurators-uc_generate
    generate_parser = subparsers.add_parser(
        "generate",
        description="Runs when project is generated using SLC-CLI or Studio.",
    )
    generate_parser.add_argument(
        "input_dir", type=Path, help="Input directory containing .tflite files"
    )
    generate_parser.add_argument(
        "output_dir",
        type=Path,
        help="Output directory to populate with serialized content.",
    )
    generate_parser.add_argument("part_number", type=str, help="Part Number")

    # parser for uc_upgrade
    # https://confluence.silabs.com/spaces/UC/pages/109688909/Adapter+Pack+Advanced+Configurators#AdapterPackAdvancedConfigurators-uc_upgrade
    upgrade_parser = subparsers.add_parser(
        "upgrade",
        description="Runs when project is upgraded using SLC-CLI or Studio.",
    )
    upgrade_parser.add_argument(
        "temp_dir", type=Path, help="Temporary directory for upgrade operation"
    )
    upgrade_parser.add_argument(
        "result_file", type=str, help="YAML file with upgrade results"
    )
    return parser.parse_args()


def setup_logging(output_dir):
    log_file = output_dir / "compiler.log"

    logging.basicConfig(
        filename=log_file,
        encoding="utf-8",
        level=logging.DEBUG,
        format="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
    )


def write_upgrade_result(result_file: Path):
    dummy_result = {
        "upgrade_results": [
            {"message": "No upgrade action taken.", "status": "nothing"}
        ]
    }
    with open(result_file, "w") as rf:
        yaml.dump(dummy_result, rf)


def main():
    args = parse_arguments()

    if args.command == "generate":
        setup_logging(args.output_dir)
        logging.debug(f"Input directory: {args.input_dir}")
        logging.debug(f"Output directory: {args.output_dir}")
        logging.debug(f"Part Number: {args.part_number}")

        model_path = find_first_tflite_file(args.input_dir)
        board_platform = get_board_platform(args.part_number)

        # Skipping if board_platform is not si91x
        if board_platform != "si91x":
            return

        model_name = model_path.stem
        model_name = re.sub(r"[^a-zA-Z0-9_]+|\s+", "_", model_name)

        if sys.platform == "darwin":
            macos_incompatibility_error = (
                "MVP Compiler is not supported on macOS, please use Linux or Windows. "
                "Other errors may occur due to this. The project will not build successfully."
            )
            print(macos_incompatibility_error)
            logging.error(macos_incompatibility_error)
            # MLSW-10582: generate all the files generated by mvp compiler with error macro
            with open("./manifest.yaml", "r") as f:
                files = yaml.safe_load(f)

            files += [
                f"{model_name}_generated.h",
                f"{model_name}_generated.parameters.h",
                f"sl_ml_model_{model_name}.h",
            ]

            for file in files:
                Path(args.output_dir / file).parent.mkdir(parents=True, exist_ok=True)
                with open(args.output_dir / file, "w") as f:
                    f.write(f"#error {macos_incompatibility_error}")
        else:
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

            generate_model_file(
                "templates/",
                "sl_ml_model_model_name.h.jinja",
                model_name,
                args.output_dir,
            )
    elif args.command == "upgrade":
        # some command to upgrade
        write_upgrade_result(args.result_file)


if __name__ == "__main__":
    main()

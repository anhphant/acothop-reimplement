import shutil
import subprocess
import sys
from pathlib import Path

# Danh sách mã instance cần chạy
codes = [
    "d2103_01_bsc_01_01",
    "d15112_01_bsc_01_01",
    "pla7397_01_bsc_01_01",
]

# Thư mục gốc repo chứa run.ps1 và thư mục instances
ROOT = Path(__file__).resolve().parent
RUN_PS1 = ROOT / "run.ps1"
INSTANCES_DIR = ROOT.parent.parent / "instances"

# Nếu bạn muốn dùng đường dẫn output/log khác, sửa tại đây
DEFAULT_OUTPUT_DIR = ROOT / "outputs"
DEFAULT_OUTPUT_DIR.mkdir(exist_ok=True)


def find_powershell() -> str:
    for name in ["powershell.exe", "powershell", "pwsh.exe", "pwsh"]:
        path = shutil.which(name)
        if path:
            return path

    candidate = Path("C:/Windows/System32/WindowsPowerShell/v1.0/powershell.exe")
    if candidate.exists():
        return str(candidate)

    raise FileNotFoundError(
        "Cannot find PowerShell executable. Add PowerShell to PATH or install PowerShell Core."
    )

POWERSHELL = find_powershell()
POWERSHELL_ARGS = [POWERSHELL, "-ExecutionPolicy", "Bypass", "-File", str(RUN_PS1)]


def build_input_path(code: str) -> Path:
    # Tên thư mục instance theo mã trước dấu gạch dưới đầu tiên
    prefix = code.split("_")[0]
    folder = INSTANCES_DIR / f"{prefix}-thop"
    return folder / f"{code}.thop"


def run_code(code: str) -> int:
    input_path = build_input_path(code)
    if not input_path.exists():
        print(f"ERROR: File not found: {input_path}")
        return 1

    output_file = DEFAULT_OUTPUT_DIR / f"{code}.out"
    log_file = DEFAULT_OUTPUT_DIR / f"{code}.log"

    command = POWERSHELL_ARGS + [
        "-InputFile",
        str(input_path),
        "-OutputFile",
        str(output_file),
        "-LogFile",
        str(log_file),
    ]

    print(f"Running {code} using {input_path}")
    result = subprocess.run(command, text=True)
    if result.returncode == 0:
        print(f"{code} completed: output={output_file}, log={log_file}\n")
    else:
        print(f"{code} failed with return code {result.returncode}\n")
    return result.returncode


def main() -> int:
    if not RUN_PS1.exists():
        print(f"ERROR: run.ps1 not found in {ROOT}")
        return 1

    total_failed = 0
    for code in codes:
        rc = run_code(code)
        if rc != 0:
            total_failed += 1

    if total_failed:
        print(f"Finished with {total_failed} failed instance(s).")
        return 1

    print("Finished all instances successfully.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

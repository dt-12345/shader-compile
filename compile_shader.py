import os
from pathlib import Path
import time

INPUT_PATH: Path = Path("sdcard/shaders")
OUTPUT_PATH: Path = Path("sdcard/output")
RYUJINX_PATH: Path = Path(os.environ["APPDATA"]) / Path("ryujinx")
TIMEOUT: float = 10.0

def compile_shaders(names: list[str], sources: list[bytes]) -> list[bytes]:
    inputs: list[Path] = []
    outputs: list[Path] = []
    for i, name in enumerate(names):
        inputs.append(RYUJINX_PATH / INPUT_PATH / Path(name))
        outputs.append(RYUJINX_PATH / OUTPUT_PATH / Path(f"{name}.bin"))
        inputs[i].write_bytes(sources[i])
    start: float = time.time()
    while not all(os.path.exists(output) for output in outputs) and time.time() - start < TIMEOUT: ...
    time.sleep(0.1)
    compiled: list[bytes] = []
    try:
        for output in outputs:
            compiled.append(output.read_bytes())
    finally:
        for i in inputs:
            os.unlink(i)
        for o in outputs:
            os.unlink(o)
    return compiled

def compile_shader(name: str, source: bytes) -> bytes:
    input: Path = RYUJINX_PATH / INPUT_PATH / Path(name)
    output: Path = RYUJINX_PATH / OUTPUT_PATH / Path(f"{name}.bin")
    input.write_bytes(source)
    start: float = time.time()
    while not os.path.exists(output) and time.time() - start < TIMEOUT: ...
    time.sleep(0.1)
    try:
        compiled: bytes = output.read_bytes()
    finally:
        os.unlink(input)
        os.unlink(output)
    return compiled

def main() -> None:
    import argparse
    import sys

    parser: argparse.ArgumentParser = argparse.ArgumentParser("shader-compile", description="Script to compile NVN shaders - Ryujinx should be running in the background already")
    parser.add_argument("--input", "-i", nargs="+", required=True)
    parser.add_argument("--output", "-o", default="")
    parser.add_argument("--ryujinx", help="Path to Ryujinx directory so that Ryujinx/sdcard exists", default=os.path.join(os.environ["APPDATA"], "ryujinx"))
    
    args, _ = parser.parse_known_args()

    paths: list[str] = list(set(args.input))
    output: str = args.output
    global RYUJINX_PATH
    RYUJINX_PATH = Path(args.ryujinx)

    for path in paths:
        if path == "" or not os.path.exists(path):
            print(f"{path} does not exist")
            sys.exit()

    if output:
        os.makedirs(output, exist_ok=True)

    if len(paths) == 1:
        source: bytes = Path(path).read_bytes()
        basename: str = os.path.basename(path)

        compiled: bytes = compile_shader(basename, source)

        with open(os.path.join(output, f"{basename}.bin"), "wb") as f:
            f.write(compiled)
        with open(os.path.join(output, f"{basename}.bin.ctrl"), "wb") as f:
            f.write(compiled[:0x880])
        with open(os.path.join(output, f"{basename}.bin.code"), "wb") as f:
            f.write(compiled[0x880:])
        with open(os.path.join(output, f"{basename}.bin.code.nv"), "wb") as f:
            f.write(compiled[0x880+0x30:])
    else:
        sources: list[bytes] = []
        basenames: list[str] = []
        for path in paths:
            sources.append(Path(path).read_bytes())
            basenames.append(os.path.basename(path))
        
        compiled: list[bytes] = compile_shaders(basenames, sources)

        for i in range(len(sources)):
            with open(os.path.join(output, f"{basenames[i]}.bin"), "wb") as f:
                f.write(compiled[i])
            with open(os.path.join(output, f"{basenames[i]}.bin.ctrl"), "wb") as f:
                f.write(compiled[i][:0x880])
            with open(os.path.join(output, f"{basenames[i]}.bin.code"), "wb") as f:
                f.write(compiled[i][0x880:])
            with open(os.path.join(output, f"{basenames[i]}.bin.code.nv"), "wb") as f:
                f.write(compiled[i][0x880+0x30:])

if __name__ == "__main__":
    main()
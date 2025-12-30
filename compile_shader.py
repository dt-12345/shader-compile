import os
from pathlib import Path
import time

INPUT_PATH: Path = Path("sdcard/shaders")
OUTPUT_PATH: Path = Path("sdcard/output")
RYUJINX_PATH: Path = Path(os.environ["APPDATA"]) / Path("ryujinx")
TIMEOUT: float = 10.0

def compile_shaders(names: list[str], sources: list[bytes]) -> list[tuple[bytes, bytes]]:
    inputs: list[Path] = []
    outputs: list[tuple[Path, Path]] = []
    for i, name in enumerate(names):
        inputs.append(RYUJINX_PATH / INPUT_PATH / Path(name))
        outputs.append((
            RYUJINX_PATH / OUTPUT_PATH / Path(f"{name}.bin.control"),
            RYUJINX_PATH / OUTPUT_PATH / Path(f"{name}.bin.code"),
        ))
        inputs[i].write_bytes(sources[i])
    start: float = time.time()
    while not all(os.path.exists(output[0]) and os.path.exists(output[1]) for output in outputs) and time.time() - start < TIMEOUT: ...
    time.sleep(0.1)
    compiled: list[tuple[bytes, bytes]] = []
    try:
        for output in outputs:
            compiled.append((output[0].read_bytes(), output[1].read_bytes()))
    finally:
        for i in inputs:
            os.unlink(i)
        for o in outputs:
            os.unlink(o[0])
            os.unlink(o[1])
    return compiled

def get_shader_size(control: bytes) -> int:
    return int.from_bytes(control[0x6f8:0x6fc], "little")

def get_constbuf_size(control: bytes) -> int:
    return int.from_bytes(control[0x6fc:0x700], "little")

def get_constbuf_offset(control: bytes) -> int:
    return int.from_bytes(control[0x700:0x704], "little")

def compile_shader(name: str, source: bytes) -> tuple[bytes, bytes]:
    input: Path = RYUJINX_PATH / INPUT_PATH / Path(name)
    control: Path = RYUJINX_PATH / OUTPUT_PATH / Path(f"{name}.bin.control")
    code: Path = RYUJINX_PATH / OUTPUT_PATH / Path(f"{name}.bin.code")
    input.write_bytes(source)
    start: float = time.time()
    while not os.path.exists(control) and not os.path.exists(code) and time.time() - start < TIMEOUT: ...
    time.sleep(0.1)
    try:
        compiled: tuple[bytes, bytes] = (control.read_bytes(), code.read_bytes())
    finally:
        os.unlink(input)
        os.unlink(control)
        os.unlink(code)
    return compiled

def main() -> None:
    import argparse
    import sys

    parser: argparse.ArgumentParser = argparse.ArgumentParser("shader-compile", description="Script to compile NVN shaders - Ryujinx should be running in the background already")
    parser.add_argument("--input", "-i", nargs="+", required=True)
    parser.add_argument("--output", "-o", default="")
    parser.add_argument("--ryujinx", "-r", help="Path to Ryujinx directory so that Ryujinx/sdcard exists", default=os.path.join(os.environ["APPDATA"], "ryujinx"))
    parser.add_argument(
        "--format", "-f", nargs="+", choices=("nv", "raw", "constbuf"), default=[],
        help="Additional format to output shaders in (nv = including Nvidia Shader Header, raw = code only, constbuf = shader constants buffer)"
    )
    
    args, _ = parser.parse_known_args()

    paths: list[str] = list(set(args.input))
    output: str = args.output
    formats: set[str] = set(args.format)
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

        compiled: tuple[bytes, bytes] = compile_shader(basename, source)

        with open(os.path.join(output, f"{basename}.bin.ctrl"), "wb") as f:
            f.write(compiled[0])
        with open(os.path.join(output, f"{basename}.bin.code"), "wb") as f:
            f.write(compiled[1])
        for format in formats:
            if format == "nv":
                with open(os.path.join(output, f"{basename}.bin.code.nv"), "wb") as f:
                    f.write(compiled[1][0x30:0x30+get_shader_size(compiled[0])])
            elif format == "raw":
                with open(os.path.join(output, f"{basename}.bin.code.raw"), "wb") as f:
                    f.write(compiled[1][0x80:0x30+get_shader_size(compiled[0])])
            elif format == "constbuf":
                constbuf_size: int = get_constbuf_size(compiled[0])
                constbuf_offset: int = get_constbuf_offset(compiled[0])
                if constbuf_size > 0:
                    with open(os.path.join(output, f"{basename}.bin.code.constbuf"), "wb") as f:
                        f.write(compiled[1][constbuf_offset:constbuf_offset+constbuf_size])
                else:
                    print(f"{path} has no constants to output, skipping")
    else:
        sources: list[bytes] = []
        basenames: list[str] = []
        for path in paths:
            sources.append(Path(path).read_bytes())
            basenames.append(os.path.basename(path))
        
        compiled: list[tuple[bytes, bytes]] = compile_shaders(basenames, sources)

        for i in range(len(sources)):
            with open(os.path.join(output, f"{basenames[i]}.bin.ctrl"), "wb") as f:
                f.write(compiled[i][0])
            with open(os.path.join(output, f"{basenames[i]}.bin.code"), "wb") as f:
                f.write(compiled[i][1])
            for format in formats:
                if format == "nv":
                    with open(os.path.join(output, f"{basenames[i]}.bin.code.nv"), "wb") as f:
                        f.write(compiled[i][1][0x30:0x30+get_shader_size(compiled[i][0])])
                elif format == "raw":
                    with open(os.path.join(output, f"{basenames[i]}.bin.code.raw"), "wb") as f:
                        f.write(compiled[i][1][0x80:0x30+get_shader_size(compiled[i][0])])
                elif format == "constbuf":
                    constbuf_size: int = get_constbuf_size(compiled[i][0])
                    constbuf_offset: int = get_constbuf_offset(compiled[i][0])
                    if constbuf_size > 0:
                        with open(os.path.join(output, f"{basenames[i]}.bin.code.constbuf"), "wb") as f:
                            f.write(compiled[i][1][constbuf_offset:constbuf_offset+constbuf_size])
                    else:
                        print(f"{paths[i]} has no constants to output, skipping")

if __name__ == "__main__":
    main()
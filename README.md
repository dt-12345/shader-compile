this is a simple mod to compile NVN shaders using a glslc binary for Tears of the Kingdom 1.2.1 (glslc binary not provided, it's shipped with certain games such as Echoes of Wisdom)

the program is very crude, it just repeatedly scans `sd:/shaders/` for files and compiles any shaders it finds outputting them to `sd:/output/` with `.bin` appended to the end of the filename

the output binary contains both the control and gpu code sections (the gpu code section comes after the control)

shader stage is automatically detected from the file extension (`.vert`, `.tesc`, `.tese`, `.geom`, `.frag`, `.comp`) and non-compute shaders are compiled together as a program with other shaders sharing the same filename excluding the extension (glslc requires a vertex shader in the program for tesselation control/evaluation and geometry shaders)

## exlaunch README

# exlaunch
A framework for injecting C/C++ code into Nintendo Switch applications/applet/sysmodules.

> [!NOTE]
> This project is a work in progress. If you have issues, reach out to `shad0w0.` on Discord.

# Credit
- Atmosphère: A great reference and guide.
- oss-rtld: Included for (pending) interop with rtld in applications (License [here](https://github.com/shadowninja108/exlaunch/blob/main/source/lib/reloc/rtld/LICENSE.txt)).

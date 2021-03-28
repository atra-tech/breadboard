# Breadboard: Digital Logic Simulator

## Dependencies

* An OpenGL 3.3 compatible graphics adapter and driver
* SDL2
* SDL2\_image
* GLEW
* CGLM (provided with source)

## Compiling

#### Linux
Run `make` in the project's root directory to compile the application.
The resulting executable is `bin/breadboard`.

#### Windows
Run `win-compile.sh` (Linux) or `win-compile.bat` (Windows) to compile the application for Windows.
The resulting executable is `win32/breadboard.exe`

**NOTE:**  Compiling for Windows requires MinGW with GCC

## Running

**NOTE:**  File paths are absolute at this time, so running the executable from the project's root directory is imperative for the program to be able to find and load resources.

#### Linux
Run `run.sh` from the project's root directory.
OR
Run `bin/breadboard` from the directory that this README file is in.

#### Windows
Run `run.bat` from the project's root directory.
OR
Run `win32/breadboard.exe` from the directory that this README file is in.


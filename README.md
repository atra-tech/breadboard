# Breadboard: Digital Logic Simulator (2021)

A final year university project made as proof that a fully functioning piece of graphical software could be made using my own game engine. This was made with an earlier iteration of the engine, and is the older version of this project. The controls are clunky, and the simulation has some bugs, but it is usable. A complete rework using the newest version of the game engine is currently under construction.

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

**NOTE:**  Compiling for Windows requires MinGW with GCC along with associated dependencies.

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

## Controls
- W, A, S and D to pan the camera
- Click components to lift off board and move.
- Click again to drop components back onto board.
- Left-click and drag to select components in an area.
- Click selected components to lift all in outlined area.
- Left-click and drag to draw wires of the selected colour.
- Right-click to erase wires of the selected colour.
- Click green input buttons to toggle their state.

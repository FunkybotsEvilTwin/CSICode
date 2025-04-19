# Control Surface Integrator

The Control Surface Integrator (CSI) is a Reaper plugin that aims to let you integrate your hardware control surfaces into Reaper.

## Information about the project

The [wiki](./wiki) is the starting point for all the documentation about CSI.

This fork has some improvements and automated build for Windows, MacOS and Linux, original code from
https://github.com/FunkybotsEvilTwin/CSICode
https://github.com/FunkybotsEvilTwin/CSIWiki

## How to build
asically by pressing ctrl+shift+B and choosing 'Build plugin using CMake' in vscode after everything is set up (tested on Windows only).

originally from https://github.com/ak5k/reaper-sdk-vscode
# [Visual Studio Code](https://code.visualstudio.com/) + [CMake](https://cmake.org/) based cross-platform template for developing a [REAPER](https://www.reaper.fm/) [Plug-in Extension](https://www.reaper.fm/sdk/plugin/plugin.php)
An entry level solution for setting up a REAPER Plug-in Extension development and build environment based on Visual Studio Code and CMake. Goal is to setup ReaScript IDE-like working environment for developing a simple scriptlike plugin, with little if any former background on C/C++ or REAPER Plug-in Extension development itself, and begin learning. Layman's documentation is included in the source files. Developed and tested on Windows 10, MacOS High Sierra 10.13.6 and Big Sur 11.1, and Ubuntu 20.04.1. Some familiarity with Visual Studio Code is beneficial. Instructions are based on clean/fresh installation.
## Installation guide
It's recommended to read all steps in advance before beginning installation.
### Windows
Install [Visual Studio Community with Develop C and C++ applications component](https://visualstudio.microsoft.com/vs/features/cplusplus/). Default installation includes MSVC compiler and CMake. Visual Studio installation can be trimmed down before installation or afterwards by cherry-picking only the necessary components from Visual Studio Installer > Individual Components section.
#### Minimal Visual Studio installation
* C++ CMake tools for Windows
* Windows SDK (e.g. Windows 10 SDK (10.0.19041.0) for Windows 10, version 2004)
### MacOS
Perhaps the easiest way is to install [Homebrew](https://brew.sh/). This will also install Apple XCode Command Line Tools. After Homebrew is installed, install CMake with `brew install cmake`. Git can also be installed from Homebrew with `brew install git`.
### Linux
Mainstream Linux distributions usually include compiler and many of other necessary tools. On Ubuntu, command `sudo apt install build-essential cmake gdb git valgrind` installs all necessary tools.
### All platforms
* Install [Visual Studio Code](https://code.visualstudio.com/) (VSCode).
* Install [Git](https://git-scm.com/downloads), if not already installed. 
* On Windows, open **Developer PowerShell (or Command Prompt) for VS**. On MacOS and Linux, use regular Terminal. Change directory to user preferred location for source repositories, or make one.
* Get [reaper-sdk-vscode](https://github.com/ak5k/reaper-sdk-vscode) files, preferably with `git clone --recursive https://github.com/ak5k/reaper-sdk-vscode.git`, or by [creating a new repository from this template](https://docs.github.com/en/free-pro-team@latest/github/creating-cloning-and-archiving-repositories/creating-a-repository-from-a-template) and cloning it to local host. Currently templates don't include submodules, in this case [WDL](https://www.cockos.com/wdl/) library.
* Change directory to root of this repository.
* Install [VSCode C/C++ Extension Pack](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools-extension-pack) with `code --verbose --install-extension ms-vscode.cpptools-extension-pack`. This might take a while.
* If command `code` isn't found on Mac, see [this](https://code.visualstudio.com/docs/setup/mac#_launching-from-the-command-line) and try again.
* Open new VSCode workspace by issuing command `code .` (in root directory of this repository).
* VSCode finishes installing the C/C++ Extensions pack. This is indicated in Status Bar.
* Wait, until all dependencies have been downloaded and installed. This might take a while. 
* Then quit VSCode and restart.
* If VSCode notifies about Reload, then Reload.
* If VSCode notifies to configure project 'plugin' with CMake Tools, choose Yes to configure. CMake Tools can also be set to allow always configure new C/C++ projects.
* Select appropriate build kit for platform and architecture. Visual Studio for Windows, Clang for MacOS and GCC for Linux (e.g. Visual Studio Community Release 2019 - amd64 for modern Windows PC).
* VSCode should also notify about setting up IntelliSense for current workspace. Allow this.
* If this did not happen, these can be set up by issuing VSCode Command Palette Commands (Ctrl/Cmd + Shift + P) `CMake: Configure` and `CMake: Select a Kit`, or from VSCode Status Bar. 
## Template files
### `CMakeLists.txt`
* From VSCode workspace Explorer (Ctrl/Cmd + Shift + E) open up `CMakeLists.txt` file.
* Configuration for plugin and building/compiling.
### `src/main.cpp`
* "The" REAPER plugin.
### `src/my_plugin.cpp`
* "The" actual source.
## First steps
* By default, VSCode builds a debug version of the plugin it by running `CMake: Build` or keyboard shortcut `F7`.
* Install plugin with VSCode command `CMake: Install`.
* Start REAPER, and new plugin and it's Action ("ak5k: my plugin" by default) should show up in the Actions List.
* Running the Action should result in 'hello, world' Console Message.
* [VSCode docs](https://code.visualstudio.com/docs/languages/cpp#_tutorials) and [Microsoft C++ docs](https://docs.microsoft.com/en-us/cpp/cpp/) are a helpful resource. And, of course, [ReaScript, JSFX, REAPER Plug-in Extensions, Developer Forum](https://forum.cockos.com/forumdisplay.php?f=3).
* Choosing between debug and release builds can be done with `CMake: Select Variant`.
* Debugging is launched with `F5`. First time, VSCode opens up default Launch Task configuration for debugging. Choose correct Environment and select Default Configuration. In `launch.json` file, edit the `"program":` value to match REAPER executable/binary installation path, e.g. `"program": "C:/Program Files/REAPER (x64)/reaper.exe"`.
* [VSCode debugger](https://code.visualstudio.com/docs/cpp/cpp-debug) allows step-by-step code execution, watching variables, etc.
* On Windows, VSCode needs to be started from Developer PowerShell (or Command Prompt) for VS.
![image](https://i.imgur.com/N4LuyFV.gif)




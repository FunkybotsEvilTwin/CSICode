# Control Surface Integrator

The Control Surface Integrator (CSI) is a Reaper plugin that aims to let you integrate your hardware control surfaces into Reaper.

## Information about the project

The [wiki](./wiki) is the starting point for all the documentation about CSI.

This fork has some improvements and automated build for Windows, MacOS and Linux, original code from
https://github.com/FunkybotsEvilTwin/CSICode
https://github.com/FunkybotsEvilTwin/CSIWiki


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
* Open the project directory in VSCode
* Install recommended extensions thru extensions panel or recommendations popup or command line or any way you like:
  * [VSCode C/C++ Extension Pack](https://marketplace.visualstudio.com/items/?itemName=ms-vscode.cpptools-extension-pack) `code --verbose --install-extension ms-vscode.cpptools-extension-pack ms-vscode.cmake-tools`.
  * [CMake Tools](https://marketplace.visualstudio.com/items/?itemName=ms-vscode.cmake-tools) with `code --verbose --install-extension ms-vscode.cmake-tools ms-vscode.cmake-tools`. This might take a while.
  * If command `code` isn't found on Mac, see [this](https://code.visualstudio.com/docs/setup/mac#_launching-from-the-command-line) and try again.
* VSCode finishes installing the C/C++ Extensions pack. This is indicated in Status Bar.
* Wait, until all dependencies have been downloaded and installed. This might take a while. 
* Then quit VSCode and restart.
* If VSCode notifies about Reload, then Reload.
* If VSCode notifies to `configure project with CMake Tools`, choose `Yes` to configure. CMake Tools can also be set to allow always configure new C/C++ projects.
* Select appropriate build kit for platform and architecture. Visual Studio for Windows, Clang for MacOS and GCC for Linux (e.g. `Visual Studio Community Release 20XX - amd64` for modern Windows PC).
* VSCode should also notify about setting up IntelliSense for current workspace. Allow this.
* If this did not happen, these can be set up by issuing VSCode Command Palette Commands (Ctrl/Cmd + Shift + P) `CMake: Configure` and `CMake: Select a Kit`, or from VSCode Status Bar. 

## First steps on Windows debugging
* Set environment cariables:
  * `REAPER_PATH` - path to folder with reaper.exe without trailing \ (like C:\Apps\ReaperPortable\App\REAPER)
  * `REAPER_RESOURCE_PATH` - path to folder with your reaper.ini and UserPlugins without trailing \ (like C:\Apps\Data\ReaperPortableConfig) - there will be installed build .dll and debug files (.ilk and .pdb)
* By default, VSCode builds a debug version of the plugin it by running `CMake: Build` or keyboard shortcut `F7`.
* Install plugin with VSCode command `CMake: Install`.
* Start debug in REAPER by `F5`.
* Choosing between debug and release builds can be done with `CMake: Select Variant`.

## Other docs
* [VSCode docs](https://code.visualstudio.com/docs/languages/cpp#_tutorials) and [Microsoft C++ docs](https://docs.microsoft.com/en-us/cpp/cpp/) are a helpful resource. And, of course, [ReaScript, JSFX, REAPER Plug-in Extensions, Developer Forum](https://forum.cockos.com/forumdisplay.php?f=3).





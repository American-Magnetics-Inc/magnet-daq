[logo]:http://www.americanmagnetics.com/images/header_r2_c1.jpg "AMI Logo"

![logo](http://www.americanmagnetics.com/images/header_r2_c1.jpg)

# Magnet-DAQ Application #

This repository is the source code and binary distribution point for the Magnet-DAQ application for comprehensive remote control of the AMI Model 430 Programmer.

**The current application version is 1.13.** An integrated Help file is included that you may preview in PDF format [here](https://bitbucket.org/americanmagneticsinc/magnet-daq/downloads/Magnet-DAQ-Help.pdf). The Magnet-DAQ application *requires* firmware version 2.55 or later in the Model 430 programmer. 

Magnet-DAQ is also a required prerequisite for the [Multi-Axis Operation](https://bitbucket.org/americanmagneticsinc/multi-axis-operation) open source application developed by AMI for control of AMI Maxes(tm) magnet systems.

------

### Included Legacy Firmware Upgrades ###

An integrated Firmware Upgrade Wizard is included in Magnet-DAQ along with the latest Model 430 firmware legacy versions 2.65 and 3.15. Upon connection to a Model 430, the application will present the Firmware Upgrade Wizard as appropriate, or on demand via the "Upgrade" toolbar icon.

**NOTE:** The latest firmware releases include optional communication on Ethernet port 7185 without the "Welcome" message. This is of importance to customers using VISA and LabVIEW to develop "stateless" communication drivers, and in fact the latest [AMI Drivers for LabVIEW](https://bitbucket.org/americanmagneticsinc/ami-drivers) require port 7185.

------

### New Table Feature in Magnet-DAQ

Starting with Magnet-DAQ version 1.08, a Table tab was added supporting a list of target fields that can be optionally auto-stepped by the application. Also included is an option to execute an external application or Python script *at each target*, as well as automatically entering and exiting persistence. These features are intended to allow the customer to use the Magnet-DAQ application for automated data acquisition or other experimental procedures that *repeat* at various field points. Example Python scripts are included in the Help. Check the Help for more information about the new "special variables" in the latest version that allow passing of selected magnet states to external apps/scripts as command line arguments.

![table](https://bitbucket.org/americanmagneticsinc/magnet-daq/raw/fd38e070b36eef59ecea22f000a22da181c272f8/help/images/screenshot4.png)



## User Manual Updates? ##

**ALERT!**  The newest Model 430 hardware revision and firmware releases has a companion manual update (Rev 12, March 2024). Legacy Model 430 hardware is also covered in this new manual release.

* [Model 430 Manual and 4-Quadrant Power Supply Systems including Multi-Axis Systems (Rev 12)](https://bitbucket.org/americanmagneticsinc/magnet-daq/downloads/mn-430-rev12.pdf)

The Rev 10 Bipolar Power Supply System manual for legacy bipolar systems is still available:

* [AMI Bipolar (2-Quadrant) Power Supply Systems (Rev 10)](https://bitbucket.org/americanmagneticsinc/magnet-daq/downloads/mn-Bipolar-rev10.pdf)

## How do I install? ##

Pre-compiled, ready-to-use binaries are available in the Downloads section of this repository:

* [Installer for 64-bit Microsoft Windows 7 or later](https://bitbucket.org/americanmagneticsinc/magnet-daq/downloads/MagnetDAQ-Setup.msi) - Simply download and run the installer.
* [Installer for 32-bit Microsoft Windows 7 or later](https://bitbucket.org/americanmagneticsinc/magnet-daq/downloads/MagnetDAQ-Setup-Win32.msi)

* [Executable for 64-bit Linux (Ubuntu 18.04 or later recommended)](https://bitbucket.org/americanmagneticsinc/magnet-daq/downloads/Magnet-DAQ.tar.gz) - See the README file in the download for the instructions for deploying on Ubuntu.
* [Application for 64-bit Apple macOS (High Sierra or later recommended)](https://bitbucket.org/americanmagneticsinc/magnet-daq/downloads/Magnet-DAQ.dmg) - Download and copy the Magnet-DAQ.app folder to your desired location (Applications folder recommended). Enabling execution via Gatekeeper may be required.

## How do I compile the source to produce my own binary? ##

Please note that this is not *required* since binary distributions are provided above. However, if you wish to produce your own binaries and/or edit the code for custom needs, the following are some suggestions:

* __Summary of set up__
	* Clone or download the source code repository to your local drive.
	
	* *For Windows*: Open the Magnet-DAQ.sln file in [Visual Studio 2019](https://visualstudio.microsoft.com/downloads/). If using Visual Studio, you should also install the [Qt Visual Studio Tools](https://marketplace.visualstudio.com/items?itemName=TheQtCompany.QtVisualStudioTools2019) extension to enable pointing your project to your currently installed Qt distribution for Visual Studio.
	
	* *For Linux and Mac*: Open the Magnet-DAQ.pro file in QtCreator.


* __Dependencies__
	* Requires [Qt 5.15 or later open-source distribution](https://www.qt.io/download-open-source/). Qt 6 is not yet recommended due the removal of FTP support needed for the integrated *Firmware Upgrade Wizard*, although the code *should* all compile on Qt 6.
	* Uses the [QCustom Plot](https://www.qcustomplot.com/) v2.1.1 built as a static library for the Windows version. Include the source code files qcustomplot.cpp and qcustomplot.h in the project if the static library is not available.
	* Files from the [QXlsx project](https://github.com/QtExcel/QXlsx) are included as source directly in the project. Note this library links to private Qt API which may break in future Qt releases.
	* The help file was written using [Help & Manual 7.5.4](https://www.helpandmanual.com/). Reproduction of the Help output included in the binaries requires a license for Help & Manual.


* __Deployment hints for your own compilations__
	* *For Windows*: The MagnetDAQ-Setup-Qt5-x64 and MagnetDAQ-Setup-Win32 folders contain setup projects for producing a Windows installer. This requires the [Visual Studio Installer](https://marketplace.visualstudio.com/items?itemName=VisualStudioClient.MicrosoftVisualStudio2017InstallerProjects) extension. A bin-qt5-x64 and bin32 folder is referenced by the installer where you should place the Qt runtime binaries and plugins for packaging by the installer.
	* *For Linux:* See the README file in the binary download (see above) for the instructions for deploying on Ubuntu. Other versions of Linux may require a different procedure.
	* Linux high-DPI display support functions flawlessly in KDE Plasma (not surprising since KDE is Qt-based). The application may exhibit various unaddressed issues with high-DPI display in other desktop managers such as Unity and Gnome, as does the general desktop environment for those desktop managers at present.
	* *For Mac*: Simply copy the .app folder to the desired location. In order to include the Qt runtime libraries in any app bundle you compile yourself, you should use the [Mac Deployment Tool](https://doc.qt.io/qt-6/macos-deployment.html). The macOS "dark mode" is supported. Note that UI files may require enlarging all font sizes by a factor of ~1.4.

## License ##

The Magnet-DAQ program is free software: you can redistribute it and/or modify it under the terms of the [GNU General Public License](https://www.gnu.org/licenses/gpl.html) as published by the Free Software Foundation, either version 3 of the License, or any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the [GNU General Public License](https://www.gnu.org/licenses/gpl.html) for more details.


## Who do I talk to? ##

<support@americanmagnetics.com>

AMI welcomes comments and feedback in order to continually improve the application to meet the needs of our customers. Also, please feel free to contact us if you encounter installation issues.

## Screenshots ##

![screenshot1](https://bitbucket.org/americanmagneticsinc/magnet-daq/raw/fd38e070b36eef59ecea22f000a22da181c272f8/help/images/screenshot1.png)

![screenshot2](https://bitbucket.org/americanmagneticsinc/magnet-daq/raw/fd38e070b36eef59ecea22f000a22da181c272f8/help/images/screenshot2.png)

![screenshot3](https://bitbucket.org/americanmagneticsinc/magnet-daq/raw/fd38e070b36eef59ecea22f000a22da181c272f8/help/images/screenshot3.png)


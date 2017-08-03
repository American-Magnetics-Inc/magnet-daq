## Magnet-DAQ Application ##

This repository is the source code and binary distribution point for the Magnet-DAQ application for comprehensive remote control of the AMI Model 430 Programmer.

**The current application version is 0.97**. The one remaining major application element to add is integrated Help files.

The Magnet-DAQ application *requires* firmware version 2.50 or later in the Model 430 programmer. An integrated Firmware Upgrade Wizard is included in the application along with the applicable firmware. Upon connection to a Model 430, the application will present the Firmware Upgrade Wizard as appropriate.

Updated Model 430 and Power Supply System manuals are also available to document all the new features in the latest Model 430 firmware:

* [Generic Model 430 Manual including High-Stability Option and 3-Axis Systems (Rev 9)](https://bitbucket.org/americanmagneticsinc/magnet-daq/downloads/mn-430-rev9.pdf)
* [AMI 4-Quadrant Power Supply Systems including High-Stability Option and 3-Axis Systems (Rev 9)](https://bitbucket.org/americanmagneticsinc/magnet-daq/downloads/mn-4QPS-rev9.pdf)
* [AMI Bipolar (2-Quadrant) Power Supply Systems (Rev 9)](https://bitbucket.org/americanmagneticsinc/magnet-daq/downloads/mn-Bipolar-rev9.pdf)

### How do I install? ###

Pre-compiled, ready-to-use binaries are available in the Downloads section of this repository:

* [Installer for 64-bit Microsoft Windows 7 or later](https://bitbucket.org/americanmagneticsinc/magnet-daq/downloads/MagnetDAQ-Setup.msi)
* [Executable for 64-bit Linux (Ubuntu 14.04 or later recommended)](https://bitbucket.org/americanmagneticsinc/magnet-daq/downloads/Magnet-DAQ.zip)
* [Application for 64-bit Apple macOS (Sierra or later recommended)](https://bitbucket.org/americanmagneticsinc/magnet-daq/downloads/Magnet-DAQ.app.zip)

### How do I compile the source? ###

* __Summary of set up__
	* Clone or [download](https://bitbucket.org/americanmagneticsinc/magnet-daq/get/4994a45aecfe.zip) the source code repository to your local drive.
	* *For Windows*: Open the Magnet-DAQ.sln file in [Visual Studio 2017](https://www.visualstudio.com/free-developer-offers/). If using Visual Studio, you should also install the [Qt Visual Studio Tools](https://marketplace.visualstudio.com/items?itemName=TheQtCompany.QtVisualStudioTools-19123) extension to enable pointing your project to your currently installed Qt distribution for Visual Studio.
	* *For Linux and Mac*: Open the Magnet-DAQ.pro file in QtCreator.

* __Dependencies__
	* Requires [Qt 5.9 or later open-source distribution](https://www.qt.io/download-open-source/)

* __Deployment instructions__
	* *For Windows*: The MagnetDAQ-Setup folder contains a setup project for producing a Windows installer. This requires the [Visual Studio Installer](https://marketplace.visualstudio.com/items?itemName=VisualStudioProductTeam.MicrosoftVisualStudio2017InstallerProjects) extension.
	* *For Linux:* See the README file in the binary download (see above) for the instructions for deploying on Ubuntu. Other versions of Linux may require a difference procedure.
	*  *For Mac*: Simply unzip the binary distribution and copy the .app folder to the desired location. In order to include the Qt runtime libraries in any app bundle you compile yourself, you should use the [Mac Deployment Tool](http://doc.qt.io/qt-5/osx-deployment.html#macdeploy).

### License ###

The Magnet-DAQ program is free software: you can redistribute it and/or modify it under the terms of the [GNU General Public License](https://www.gnu.org/licenses/gpl.html) as published by the Free Software Foundation, either version 3 of the License, or any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the [GNU General Public License](https://www.gnu.org/licenses/gpl.html) for more details.


### Who do I talk to? ###

[support@americanmagnetics.com](mailto:support@americanmagnetics.com)

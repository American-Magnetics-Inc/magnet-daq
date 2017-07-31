#include "stdafx.h"
#include "magnetdaq.h"
#include "replytimeout.h"

//---------------------------------------------------------------------------
// Contains methods related to the firmware upgrade wizard. 
// Broken out from magnetdaq.cpp for ease of editing.
//---------------------------------------------------------------------------

enum { INTRO_PAGE, UPLOAD_PAGE, FINISHED_PAGE };
bool ReplyTimeout::timeoutOccurred = false;

//---------------------------------------------------------------------------
// Have to support "legacy" firmware upgrades because we cannot
// easily field-upgrade the CE kernel. The legacy firmware links to
// a specific CE SDK and therefore must be distributed separately.
//---------------------------------------------------------------------------

// required firmware version
const double requiredLegacyFirmwareId = 2.50;
const double requiredFirmwareId = 3.00;

// firmware versions included in Resource file (update these values when new version is included)
const double legacyFirmwareId = 2.50;
const QString legacyFirmwareSuffix = "";
const double firmwareId = 3.00;
const QString firmwareSuffix = "";

//---------------------------------------------------------------------------
//	Returns "true" if acceptable, "false" if upgrade required.
//---------------------------------------------------------------------------
bool magnetdaq::checkFirmwareVersion(void)
{
	double version = model430.firmwareVersion();
	QString suffix = model430.getFirmwareSuffix();

	if (version >= requiredFirmwareId)
	{
		if (version > requiredFirmwareId)
			return true;

		if (version == requiredFirmwareId)
		{
			if (suffix.isEmpty())
				return true;

			if (suffix >= firmwareSuffix)	// minor upgrades
				return true;
		}
	}
	else if (version >= requiredLegacyFirmwareId)
	{
		if (version > requiredLegacyFirmwareId)
			return true;

		if (version == requiredLegacyFirmwareId)
		{
			if (suffix.isEmpty())
				return true;

			if (suffix >= legacyFirmwareSuffix)	// minor upgrades
				return true;
		}
	}
	
	return false;
}

//---------------------------------------------------------------------------
QString magnetdaq::formatFirmwareUpgradeStr(void)
{
	bool isLegacy = model430.firmwareVersion() < 3.00 ? true : false;
	QString version;

	if (isLegacy)
	{
		if (legacyFirmwareSuffix.isEmpty())
			version = QString::number(legacyFirmwareId, 'f', 2);
		else
			version = QString::number(legacyFirmwareId, 'f', 2) + " (" + legacyFirmwareSuffix + ")";
	}
	else
	{
		if (firmwareSuffix.isEmpty())
			version = QString::number(firmwareId, 'f', 2);
		else
			version = QString::number(firmwareId, 'f', 2) + " (" + firmwareSuffix + ")";
	}

	return version;
}

//---------------------------------------------------------------------------
QString magnetdaq::formatFirmwareUpgradeMsg(void)
{
	QString retVal;

	if (model430.getFirmwareSuffix().isEmpty())
	{
		retVal = "Firmware version " + QString::number(model430.firmwareVersion(), 'f', 2) + " detected. Firmware version " +
			formatFirmwareUpgradeStr() + " or later is required.";
	}
	else
	{
		retVal = "Firmware version " + QString::number(model430.firmwareVersion(), 'f', 2) + " (" + model430.getFirmwareSuffix() +
			") detected. Firmware version " + formatFirmwareUpgradeStr() + " or later is required.";
	}

	return retVal;
}

//---------------------------------------------------------------------------
void magnetdaq::initializeUpgradeWizard(void)
{
	upgradeWizard = new QWizard();

	upgradeWizard->setFont(QFont("Segoe UI", 9));
	upgradeWizard->setWizardStyle(QWizard::ClassicStyle);
	upgradeWizard->setPixmap(QWizard::WatermarkPixmap, QPixmap(":/magnetdaq/Resources/appicon.png"));
	upgradeWizard->setPage(INTRO_PAGE, createIntroPage());
	upgradeWizard->setPage(UPLOAD_PAGE, createUploadPage());
	upgradeWizard->setPage(FINISHED_PAGE, createConclusionPage());
	upgradeWizard->setWindowTitle("Model 430 Firmware Upgrade Wizard");
	upgradeWizard->setOptions(QWizard::NoBackButtonOnStartPage | QWizard::NoCancelButtonOnLastPage | QWizard::NoBackButtonOnLastPage);

	connect(upgradeWizard, SIGNAL(currentIdChanged(int)), this, SLOT(upgradeWizardPageChanged(int)));
}

//---------------------------------------------------------------------------
QWizardPage* magnetdaq::createIntroPage(void)
{
	QWizardPage *page = new QWizardPage;
	page->setTitle("Introduction");

	QLabel *label = new QLabel("This wizard will upload new firmware version " + formatFirmwareUpgradeStr() + " to the Model 430 at IP address " + 
		ui.ipAddressEdit->text() + ". Please ensure the power supply is OFF and that any connected magnet is in a safe state before continuing.<br><br>" +
		"This upgrade process will require you to cycle the power on the Model 430 when finished.");
	label->setWordWrap(true);

	QVBoxLayout *layout = new QVBoxLayout;
	layout->addWidget(label);
	page->setLayout(layout);

	return page;
}

//---------------------------------------------------------------------------
QWizardPage* magnetdaq::createUploadPage(void)
{
	QWizardPage *page = new QWizardPage;
	page->setTitle("Upload Firmware");

	QLabel *label = new QLabel("Press Next to upload the firmware upgrade to the Model 430.");
	label->setWordWrap(true);

	QVBoxLayout *layout = new QVBoxLayout;
	layout->addWidget(label);
	page->setLayout(layout);

	return page;
}

//---------------------------------------------------------------------------
QWizardPage* magnetdaq::createConclusionPage(void)
{
	finishedPage = new QWizardPage;
	finishedPage->setTitle("Finished");

	finishedPageLabel = new QLabel();	// set text in method below based on success/failure
	finishedPageLabel->setWordWrap(true);

	QVBoxLayout *layout = new QVBoxLayout;
	layout->addWidget(finishedPageLabel);
	finishedPage->setLayout(layout);

	return finishedPage;
}

//---------------------------------------------------------------------------
void magnetdaq::upgradeWizardPageChanged(int pageId)
{
	if (pageId == FINISHED_PAGE)
	{
		QApplication::setOverrideCursor(Qt::WaitCursor);

		finishedPage->setTitle("Uploading Firmware");
		finishedPageLabel->setText("Please wait while the new firmware uploads to the Model 430 at IP address " + ui.ipAddressEdit->text() + "...");
		upgradeWizard->button(QWizard::FinishButton)->setEnabled(false);

		QUrl uploadURL("ftp://" + ui.ipAddressEdit->text() + "/Upgrade/Model430.exe");
		uploadURL.setUserName("model430admin");
		uploadURL.setPassword("supermagnets");
		uploadURL.setPort(21);
		QNetworkRequest upload(uploadURL);
		QNetworkAccessManager *uploadManager = new QNetworkAccessManager(this);

		bool isLegacy = model430.firmwareVersion() < 3.00 ? true : false;
		QString resourceId;

		if (isLegacy)
			resourceId = ":/magnetdaq/Resources/Legacy/Model430.exe";
		else
			resourceId = ":/magnetdaq/Resources/Model430.exe";

		QResource firmware(resourceId);
		QByteArray firmwareBytes(reinterpret_cast< const char* >(firmware.data()), firmware.size());
		connect(uploadManager, SIGNAL(finished(QNetworkReply *)), this, SLOT(finishedFirmwareUpload(QNetworkReply *)));
		QNetworkReply *reply = uploadManager->put(upload, firmwareBytes);

		// make the upload synchronous with the wizard state and add a timeout of 30 seconds
		QEventLoop loop;
		ReplyTimeout::set(reply, 30000);
		connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
		//connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorDuringFirmwareUpload(QNetworkReply::NetworkError)));
		loop.exec();
	}
}

//---------------------------------------------------------------------------
void magnetdaq::errorDuringFirmwareUpload(QNetworkReply::NetworkError code)
{
	QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());

	finishedPage->setTitle("Finished");
	upgradeWizard->button(QWizard::FinishButton)->setEnabled(true);

	finishedPageLabel->setText("Firmware upload to Model 430 at IP address " + ui.ipAddressEdit->text() +
		" failed with network error: " + reply->errorString());
	qDebug() << "Firmware upload failed to " + ui.ipAddressEdit->text() + " with error: " + reply->errorString();

	QApplication::restoreOverrideCursor();
}

//---------------------------------------------------------------------------
void magnetdaq::finishedFirmwareUpload(QNetworkReply *reply)
{
	finishedPage->setTitle("Finished");
	upgradeWizard->button(QWizard::FinishButton)->setEnabled(true);

	if (reply->error() == QNetworkReply::NoError)
	{
		finishedPageLabel->setText("New firmware has been uploaded to the Model 430. Please Finish this wizard and cycle the Model 430 power.<br><br>"
			"Observe the displayed message during Model 430 boot to verify the firmware has been upgraded to version 2.50 or later.");
		qDebug() << "Successful firmware upload to " + ui.ipAddressEdit->text();
	}
	else if (ReplyTimeout::timeoutOccurred)
	{
		finishedPage->setTitle("Upgrade Failed");
		finishedPageLabel->setText("Firmware upload to Model 430 at IP address " + ui.ipAddressEdit->text() + " failed due to network timeout.");
		qDebug() << "Firmware upload failed to " + ui.ipAddressEdit->text() + " due to network timeout";
	}
	else
	{
		finishedPage->setTitle("Upgrade Failed");
		finishedPageLabel->setText("Firmware upload to Model 430 at IP address " + ui.ipAddressEdit->text() +
			" failed with network error: " + reply->errorString());
		qDebug() << "Firmware upload failed to " + ui.ipAddressEdit->text() + " with error: " + reply->errorString();
	}

	reply->deleteLater();
	QApplication::restoreOverrideCursor();
}

//---------------------------------------------------------------------------
void magnetdaq::showFirmwareUpgradeWizard(void)
{
	if (!upgradeWizard)
		initializeUpgradeWizard();
	else
		upgradeWizard->restart();

	upgradeWizard->show();
}

//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
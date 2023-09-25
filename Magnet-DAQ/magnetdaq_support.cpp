#include "stdafx.h"
#include "magnetdaq.h"

//---------------------------------------------------------------------------
// Contains methods related to the Support tab.
// Broken out from magnetdaq.cpp for ease of editing.
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
void magnetdaq::restoreSupportSettings(QSettings *settings)
{
	// restore values
	ui.caseEdit->setText(settings->value(axisStr + "Support/Subject", "").toString());

	// make connections
	connect(ui.sendSupportEmailButton, SIGNAL(clicked()), this, SLOT(sendSupportEmailClicked()));
	connect(ui.copySettingsToClipboardButton, SIGNAL(clicked()), this, SLOT(copySettingsToClipboard()));
}

//---------------------------------------------------------------------------
void magnetdaq::refreshSupportSettings(void)
{
	if (socket)
	{
		QApplication::setOverrideCursor(Qt::WaitCursor);
		socket->sendExtendedQuery("SETTINGS?\r\n", QueryState::SETTINGS, 4); // 4 second time limit on reply
		QApplication::restoreOverrideCursor();
	}
}

//---------------------------------------------------------------------------
void magnetdaq::syncTextSettings(QString str)
{
	bool isLegacy = model430.firmwareVersion() < 3.00 ? true : false;

	ui.settingsTextEdit->clear();

	// versions 2.10/3.10 and later output this info already
	if ((isLegacy && (model430.firmwareVersion() < 2.10)) || (!isLegacy && (model430.firmwareVersion() < 3.10)))
	{
		// prepend firmware version and serial number
		str.prepend("Firmware Rev: " + QString::number(model430.firmwareVersion(), 'f', 2) + model430.getFirmwareSuffix() + "\n" +
            "Serial Number: " + model430.serialNumber() + "\n" + "Internal Switch S2 State: 0x" + QString::number((ushort)model430.mode(), 16) + "\n\n");
	}
	else
        str.prepend("Internal Switch S2 State: 0x" + QString::number((ushort)model430.mode(), 16) + "\n\n");

	ui.settingsTextEdit->setPlainText(str);
}

//---------------------------------------------------------------------------
void magnetdaq::sendSupportEmailClicked(void)
{
	// Check that settings have been retrieved
	if (ui.settingsTextEdit->toPlainText().isEmpty())
	{
		QMessageBox msgBox;
		msgBox.setText("Retrieve Model 430 Settings");
		msgBox.setInformativeText("Please Connect to a Model 430 in order to retrieve the settings to include in your correspondence.");
		msgBox.setStandardButtons(QMessageBox::Ok);
		msgBox.setDefaultButton(QMessageBox::Ok);
		msgBox.setIcon(QMessageBox::Information);
		int ret = msgBox.exec();
	}
	else
	{
		// Create email body and use system email service
		QString body;

		body = "Notes:\n\n" + ui.notesTextEdit->toPlainText() + "\n\n";
		body += ui.settingsTextEdit->toPlainText();

		QString url = "mailto:support@americanmagnetics.com?subject=" + ui.caseEdit->text() + "&body=" + body;
		QDesktopServices::openUrl(QUrl(url, QUrl::TolerantMode));
	}
}

//---------------------------------------------------------------------------
void magnetdaq::copySettingsToClipboard(void)
{
	QClipboard *clipboard = QApplication::clipboard();
	clipboard->setText(ui.settingsTextEdit->toPlainText());
}

//---------------------------------------------------------------------------
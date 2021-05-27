#include "stdafx.h"
#include "magnetdaq.h"

//---------------------------------------------------------------------------
// Contains methods related to managing the Setup device list.
// Broken out from magnetdaq.cpp for ease of editing.
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
void magnetdaq::restoreDeviceList(QSettings *settings)
{
	// restore data from saved settings
	qRegisterMetaType<QList<QStringList>>("Devices");

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	qRegisterMetaTypeStreamOperators<QList<QStringList>>("Devices");
#endif
	QList<QStringList> data = settings->value("DeviceList").value<QList<QStringList>>();

	// create the table from saved data
	int numRows = data.count();
	ui.devicesTableWidget->setRowCount(numRows);
	ui.devicesTableWidget->setColumnCount(2);
	for (auto row = 0; row < numRows; row++)
		for (auto col = 0; col < 2; col++)
		{
			QTableWidgetItem *item;
			ui.devicesTableWidget->setItem(row, col, item = new QTableWidgetItem(data.at(row).at(col)));
			item->setFlags(item->flags() & ~Qt::ItemIsEditable);
		}

	// restore header layout
	// restore different geometry for different DPI screens
	QScreen* screen = QApplication::primaryScreen();
	QString dpiStr = QString::number(screen->logicalDotsPerInch());
	ui.devicesTableWidget->horizontalHeader()->restoreState(settings->value(axisStr + "DeviceList/HorizontalState/" + dpiStr).toByteArray());
	ui.devicesTableWidget->horizontalHeader()->restoreGeometry(settings->value(axisStr + "DeviceList/HorizontalGeometry/" + dpiStr).toByteArray());

	// make connections
	connect(ui.ipNameEdit, SIGNAL(editingFinished()), this, SLOT(ipNameChanged()));
	connect(ui.devicesTableWidget, SIGNAL(itemSelectionChanged()), this, SLOT(selectedDeviceChanged()));
	connect(ui.deleteDeviceButton, SIGNAL(clicked(bool)), this, SLOT(deleteDeviceClicked(bool)));
}

//---------------------------------------------------------------------------
void magnetdaq::saveDeviceList(void)
{
	QSettings settings;
	QList<QStringList> data;

	// loop through table
	for (int i = 0; i < ui.devicesTableWidget->rowCount(); ++i)
	{
		QStringList rowText;
		rowText << ui.devicesTableWidget->item(i, 0)->text();	// ip name
		rowText << ui.devicesTableWidget->item(i, 1)->text();	// ip address
		data << rowText;
	}

	settings.setValue("DeviceList", QVariant::fromValue(data));
}

//---------------------------------------------------------------------------
void magnetdaq::addOrUpdateDevice(QString ipAddress, QString ipName)
{
	bool deviceInTable = false;

	// first, check if device is already in table
	for (int i = 0; i < ui.devicesTableWidget->rowCount(); ++i)
	{
		if (ui.devicesTableWidget->item(i, 1)->text() == ipAddress)
		{
			deviceInTable = true;

			// update ipName
			if (ui.devicesTableWidget->item(i, 0)->text() != ipName)
			{
				ui.devicesTableWidget->item(i, 0)->setText(ipName);
				saveDeviceList();
			}
		}
	}

	if (!deviceInTable)		// if not in table, add it
	{
		ui.devicesTableWidget->setSortingEnabled(false);
		int insertedRow = ui.devicesTableWidget->rowCount();

		QTableWidgetItem *col1, *col2;
		ui.devicesTableWidget->insertRow(insertedRow);
		ui.devicesTableWidget->setItem(insertedRow, 0, col1 = new QTableWidgetItem(ipName));
		col1->setFlags(col1->flags() & ~Qt::ItemIsEditable);
		ui.devicesTableWidget->setItem(insertedRow, 1, col2 = new QTableWidgetItem(ipAddress));
		col2->setFlags(col2->flags() & ~Qt::ItemIsEditable);
		ui.devicesTableWidget->setSortingEnabled(true);

		saveDeviceList();
	}
}

//---------------------------------------------------------------------------
void magnetdaq::ipNameChanged(void)
{
	if (socket)
	{
		// check to see that the name really changed from what is stored
		for (int i = 0; i < ui.devicesTableWidget->rowCount(); ++i)
		{
			if (ui.devicesTableWidget->item(i, 1)->text() == ui.ipAddressEdit->text())
			{
				// update ipName
				if (ui.devicesTableWidget->item(i, 0)->text() != ui.ipNameEdit->text())
				{
					ui.devicesTableWidget->item(i, 0)->setText(ui.ipNameEdit->text());
					saveDeviceList();

					// send command to change in connected model 430
					socket->sendCommand("CONF:IPNAME " + ui.ipNameEdit->text() + "\r\n");

					// update window title
					setDeviceWindowTitle();
				}
			}
		}
	}
}

//---------------------------------------------------------------------------
void magnetdaq::selectedDeviceChanged(void)
{
	QModelIndexList selection = ui.devicesTableWidget->selectionModel()->selectedRows();

	if (selection.count())
	{
		// we've already set the UI to only allow one selection by row
		int index = selection.at(0).row();

		// fill the Setup fields in preparation for a Connect (Run) action
		ui.ipAddressEdit->setText(ui.devicesTableWidget->item(index, 1)->text());
		ui.ipNameEdit->setText(ui.devicesTableWidget->item(index, 0)->text());

		ui.deleteDeviceButton->setEnabled(true);
	}
	else
		ui.deleteDeviceButton->setEnabled(false);
}

//---------------------------------------------------------------------------
void magnetdaq::deleteDeviceClicked(bool checked)
{
	QModelIndexList selection = ui.devicesTableWidget->selectionModel()->selectedRows();

	if (selection.count())
	{
		// we've already set the UI to only allow one selection by row
		int row = selection.at(0).row();

		// delete the selection and save the modified device list
		ui.devicesTableWidget->removeRow(row);
		saveDeviceList();
	}
}

//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
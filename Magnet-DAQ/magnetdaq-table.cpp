#include "stdafx.h"
#include "magnetdaq.h"

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
#include <unistd.h>
// on Linux and macOS we include the Qxlsx source in the ./header and ./source folders
#include "header/xlsxdocument.h"
#else
#include <xlsxdocument.h>
#endif

//---------------------------------------------------------------------------
// Local constants and static variables.
//---------------------------------------------------------------------------

// needed for ramp segments calculations
constexpr auto FUDGE_FACTOR = 1e-12;

// make this a user preference?
constexpr auto SETTLING_TIME = 20;

double targetValue;
int quenchPassCnt;

// flag that indicates whether app/script has executed at current table step
bool haveExecuted = false;

// flag that indicates when the table is loading to temporarily suspend time calcs
bool tableIsLoading = false;

//---------------------------------------------------------------------------
// Utility function
//---------------------------------------------------------------------------
double avoidSignedZeroOutput(double number, int precision)
{
	if ((number < 0.0) && (-log10(fabs(number)) > precision))
	{
		return 0.0;
	}
	else
	{
		return number;
	}
}


//---------------------------------------------------------------------------
// Contains methods related to the Table tab view.
// Broken out from magnetdaq.cpp for ease of editing.
//---------------------------------------------------------------------------

void magnetdaq::restoreTableSettings(QSettings *settings)
{
	// initializations
	tableUnits = AMPS;	// amps by default
	ui.tableWidget->setMinimumNumCols(3); // field, hold time, pass/fail

	// restore any persistent values
	ui.autosaveReportCheckBox->setChecked(settings->value("Table/AutosaveReport", false).toBool());
	ui.executeCheckBox->setChecked(settings->value("Table/EnableExecution", false).toBool());
	ui.appLocationEdit->setText(settings->value("Table/AppPath", "").toString());
	ui.appArgsEdit->setText(settings->value("Table/AppArgs", "").toString());
	ui.pythonPathEdit->setText(settings->value("Table/PythonPath", "").toString());
	ui.appStartEdit->setText(settings->value("Table/ExecutionTime", "").toString());
	ui.pythonCheckBox->setChecked(settings->value("Table/PythonScript", false).toBool());

	// execution initially disabled until connect
	ui.manualControlGroupBox->setEnabled(false);
	ui.autoStepGroupBox->setEnabled(false);

	// create manual control timer
	manualCtrlTimer = new QTimer(this);
	manualCtrlTimer->setInterval(1000);	// update once per second
	connect(manualCtrlTimer, SIGNAL(timeout()), this, SLOT(manualCtrlTimerTick()));

	// create autostep timer
	autostepTimer = new QTimer(this);
	autostepTimer->setInterval(1000);	// update once per second
	connect(autostepTimer, SIGNAL(timeout()), this, SLOT(autostepTimerTick()));

	// init app/script execution
	appCheckBoxChanged(0);

	// make connections
	connect(ui.importDataButton, SIGNAL(clicked()), this, SLOT(actionLoad_Table()));
	connect(ui.saveDataButton, SIGNAL(clicked()), this, SLOT(actionSave_Table()));
	connect(ui.saveToExcelReportButton, SIGNAL(clicked()), this, SLOT(actionGenerate_Excel_Report()));
	connect(ui.tableWidget, SIGNAL(itemSelectionChanged()), this, SLOT(tableSelectionChanged()));
	connect(ui.tableWidget, SIGNAL(itemChanged(QTableWidgetItem*)), this, SLOT(tableItemChanged(QTableWidgetItem*)));
	connect(ui.startIndexEdit, SIGNAL(editingFinished()), this, SLOT(autostepRangeChanged()));
	connect(ui.endIndexEdit, SIGNAL(editingFinished()), this, SLOT(autostepRangeChanged()));
	connect(ui.executeCheckBox, SIGNAL(stateChanged(int)), this, SLOT(appCheckBoxChanged(int)));
	connect(ui.pythonCheckBox, SIGNAL(stateChanged(int)), this, SLOT(pythonCheckBoxChanged(int)));
	connect(ui.appLocationButton, SIGNAL(clicked()), this, SLOT(browseForAppPath()));
	connect(ui.pythonLocationButton, SIGNAL(clicked()), this, SLOT(browseForPythonPath()));
	connect(ui.executeNowButton, SIGNAL(clicked()), this, SLOT(executeNowClick()));
}

//---------------------------------------------------------------------------
void magnetdaq::actionLoad_Table(void)
{
	QSettings settings;
	lastTableLoadPath = settings.value("LastTableFilePath").toString();
	bool convertFieldUnits = false;

	if (manualCtrlTimer->isActive())
	{
		abortManualCtrl("Table target interrupted by new table import action");
		rampPauseButtonClicked();
	}

	tableFileName = QFileDialog::getOpenFileName(this, "Choose Table File", lastTableLoadPath, "Table Files (*.txt *.log *.csv)");

	if (!tableFileName.isEmpty())
	{
		QApplication::setOverrideCursor(Qt::WaitCursor);

		lastTableLoadPath = tableFileName;

		FILE *inFile;
		inFile = fopen(tableFileName.toLocal8Bit(), "r");

        if (inFile != nullptr)
		{
			QTextStream in(inFile);

			// read first line for for target value units in header
			QString str = in.readLine();

			// convert to all caps and remove all leading/trailing whitespace
			str = str.toUpper();
			str = str.trimmed();

			if (str.contains("(KG)") || str.contains("(KILOGAUSS)"))
			{
				if (model430.fieldUnits() == TESLA)
				{
					tableUnits = TESLA;
					convertFieldUnits = true;
				}
				else
					tableUnits = KG;
			}
			else if (str.contains("(T)") || str.contains("(TESLA)"))
			{
				if (model430.fieldUnits() == KG)
				{
					tableUnits = KG;
					convertFieldUnits = true;
				}
				else
					tableUnits = TESLA;
			}
			else
			{
				// assume AMPS
				tableUnits = AMPS;
			}

			fclose(inFile);
		}

		// clear status text
		statusMisc->clear();

		if ((model430.mode() & SHORT_SAMPLE_MODE) && tableUnits != AMPS)
		{
			QApplication::restoreOverrideCursor();

			QMessageBox msgBox;
			msgBox.setText("File Import Error");
			msgBox.setInformativeText("Cannot load target values in field units in Short-Sample Mode.");
			msgBox.setStandardButtons(QMessageBox::Ok);
			msgBox.setDefaultButton(QMessageBox::Ok);
			msgBox.setIcon(QMessageBox::Critical);
			int ret = msgBox.exec();
		}
		else
		{
			// now read in data
			tableIsLoading = true;

			if (model430.switchInstalled())
				ui.tableWidget->loadFromFile(tableFileName, true, 1);
			else
				ui.tableWidget->loadFromFile(tableFileName, false, 1);

			// save path
			settings.setValue("LastTableFilePath", lastTableLoadPath);

			presentTableValue = -1;	// no selection
			ui.startIndexEdit->clear();
			ui.endIndexEdit->clear();

			// set headings as appropriate
			setTableHeader();

			if (convertFieldUnits)
			{
				convertFieldValues(tableUnits, true);
			}

			QApplication::restoreOverrideCursor();
			tableSelectionChanged();

			tableIsLoading = false;
		}
	}
}

//---------------------------------------------------------------------------
void magnetdaq::convertFieldValues(int newUnits, bool forceConversion)
{
	if ((tableUnits != AMPS && (newUnits != tableUnits)) || forceConversion)
	{
		// loop through table and convert target values
		int numRows = ui.tableWidget->rowCount();

		for (int i = 0; i < numRows; i++)
		{
			QTableWidgetItem *cell = ui.tableWidget->item(i, 0);
			QString cellStr = cell->text();

			if (!cellStr.isEmpty())
			{
				// try to convert to number
				bool ok;
				double value = cellStr.toDouble(&ok);

				if (ok)
				{
					if (newUnits == TESLA)	// convert from KG
						value /= 10.0;
					else if (newUnits == KG)
						value *= 10.0;

					cell->setText(QString::number(value, 'g', 10));
				}
			}
		}

		// save new units
		tableUnits = newUnits;

		// update table header
		setTableHeader();
	}
}

//---------------------------------------------------------------------------
void magnetdaq::setTableHeader(void)
{
	// set target column units
	if (tableUnits == AMPS)
	{
		ui.tableWidget->horizontalHeaderItem(0)->setText("Target Current (A)");
	}
	else
	{
		if (model430.fieldUnits() == KG)
		{
			ui.tableWidget->horizontalHeaderItem(0)->setText("Target Field (kG)");
		}
		else
		{
			ui.tableWidget->horizontalHeaderItem(0)->setText("Target Field (T)");
		}
	}

	// set hold time format
	if (model430.switchInstalled())
		ui.tableWidget->horizontalHeaderItem(1)->setText("Enter Persistence?/\nHold Time (sec)");
	else
		ui.tableWidget->horizontalHeaderItem(1)->setText("Hold Time (sec)");
}

//---------------------------------------------------------------------------
void magnetdaq::syncTableUnits(void)
{
	// are we locally working in ramping units of A?
	if (ui.rampUnitsComboBox->currentIndex() == 0)
	{
		if (tableUnits != AMPS && model430.coilConstant() > 0.0)
		{
			// convert field values to amps
			// loop through table and convert target values
			int numRows = ui.tableWidget->rowCount();

			for (int i = 0; i < numRows; i++)
			{
				QTableWidgetItem *cell = ui.tableWidget->item(i, 0);
				QString cellStr = cell->text();

				if (!cellStr.isEmpty())
				{
					// try to convert to number
					bool ok;
					double value = cellStr.toDouble(&ok);

					if (ok)
					{
						// coil constant is stored in Model 430 field units
						value /= model430.coilConstant();
						cell->setText(QString::number(value, 'g', 10));
					}
				}
			}

			tableUnits = AMPS;
			setTableHeader();
		}
	}

	// are we locally working in units of kG or T?
	else if (ui.rampUnitsComboBox->currentIndex() == 1)
	{
		if (tableUnits == AMPS && model430.coilConstant() > 0.0)
		{
			// convert current values to field
			// loop through table and convert target values
			int numRows = ui.tableWidget->rowCount();

			for (int i = 0; i < numRows; i++)
			{
				QTableWidgetItem *cell = ui.tableWidget->item(i, 0);
				QString cellStr = cell->text();

				if (!cellStr.isEmpty())
				{
					// try to convert to number
					bool ok;
					double value = cellStr.toDouble(&ok);

					if (ok)
					{
						// coil constant is stored in Model 430 field units
						value *= model430.coilConstant();
						cell->setText(QString::number(value, 'g', 10));
					}
				}
			}

			if (model430.fieldUnits() == KG)
				tableUnits = KG;
			else if (model430.fieldUnits() == TESLA)
				tableUnits = TESLA;

			setTableHeader();
		}

		else if (tableUnits != model430.fieldUnits() && model430.coilConstant() > 0.0)
		{
			if (model430.fieldUnits() == KG)
				convertFieldValues(TESLA, true);
			else if (model430.fieldUnits() == TESLA)
				convertFieldValues(KG, true);
		}
	}
}

//---------------------------------------------------------------------------
void magnetdaq::actionSave_Table(void)
{
	QSettings settings;
	lastTableSavePath = settings.value("LastTableSavePath").toString();

	saveTableFileName = QFileDialog::getSaveFileName(this, "Save Table to File", lastTableSavePath, "Table Files (*.txt *.log *.csv)");

	if (!saveTableFileName.isEmpty())
	{
		QApplication::setOverrideCursor(Qt::WaitCursor);

		lastTableSavePath = saveTableFileName;

		FILE *outFile;
		outFile = fopen(saveTableFileName.toLocal8Bit(), "w");

		// save table contents
		ui.tableWidget->saveToFile(saveTableFileName);

		// save path
		settings.setValue("LastTableSavePath", lastTableSavePath);

		QApplication::restoreOverrideCursor();
	}
}

//---------------------------------------------------------------------------
void magnetdaq::tableSelectionChanged(void)
{
	if (autostepTimer->isActive())
	{
		// no table mods during autostep!
		ui.addRowAboveToolButton->setEnabled(false);
		ui.addRowBelowToolButton->setEnabled(false);
		ui.removeRowToolButton->setEnabled(false);
		ui.tableClearToolButton->setEnabled(false);
		ui.togglePersistenceToolButton->setEnabled(false);
	}
	else
	{
		// any selected vectors?
		QList<QTableWidgetItem *> list = ui.tableWidget->selectedItems();

		if (list.count())
		{
			ui.addRowAboveToolButton->setEnabled(true);
			ui.addRowBelowToolButton->setEnabled(true);
			ui.removeRowToolButton->setEnabled(true);
		}
		else
		{
			if (ui.tableWidget->rowCount())
			{
				ui.addRowAboveToolButton->setEnabled(false);
				ui.addRowBelowToolButton->setEnabled(false);
			}
			else
			{
				ui.addRowAboveToolButton->setEnabled(true);
				ui.addRowBelowToolButton->setEnabled(true);
			}

			ui.removeRowToolButton->setEnabled(false);
		}

		if (ui.tableWidget->rowCount())
		{
			ui.tableClearToolButton->setEnabled(true);

			if (model430.switchInstalled())
				ui.togglePersistenceToolButton->setEnabled(true);
		}
		else
		{
			ui.tableClearToolButton->setEnabled(false);
			ui.togglePersistenceToolButton->setEnabled(false);
		}
	}
}

//---------------------------------------------------------------------------
void magnetdaq::tableAddRowAbove(void)
{
	int newRow = -1;
	tableIsLoading = true;

	// find selected vector
	QList<QTableWidgetItem *> list = ui.tableWidget->selectedItems();

	if (list.count())
	{
		int currentRow = list[0]->row();
		ui.tableWidget->insertRow(currentRow);
		newRow = currentRow;
	}
	else
	{
		if (ui.tableWidget->rowCount() == 0)
		{
			ui.tableWidget->insertRow(0);
			newRow = 0;
		}
	}

	if (newRow > -1)
	{
		initNewRow(newRow);
		updatePresentTableSelection(newRow, false);

		tableIsLoading = false;
		tableSelectionChanged();
	}

	tableIsLoading = false;
}

//---------------------------------------------------------------------------
void magnetdaq::tableAddRowBelow(void)
{
	int newRow = -1;
	tableIsLoading = true;

	// find selected vector
	QList<QTableWidgetItem *> list = ui.tableWidget->selectedItems();

	if (list.count())
	{
		int currentRow = list[0]->row();
		ui.tableWidget->insertRow(currentRow + 1);
		newRow = currentRow + 1;
	}
	else
	{
		if (ui.tableWidget->rowCount() == 0)
		{
			ui.tableWidget->insertRow(0);
			newRow = 0;
		}
	}

	if (newRow > -1)
	{
		initNewRow(newRow);
		updatePresentTableSelection(newRow, false);

		tableIsLoading = false;
		tableSelectionChanged();
	}

	tableIsLoading = false;
}

//---------------------------------------------------------------------------
void magnetdaq::initNewRow(int newRow)
{
	int numColumns = ui.tableWidget->columnCount();

	// initialize cells
	for (int i = 0; i < numColumns; ++i)
	{
		QTableWidgetItem *cell = ui.tableWidget->item(newRow, i);

        if (cell == nullptr)
			ui.tableWidget->setItem(newRow, i, cell = new QTableWidgetItem(""));
		else
			cell->setText("");

		if (i == (numColumns - 1))
			cell->setTextAlignment(Qt::AlignCenter);
		else
			cell->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

		if (i == 1 && model430.switchInstalled())
		{
			cell->setFlags(cell->flags() | Qt::ItemIsUserCheckable);

			if (cell->text().length() > 0)
				cell->setCheckState(Qt::Checked);
			else
				cell->setCheckState(Qt::Unchecked);
		}
		else
		{
			cell->setFlags(cell->flags() & ~Qt::ItemIsUserCheckable);
		}
	}
}

//---------------------------------------------------------------------------
void magnetdaq::updatePresentTableSelection(int row, bool removed)
{
	if (removed)
	{
		if (presentTableValue == row)
		{
			lastStatusMiscString.clear();
			setStatusMsg("");
			presentTableValue = lastTableValue = -1;
		}
		else if (row < presentTableValue)
		{
			// selection shift up by one
			presentTableValue--;
			lastTableValue = presentTableValue;
			lastStatusMiscString = "Target Vector : Table Row #" + QString::number(presentTableValue + 1);
			setStatusMsg(lastStatusMiscString);
		}
	}
	else
	{
		if (row <= presentTableValue)
		{
			// selection shifted down by one
			presentTableValue++;
			lastTableValue = presentTableValue;
			lastStatusMiscString = "Target Value : Table Row #" + QString::number(presentTableValue + 1);
			setStatusMsg(lastStatusMiscString);
		}
	}
}

//---------------------------------------------------------------------------
void magnetdaq::tableRemoveRow(void)
{
	// find selected vector
	QList<QTableWidgetItem *> list = ui.tableWidget->selectedItems();

	if (list.count())
	{
		int currentRow = list[0]->row();
		ui.tableWidget->removeRow(currentRow);
		updatePresentTableSelection(currentRow, true);
		tableSelectionChanged();
	}
}

//---------------------------------------------------------------------------
void magnetdaq::tableClear(void)
{
	int numRows = ui.tableWidget->rowCount();

	if (numRows)
	{
		for (int i = numRows; i > 0; i--)
			ui.tableWidget->removeRow(i - 1);
	}

	presentTableValue = lastTableValue = -1;
	ui.startIndexEdit->clear();
	ui.endIndexEdit->clear();
	lastStatusMiscString.clear();
	setStatusMsg("");
	tableSelectionChanged();
}

//---------------------------------------------------------------------------
// Toggles persistence for all entries in table.
void magnetdaq::tableTogglePersistence(void)
{
	int numRows = ui.tableWidget->rowCount();

	if (numRows)
	{
		for (int i = numRows; i > 0; i--)
		{
			QTableWidgetItem *cell = ui.tableWidget->item(i-1, 1);

			if (cell)
			{
				if (cell->checkState() == Qt::Checked)
					cell->setCheckState(Qt::Unchecked);
				else
					cell->setCheckState(Qt::Checked);
			}
		}
	}
}

//---------------------------------------------------------------------------
void magnetdaq::tableItemChanged(QTableWidgetItem *item)
{
	// recalculate time after change
	if (!tableIsLoading)
		recalculateRemainingTime();
}

//---------------------------------------------------------------------------
void magnetdaq::actionGenerate_Excel_Report(void)
{
	QSettings settings;
	lastReportPath = settings.value("LastReportPath").toString();

	reportFileName = QFileDialog::getSaveFileName(this, "Save Excel Report File", lastReportPath, "Excel Report Files (*.xlsx)");

	if (!reportFileName.isEmpty())
		saveReport(reportFileName);
}

//---------------------------------------------------------------------------
void magnetdaq::saveReport(QString reportFileName)
{
	if (!reportFileName.isEmpty())
	{
		QSettings settings;
		QString tempStr;
		QXlsx::Document xlsx;
		QString limitsUnitsStr;
		QString rateUnitsStr;
		QString pm(const QChar(0x00B1));	// plus minus character

		// save filename path
		settings.setValue("LastReportPath", reportFileName);

		// set document properties
		xlsx.setDocumentProperty("title", "AMI Magnet " + ui.caseEdit->text() + " Test Report");
		xlsx.setDocumentProperty("subject", "Magnet ID " + ui.caseEdit->text());
		xlsx.setDocumentProperty("company", "American Magnetics, Inc.");
		xlsx.setDocumentProperty("description", "Test Results");

		QXlsx::Format alignRightFormat;
		QXlsx::Format alignCenterFormat;
		QXlsx::Format boldAlignRightFormat;
		QXlsx::Format boldAlignCenterFormat;
		QXlsx::Format boldAlignLeftFormat;
		alignRightFormat.setHorizontalAlignment(QXlsx::Format::AlignRight);
		alignCenterFormat.setHorizontalAlignment(QXlsx::Format::AlignHCenter);
		boldAlignRightFormat.setFontBold(true);
		boldAlignRightFormat.setHorizontalAlignment(QXlsx::Format::AlignRight);
		boldAlignCenterFormat.setFontBold(true);
		boldAlignCenterFormat.setHorizontalAlignment(QXlsx::Format::AlignHCenter);
		boldAlignLeftFormat.setFontBold(true);
		boldAlignLeftFormat.setHorizontalAlignment(QXlsx::Format::AlignLeft);

		xlsx.deleteSheet("Sheet1");

		// create table output sheet
		xlsx.addSheet("Table Results");
		xlsx.setColumnWidth(1, 11, 15);

		// write magnet ID
		xlsx.write("A1", "Magnet ID:", boldAlignRightFormat);
		xlsx.write("B1", ui.caseEdit->text());

		// output date/time
		xlsx.write("A2", "Date:", boldAlignRightFormat);
		xlsx.write("B2", QDate::currentDate().toString());
		xlsx.write("A3", "Time:", boldAlignRightFormat);
		xlsx.write("B3", QTime::currentTime().toString());

		if (tableUnits == AMPS)
		{
			limitsUnitsStr = "(A)";

			if (model430.rampRateTimeUnits() == 0)
				rateUnitsStr = "(A/sec)";
			else
				rateUnitsStr = "(A/min)";
		}
		else if (tableUnits == KG)
		{
			limitsUnitsStr = "(kG)";

			if (model430.rampRateTimeUnits() == 0)
				rateUnitsStr = "(kG/sec)";
			else
				rateUnitsStr = "(kG/min)";
		}
		else if (tableUnits == TESLA)
		{
			limitsUnitsStr = "(T)";

			if (model430.rampRateTimeUnits() == 0)
				rateUnitsStr = "(T/sec)";
			else
				rateUnitsStr = "(T/min)";
		}

		// output ramp rates
		xlsx.write("A5", "Ramp Segments", boldAlignLeftFormat);
		xlsx.write("A6", "Limits " + limitsUnitsStr + ":", boldAlignRightFormat);
		xlsx.write("A7", "Rate " + rateUnitsStr + ":", boldAlignRightFormat);

		if (tableUnits == AMPS)
			xlsx.write("B6", QString("0.0 to ") + pm + QString::number(model430.currentRampLimits[0](), 'f', 1), alignCenterFormat);
		else
			xlsx.write("B6", QString("0.0 to ") + pm + QString::number(model430.fieldRampLimits[0](), 'f', 1), alignCenterFormat);

		for (int i = 0; i < model430.rampRateSegments(); i++)
		{
			if (tableUnits == AMPS)
			{
				if (i > 0)
					xlsx.write(6, i + 2, pm + QString::number(model430.currentRampLimits[i - 1](), 'f', 1) + QString(" to ") + pm + QString::number(model430.currentRampLimits[i](), 'f', 1), alignCenterFormat);

				xlsx.write(7, i + 2, QString::number(model430.currentRampRates[i](), 'g', 10), alignCenterFormat);
			}
			else
			{
				if (i > 0)
					xlsx.write(6, i + 2, pm + QString::number(model430.fieldRampLimits[i - 1](), 'f', 1) + QString(" to ") + pm + QString::number(model430.fieldRampLimits[i](), 'f', 1), alignCenterFormat);

				xlsx.write(7, i + 2, QString::number(model430.fieldRampRates[i](), 'g', 10), alignCenterFormat);
			}
		}

		// output headers
		for (int i = 0; i < ui.tableWidget->columnCount(); i++)
			xlsx.write(9, i + 1, ui.tableWidget->horizontalHeaderItem(i)->text(), boldAlignCenterFormat);

		// output data
		for (int i = 0; i < ui.tableWidget->rowCount(); i++)
		{
			for (int j = 0; j < ui.tableWidget->columnCount(); j++)
			{
				QTableWidgetItem *item;

                if ((item = ui.tableWidget->item(i, j)))
				{
					if (item->text().contains("Pass") || item->text().contains("Fail") || item->text().contains(":") || item->text().isEmpty())
					{
						xlsx.write(i + 10, j + 1, ui.tableWidget->item(i, j)->text(), alignCenterFormat);
					}
					else
					{
						if (model430.switchInstalled() && j == 1)
						{
							if (item->checkState() == Qt::Checked)
								xlsx.write(i + 10, j + 1, "Yes / " + ui.tableWidget->item(i, j)->text(), alignRightFormat);
							else
								xlsx.write(i + 10, j + 1, "No / " + ui.tableWidget->item(i, j)->text(), alignRightFormat);
						}
						else
							xlsx.write(i + 10, j + 1, ui.tableWidget->item(i, j)->text().toDouble(), alignRightFormat);
					}
				}
			}
		}

		xlsx.saveAs(reportFileName);
	}
}

//---------------------------------------------------------------------------
void magnetdaq::goToSelectedTableEntry(void)
{
	if (socket)
	{
		if (socket->isConnected())
		{
			socket->getState();

			if (model430.state() < State::QUENCH || model430.state() == State::AT_ZERO || model430.state() == State::ZEROING)
			{
				// find selected table row
				QList<QTableWidgetItem *> list = ui.tableWidget->selectedItems();

				if (list.count())
				{
					presentTableValue = list[0]->row();

					if (goToTableSelection(presentTableValue, true))
					{
						manualCtrlTimer->start();
						quenchPassCnt = 0;
					}
				}
			}
			else
			{
				showErrorString("Cannot set new target in present Model 430 state", true);
				lastStatusMiscString.clear();
				QApplication::beep();
			}
		}
	}
}

//---------------------------------------------------------------------------
void magnetdaq::goToNextTableEntry(void)
{
	if (socket)
	{
		if (socket->isConnected())
		{
			socket->getState();

			if (model430.state() < State::QUENCH || model430.state() == State::AT_ZERO || model430.state() == State::ZEROING)
			{
				// find selected row
				QList<QTableWidgetItem *> list = ui.tableWidget->selectedItems();

				if (list.count())
				{
					presentTableValue = list[0]->row();

					// is next target a valid request?
					if (((presentTableValue + 1) >= 1) && ((presentTableValue + 1) < ui.tableWidget->rowCount()))
					{
						presentTableValue++;	// choose next vector

						// highlight row in table
						ui.tableWidget->selectRow(presentTableValue);

						// send target!
						if (goToTableSelection(presentTableValue, true))
						{
							manualCtrlTimer->start();
							quenchPassCnt = 0;
						}
					}
				}
			}
			else
			{
				showErrorString("Cannot set new target in present Model 430 state", true);
				lastStatusMiscString.clear();
				QApplication::beep();
			}
		}
	}
}

//---------------------------------------------------------------------------
TableError magnetdaq::checkNextTarget(double target, QString label)
{
	double checkValue = target;

	// convert to amps if in field units
	if (tableUnits == KG || tableUnits == TESLA)
		checkValue = fabs(target / model430.coilConstant());

	if (fabs(checkValue) > model430.currentLimit())
	{
		showErrorString(label + " exceeds Current Limit!", true);
		QApplication::beep();
		return TableError::EXCEEDS_CURRENT_LIMIT;
	}

	return TableError::NO_TABLE_ERROR;
}

//---------------------------------------------------------------------------
// Sends next target down to 430
//---------------------------------------------------------------------------
void magnetdaq::sendNextTarget(double target)
{
	if (tableUnits == AMPS) // Amps
	{
		// calculate ramping time
		targetValue = target;
		remainingTime = calculateRampingTime(target, model430.magnetCurrent);

		// send down command
		socket->sendCommand("CONF:CURR:TARG " + QString::number(target, 'g', 10) + "\r\n");
		model430.targetCurrent = target;

		// also update the field value (bug: LMM 10/26/22)
		model430.targetField = target * model430.coilConstant();
	}
	else
	{
		// calculate ramping time
		targetValue = target / model430.coilConstant();
		remainingTime = calculateRampingTime(targetValue, model430.magnetCurrent);

		socket->sendCommand("CONF:FIELD:TARG " + QString::number(target, 'g', 10) + "\r\n");
		model430.targetField = target;

		// also update the current value (bug: LMM 10/26/22)
		model430.targetCurrent = targetValue;
	}

	// ask to ramp to target
	socket->sendBlockingCommand("RAMP\r\n");
}

//---------------------------------------------------------------------------
// Calculate ramping time from present magnet current using all ramp segments
// Target should be in units of amps
//---------------------------------------------------------------------------
int magnetdaq::calculateRampingTime(double target, double currentValue)
{
	double current = currentValue;
	int rampTime = 0;	// in seconds
	double delta = 0.0;

	// easy case first... only one segment
	if (model430.rampRateSegments() == 1)
	{
		delta = fabs(target - current);
		rampTime = delta / model430.currentRampRates[0]();
	}
	else
	{
		// handle the multiple segment case
		bool increasing = (target > current);	// is direction of ramp increasing?

		while (current != target)
		{
			for (int i = 0; i < model430.rampRateSegments(); i++)
			{
				if ((fabs(model430.currentRampLimits[i]()) >= fabs(current)) || i == (model430.rampRateSegments() - 1))
				{
					if (increasing)	// positive current direction
					{
						if (current >= 0.0)
						{
							delta = model430.currentRampLimits[i]() - current;
						}
						else
						{
							if (i > 0)
								delta = fabs(current + model430.currentRampLimits[i - 1]());
							else
								delta = fabs(current);
						}

						if ((current + delta) >= target)
						{
							// current and target in same segment
							delta = target - current;
							current = target;
						}
						else
							current += delta + FUDGE_FACTOR;
					}
					else  // negative current direction
					{
						if (current >= 0.0)
						{
							if (i > 0)
								delta = current - model430.currentRampLimits[i - 1]();
							else
								delta = current;
						}
						else
						{
							delta = current + model430.currentRampLimits[i]();
						}

						if ((current - delta) <= target)
						{
							// current and target in same segment
							delta = current - target;
							current = target;
						}
						else
							current -= delta + FUDGE_FACTOR;
					}

					rampTime += delta / model430.currentRampRates[i]();
					break;
				}
			}
		}
	}

	return rampTime;
}

//---------------------------------------------------------------------------
void magnetdaq::manualCtrlTimerTick(void)
{
	if (!autostepTimer->isActive())
	{
		if (socket)
			socket->getState();
	}

	if (model430.state() != State::PAUSED)
		remainingTime--;	// subtract one second

#ifdef DEBUG
	if (remainingTime == 15) // beep with 15 seconds to go for debug assist
		QApplication::beep();
#endif

	// handle quench event
	if (model430.state() == State::QUENCH)
	{
		quenchPassCnt++;

		// stop any autostep cycle
		if (autostepTimer->isActive())
		{
			stopAutostep();
			lastStatusMiscString.clear();
			setStatusMsg("Auto-Stepping aborted due to quench detection");
		}
		else
		{
			lastStatusMiscString.clear();
			setStatusMsg("");
		}

		if (quenchPassCnt > 1)
		{
			// quench detected, stop timer
			manualCtrlTimer->stop();

			// mark target line as "Fail" after quench display updates (quenchPassCnt > 1)
			markTableSelectionAsFail(presentTableValue, model430.quenchCurrent);
		}
	}

	// check for manual control action
	else if (!(model430.state() == State::PAUSED || model430.state() == State::RAMPING || model430.state() == State::HOLDING))
	{
		abortTableTarget();
	}

	else // normal status update on table row value
	{
		if (remainingTime > 0)
		{
			int hours, minutes, seconds, remainder;

			hours = remainingTime / 3600;
			remainder = remainingTime % 3600;
			minutes = remainder / 60;
			seconds = remainder % 60;

			QString timeStr;

			if (hours > 0)
				timeStr = QString("%1:%2:%3").arg(hours).arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
			else if (minutes > 0)
				timeStr = QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0'));
			else
				timeStr = QString("%1").arg(seconds);

			// append countdown time to next target
			QString tempStr = statusMisc->text();
			int index = tempStr.indexOf('(');
			if (index >= 1)
				tempStr.truncate(index - 1);

			setStatusMsg(tempStr + " (" + timeStr + ")");
		}
		else
		{
			QString tempStr = statusMisc->text();
			int index = tempStr.indexOf('(');
			if (index >= 1)
			{
				tempStr.truncate(index - 1);
				setStatusMsg(tempStr);
			}

			if (model430.state() == State::HOLDING)
			{
				// at target, stop timer
				manualCtrlTimer->stop();

				// mark target line as "Pass"
				//if (fabs(model430.targetCurrent() - targetValue) < 1e-6)
					markTableSelectionAsPass(presentTableValue);
			}
		}
	}
}

//---------------------------------------------------------------------------
void magnetdaq::markTableSelectionWithOutput(int rowIndex, QString output)
{
	tableIsLoading = true;	// inhibit itemChanged() actions

	if (rowIndex >= 0)
	{
		QTableWidgetItem* cell = ui.tableWidget->item(rowIndex, 2);

		// create cell if it does not exist
		if (cell == nullptr)
			ui.tableWidget->setItem(rowIndex, 2, cell = new QTableWidgetItem(""));

		if (cell->text().isEmpty())
			cell->setText(output);
		else
			cell->setText(cell->text() + ": " + output);
	}

	tableIsLoading = false;
}

//---------------------------------------------------------------------------
void magnetdaq::markTableSelectionAsPass(int rowIndex)
{
	tableIsLoading = true;	// inhibit itemChanged() actions

	if (rowIndex >= 0)
	{
		QTableWidgetItem *cell = ui.tableWidget->item(rowIndex, 2);

		// create cell if it does not exist
		if (cell == nullptr)
			ui.tableWidget->setItem(rowIndex, 2, cell = new QTableWidgetItem(""));

		cell->setText("Pass");

		// clear any quench data
		cell = ui.tableWidget->item(rowIndex, 3);
		if (cell)
			cell->setText("");
	}

	tableIsLoading = false;
}

//---------------------------------------------------------------------------
void magnetdaq::markTableSelectionAsFail(int rowIndex, double quenchCurrent)
{
	tableIsLoading = true;	// inhibit itemChanged() actions

	if (presentTableValue >= 0)
	{
		QTableWidgetItem *cell = ui.tableWidget->item(presentTableValue, 2);

		// create Pass/Fail cell if it does not exist
		if (cell == nullptr)
			ui.tableWidget->setItem(rowIndex, 2, cell = new QTableWidgetItem(""));

		cell->setText("Fail");

		// if needed, add column quench current
		if (ui.tableWidget->columnCount() < 4)
		{
			ui.tableWidget->setColumnCount(4);
			ui.tableWidget->setHorizontalHeaderItem(3, new QTableWidgetItem("Quench (A)"));
			ui.tableWidget->horizontalHeaderItem(3)->font().setBold(true);
			ui.tableWidget->repaint();
		}

		// add quench data
		{
			QString cellStr = QString::number(quenchCurrent, 'f', 2);
			QTableWidgetItem *cell = ui.tableWidget->item(presentTableValue, 3);

			if (cell == nullptr)
				ui.tableWidget->setItem(presentTableValue, 3, cell = new QTableWidgetItem(cellStr));
			else
				cell->setText(cellStr);

			cell->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
		}
	}

	doAutosaveReport(true);	// force report output

	tableIsLoading = false;
}

//---------------------------------------------------------------------------
void magnetdaq::abortTableTarget(void)
{
	if (!autostepTimer->isActive() && !manualCtrlTimer->isActive())
		statusMisc->clear();

	abortAutostep("Auto-stepping aborted due to manual control action");
	abortManualCtrl("Table target interrupted by manual control action");
}

//---------------------------------------------------------------------------
// Argument rowIndex is referenced from a start of 0
//---------------------------------------------------------------------------
bool magnetdaq::goToTableSelection(int rowIndex, bool makeTarget)
{
	bool success = false;
	bool ok;
	double temp;

	if (ui.tableWidget->item(rowIndex, 0) != nullptr)
	{
		// get vector values and check for numerical conversion
		temp = ui.tableWidget->item(rowIndex, 0)->text().toDouble(&ok);

		if (ok)
		{
			// attempt to go to target current/field
			if ((tableError = checkNextTarget(temp, "Table Row #" + QString::number(rowIndex + 1))) == TableError::NO_TABLE_ERROR)
			{
				if (makeTarget)
				{
					//////////////////////////////////////
					// everything good, go to the target!
					//////////////////////////////////////
					success = true;
					sendNextTarget(temp);
					lastTableValue = rowIndex;

					if (autostepTimer->isActive())
					{
						lastStatusMiscString = "Auto-Stepping : Table Row #" + QString::number(rowIndex + 1);
						setStatusMsg(lastStatusMiscString);
					}
					else
					{
						lastStatusMiscString = "Target : Table Row #" + QString::number(rowIndex + 1);
						setStatusMsg(lastStatusMiscString);
					}
				}
			}
			else
			{
				abortAutostep("Auto-Stepping aborted due to an error with Table Row #" + QString::number(rowIndex + 1));
			}
		}
		else
		{
			tableError = TableError::NON_NUMERICAL_ENTRY;
			showErrorString("Table Row #" + QString::number(rowIndex + 1) + " has non-numerical entry", true);	// error annunciation
			abortAutostep("Auto-Stepping aborted due to an error with Table Row #" + QString::number(rowIndex + 1));
		}
	}

	return success;
}

//---------------------------------------------------------------------------
void magnetdaq::autostepRangeChanged(void)
{
	if (socket)
	{
		if (socket->isConnected())
		{
			autostepStartIndex = ui.startIndexEdit->text().toInt();
			autostepEndIndex = ui.endIndexEdit->text().toInt();

			if (autostepStartIndex < 1 || autostepStartIndex > ui.tableWidget->rowCount())
				return;
			else if (autostepEndIndex <= autostepStartIndex || autostepEndIndex > ui.tableWidget->rowCount())
				return;
			else
				calculateAutostepRemainingTime(autostepStartIndex, autostepEndIndex);

			// display total autostep time
			displayAutostepRemainingTime();
		}
		else
		{
			ui.autostepRemainingTimeValue->clear();
		}
	}
}

//---------------------------------------------------------------------------
void magnetdaq::calculateAutostepRemainingTime(int startIndex, int endIndex)
{
	autostepRemainingTime = 0;
	double currentValue = model430.magnetCurrent;

	if (startIndex < autostepStartIndex || endIndex > autostepEndIndex)	// out of range
		return;

	// calculate total remaining time
	for (int i = startIndex - 1; i < endIndex; i++)
	{
		goToTableSelection(i, false);	// check for errors

		if (tableError == TableError::NO_TABLE_ERROR)
		{
			bool ok;
			double temp = 0;
			QTableWidgetItem *cell = ui.tableWidget->item(i, 0);

			if (cell)
			{
				// get table value
				temp = cell->text().toDouble(&ok);

				if (ok)
				{
					// calculate ramping time
					autostepRemainingTime += calculateRampingTime(temp, currentValue);

					// save as start for next step
					currentValue = temp;

					// add any hold time
					temp = ui.tableWidget->item(i, 1)->text().toDouble(&ok);

					if (ok)
						autostepRemainingTime += static_cast<int>(temp);

					if (model430.switchInstalled())
					{
						// transition switch at this step?
						if (ui.tableWidget->item(i, 1)->checkState() == Qt::Checked)
						{
							// add time required to cool and reheat switch, plus settling time
							autostepRemainingTime += model430.switchCooledTime() + model430.switchHeatedTime() + SETTLING_TIME;
						}
					}
				}
			}
		}
		else
			break;	// break on any table error
	}
}

//---------------------------------------------------------------------------
void magnetdaq::displayAutostepRemainingTime(void)
{
	int hours, minutes, seconds, remainder;

	hours = autostepRemainingTime / 3600;
	remainder = autostepRemainingTime % 3600;
	minutes = remainder / 60;
	seconds = remainder % 60;

	QString timeStr = QString("%1:%2:%3").arg(hours, 2, 10, QChar('0')).arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
	ui.autostepRemainingTimeValue->setText(timeStr);
}

//---------------------------------------------------------------------------
void magnetdaq::startAutostep(void)
{
	if (socket)
	{
		if (socket->isConnected())
		{
			socket->getState();

			if (model430.state() < State::QUENCH || model430.state() == State::AT_ZERO || model430.state() == State::ZEROING)
			{
				autostepStartIndex = ui.startIndexEdit->text().toInt();
				autostepEndIndex = ui.endIndexEdit->text().toInt();
				autostepRangeChanged();

				if (tableError == TableError::NO_TABLE_ERROR)
				{
					if (autostepStartIndex < 1 || autostepStartIndex > ui.tableWidget->rowCount())
					{
						showErrorString("Starting Index is out of range!", true);
					}
					else if (autostepEndIndex <= autostepStartIndex || autostepEndIndex > ui.tableWidget->rowCount())
					{
						showErrorString("Ending Index is out of range!", true);
					}
					else if (ui.executeCheckBox->isChecked() && !QFile::exists(ui.appLocationEdit->text()))
					{
						showErrorString("App/script does not exist at specified path!", true);
					}
					else if (ui.executeCheckBox->isChecked() && !checkExecutionTime())
					{
						showErrorString("App/script execution time is not a positive, non-zero integer!", true);
					}
					else if (ui.executeCheckBox->isChecked() && ui.pythonCheckBox->isChecked() && !QFile::exists(ui.pythonPathEdit->text()))
					{
						showErrorString("Python not found at specified path!", true);
					}
					else if (model430.switchInstalled() && !model430.switchHeaterState)
					{
						showErrorString("Cannot ramp to target current or field with cooled switch!", true);
						lastStatusMiscString.clear();
						QApplication::beep();
					}
					else // all good! start!
					{
						ui.startIndexEdit->setEnabled(false);
						ui.endIndexEdit->setEnabled(false);
						ui.autostepRemainingTimeLabel->setEnabled(true);
						ui.autostepRemainingTimeValue->setEnabled(true);
						ui.autostepStartButton->setEnabled(false);
						ui.autostartStopButton->setEnabled(true);
						ui.manualControlGroupBox->setEnabled(false);
						ui.importDataButton->setEnabled(false);
						ui.appFrame->setEnabled(false);
						ui.executeCheckBox->setEnabled(false);
						ui.executeNowButton->setEnabled(false);

						elapsedTimerTicks = 0;
						autostepTimer->start();
						ui.autoStepGroupBox->setTitle("Auto-Stepping (Active)");

						// begin with first vector
						presentTableValue = autostepStartIndex - 1;

						// highlight row in table
						ui.tableWidget->selectRow(presentTableValue);
						tableSelectionChanged(); // lockout row changes
						haveAutosavedReport = false;

						if (goToTableSelection(presentTableValue, true))
						{
							// update remaining time
							socket->getState();
							calculateAutostepRemainingTime(presentTableValue + 1, autostepEndIndex);
							manualCtrlTimer->start();
							quenchPassCnt = 0;
						}
					}
				}
			}
			else
			{
				showErrorString("Cannot start autostep mode in present Model 430 state", true);
				lastStatusMiscString.clear();
				QApplication::beep();
			}
		}
	}
}

//---------------------------------------------------------------------------
void magnetdaq::abortAutostep(QString errorString)
{
	if (autostepTimer->isActive())	// first checks for active autostep sequence
	{
		manualCtrlTimer->stop();	// this displays ramp timing for each step
		autostepTimer->stop();
		ui.autoStepGroupBox->setTitle("Auto-Stepping");

		while (errorStatusIsActive.load())	// show any error condition first
		{
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
            usleep(100000);
#else
			Sleep(100);
#endif
		}
		showErrorString(errorString, false);
		lastStatusMiscString.clear();

		enableTableControls();

		tableSelectionChanged();
		autostepRangeChanged();
		haveExecuted = false;

		// PAUSE Model 430
		socket->sendCommand("PAUSE\r\n");
	}
}

//---------------------------------------------------------------------------
void magnetdaq::stopAutostep(void)
{
	if (manualCtrlTimer->isActive() && model430.state() != State::QUENCH)
		manualCtrlTimer->stop();

	if (autostepTimer->isActive())
	{
		autostepTimer->stop();
		ui.autoStepGroupBox->setTitle("Auto-Stepping");
		lastStatusMiscString.clear();
		setStatusMsg("Auto-Stepping aborted via Stop button!");
		enableTableControls();

		tableSelectionChanged();
		autostepRangeChanged();
		haveExecuted = false;

		// PAUSE Model 430
		if (model430.state() < State::QUENCH || model430.state() == State::AT_ZERO || model430.state() == State::ZEROING)
			socket->sendCommand("PAUSE\r\n");
	}
}

//---------------------------------------------------------------------------
void magnetdaq::autostepTimerTick(void)
{
	static int settlingTime = 0;

	socket->getState();

	if (autostepRemainingTime)
	{
		autostepRemainingTime--;	// decrement by one second
		displayAutostepRemainingTime();
	}

	if (!manualCtrlTimer->isActive() && ((model430.state() == State::HOLDING) ||
		(model430.switchInstalled() && model430.state() == State::PAUSED)))
	{
		// if a switch is installed, check to see if we want to enter persistent mode
		if (model430.switchInstalled())
		{
			if (ui.tableWidget->item(presentTableValue, 1)->checkState() == Qt::Checked)
			{
				if (elapsedTimerTicks == 0)
				{
					if (model430.switchHeaterState)	// heater is ON
					{
						settlingTime++;

						// wait for settling time and magnetVoltage to decay
						if (settlingTime >= SETTLING_TIME && fabs(model430.magnetVoltage) < 0.01)
						{
							/////////////////////
							// enter persistence
							/////////////////////
							socket->sendCommand("PS 0\r\n");

							lastStatusMiscString = "Entering persistence, wait for cooling cycle to complete...";
							setStatusMsg(lastStatusMiscString);

							settlingTime = 0; // reset for next time
							return;
						}
						else
						{
							int settlingRemaining = SETTLING_TIME - settlingTime;

							if (settlingTime > 0)
								lastStatusMiscString = "Waiting for settling time of " + QString::number(settlingRemaining) + " sec before entering persistent mode...";
							else
								lastStatusMiscString = "Waiting for magnet voltage to decay to zero before entering persistent mode...";

							setStatusMsg(lastStatusMiscString);
							return;
						}
					}
				}
			}
		}

		elapsedTimerTicks++;

		// get time
		if (ui.tableWidget->item(presentTableValue, 1) != nullptr)
		{
			bool ok;
			double temp;

			temp = ui.tableWidget->item(presentTableValue, 1)->text().toDouble(&ok);

			if (ok)
			{
				// check for external execution
				if (ui.executeCheckBox->isChecked())
				{
					int executionTime = ui.appStartEdit->text().toInt();	// we already verified the time is proper format

					if ((elapsedTimerTicks >= (static_cast<int>(temp) - executionTime)) && !haveExecuted)
					{
						haveExecuted = true;
						executeApp();
					}
				}

				// check for next vector
				if (elapsedTimerTicks >= static_cast<int>(temp))  // if true, hold time has expired
				{
					// first check to see if we need to exit persistence
					if (model430.switchInstalled())
					{
						if (ui.tableWidget->item(presentTableValue, 1)->checkState() == Qt::Checked)
						{
							if (!model430.switchHeaterState)	// heater is OFF, persistent
							{
								////////////////////
								// exit persistence
								////////////////////
								socket->sendCommand("PS 1\r\n");

								lastStatusMiscString = "Exiting persistence, wait for heating cycle to complete...";
								setStatusMsg(lastStatusMiscString);

								return;
							}
						}
					}

					elapsedTimerTicks = 0;

					if (presentTableValue + 1 < autostepEndIndex)
					{
						// highlight row in table
						presentTableValue++;
						ui.tableWidget->selectRow(presentTableValue);

						///////////////////////////////////////////////
						// go to next vector!
						///////////////////////////////////////////////
						if (goToTableSelection(presentTableValue, true))
						{
							// update remaining time
							calculateAutostepRemainingTime(presentTableValue + 1, autostepEndIndex);
							manualCtrlTimer->start();
							quenchPassCnt = 0;
							haveExecuted = false;
						}
					}
					else
					{
						/////////////////////////////////////////////////////
						// successfully completed vector table auto-stepping
						/////////////////////////////////////////////////////
						lastStatusMiscString = "Auto-Step Completed @ Table Row #" + QString::number(presentTableValue + 1);
						setStatusMsg(lastStatusMiscString);
						autostepTimer->stop();
						ui.autoStepGroupBox->setTitle("Auto-Stepping");
						enableTableControls();
						tableSelectionChanged();
						doAutosaveReport(false);
						haveExecuted = false;
					}
				}
				else
				{
					if (!errorStatusIsActive.load() && ((model430.state() == State::HOLDING) ||
						(model430.switchInstalled() && model430.state() == State::PAUSED)))
					{
						if (model430.switchInstalled())
						{
							// have to reset this because of switch transitions
							lastStatusMiscString = "Auto-Stepping : Table Row #" + QString::number(lastTableValue + 1);
							setStatusMsg(lastStatusMiscString);
						}

						// update the HOLDING or PAUSED in persistent mode countdown
						QString tempStr = statusMisc->text();
						int index = tempStr.indexOf('(');
						if (index >= 1)
							tempStr.truncate(index - 1);

						QString timeStr = " (" + QString::number(temp - elapsedTimerTicks) + " sec of Hold Time remaining)";
						setStatusMsg(tempStr + timeStr);
					}
				}
			}
			else
			{
				autostepTimer->stop();
				ui.autoStepGroupBox->setTitle("Auto-Stepping");
				lastStatusMiscString.clear();
				setStatusMsg("Auto-Stepping aborted due to unknown dwell time in Table Row #" + QString::number(presentTableValue + 1));
				enableTableControls();
				tableSelectionChanged();
				haveExecuted = false;
			}
		}
	}
	else if (model430.state() == State::SWITCH_COOLING || model430.state() == State::SWITCH_HEATING)
	{
		if (model430.state() == State::SWITCH_HEATING)
		{
			lastStatusMiscString = "Exiting persistence, wait for switch heating cycle to complete...";
			setStatusMsg(lastStatusMiscString);
		}
		else if (model430.state() == State::SWITCH_COOLING)
		{
			lastStatusMiscString = "Entering persistence, wait for switch cooling cycle to complete...";
			setStatusMsg(lastStatusMiscString);
		}
	}
}

//---------------------------------------------------------------------------
void magnetdaq::abortManualCtrl(QString errorString)
{
	if (manualCtrlTimer->isActive())	// first checks for active autostep sequence
	{
		manualCtrlTimer->stop();	// this displays ramp timing for each step

		while (errorStatusIsActive.load())	// show any error condition first
		{
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
			usleep(100000);
#else
			Sleep(100);
#endif
		}

		showErrorString(errorString, false);
		lastStatusMiscString.clear();

		tableSelectionChanged();
	}
}

//---------------------------------------------------------------------------
void magnetdaq::recalculateRemainingTime(void)
{
	if (autostepTimer->isActive())
		calculateAutostepRemainingTime(presentTableValue + 1, autostepEndIndex);
	else if (manualCtrlTimer->isActive())
		remainingTime = calculateRampingTime(targetValue, model430.magnetCurrent);
	else
		autostepRangeChanged();
}

//---------------------------------------------------------------------------
void magnetdaq::doAutosaveReport(bool forceOutput)
{
	// auto report generation?
	if ((ui.autosaveReportCheckBox->isChecked() && !haveAutosavedReport) || forceOutput)
	{
		// guess appropriate filename using lastVectorsLoadPath
		if (!lastTableLoadPath.isEmpty())
		{
			if (QFileInfo::exists(lastTableLoadPath))
			{
				QFileInfo file(lastTableLoadPath);
				QString filepath = file.absolutePath();
				QString filename = file.baseName();	// no extension

				if (!filepath.isEmpty())
				{
					QString reportName = filepath + "/" + filename + ".xlsx";

					int i = 1;
					while (QFileInfo::exists(reportName))
					{
						reportName = filepath + "/" + filename + "." + QString::number(i) + ".xlsx";
						i++;
					}

					// filename constructed, now autosave the report
					saveReport(reportName);

					QFileInfo reportFile(reportName);

					if (reportFile.exists())
					{
						setStatusMsg("Saved as " + reportFile.fileName());
						haveAutosavedReport = true;
					}
				}
			}
		}
	}
}

//---------------------------------------------------------------------------
void magnetdaq::enableTableControls(void)
{
	ui.startIndexEdit->setEnabled(true);
	ui.endIndexEdit->setEnabled(true);
	ui.autostepRemainingTimeLabel->setEnabled(false);
	ui.autostepRemainingTimeValue->setEnabled(false);
	ui.autostepStartButton->setEnabled(true);
	ui.autostartStopButton->setEnabled(false);
	ui.manualControlGroupBox->setEnabled(true);
	ui.importDataButton->setEnabled(true);
	ui.appFrame->setEnabled(true);
	ui.executeCheckBox->setEnabled(true);
	ui.executeNowButton->setEnabled(true);
}

//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// App/Script management and processing support
//---------------------------------------------------------------------------
void magnetdaq::browseForAppPath(void)
{
	QString exeFileName;
	QSettings settings;

	lastAppFilePath = settings.value("LastAppFilePath").toString();

	exeFileName = QFileDialog::getOpenFileName(this, "Choose App/Script File", lastAppFilePath, "App/Script Files (*.exe *.py)");

	if (!exeFileName.isEmpty())
	{
		lastAppFilePath = exeFileName;
		ui.appLocationEdit->setText(exeFileName);

		// save path
		settings.setValue("LastAppFilePath", lastAppFilePath);
	}
}

//---------------------------------------------------------------------------
void magnetdaq::browseForPythonPath(void)
{
	QString pythonFileName;
	QSettings settings;

	lastPythonPath = settings.value("LastPythonPath").toString();

	pythonFileName = QFileDialog::getOpenFileName(this, "Choose Python Executable", lastPythonPath, "Python Executable (*.exe)");

	if (!pythonFileName.isEmpty())
	{
		lastPythonPath = pythonFileName;
		ui.pythonPathEdit->setText(pythonFileName);

		// save path
		settings.setValue("LastPythonPath", lastPythonPath);
	}
}

//---------------------------------------------------------------------------
bool magnetdaq::checkExecutionTime(void)
{
	bool ok = false;

	int temp = ui.appStartEdit->text().toInt(&ok);

	if (ok)
	{
		// must be >= 0
		if (temp < 0)
			ok = false;
	}

	return ok;
}

//---------------------------------------------------------------------------
void magnetdaq::executeNowClick(void)
{
	if (ui.executeCheckBox->isChecked() && !QFile::exists(ui.appLocationEdit->text()))
	{
		showErrorString("App/script does not exist at specified path!", true);
	}
	else if (ui.executeCheckBox->isChecked() && !checkExecutionTime())
	{
		showErrorString("App/script execution time is not a positive, non-zero integer!", true);
	}
	else if (ui.executeCheckBox->isChecked() && ui.pythonCheckBox->isChecked() && !QFile::exists(ui.pythonPathEdit->text()))
	{
		showErrorString("Python not found at specified path!", true);
	}
	else // all good! start!
	{
		executeApp();
	}
}

//---------------------------------------------------------------------------
void magnetdaq::executeApp(void)
{
	int precision = 10;
	QString program = ui.appLocationEdit->text();
	QString args = ui.appArgsEdit->text();
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
	QStringList arguments = args.split(" ", QString::SkipEmptyParts);
#else
	QStringList arguments = args.split(" ", Qt::SkipEmptyParts);
#endif

	if (model430.fieldUnits == TESLA)
		precision = 11;

	// loop through argument list and replace "special" variables with present value
	for (int i = 0; i < arguments.count(); i++)
	{
		QString testString = arguments[i].toUpper();

		if (testString == "%CURR:MAG%" || testString == "$CURR:MAG")
			arguments[i] = QString::number(avoidSignedZeroOutput(model430.magnetField, precision), 'g', precision);
		else if (testString == "%CURR:REF%" || testString == "$CURR:REF")
			arguments[i] = QString::number(avoidSignedZeroOutput(model430.referenceCurrent, precision), 'g', precision);
		else if (testString == "%FIELD:MAG%" || testString == "$FIELD:MAG")
			arguments[i] = QString::number(avoidSignedZeroOutput(model430.magnetField, precision), 'g', precision);
		else if (testString == "%TARG:CURR%" || testString == "$TARG:CURR")
			arguments[i] = QString::number(avoidSignedZeroOutput(model430.targetCurrent(), precision), 'g', precision);
		else if (testString == "%TARG:FIELD%" || testString == "$TARG:FIELD")
			arguments[i] = QString::number(avoidSignedZeroOutput(model430.targetField(), precision), 'g', precision);
		else if (testString == "%IPADDR%" || testString == "$IPADDR")
			arguments[i] = ui.ipAddressEdit->text();
	}

	// if a python script, use python path for executable
	if (ui.pythonCheckBox->isChecked())
	{
		program = ui.pythonPathEdit->text();
		arguments.insert(0, ui.appLocationEdit->text());
	}

	// launch background process
	process = new QProcess(this);
	process->setProgram(program);
	process->setArguments(arguments);

	connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
		[=](int exitCode, QProcess::ExitStatus exitStatus) { finishedApp(exitCode, exitStatus); });

	// disable Execute Now and Start button
	ui.executeNowButton->setEnabled(false);
	ui.autostepStartButton->setEnabled(false);

	process->start(program, arguments);
}

//---------------------------------------------------------------------------
void magnetdaq::finishedApp(int exitCode, QProcess::ExitStatus exitStatus)
{
	if (socket)
	{
		if (!socket->isConnected())
			return;
	}
	else
		return;

	QString output = process->readAllStandardOutput();

	// strip cr/lf
	output = output.simplified();

	// post any output text from process
	if (!output.isEmpty())
		markTableSelectionWithOutput(presentTableValue, output);

	process->deleteLater();
	process = nullptr;

	// check for error state, and if error stop autostepping and show error
	if (exitCode)
	{
		if (autostepTimer->isActive())	// first checks for active autostep sequence
		{
			manualCtrlTimer->stop();	// this displays ramp timing for each step
			autostepTimer->stop();
			ui.autoStepGroupBox->setTitle("Auto-Stepping");

			pinErrorString("Auto-stepping aborted: " + output, true);
			lastStatusMiscString.clear();

			enableTableControls();

			tableSelectionChanged();
			autostepRangeChanged();
			haveExecuted = false;
		}
	}

	if (!autostepTimer->isActive())	// re-enable Execute Now button
	{
		ui.executeNowButton->setEnabled(true);
		ui.autostepStartButton->setEnabled(true);
	}
}

//---------------------------------------------------------------------------
void magnetdaq::appCheckBoxChanged(int state)
{
	if (ui.executeCheckBox->isChecked())
	{
		ui.appFrame->setVisible(true);
		ui.executeNowButton->setVisible(true);
	}
	else
	{
		ui.appFrame->setVisible(false);
		ui.executeNowButton->setVisible(false);
	}

	pythonCheckBoxChanged(0);
}

//---------------------------------------------------------------------------
void magnetdaq::pythonCheckBoxChanged(int state)
{
	if (ui.pythonCheckBox->isChecked())
	{
		ui.pythonPathLabel->setVisible(true);
		ui.pythonPathEdit->setVisible(true);
		ui.pythonLocationButton->setVisible(true);
	}
	else
	{
		ui.pythonPathLabel->setVisible(false);
		ui.pythonPathEdit->setVisible(false);
		ui.pythonLocationButton->setVisible(false);
	}
}

//---------------------------------------------------------------------------

#include "stdafx.h"
#include "magnetdaq.h"

// some definitions for readability
enum SETUP_TOOLBOX
{
	SUPPLY_PAGE = 0,
	LOAD_PAGE,
	SWITCH_PAGE,
	PROTECTION_PAGE
};

// NOTE:
// Used the QMetaObject::invokeMethod() function to get queued behavior 
// (i.e. function call runs after current event loop finishes) instead 
// of connecting a lot of signals. This is critical to ensure that the
// 430 value has been updated before trying to update the interface.

//---------------------------------------------------------------------------
// Contains methods related to the configuration panel of the main tab view.
// Broken out from magnetdaq.cpp for ease of editing.
//---------------------------------------------------------------------------
void magnetdaq::mainTabChanged(int index)
{
	if (ui.mainTabWidget->currentIndex() == CONFIG_TAB)
	{
		setupToolBoxChanged(ui.setupToolBox->currentIndex());
	}
	else if (ui.mainTabWidget->currentIndex() == RAMP_TAB)
	{
		QApplication::setOverrideCursor(Qt::WaitCursor);
		QMetaObject::invokeMethod(&model430, "syncRampRates", Qt::QueuedConnection);
		QMetaObject::invokeMethod(this, "syncRampRates", Qt::QueuedConnection);
	}
	else if (ui.mainTabWidget->currentIndex() == RAMPDOWN_TAB)
	{
		QApplication::setOverrideCursor(Qt::WaitCursor);
		QMetaObject::invokeMethod(&model430, "syncRampdownSegmentValues", Qt::QueuedConnection);
		QMetaObject::invokeMethod(this, "syncRampdownRates", Qt::QueuedConnection);
	}
	else if (ui.mainTabWidget->currentIndex() == RAMPDOWN_EVENTS_TAB)
	{
		refreshRampdownList();
	}
	else if (ui.mainTabWidget->currentIndex() == QUENCH_EVENTS_TAB)
	{
		refreshQuenchList();
	}
	else if (ui.mainTabWidget->currentIndex() == SUPPORT_TAB)
	{
		refreshSupportSettings();
	}
}

//---------------------------------------------------------------------------
// Refresh the visible values if an error was returned from the 430.
//---------------------------------------------------------------------------
void magnetdaq::postErrorRefresh(void)
{
	if (ui.mainTabWidget->currentIndex() == CONFIG_TAB)
	{
		setupToolBoxChanged(ui.setupToolBox->currentIndex());
	}
	else if (ui.mainTabWidget->currentIndex() == RAMP_TAB)
	{
		QApplication::setOverrideCursor(Qt::WaitCursor);
		QMetaObject::invokeMethod(&model430, "syncRampRates", Qt::QueuedConnection);
		QMetaObject::invokeMethod(this, "syncRampRates", Qt::QueuedConnection);
	}
	else if (ui.mainTabWidget->currentIndex() == RAMPDOWN_TAB)
	{
		QApplication::setOverrideCursor(Qt::WaitCursor);
		QMetaObject::invokeMethod(&model430, "syncRampdownSegmentValues", Qt::QueuedConnection);
		QMetaObject::invokeMethod(this, "syncRampdownRates", Qt::QueuedConnection);
	}
}

//---------------------------------------------------------------------------
void magnetdaq::setupToolBoxChanged(int index)
{
	QApplication::setOverrideCursor(Qt::WaitCursor);

	if (ui.setupToolBox->currentIndex() == SUPPLY_PAGE)
	{
		QMetaObject::invokeMethod(&model430, "syncSupplySetup", Qt::QueuedConnection);
		QMetaObject::invokeMethod(this, "syncSupplyTab", Qt::QueuedConnection);
	}
	else if (ui.setupToolBox->currentIndex() == LOAD_PAGE)
	{
		QMetaObject::invokeMethod(&model430, "syncLoadSetup", Qt::QueuedConnection);
		QMetaObject::invokeMethod(this, "syncLoadTab", Qt::QueuedConnection);
	}
	else if (ui.setupToolBox->currentIndex() == SWITCH_PAGE)
	{
		QMetaObject::invokeMethod(&model430, "syncSwitchSetup", Qt::QueuedConnection);
		QMetaObject::invokeMethod(this, "syncSwitchTab", Qt::QueuedConnection);
	}
	else if (ui.setupToolBox->currentIndex() == PROTECTION_PAGE)
	{
		QMetaObject::invokeMethod(&model430, "syncProtectionSetup", Qt::QueuedConnection);
		QMetaObject::invokeMethod(this, "syncProtectionTab", Qt::QueuedConnection);
	}
}

//---------------------------------------------------------------------------
void magnetdaq::menuValueChanged(int index)
{
	if (socket)
	{
		int temp;

		// which menu changed?
		QComboBox* comboBox = qobject_cast<QComboBox*>(sender());

		temp = comboBox->currentIndex();

		if (comboBox == ui.stabilityModeComboBox)
		{
			if (temp != model430.stabilityMode())
			{
				socket->sendCommand("CONF:STAB:MODE " + QString::number(temp) + "\r\n");
				model430.stabilityMode = temp;

				// sync stability setting field
				QMetaObject::invokeMethod(&model430, "syncStabilitySetting", Qt::QueuedConnection);
			}
		}
		else if (comboBox == ui.absorberComboBox)
		{
			if ((bool)temp != model430.absorberPresent())
			{
				socket->sendCommand("CONF:AB " + QString::number(temp) + "\r\n");
				model430.absorberPresent = (bool)temp;
			}
		}
		else if (comboBox == ui.switchInstalledComboBox)
		{
			if ((bool)temp != model430.switchInstalled())
			{
				socket->sendCommand("CONF:PS " + QString::number(temp) + "\r\n");
				model430.switchInstalled = (bool)temp;
				switchSettingsEnable();
			}
		}
		else if (comboBox == ui.switchTransitionComboBox)
		{
			if (temp != model430.switchTransition())
			{
				socket->sendCommand("CONF:PS:TRAN " + QString::number(temp) + "\r\n");
				model430.switchTransition = temp;
			}
		}
		else if (comboBox == ui.quenchEnableComboBox)
		{
			if (temp != model430.quenchDetection())
			{
				socket->sendCommand("CONF:QU:DET " + QString::number(temp) + "\r\n");
				model430.quenchDetection = temp;
			}
		}
		else if (comboBox == ui.quenchSensitivityComboBox)
		{
			if (temp != (model430.quenchSensitivity() - 1))
			{
				socket->sendCommand("CONF:QU:RATE " + QString::number(temp + 1) + "\r\n");
				model430.quenchSensitivity = temp + 1;
			}
		}
		else if (comboBox == ui.externRampdownComboBox)
		{
			if ((bool)temp != model430.extRampdownEnabled())
			{
				socket->sendCommand("CONF:RAMPD:ENAB " + QString::number(temp) + "\r\n");
				model430.extRampdownEnabled = (bool)temp;
			}
		}
		else if (comboBox == ui.protectionModeComboBox)
		{
			if (temp != model430.protectionMode())
			{
				socket->sendCommand("CONF:OPL:MODE " + QString::number(temp) + "\r\n");
				model430.protectionMode = temp;
			}
		}
		else if (comboBox == ui.rampUnitsComboBox)
		{
			syncRampRates();
			syncRampPlot();
		}
		else if (comboBox == ui.rampTimebaseComboBox)
		{
			if (temp != model430.rampRateTimeUnits())
			{
				QApplication::setOverrideCursor(Qt::WaitCursor);

				socket->sendBlockingCommand("CONF:RAMP:RATE:UNITS " + QString::number(temp) + "\r\n");
				model430.rampRateTimeUnits = temp;
			}
		}
	}
}

//---------------------------------------------------------------------------
void magnetdaq::textValueChanged(void)
{
	if (socket)
	{
		double temp;
		bool ok;

		// which field changed?
		QLineEdit* lineEdit = qobject_cast<QLineEdit*>(sender());

		// does field convert to a double value?
		temp = lineEdit->text().toDouble(&ok);

		if (ok)	// if true, text converted to a double value without error
		{
			if (lineEdit == ui.voltageLimitEdit)
			{
				if (temp != model430.voltageLimit())	// did value change?
				{
					socket->sendCommand("CONF:VOLT:LIM " + lineEdit->text() + "\r\n");
					model430.voltageLimit = temp;
				}
			}
			else if (lineEdit == ui.targetSetpointEdit)
			{
				if (ui.rampUnitsComboBox->currentIndex() == 0) // Amps
				{
					if (temp != model430.targetCurrent())	// did value change?
					{
						socket->sendCommand("CONF:CURR:TARG " + lineEdit->text() + "\r\n");
						model430.targetCurrent = temp;

						// sync target field
						QMetaObject::invokeMethod(&model430, "syncTargetField", Qt::QueuedConnection);
					}
				}
				else
				{
					if (temp != model430.targetField())	// did value change?
					{
						socket->sendCommand("CONF:FIELD:TARG " + lineEdit->text() + "\r\n");
						model430.targetField = temp;

						// sync target current
						QMetaObject::invokeMethod(&model430, "syncTargetCurrent", Qt::QueuedConnection);
					}
				}
			}
			else if (lineEdit == ui.stabilitySettingEdit)
			{
				if (temp != model430.stabilitySetting())	// did value change?
				{
					socket->sendCommand("CONF:STAB " + lineEdit->text() + "\r\n");
					model430.stabilitySetting = temp;

					// sync stability mode
					QMetaObject::invokeMethod(&model430, "syncStabilityMode", Qt::QueuedConnection);
				}
			}
			else if (lineEdit == ui.coilConstantEdit)
			{
				if (temp != model430.coilConstant())	// did value change?
				{
					socket->sendCommand("CONF:COIL " + lineEdit->text() + "\r\n");
					model430.coilConstant = temp;
				}
			}
			else if (lineEdit == ui.magInductanceEdit)
			{
				if (temp != model430.inductance())	// did value change?
				{
					socket->sendCommand("CONF:IND " + lineEdit->text() + "\r\n");
					model430.inductance = temp;

					// sync stability setting field
					QMetaObject::invokeMethod(&model430, "syncStabilitySetting", Qt::QueuedConnection);
				}
			}
			else if (lineEdit == ui.switchCurrentEdit)
			{
				if (temp != model430.switchCurrent())	// did value change?
				{
					socket->sendCommand("CONF:PS:CURR " + lineEdit->text() + "\r\n");
					model430.switchCurrent = temp;
				}
			}
			else if (lineEdit == ui.switchHeatedTimeEdit)
			{
				if ((int)temp != model430.switchHeatedTime())	// did value change?
				{
					socket->sendCommand("CONF:PS:HTIME " + lineEdit->text() + "\r\n");
					model430.switchHeatedTime = (int)temp;
				}
			}
			else if (lineEdit == ui.switchCooledTimeEdit)
			{
				if ((int)temp != model430.switchCooledTime())	// did value change?
				{
					socket->sendCommand("CONF:PS:CTIME " + lineEdit->text() + "\r\n");
					model430.switchCooledTime = (int)temp;
				}
			}
			else if (lineEdit == ui.switchCooledRampRateEdit)
			{
				if (temp != model430.cooledSwitchRampRate())	// did value change?
				{
					socket->sendCommand("CONF:PS:PSRR " + lineEdit->text() + "\r\n");
					model430.cooledSwitchRampRate = temp;
				}
			}
			else if (lineEdit == ui.switchCoolingGainEdit)
			{
				if (temp != model430.switchCoolingGain())	// did value change?
				{
					socket->sendCommand("CONF:PS:CGAIN " + lineEdit->text() + "\r\n");
					model430.switchCoolingGain = temp;
				}
			}
			else if (lineEdit == ui.currentLimitEdit)
			{
				if (temp != model430.currentLimit())	// did value change?
				{
					socket->sendCommand("CONF:CURR:LIM " + lineEdit->text() + "\r\n");
					model430.currentLimit = temp;
				}
			}
			else if (lineEdit == ui.icSlopeEdit)
			{
				if (temp != model430.IcSlope())	// did value change?
				{
					socket->sendCommand("CONF:OPL:ICSLOPE " + lineEdit->text() + "\r\n");
					model430.IcSlope = temp;
				}
			}
			else if (lineEdit == ui.icOffsetEdit)
			{
				if (temp != model430.IcOffset())	// did value change?
				{
					socket->sendCommand("CONF:OPL:ICOFFSET " + lineEdit->text() + "\r\n");
					model430.IcOffset = temp;
				}
			}
			else if (lineEdit == ui.tmaxEdit)
			{
				if (temp != model430.Tmax())	// did value change?
				{
					socket->sendCommand("CONF:OPL:TMAX " + lineEdit->text() + "\r\n");
					model430.Tmax = temp;
				}
			}
			else if (lineEdit == ui.tscaleEdit)
			{
				if (temp != model430.Tscale())	// did value change?
				{
					socket->sendCommand("CONF:OPL:TSCALE " + lineEdit->text() + "\r\n");
					model430.Tscale = temp;
				}
			}
			else if (lineEdit == ui.toffsetEdit)
			{
				if (temp != model430.Toffset())	// did value change?
				{
					socket->sendCommand("CONF:OPL:TOFFSET " + lineEdit->text() + "\r\n");
					model430.Toffset = temp;
				}
			}
		}
		else
		{
			QApplication::beep();

			// non-numerical entry
			displaySystemError("Non-numerical entry", lineEdit->text());
		}
	}
}

//---------------------------------------------------------------------------
void magnetdaq::checkBoxValueChanged(bool checked)
{
	if (socket)
	{
		bool temp;

		// which menu changed?
		QCheckBox* checkBox = qobject_cast<QCheckBox*>(sender());

		temp = checkBox->isChecked();

		if (checkBox == ui.stabilityResistorCheckBox)
		{
			if (temp != model430.stabilityResistor())
			{
				socket->sendCommand("CONF:STAB:RES " + QString::number(temp) + "\r\n");
				model430.stabilityResistor = temp;
			}
		}

	}
}

//---------------------------------------------------------------------------
void magnetdaq::rampSegmentCountChanged(int value)
{
	model430.syncFieldUnits();
	if (socket && telnet)	// added telnet check here to prevent setting during connection process
	{
		socket->sendBlockingCommand("CONF:RAMP:RATE:SEG " + QString::number(value) + "\r\n");
		model430.rampRateSegments = value;
	}
}

//---------------------------------------------------------------------------
void magnetdaq::rampSegmentValueChanged(void)
{
	if (socket)
	{
		double temp;
		bool ok;
		bool valueChanged = false;

		// which field changed?
		QLineEdit* lineEdit = qobject_cast<QLineEdit*>(sender());

		// does field convert to a double value?
		temp = lineEdit->text().toDouble(&ok);

		for (int i = 0; i < 10; i++)
		{
			if (lineEdit == rampSegMaxLimits[i])
			{
				// are we locally working in units of A?
				if (ui.rampUnitsComboBox->currentIndex() == 0)
				{
					// did value change?
					if (temp != model430.currentRampLimits[i]())
					{
						valueChanged = true;
						QString queryStr = "CONF:RAMP:RATE:CURR " + QString::number(i + 1) + "," +
							QString::number(model430.currentRampRates[i](), 'f', 7) + "," + QString::number(temp, 'f', 1) + "\r\n";
						socket->sendBlockingCommand(queryStr);
						model430.currentRampLimits[i] = temp;
					}
				}
				else // field units
				{
					// did value change?
					if (temp != model430.fieldRampLimits[i]())
					{
						valueChanged = true;
						QString queryStr = "CONF:RAMP:RATE:FIELD " + QString::number(i + 1) + "," +
							QString::number(model430.fieldRampRates[i](), 'f', 7) + "," + QString::number(temp, 'f', 1) + "\r\n";
						socket->sendBlockingCommand(queryStr);
						model430.fieldRampLimits[i] = temp;
					}
				}
				break;
			}

			if (lineEdit == rampSegValues[i])
			{
				// are we locally working in units of A?
				if (ui.rampUnitsComboBox->currentIndex() == 0)
				{
					// did value change?
					if (temp != model430.currentRampRates[i]())
					{
						valueChanged = true;
						QString queryStr = "CONF:RAMP:RATE:CURR " + QString::number(i + 1) + "," +
							QString::number(temp, 'f', 7) + "," + QString::number(model430.currentRampLimits[i](), 'f', 1) + "\r\n";
						socket->sendBlockingCommand(queryStr);
						model430.currentRampRates[i] = temp;
					}
				}
				else // field units
				{
					// did value change?
					if (temp != model430.fieldRampRates[i]())
					{
						valueChanged = true;
						QString queryStr = "CONF:RAMP:RATE:FIELD " + QString::number(i + 1) + "," +
							QString::number(temp, 'f', 7) + "," + QString::number(model430.fieldRampLimits[i](), 'f', 1) + "\r\n";
						socket->sendBlockingCommand(queryStr);
						model430.fieldRampRates[i] = temp;
					}
				}
				break;
			}
		}
	}
}

//---------------------------------------------------------------------------
void magnetdaq::rampdownSegmentCountChanged(int value)
{
	model430.syncFieldUnits();
	if (socket && telnet)	// added telnet check here to prevent setting during connection process
	{
		socket->sendBlockingCommand("CONF:RAMPD:RATE:SEG " + QString::number(value) + "\r\n");
		model430.rampdownSegments = value;
	}
}

//---------------------------------------------------------------------------
void magnetdaq::rampdownSegmentValueChanged(void)
{
	if (socket)
	{
		double temp;
		bool ok;
		bool valueChanged = false;

		// which field changed?
		QLineEdit* lineEdit = qobject_cast<QLineEdit*>(sender());

		// does field convert to a double value?
		temp = lineEdit->text().toDouble(&ok);

		for (int i = 0; i < 10; i++)
		{
			if (lineEdit == rampdownSegMaxLimits[i])
			{
				// are we locally working in units of A?
				if (ui.rampUnitsComboBox->currentIndex() == 0)
				{
					// did value change?
					if (temp != model430.currentRampdownLimits[i]())
					{
						valueChanged = true;
						QString queryStr = "CONF:RAMPD:RATE:CURR " + QString::number(i + 1) + "," +
							QString::number(model430.currentRampdownRates[i](), 'f', 7) + "," + QString::number(temp, 'f', 1) + "\r\n";
						socket->sendBlockingCommand(queryStr);
						model430.currentRampdownLimits[i] = temp;
					}
				}
				else // field units
				{
					// did value change?
					if (temp != model430.fieldRampdownLimits[i]())
					{
						valueChanged = true;
						QString queryStr = "CONF:RAMPD:RATE:FIELD " + QString::number(i + 1) + "," +
							QString::number(model430.fieldRampdownRates[i](), 'f', 7) + "," + QString::number(temp, 'f', 1) + "\r\n";
						socket->sendBlockingCommand(queryStr);
						model430.fieldRampdownLimits[i] = temp;
					}
				}
				break;
			}

			if (lineEdit == rampdownSegValues[i])
			{
				// are we locally working in units of A?
				if (ui.rampUnitsComboBox->currentIndex() == 0)
				{
					// did value change?
					if (temp != model430.currentRampdownRates[i]())
					{
						valueChanged = true;
						QString queryStr = "CONF:RAMPD:RATE:CURR " + QString::number(i + 1) + "," +
							QString::number(temp, 'f', 7) + "," + QString::number(model430.currentRampdownLimits[i](), 'f', 1) + "\r\n";
						socket->sendBlockingCommand(queryStr);
						model430.currentRampdownRates[i] = temp;
					}
				}
				else // field units
				{
					// did value change?
					if (temp != model430.fieldRampdownRates[i]())
					{
						valueChanged = true;
						QString queryStr = "CONF:RAMPD:RATE:FIELD " + QString::number(i + 1) + "," +
							QString::number(temp, 'f', 7) + "," + QString::number(model430.fieldRampdownLimits[i](), 'f', 1) + "\r\n";
						socket->sendBlockingCommand(queryStr);
						model430.fieldRampdownRates[i] = temp;
					}
				}
				break;
			}
		}

		if (valueChanged)
		{
			QApplication::setOverrideCursor(Qt::WaitCursor);

			QMetaObject::invokeMethod(&model430, "syncRampdownSegmentValues", Qt::QueuedConnection);
			QMetaObject::invokeMethod(this, "syncRampdownPlot", Qt::QueuedConnection);
		}
	}
}

//---------------------------------------------------------------------------
void magnetdaq::configurationChanged(QueryState state)
{
	// check for OPC group box enable under Protection
	if (model430.mode() & 0x02)
		ui.opcGroupBox->setEnabled(true);
	else
		ui.opcGroupBox->setEnabled(false);

	// SETUP Supply
	if (state == SUPPLY_TYPE)
		ui.supplyListWidget->setCurrentRow(model430.powerSupplySelection());
	else if (state == SUPPLY_MIN_VOLTAGE)
		ui.minOutputVoltageEdit->setText(QString::number(model430.minSupplyVoltage(), 'f', 3));
	else if (state == SUPPLY_MAX_VOLTAGE)
		ui.maxOutputVoltageEdit->setText(QString::number(model430.maxSupplyVoltage(), 'f', 3));
	else if (state == SUPPLY_MIN_CURRENT)
		ui.minOutputCurrentEdit->setText(QString::number(model430.minSupplyCurrent(), 'f', 3));
	else if (state == SUPPLY_MAX_CURRENT)
		ui.maxOutputCurrentEdit->setText(QString::number(model430.maxSupplyCurrent(), 'f', 3));
	else if (state == SUPPLY_VV_INPUT)
		ui.voltageModeComboBox->setCurrentIndex(model430.inputVoltageRange());

	// SETUP Load
	else if (state == STABILITY_MODE)
		ui.stabilityModeComboBox->setCurrentIndex(model430.stabilityMode());
	else if (state == STABILITY_SETTING)
		ui.stabilitySettingEdit->setText(QString::number(model430.stabilitySetting(), 'f', 1));
	else if (state == COIL_CONSTANT)
		ui.coilConstantEdit->setText(QString::number(model430.coilConstant(), 'f', 6));
	else if (state == INDUCTANCE)
		ui.magInductanceEdit->setText(QString::number(model430.inductance(), 'f', 2));
	else if (state == ABSORBER_PRESENT)
		ui.absorberComboBox->setCurrentIndex((int)model430.absorberPresent());

	// SETUP Switch
	else if (state == SWITCH_INSTALLED)
	{
		ui.switchInstalledComboBox->setCurrentIndex((int)model430.switchInstalled());
		switchSettingsEnable();
	}
	else if (state == STABILITY_RESISTOR)
		ui.stabilityResistorCheckBox->setChecked(model430.stabilityResistor());
	else if (state == SWITCH_CURRENT)
		ui.switchCurrentEdit->setText(QString::number(model430.switchCurrent(), 'f', 1));
	else if (state == SWITCH_TRANSITION)
		ui.switchTransitionComboBox->setCurrentIndex(model430.switchTransition());
	else if (state == SWITCH_HEATED_TIME)
		ui.switchHeatedTimeEdit->setText(QString::number(model430.switchHeatedTime()));
	else if (state == SWITCH_COOLED_TIME)
		ui.switchCooledTimeEdit->setText(QString::number(model430.switchCooledTime()));
	else if (state == PS_RAMP_RATE)
		ui.switchCooledRampRateEdit->setText(QString::number(model430.cooledSwitchRampRate(), 'f', 1));
	else if (state == SWITCH_COOLING_GAIN)
		ui.switchCoolingGainEdit->setText(QString::number(model430.switchCoolingGain(), 'f', 1));

	// SETUP Protection
	else if (state == CURRENT_LIMIT)
		ui.currentLimitEdit->setText(QString::number(model430.currentLimit(), 'f', 3));
	else if (state == QUENCH_ENABLE)
		ui.quenchEnableComboBox->setCurrentIndex(model430.quenchDetection());
	else if (state == QUENCH_SENSITIVITY)
		ui.quenchSensitivityComboBox->setCurrentIndex(model430.quenchSensitivity() - 1);
	else if (state == EXT_RAMPDOWN)
		ui.externRampdownComboBox->setCurrentIndex((int)model430.extRampdownEnabled());
	else if (state == PROTECTION_MODE)
		ui.protectionModeComboBox->setCurrentIndex(model430.protectionMode());
	else if (state == IC_SLOPE)
		ui.icSlopeEdit->setText(QString::number(model430.IcSlope(), 'f', 3));
	else if (state == IC_OFFSET)
		ui.icOffsetEdit->setText(QString::number(model430.IcOffset(), 'f', 3));
	else if (state == TMAX)
		ui.tmaxEdit->setText(QString::number(model430.Tmax(), 'f', 3));
	else if (state == TSCALE)
		ui.tscaleEdit->setText(QString::number(model430.Tscale(), 'f', 3));
	else if (state == TOFFSET)
		ui.toffsetEdit->setText(QString::number(model430.Toffset(), 'f', 3));

	// Ramp Tab
	else if (state == TARGET_CURRENT)
	{
		if (ui.rampUnitsComboBox->currentIndex() == 0)
			ui.targetSetpointEdit->setText(QString::number(model430.targetCurrent(), 'g', 10));

		// if a ramping tab is visible, update the plot
		if (ui.mainTabWidget->currentIndex() == RAMP_TAB)
			QMetaObject::invokeMethod(this, "syncRampPlot", Qt::QueuedConnection);
	}
	else if (state == TARGET_FIELD)
	{
		if (ui.rampUnitsComboBox->currentIndex() == 1)
			ui.targetSetpointEdit->setText(QString::number(model430.targetField(), 'g', 10));

		// if a ramping tab is visible, update the plot
		if (ui.mainTabWidget->currentIndex() == RAMP_TAB)
			QMetaObject::invokeMethod(this, "syncRampPlot", Qt::QueuedConnection);
	}
	else if (state == VOLTAGE_LIMIT)
	{
		ui.voltageLimitEdit->setText(QString::number(model430.voltageLimit(), 'f', 3));
	}
	else if (state == RAMP_SEGMENTS || state == RAMP_TIMEBASE || state == RAMP_RATE_CURRENT || state == RAMP_RATE_FIELD)
	{
		// if a ramping tab is visible, update the contents
		if (ui.mainTabWidget->currentIndex() == RAMP_TAB)
		{
			QMetaObject::invokeMethod(&model430, "syncRampSegmentValues", Qt::QueuedConnection);
			QMetaObject::invokeMethod(this, "syncRampRates", Qt::QueuedConnection);
		}
	}
	else if (state == FIELD_UNITS)
	{
		this->writeLogHeader();
		this->setCurrentAxisLabel();

		// change coil constant label
		if (model430.fieldUnits() == 0)
			ui.coilConstantLabel->setText("Coil Constant (kG/A) :");
		else
			ui.coilConstantLabel->setText("Coil Constant (T/A) :");

		// if a ramping tab is visible, update it
		if (ui.mainTabWidget->currentIndex() == RAMP_TAB)
		{
			QMetaObject::invokeMethod(&model430, "syncRampRates", Qt::QueuedConnection);
			QMetaObject::invokeMethod(this, "syncRampRates", Qt::QueuedConnection);
		}
		else if (ui.mainTabWidget->currentIndex() == RAMPDOWN_TAB)
		{
			QMetaObject::invokeMethod(&model430, "syncRampdownSegmentValues", Qt::QueuedConnection);
			QMetaObject::invokeMethod(this, "syncRampdownRates", Qt::QueuedConnection);
		}
	}

	// Rampdown
	else if (state == RAMPDOWN_SEGMENTS)
	{
		QMetaObject::invokeMethod(&model430, "syncRampdownSegmentValues", Qt::QueuedConnection);
		QMetaObject::invokeMethod(this, "syncRampdownRates", Qt::QueuedConnection);
	}
	else if (state == RAMPDOWN_CURRENT)
	{
		QMetaObject::invokeMethod(this, "syncRampdownRates", Qt::QueuedConnection);
	}
	else if (state == RAMPDOWN_FIELD)
	{
		QMetaObject::invokeMethod(this, "syncRampdownRates", Qt::QueuedConnection);
	}
}

//---------------------------------------------------------------------------
// Sync the displayed settings with the actual 430 configuration.
//---------------------------------------------------------------------------
void magnetdaq::syncDisplay(void)
{
	QMetaObject::invokeMethod(this, "syncSupplyTab", Qt::QueuedConnection);
	QMetaObject::invokeMethod(this, "syncLoadTab", Qt::QueuedConnection);
	QMetaObject::invokeMethod(this, "syncSwitchTab", Qt::QueuedConnection);
	QMetaObject::invokeMethod(this, "syncProtectionTab", Qt::QueuedConnection);
	QMetaObject::invokeMethod(this, "syncRampRates", Qt::QueuedConnection);
}

//---------------------------------------------------------------------------
void magnetdaq::syncSupplyTab(void)
{
	ui.supplyListWidget->setCurrentRow(model430.powerSupplySelection());
	ui.minOutputVoltageEdit->setText(QString::number(model430.minSupplyVoltage(), 'f', 3));
	ui.maxOutputVoltageEdit->setText(QString::number(model430.maxSupplyVoltage(), 'f', 3));
	ui.minOutputCurrentEdit->setText(QString::number(model430.minSupplyCurrent(), 'f', 3));
	ui.maxOutputCurrentEdit->setText(QString::number(model430.maxSupplyCurrent(), 'f', 3));
	ui.voltageModeComboBox->setCurrentIndex(model430.inputVoltageRange());

	QApplication::restoreOverrideCursor();
}

//---------------------------------------------------------------------------
void magnetdaq::syncLoadTab(void)
{
	ui.stabilityModeComboBox->setCurrentIndex(model430.stabilityMode());
	ui.stabilitySettingEdit->setText(QString::number(model430.stabilitySetting(), 'f', 1));
	ui.coilConstantEdit->setText(QString::number(model430.coilConstant(), 'f', 6));
	ui.magInductanceEdit->setText(QString::number(model430.inductance(), 'f', 2));
	ui.absorberComboBox->setCurrentIndex((int)model430.absorberPresent());

	QApplication::restoreOverrideCursor();
}

//---------------------------------------------------------------------------
void magnetdaq::syncSwitchTab(void)
{
	ui.switchInstalledComboBox->setCurrentIndex((int)model430.switchInstalled());
	ui.stabilityResistorCheckBox->setChecked(model430.stabilityResistor());
	ui.switchCurrentEdit->setText(QString::number(model430.switchCurrent(), 'f', 1));
	ui.switchTransitionComboBox->setCurrentIndex(model430.switchTransition());
	ui.switchHeatedTimeEdit->setText(QString::number(model430.switchHeatedTime()));
	ui.switchCooledTimeEdit->setText(QString::number(model430.switchCooledTime()));
	ui.switchCooledRampRateEdit->setText(QString::number(model430.cooledSwitchRampRate(), 'f', 1));
	ui.switchCoolingGainEdit->setText(QString::number(model430.switchCoolingGain(), 'f', 1));

	switchSettingsEnable();

	QApplication::restoreOverrideCursor();
}

//---------------------------------------------------------------------------
void magnetdaq::switchSettingsEnable()
{
	// enable/disable other user interface items per selection
	if (model430.switchInstalled())
	{
		ui.stabilityResistorCheckBox->setEnabled(false);
		ui.switchCurrentLabel->setEnabled(true);
		ui.switchCurrentEdit->setEnabled(true);
		ui.switchTransitionLabel->setEnabled(true);
		ui.switchTransitionComboBox->setEnabled(true);
		ui.switchHeatedTimeLabel->setEnabled(true);
		ui.switchHeatedTimeEdit->setEnabled(true);
		ui.switchCooledTimeLabel->setEnabled(true);
		ui.switchCooledTimeEdit->setEnabled(true);
		ui.switchCooledRampRateLabel->setEnabled(true);
		ui.switchCooledRampRateEdit->setEnabled(true);
		ui.switchCoolingGainLabel->setEnabled(true);
		ui.switchCoolingGainEdit->setEnabled(true);
		ui.persistSwitchControlButton->setEnabled(true);
		ui.autoDetectCurrentButton->setEnabled(true);
	}
	else
	{
		ui.stabilityResistorCheckBox->setEnabled(true);
		ui.switchCurrentLabel->setEnabled(false);
		ui.switchCurrentEdit->setEnabled(false);
		ui.switchTransitionLabel->setEnabled(false);
		ui.switchTransitionComboBox->setEnabled(false);
		ui.switchHeatedTimeLabel->setEnabled(false);
		ui.switchHeatedTimeEdit->setEnabled(false);
		ui.switchCooledTimeLabel->setEnabled(false);
		ui.switchCooledTimeEdit->setEnabled(false);
		ui.switchCooledRampRateLabel->setEnabled(false);
		ui.switchCooledRampRateEdit->setEnabled(false);
		ui.switchCoolingGainLabel->setEnabled(false);
		ui.switchCoolingGainEdit->setEnabled(false);
		ui.persistSwitchControlButton->setEnabled(false);
		ui.autoDetectCurrentButton->setEnabled(false);
	}
}

//---------------------------------------------------------------------------
void magnetdaq::syncProtectionTab(void)
{
	ui.currentLimitEdit->setText(QString::number(model430.currentLimit(), 'f', 3));
	ui.quenchEnableComboBox->setCurrentIndex(model430.quenchDetection());
	ui.quenchSensitivityComboBox->setCurrentIndex(model430.quenchSensitivity() - 1);
	ui.protectionModeComboBox->setCurrentIndex(model430.protectionMode());
	ui.externRampdownComboBox->setCurrentIndex((int)model430.extRampdownEnabled());

	// operational constants
	ui.icSlopeEdit->setText(QString::number(model430.IcSlope(), 'f', 3));
	ui.icOffsetEdit->setText(QString::number(model430.IcOffset(), 'f', 3));
	ui.tmaxEdit->setText(QString::number(model430.Tmax(), 'f', 3));
	ui.tscaleEdit->setText(QString::number(model430.Tscale(), 'f', 3));
	ui.toffsetEdit->setText(QString::number(model430.Toffset(), 'f', 3));

	QApplication::restoreOverrideCursor();
}

//---------------------------------------------------------------------------
void magnetdaq::syncRampRates(void)
{
	// get ramp segments
	if (model430.rampRateSegments != ui.rampSegmentsSpinBox->value())
		ui.rampSegmentsSpinBox->setValue(model430.rampRateSegments());

	// set ramp units
	if (model430.rampRateTimeUnits != ui.rampTimebaseComboBox->currentIndex())
		ui.rampTimebaseComboBox->setCurrentIndex(model430.rampRateTimeUnits());

	// get target
	if (ui.rampUnitsComboBox->currentIndex() == 0)
		ui.targetSetpointEdit->setText(QString::number(model430.targetCurrent(), 'g', 10));
	else
		ui.targetSetpointEdit->setText(QString::number(model430.targetField(), 'g', 10));

	// enable/disable ramp rate fields per segment count
	for (int i = 0; i < model430.rampRateSegments(); i++)
	{
		rampSegLabels[i]->setEnabled(true);
		rampSegMinLimits[i]->setEnabled(true);
		rampSegMaxLimits[i]->setEnabled(true);
		rampSegValues[i]->setEnabled(true);
	}

	for (int i = model430.rampRateSegments(); i < 10; i++)
	{
		rampSegLabels[i]->setEnabled(false);
		rampSegMinLimits[i]->setEnabled(false);
		rampSegMaxLimits[i]->setEnabled(false);
		rampSegValues[i]->setEnabled(false);
	}

	// are we locally working in units of A?
	if (ui.rampUnitsComboBox->currentIndex() == 0)
	{
		ui.targetSetpointLabel->setText("Target Setpoint (A) :");

		if (model430.rampRateTimeUnits() == 0)
			ui.rampRateUnitsLabel->setText("Rate (A/sec)");
		else
			ui.rampRateUnitsLabel->setText("Rate (A/min)");

		ui.rampLimitUnitsLabel->setText("Limit (A)");

		rampSegMinLimits[0]->setText("<html>0.0 A to &plusmn;</html>");

		for (int i = 0; i < model430.rampRateSegments(); i++)
		{
			if (i > 0)
				rampSegMinLimits[i]->setText("<html>&plusmn;" + QString::number(model430.currentRampLimits[i - 1](), 'f', 1) + " A to &plusmn;</html>");

			rampSegMaxLimits[i]->setText(QString::number(model430.currentRampLimits[i](), 'f', 1));
			rampSegValues[i]->setText(QString::number(model430.currentRampRates[i](), 'g', 10));
		}

		for (int i = model430.rampRateSegments(); i < 10; i++)
		{
			rampSegMinLimits[i]->clear();
			rampSegMaxLimits[i]->clear();
			rampSegValues[i]->clear();
		}
	}

	// are we locally working in units of kG or T?
	else if (ui.rampUnitsComboBox->currentIndex() == 1)
	{
		QString unitsStr = " kG to";

		if (model430.fieldUnits == 0)
		{
			ui.targetSetpointLabel->setText("Target Setpoint (kG) :");

			if (model430.rampRateTimeUnits() == 0)
				ui.rampRateUnitsLabel->setText("Rate (kG/sec)");
			else
				ui.rampRateUnitsLabel->setText("Rate (kG/min)");

			ui.rampLimitUnitsLabel->setText("Limit (kG)");
			rampSegMinLimits[0]->setText("<html>0.0 kG to &plusmn;</html>");
		}
		else
		{
			ui.targetSetpointLabel->setText("Target Setpoint (T) :");

			if (model430.rampRateTimeUnits() == 0)
				ui.rampRateUnitsLabel->setText("Rate (T/sec)");
			else
				ui.rampRateUnitsLabel->setText("Rate (T/min)");

			ui.rampLimitUnitsLabel->setText("Limit (T)");
			unitsStr = " T to";
			rampSegMinLimits[0]->setText("<html>0.0 T to &plusmn;</html>");
		}

		for (int i = 0; i < model430.rampRateSegments(); i++)
		{
			if (i > 0)
				rampSegMinLimits[i]->setText("<html>&plusmn;" + QString::number(model430.fieldRampLimits[i - 1](), 'f', 1) + unitsStr + " &plusmn;</html>");

			rampSegMaxLimits[i]->setText(QString::number(model430.fieldRampLimits[i](), 'f', 1));
			rampSegValues[i]->setText(QString::number(model430.fieldRampRates[i](), 'g', 10));
		}

		for (int i = model430.rampRateSegments(); i < 10; i++)
		{
			rampSegMinLimits[i]->clear();
			rampSegMaxLimits[i]->clear();
			rampSegValues[i]->clear();
		}
	}

	QApplication::restoreOverrideCursor();
}

//---------------------------------------------------------------------------
void magnetdaq::syncRampdownRates(void)
{
	// get ramp segments
	if (model430.rampdownSegments != ui.rampdownSegmentsSpinBox->value())
		ui.rampdownSegmentsSpinBox->setValue(model430.rampdownSegments());

	// enable/disable ramp rate fields per segment count
	for (int i = 0; i < model430.rampdownSegments(); i++)
	{
		rampdownSegLabels[i]->setEnabled(true);
		rampdownSegMinLimits[i]->setEnabled(true);
		rampdownSegMaxLimits[i]->setEnabled(true);
		rampdownSegValues[i]->setEnabled(true);
	}

	for (int i = model430.rampdownSegments(); i < 10; i++)
	{
		rampdownSegLabels[i]->setEnabled(false);
		rampdownSegMinLimits[i]->setEnabled(false);
		rampdownSegMaxLimits[i]->setEnabled(false);
		rampdownSegValues[i]->setEnabled(false);
	}

	// are we locally working in units of A?
	if (ui.rampUnitsComboBox->currentIndex() == 0)
	{
		if (model430.rampRateTimeUnits() == 0)
			ui.rampdownRateUnitsLabel->setText("Rate (A/sec)");
		else
			ui.rampdownRateUnitsLabel->setText("Rate (A/min)");

		ui.rampdownLimitUnitsLabel->setText("Limit (A)");

		rampdownSegMinLimits[0]->setText("<html>0.0 A to &plusmn;</html>");

		for (int i = 0; i < model430.rampdownSegments(); i++)
		{
			if (i > 0)
				rampdownSegMinLimits[i]->setText("<html>&plusmn;" + QString::number(model430.currentRampdownLimits[i - 1](), 'f', 1) + " A to &plusmn;</html>");

			rampdownSegMaxLimits[i]->setText(QString::number(model430.currentRampdownLimits[i](), 'f', 1));
			rampdownSegValues[i]->setText(QString::number(model430.currentRampdownRates[i](), 'g', 10));
		}

		for (int i = model430.rampdownSegments(); i < 10; i++)
		{
			rampdownSegMinLimits[i]->clear();
			rampdownSegMaxLimits[i]->clear();
			rampdownSegValues[i]->clear();
		}
	}

	// are we locally working in units of kG or T?
	else if (ui.rampUnitsComboBox->currentIndex() == 1)
	{
		QString unitsStr = " kG to";

		if (model430.fieldUnits == 0)
		{
			if (model430.rampRateTimeUnits() == 0)
				ui.rampdownRateUnitsLabel->setText("Rate (kG/sec)");
			else
				ui.rampdownRateUnitsLabel->setText("Rate (kG/min)");

			ui.rampdownLimitUnitsLabel->setText("Limit (kG)");
			rampdownSegMinLimits[0]->setText("<html>0.0 kG to &plusmn;</html>");
		}
		else
		{
			if (model430.rampRateTimeUnits() == 0)
				ui.rampdownRateUnitsLabel->setText("Rate (T/sec)");
			else
				ui.rampdownRateUnitsLabel->setText("Rate (T/min)");

			ui.rampdownLimitUnitsLabel->setText("Limit (T)");
			unitsStr = " T to";
			rampdownSegMinLimits[0]->setText("<html>0.0 T to &plusmn;</html>");
		}

		for (int i = 0; i < model430.rampdownSegments(); i++)
		{
			if (i > 0)
				rampdownSegMinLimits[i]->setText("<html>&plusmn;" + QString::number(model430.fieldRampdownLimits[i - 1](), 'f', 1) + unitsStr + " &plusmn;</html>");

			rampdownSegMaxLimits[i]->setText(QString::number(model430.fieldRampdownLimits[i](), 'f', 1));
			rampdownSegValues[i]->setText(QString::number(model430.fieldRampdownRates[i](), 'g', 10));
		}

		for (int i = model430.rampdownSegments(); i < 10; i++)
		{
			rampdownSegMinLimits[i]->clear();
			rampdownSegMaxLimits[i]->clear();
			rampdownSegValues[i]->clear();
		}
	}

	QApplication::restoreOverrideCursor();
}

//---------------------------------------------------------------------------
void magnetdaq::inductanceSenseButtonClicked(void)
{
	if (socket)
	{
		QApplication::setOverrideCursor(Qt::WaitCursor);
		statusMisc->setStyleSheet("color: red; font: bold");
		statusMisc->setText("Inductance measurement blocks comm; observe front panel...");
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

		socket->sendExtendedQuery("IND:SENSE?\r\n", SENSE_INDUCTANCE, 10); // 10 second time limit on reply
		QApplication::restoreOverrideCursor();
		clearMiscDisplay();
	}
}

//---------------------------------------------------------------------------
void magnetdaq::switchCurrentDetectionButtonClicked(void)
{
	if (socket)
	{
		QApplication::setOverrideCursor(Qt::WaitCursor);
		statusMisc->setStyleSheet("color: red; font: bold");
		statusMisc->setText("Switch current detection blocks comm; observe front panel...");
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

		socket->sendExtendedQuery("PS:AUTOD?\r\n", AUTODETECT_SWITCH_CURRENT, 10); // 10 second time limit on reply
		QApplication::restoreOverrideCursor();
		clearMiscDisplay();
	}
}

//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
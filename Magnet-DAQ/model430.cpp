#include <stdafx.h>
#include "model430.h"
#include "socket.h"
#include "magnetdaq.h"

//---------------------------------------------------------------------------
Model430::Model430(QObject *parent) : QObject(parent)
{
	socket = nullptr;
	persistentState = false;
	switchHeaterState = false;
	shortSampleMode = false;
	magnetField = 0.0;
	magnetCurrent = 0.0;
	magnetVoltage = 0.0;
	supplyCurrent = 0.0;
	supplyVoltage = 0.0;
	quenchCurrent = 0.0;

	// setup on_change() connections for properties
	mode.on_change().connect([this](int val)					{ this->modeValueChanged(); });
	fieldUnits.on_change().connect([this](int val)				{ this->fieldUnitsChanged(); });
	targetCurrent.on_change().connect([this](double val)		{ this->valueChanged(QueryState::TARGET_CURRENT); });
	targetField.on_change().connect([this](double val)			{ this->valueChanged(QueryState::TARGET_FIELD); });
	voltageLimit.on_change().connect([this](double val)			{ this->valueChanged(QueryState::VOLTAGE_LIMIT); });

	currentRange.on_change().connect([this](int val)			{ this->valueChanged(QueryState::CURRENT_RANGE); });
	powerSupplySelection.on_change().connect([this](int val)	{ this->valueChanged(QueryState::SUPPLY_TYPE); });
	minSupplyVoltage.on_change().connect([this](double val)		{ this->valueChanged(QueryState::SUPPLY_MIN_VOLTAGE); });
	maxSupplyVoltage.on_change().connect([this](double val)		{ this->valueChanged(QueryState::SUPPLY_MAX_VOLTAGE); });
	minSupplyCurrent.on_change().connect([this](double val)		{ this->valueChanged(QueryState::SUPPLY_MIN_CURRENT); });
	maxSupplyCurrent.on_change().connect([this](double val)		{ this->valueChanged(QueryState::SUPPLY_MAX_CURRENT); });
	inputVoltageRange.on_change().connect([this](int val)		{ this->valueChanged(QueryState::SUPPLY_VV_INPUT); });

	stabilityMode.on_change().connect([this](int val)			{ this->valueChanged(QueryState::STABILITY_MODE); });
	stabilitySetting.on_change().connect([this](double val)		{ this->valueChanged(QueryState::STABILITY_SETTING); });
	stabilityResistor.on_change().connect([this](bool val)		{ this->valueChanged(QueryState::STABILITY_RESISTOR); });
	coilConstant.on_change().connect([this](double val)			{ this->valueChanged(QueryState::COIL_CONSTANT); });
	inductance.on_change().connect([this](double val)			{ this->valueChanged(QueryState::INDUCTANCE); });
	absorberPresent.on_change().connect([this](bool val)		{ this->valueChanged(QueryState::ABSORBER_PRESENT); });

	switchInstalled.on_change().connect([this](bool val)		{ this->valueChanged(QueryState::SWITCH_INSTALLED); });
	switchCurrent.on_change().connect([this](double val)		{ this->valueChanged(QueryState::SWITCH_CURRENT); });
	switchTransition.on_change().connect([this](int val)		{ this->valueChanged(QueryState::SWITCH_TRANSITION); });
	switchHeatedTime.on_change().connect([this](int val)		{ this->valueChanged(QueryState::SWITCH_HEATED_TIME); });
	switchCooledTime.on_change().connect([this](int val)		{ this->valueChanged(QueryState::SWITCH_COOLED_TIME); });
	cooledSwitchRampRate.on_change().connect([this](double val) { this->valueChanged(QueryState::PS_RAMP_RATE); });
	switchCoolingGain.on_change().connect([this](double val)	{ this->valueChanged(QueryState::SWITCH_COOLING_GAIN); });

	currentLimit.on_change().connect([this](double val)			{ this->valueChanged(QueryState::CURRENT_LIMIT); });
	quenchDetection.on_change().connect([this](int val)			{ this->valueChanged(QueryState::QUENCH_ENABLE); });
	sampleQuenchDetection.on_change().connect([this](bool val)	{ this->valueChanged(QueryState::SAMPLE_QUENCH_ENABLE); });
	sampleQuenchLimit.on_change().connect([this](int val)		{ this->valueChanged(QueryState::SAMPLE_QUENCH_LIMIT); });
	quenchSensitivity.on_change().connect([this](int val)		{ this->valueChanged(QueryState::QUENCH_SENSITIVITY); });
	protectionMode.on_change().connect([this](int val)			{ this->valueChanged(QueryState::PROTECTION_MODE); });
	IcSlope.on_change().connect([this](double val)				{ this->valueChanged(QueryState::IC_SLOPE); });
	IcOffset.on_change().connect([this](double val)				{ this->valueChanged(QueryState::IC_OFFSET); });
	Tmax.on_change().connect([this](double val)					{ this->valueChanged(QueryState::TMAX); });
	Tscale.on_change().connect([this](double val)				{ this->valueChanged(QueryState::TSCALE); });
	Toffset.on_change().connect([this](double val)				{ this->valueChanged(QueryState::TOFFSET); });
	extRampdownEnabled.on_change().connect([this](bool val)		{ this->valueChanged(QueryState::EXT_RAMPDOWN); });

	rampRateTimeUnits.on_change().connect([this](int val)		{ this->valueChanged(QueryState::RAMP_TIMEBASE); });
	rampRateSegments.on_change().connect([this](int val)		{ this->valueChanged(QueryState::RAMP_SEGMENTS); });
	rampdownSegments.on_change().connect([this](int val)		{ this->valueChanged(QueryState::RAMPDOWN_SEGMENTS); });

	for (int i = 0; i < 10; i++)	// up to 10 segments
	{
		currentRampRates[i].on_change().connect([this](double val)	{ this->valueChanged(QueryState::RAMP_RATE_CURRENT); });
		currentRampLimits[i].on_change().connect([this](double val) { this->valueChanged(QueryState::RAMP_RATE_CURRENT); });
		fieldRampRates[i].on_change().connect([this](double val)	{ this->valueChanged(QueryState::RAMP_RATE_FIELD); });
		fieldRampLimits[i].on_change().connect([this](double val)	{ this->valueChanged(QueryState::RAMP_RATE_FIELD); });
	}

	for (int i = 0; i < 10; i++)	// up to 10 segments
	{
		currentRampdownRates[i].on_change().connect([this](double val)	{ this->valueChanged(QueryState::RAMPDOWN_CURRENT); });
		currentRampdownLimits[i].on_change().connect([this](double val) { this->valueChanged(QueryState::RAMPDOWN_CURRENT); });
		fieldRampdownRates[i].on_change().connect([this](double val)	{ this->valueChanged(QueryState::RAMPDOWN_FIELD); });
		fieldRampdownLimits[i].on_change().connect([this](double val)	{ this->valueChanged(QueryState::RAMPDOWN_FIELD); });
	}
}

//---------------------------------------------------------------------------
Model430::~Model430()
{

}

//---------------------------------------------------------------------------
void Model430::sync(void)
{
	// sync the state of this object with remote instrument's values
	if (socket)
	{
		syncSupplySetup();
		syncLoadSetup();
		syncSwitchSetup();
		syncProtectionSetup();
		syncRampRates();
	}
}

//---------------------------------------------------------------------------
void Model430::syncFieldUnits(void)
{
	if (socket && !shortSampleMode)
	{
		socket->sendQuery("FIELD::UNITS?\r\n", QueryState::FIELD_UNITS);
	}
}

//---------------------------------------------------------------------------
void Model430::fieldUnitsChanged(void)
{
	emit configurationChanged(QueryState::FIELD_UNITS);
}

//---------------------------------------------------------------------------
void Model430::syncSupplySetup(void)
{
	// sync the state of this object with remote instrument's values
	if (socket)
	{
		if (firmwareVersion() > 3.15 || (firmwareVersion() < 3.0 && firmwareVersion() > 2.65))
			socket->sendQuery("SUPP:RANGE?\r\n", QueryState::CURRENT_RANGE);
		else
			currentRange = 0;	// always Normal (High) for legacy units

		socket->sendQuery("SUPP:TYPE?\r\n", QueryState::SUPPLY_TYPE);
		socket->sendQuery("SUPP:VOLT:MIN?\r\n", QueryState::SUPPLY_MIN_VOLTAGE);
		socket->sendQuery("SUPP:VOLT:MAX?\r\n", QueryState::SUPPLY_MAX_VOLTAGE);
		socket->sendQuery("SUPP:CURR:MIN?\r\n", QueryState::SUPPLY_MIN_CURRENT);
		socket->sendQuery("SUPP:CURR:MAX?\r\n", QueryState::SUPPLY_MAX_CURRENT);
		socket->sendQuery("SUPP:MODE?\r\n", QueryState::SUPPLY_VV_INPUT);
	}
}

//---------------------------------------------------------------------------
void Model430::syncLoadSetup(void)
{
	// sync the state of this object with remote instrument's values
	if (socket)
	{
		socket->sendQuery("STAB:MODE?\r\n", QueryState::STABILITY_MODE);
		socket->sendQuery("STAB?\r\n", QueryState::STABILITY_SETTING);
		socket->sendQuery("STAB:RES?\r\n", QueryState::STABILITY_RESISTOR);
		if (!shortSampleMode)
		{
			socket->sendQuery("COIL?\r\n", QueryState::COIL_CONSTANT);
			socket->sendQuery("IND?\r\n", QueryState::INDUCTANCE);
			socket->sendQuery("AB?\r\n", QueryState::ABSORBER_PRESENT);
		}
	}
}

//---------------------------------------------------------------------------
void Model430::syncSwitchSetup(void)
{
	// sync the state of this object with remote instrument's values
	if (socket && !shortSampleMode)
	{
		socket->sendQuery("PS:INST?\r\n", QueryState::SWITCH_INSTALLED);
		socket->sendQuery("STAB:RES?\r\n", QueryState::STABILITY_RESISTOR);
		socket->sendQuery("PS:CURR?\r\n", QueryState::SWITCH_CURRENT);
		socket->sendQuery("PS:TRAN?\r\n", QueryState::SWITCH_TRANSITION);
		socket->sendQuery("PS:HTIME?\r\n", QueryState::SWITCH_HEATED_TIME);
		socket->sendQuery("PS:CTIME?\r\n", QueryState::SWITCH_COOLED_TIME);
		socket->sendQuery("PS:PSRR?\r\n", QueryState::PS_RAMP_RATE);
		socket->sendQuery("PS:CGAIN?\r\n", QueryState::SWITCH_COOLING_GAIN);

		if (switchInstalled())
			socket->sendQuery("PS?\r\n", QueryState::SWITCH_HTR_STATE);
		else
			switchHeaterState = false;
	}
}

//---------------------------------------------------------------------------
void Model430::syncProtectionSetup(void)
{
	// sync the state of this object with remote instrument's values
	if (socket)
	{
		socket->sendQuery("CURR:LIM?\r\n", QueryState::CURRENT_LIMIT);
		
		if (shortSampleMode)
		{
			socket->sendQuery("QU:DET?\r\n", QueryState::SAMPLE_QUENCH_ENABLE);

			if (firmwareVersion() > 3.15 || (firmwareVersion() < 3.0 && firmwareVersion() > 2.65))
				socket->sendQuery("QU:SAM?\r\n", QueryState::SAMPLE_QUENCH_LIMIT);
		}
		else
		{
			socket->sendQuery("QU:DET?\r\n", QueryState::QUENCH_ENABLE);
			socket->sendQuery("QU:RATE?\r\n", QueryState::QUENCH_SENSITIVITY);
			socket->sendQuery("RAMPD:ENAB?\r\n", QueryState::EXT_RAMPDOWN);
		}

		if (!shortSampleMode && (mode() & USE_OPCONSTS))
		{
			socket->sendQuery("OPL:MODE?\r\n", QueryState::PROTECTION_MODE);
			socket->sendQuery("OPL:ICSLOPE?\r\n", QueryState::IC_SLOPE);
			socket->sendQuery("OPL:ICOFFSET?\r\n", QueryState::IC_OFFSET);
			socket->sendQuery("OPL:TMAX?\r\n", QueryState::TMAX);
			socket->sendQuery("OPL:TSCALE?\r\n", QueryState::TSCALE);
			socket->sendQuery("OPL:TOFFSET?\r\n", QueryState::TOFFSET);
		}
	}
}

//---------------------------------------------------------------------------
void Model430::syncEventCounts(bool isBlocking)
{
	// sync the state of this object with remote instrument's values
	if (socket)
	{
		if (isBlocking)
		{
			if (!shortSampleMode)
				socket->sendExtendedQuery("RAMPD:COUNT?\r\n", QueryState::RAMPDOWN_COUNT, 2); // 2 second time limit on reply

			socket->sendExtendedQuery("QU:COUNT?\r\n", QueryState::QUENCH_COUNT, 2); // 2 second time limit on reply
		}
		else
		{
			if (!shortSampleMode)
				socket->sendQuery("RAMPD:COUNT?\r\n", QueryState::RAMPDOWN_COUNT);

			socket->sendQuery("QU:COUNT?\r\n", QueryState::QUENCH_COUNT);
		}
	}
}

//---------------------------------------------------------------------------
void Model430::syncTargetCurrent(void)
{
	if (socket)
		socket->sendQuery("CURR:TARG?\r\n", QueryState::TARGET_CURRENT);
}

//---------------------------------------------------------------------------
void Model430::syncTargetField(void)
{
	if (socket && !shortSampleMode)
		socket->sendQuery("FIELD:TARG?\r\n", QueryState::TARGET_FIELD);
}

//---------------------------------------------------------------------------
void Model430::syncStabilityMode(void)
{
	if (socket)
		socket->sendQuery("STAB:MODE?\r\n", QueryState::STABILITY_MODE);
}

//---------------------------------------------------------------------------
void Model430::syncStabilitySetting(void)
{
	if (socket)
		socket->sendQuery("STAB?\r\n", QueryState::STABILITY_SETTING);
}

//---------------------------------------------------------------------------
void Model430::syncInductance(void)
{
	if (socket && !shortSampleMode)
		socket->sendQuery("IND?\r\n", QueryState::INDUCTANCE);
}

//---------------------------------------------------------------------------
void Model430::syncRampRates(void)
{
	// get all the present ramp rate segments
	if (socket)
	{
		// get the present target setpoint in A
		socket->sendQuery("CURR:TARG?\r\n", QueryState::TARGET_CURRENT);

		// get the present number of segments
		socket->sendQuery("RAMP:RATE:SEG?\r\n", QueryState::RAMP_SEGMENTS);

		// get the time units (sec or min)
		socket->sendQuery("RAMP:RATE:UNITS?\r\n", QueryState::RAMP_TIMEBASE);

		if (socket && !shortSampleMode)
		{
			// get the present target setpoint in present field units
			socket->sendQuery("FIELD:TARG?\r\n", QueryState::TARGET_FIELD);

			// get the present voltage limit
			socket->sendQuery("VOLT:LIM?\r\n", QueryState::VOLTAGE_LIMIT);

			// get the present number of rampdown segments
			socket->sendQuery("RAMPD:RATE:SEG?\r\n", QueryState::RAMPDOWN_SEGMENTS);

			// get the field units (kG or T)
			socket->sendQuery("FIELD:UNITS?\r\n", QueryState::FIELD_UNITS);
		}

		syncRampSegmentValues();
	}
}

//---------------------------------------------------------------------------
void Model430::syncRampSegmentValues(void)
{
	if (socket)
	{
		// avoid memory crashes
		int segCount = 10;

		if (rampRateSegments() < 10 && rampRateSegments() >= 0)
			segCount = rampRateSegments();

		// get the ramp segments
		for (int i = 0; i < segCount; i++)
		{
			if (socket)
			{
				// get the "i"th segment, starts at 1
				QString queryStr = "RAMP:RATE:CURR:" + QString::number(i + 1) + "?\r\n";
				socket->sendRampQuery(queryStr, QueryState::RAMP_RATE_CURRENT, i + 1);

				if (socket && !shortSampleMode)
				{
					queryStr = "RAMP:RATE:FIELD:" + QString::number(i + 1) + "?\r\n";
					socket->sendRampQuery(queryStr, QueryState::RAMP_RATE_FIELD, i + 1);
				}
			}
			else
				break;
		}

		if (!shortSampleMode)
			emit syncRampPlot();
	}
}

//---------------------------------------------------------------------------
void Model430::syncRampdownSegmentValues(void)
{
	if (socket && !shortSampleMode)
	{
		// avoid memory crashes
		int segCount = 10;

		// get the present number of segments
		socket->sendExtendedQuery("RAMPD:RATE:SEG?\r\n", QueryState::RAMPDOWN_SEGMENTS, 2); // 2 second time limit on reply

		if (rampdownSegments() < 10 && rampdownSegments() >= 0)
			segCount = rampdownSegments();

		// get the ramp segments
		for (int i = 0; i < segCount; i++)
		{
			if (socket)
			{
				// get the "i"th segment, starts at 1
				QString queryStr = "RAMPD:RATE:CURR:" + QString::number(i + 1) + "?\r\n";
				socket->sendRampQuery(queryStr, QueryState::RAMPDOWN_CURRENT, i + 1);

				queryStr = "RAMPD:RATE:FIELD:" + QString::number(i + 1) + "?\r\n";
				socket->sendRampQuery(queryStr, QueryState::RAMPDOWN_FIELD, i + 1);
			}
			else
				break;
		}

		syncRampdownPlot();
	}
}

//---------------------------------------------------------------------------
void Model430::valueChanged(QueryState aState)
{
	emit configurationChanged(aState);
}

//---------------------------------------------------------------------------
void Model430::modeValueChanged(void)
{
	if (mode() & SHORT_SAMPLE_MODE)
	{
		if (shortSampleMode == false)
		{
			shortSampleMode = true;
			emit shortSampleModeChanged(shortSampleMode);
		}
	}
	else
	{
		if (shortSampleMode == true)
		{
			shortSampleMode = false;
			emit shortSampleModeChanged(shortSampleMode);
		}
	}
}

//---------------------------------------------------------------------------
void Model430::setRampdownFile(QString str)
{
	rampdownFile = str;
	emit syncRampdownEvents(rampdownFile);
}

//---------------------------------------------------------------------------
void Model430::setQuenchFile(QString str)
{
	quenchFile = str;
	emit syncQuenchEvents(quenchFile);
}

//---------------------------------------------------------------------------
void Model430::setSettings(QString str)
{
	textSettings = str;
	emit syncTextSettings(textSettings);
}

//---------------------------------------------------------------------------
void Model430::setCurrentData(qint64 time, double magField, double magCurrent, double magVoltage, double supCurrent, double supVoltage, double refCurrent)
{
	timestamp = time;
	magnetField = magField;
	magnetCurrent = magCurrent;
	magnetVoltage = magVoltage;
	supplyCurrent = supCurrent;
	supplyVoltage = supVoltage;
	referenceCurrent = refCurrent;
}

//---------------------------------------------------------------------------
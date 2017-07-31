#include <stdafx.h>
#include "model430.h"
#include "socket.h"
#include "magnetdaq.h"

//---------------------------------------------------------------------------
Model430::Model430(QObject *parent) : QObject(parent) 
{
	socket = NULL;

	// setup on_change() connections for properties
	mode.on_change().connect([this](int val)					{ this->modeValueChanged(); });
	fieldUnits.on_change().connect([this](int val)				{ this->fieldUnitsChanged(); });

	powerSupplySelection.on_change().connect([this](int val)	{ this->valueChanged(SUPPLY_TYPE); });
	minSupplyVoltage.on_change().connect([this](double val)		{ this->valueChanged(SUPPLY_MIN_VOLTAGE); });
	maxSupplyVoltage.on_change().connect([this](double val)		{ this->valueChanged(SUPPLY_MAX_VOLTAGE); });
	minSupplyCurrent.on_change().connect([this](double val)		{ this->valueChanged(SUPPLY_MIN_CURRENT); });
	maxSupplyCurrent.on_change().connect([this](double val)		{ this->valueChanged(SUPPLY_MAX_CURRENT); });
	inputVoltageRange.on_change().connect([this](int val)		{ this->valueChanged(SUPPLY_VV_INPUT); });

	stabilityMode.on_change().connect([this](int val)			{ this->valueChanged(STABILITY_MODE); });
	stabilitySetting.on_change().connect([this](double val)		{ this->valueChanged(STABILITY_SETTING); });
	stabilityResistor.on_change().connect([this](bool val)		{ this->valueChanged(STABILITY_RESISTOR); });
	coilConstant.on_change().connect([this](double val)			{ this->valueChanged(COIL_CONSTANT); });
	inductance.on_change().connect([this](double val)			{ this->valueChanged(INDUCTANCE); });
	absorberPresent.on_change().connect([this](bool val)		{ this->valueChanged(ABSORBER_PRESENT); });

	switchInstalled.on_change().connect([this](bool val)		{ this->valueChanged(SWITCH_INSTALLED); });
	switchCurrent.on_change().connect([this](double val)		{ this->valueChanged(SWITCH_CURRENT); });
	switchTransition.on_change().connect([this](int val)		{ this->valueChanged(SWITCH_TRANSITION); });
	switchHeatedTime.on_change().connect([this](int val)		{ this->valueChanged(SWITCH_HEATED_TIME); });
	switchCooledTime.on_change().connect([this](int val)		{ this->valueChanged(SWITCH_COOLED_TIME); });
	cooledSwitchRampRate.on_change().connect([this](double val) { this->valueChanged(PS_RAMP_RATE); });
	switchCoolingGain.on_change().connect([this](double val)	{ this->valueChanged(SWITCH_COOLING_GAIN); });

	currentLimit.on_change().connect([this](double val) { this->valueChanged(CURRENT_LIMIT); });
	quenchDetection.on_change().connect([this](int val)			{ this->valueChanged(QUENCH_ENABLE); });
	quenchSensitivity.on_change().connect([this](int val)		{ this->valueChanged(QUENCH_SENSITIVITY); });
	protectionMode.on_change().connect([this](int val)			{ this->valueChanged(PROTECTION_MODE); });
	IcSlope.on_change().connect([this](double val)				{ this->valueChanged(IC_SLOPE); });
	IcOffset.on_change().connect([this](double val)				{ this->valueChanged(IC_OFFSET); });
	Tmax.on_change().connect([this](double val)					{ this->valueChanged(TMAX); });
	Tscale.on_change().connect([this](double val)				{ this->valueChanged(TSCALE); });
	Toffset.on_change().connect([this](double val)				{ this->valueChanged(TOFFSET); });
	extRampdownEnabled.on_change().connect([this](bool val)		{ this->valueChanged(EXT_RAMPDOWN); });

	rampRateUnits.on_change().connect([this](int val)			{ this->valueChanged(RAMP_UNITS); });
	rampRateSegments.on_change().connect([this](int val)		{ this->valueChanged(RAMP_SEGMENTS); });
	rampdownSegments.on_change().connect([this](int val)		{ this->valueChanged(RAMPDOWN_SEGMENTS); });
	
	for (int i = 0; i < 10; i++)	// up to 10 segments
	{
		currentRampRates[i].on_change().connect([this](double val)	{ this->valueChanged(RAMP_RATE_CURRENT); });
		currentRampLimits[i].on_change().connect([this](double val) { this->valueChanged(RAMP_RATE_CURRENT); });
		fieldRampRates[i].on_change().connect([this](double val)	{ this->valueChanged(RAMP_RATE_FIELD); });
		fieldRampLimits[i].on_change().connect([this](double val)	{ this->valueChanged(RAMP_RATE_FIELD); });
	}

	for (int i = 0; i < 10; i++)	// up to 10 segments
	{
		currentRampdownRates[i].on_change().connect([this](double val)	{ this->valueChanged(RAMPDOWN_CURRENT); });
		currentRampdownLimits[i].on_change().connect([this](double val) { this->valueChanged(RAMPDOWN_CURRENT); });
		fieldRampdownRates[i].on_change().connect([this](double val)	{ this->valueChanged(RAMPDOWN_FIELD); });
		fieldRampdownLimits[i].on_change().connect([this](double val)	{ this->valueChanged(RAMPDOWN_FIELD); });
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
	if (socket)
	{
		socket->sendQuery("FIELD::UNITS?\r\n", FIELD_UNITS);
	}
}

//---------------------------------------------------------------------------
void Model430::fieldUnitsChanged(void)
{
	emit configurationChanged(FIELD_UNITS);
}

//---------------------------------------------------------------------------
void Model430::syncSupplySetup(void)
{
	// sync the state of this object with remote instrument's values
	if (socket)
	{
		socket->sendQuery("SUPP:TYPE?\r\n", SUPPLY_TYPE);
		socket->sendQuery("SUPP:VOLT:MIN?\r\n", SUPPLY_MIN_VOLTAGE);
		socket->sendQuery("SUPP:VOLT:MAX?\r\n", SUPPLY_MAX_VOLTAGE);
		socket->sendQuery("SUPP:CURR:MIN?\r\n", SUPPLY_MIN_CURRENT);
		socket->sendQuery("SUPP:CURR:MAX?\r\n", SUPPLY_MAX_CURRENT);
		socket->sendQuery("SUPP:MODE?\r\n", SUPPLY_VV_INPUT);
	}
}

//---------------------------------------------------------------------------
void Model430::syncLoadSetup(void)
{
	// sync the state of this object with remote instrument's values
	if (socket)
	{
		socket->sendQuery("STAB:MODE?\r\n", STABILITY_MODE);
		socket->sendQuery("STAB?\r\n", STABILITY_SETTING);
		socket->sendQuery("STAB:RES?\r\n", STABILITY_RESISTOR);
		socket->sendQuery("COIL?\r\n", COIL_CONSTANT);
		socket->sendQuery("IND?\r\n", INDUCTANCE);
		socket->sendQuery("AB?\r\n", ABSORBER_PRESENT);
	}
}

//---------------------------------------------------------------------------
void Model430::syncSwitchSetup(void)
{
	// sync the state of this object with remote instrument's values
	if (socket)
	{
		socket->sendQuery("PS:INST?\r\n", SWITCH_INSTALLED);
		socket->sendQuery("STAB:RES?\r\n", STABILITY_RESISTOR);
		socket->sendQuery("PS:CURR?\r\n", SWITCH_CURRENT);
		socket->sendQuery("PS:TRAN?\r\n", SWITCH_TRANSITION);
		socket->sendQuery("PS:HTIME?\r\n", SWITCH_HEATED_TIME);
		socket->sendQuery("PS:CTIME?\r\n", SWITCH_COOLED_TIME);
		socket->sendQuery("PS:PSRR?\r\n", PS_RAMP_RATE);
		socket->sendQuery("PS:CGAIN?\r\n", SWITCH_COOLING_GAIN);
	}
}

//---------------------------------------------------------------------------
void Model430::syncProtectionSetup(void)
{
	// sync the state of this object with remote instrument's values
	if (socket)
	{
		socket->sendQuery("CURR:LIM?\r\n", CURRENT_LIMIT);
		socket->sendQuery("QU:DET?\r\n", QUENCH_ENABLE);
		socket->sendQuery("QU:RATE?\r\n", QUENCH_SENSITIVITY);
		socket->sendQuery("RAMPD:ENAB?\r\n", EXT_RAMPDOWN);

		if (mode() & 0x02)
		{
			socket->sendQuery("OPL:MODE?\r\n", PROTECTION_MODE);
			socket->sendQuery("OPL:ICSLOPE?\r\n", IC_SLOPE);
			socket->sendQuery("OPL:ICOFFSET?\r\n", IC_OFFSET);
			socket->sendQuery("OPL:TMAX?\r\n", TMAX);
			socket->sendQuery("OPL:TSCALE?\r\n", TSCALE);
			socket->sendQuery("OPL:TOFFSET?\r\n", TOFFSET);
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
			socket->sendExtendedQuery("RAMPD:COUNT?\r\n", RAMPDOWN_COUNT, 2); // 2 second time limit on reply
			socket->sendExtendedQuery("QU:COUNT?\r\n", QUENCH_COUNT, 2); // 2 second time limit on reply
		}
		else
		{
			socket->sendQuery("RAMPD:COUNT?\r\n", RAMPDOWN_COUNT);
			socket->sendQuery("QU:COUNT?\r\n", QUENCH_COUNT);
		}		
	}
}

//---------------------------------------------------------------------------
void Model430::syncStabilityMode(void)
{
	if (socket)
		socket->sendQuery("STAB:MODE?\r\n", STABILITY_MODE);
}

//---------------------------------------------------------------------------
void Model430::syncStabilitySetting(void)
{
	if (socket)
		socket->sendQuery("STAB?\r\n", STABILITY_SETTING);
}

//---------------------------------------------------------------------------
void Model430::syncInductance(void)
{
	if (socket)
		socket->sendQuery("IND?\r\n", INDUCTANCE);
}

//---------------------------------------------------------------------------
void Model430::syncRampRates(void)
{
	// get all the present ramp rate segments
	if (socket)
	{
		// get the present number of segments
		socket->sendQuery("RAMP:RATE:SEG?\r\n", RAMP_SEGMENTS);

		// get the present number of rampdown segments
		socket->sendQuery("RAMPD:RATE:SEG?\r\n", RAMPDOWN_SEGMENTS);

		// get the units (A or kg/T)
		socket->sendQuery("RAMP:RATE:UNITS?\r\n", RAMP_UNITS);

		// get the timebase (sec or min)
		socket->sendQuery("FIELD:UNITS?\r\n", FIELD_UNITS);

		syncRampSegmentValues();
	}
}

//---------------------------------------------------------------------------
void Model430::syncRampSegmentValues(void)
{
	if (socket)
	{
		for (int i = 0; i < rampRateSegments(); i++)
		{
			// get the "i"th segment, starts at 1
			QString queryStr = "RAMP:RATE:CURR:" + QString::number(i + 1) + "?\r\n";
			socket->sendRampQuery(queryStr, RAMP_RATE_CURRENT, i + 1);

			queryStr = "RAMP:RATE:FIELD:" + QString::number(i + 1) + "?\r\n";
			socket->sendRampQuery(queryStr, RAMP_RATE_FIELD, i + 1);
		}

		emit syncRampPlot();
	}
}

//---------------------------------------------------------------------------
void Model430::syncRampdownSegmentValues(void)
{
	if (socket)
	{
		// get the present number of segments
		socket->sendExtendedQuery("RAMPD:RATE:SEG?\r\n", RAMPDOWN_SEGMENTS, 2); // 2 second time limit on reply

		for (int i = 0; i < rampdownSegments(); i++)
		{
			// get the "i"th segment, starts at 1
			QString queryStr = "RAMPD:RATE:CURR:" + QString::number(i + 1) + "?\r\n";
			socket->sendRampQuery(queryStr, RAMPDOWN_CURRENT, i + 1);

			queryStr = "RAMPD:RATE:FIELD:" + QString::number(i + 1) + "?\r\n";
			socket->sendRampQuery(queryStr, RAMPDOWN_FIELD, i + 1);
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

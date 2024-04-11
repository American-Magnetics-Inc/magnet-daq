#include "stdafx.h"
#include "socket.h"
#include "QDateTime"
#include <magnetdaq.h>

#undef DEBUG
//#define DEBUG

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
#include <unistd.h>
#endif

// timeout constant
const int TIMEOUT = 1000;

// save parent
static magnetdaq* magnetdaqParent;


//---------------------------------------------------------------------------
Socket::Socket(QObject *parent)
	: QObject(parent)
{
	model430 = NULL;
	socket = NULL;
	unitConnected = false;
	queryState.store(QueryState::WELCOME_STRING);
	commandTimer.setInterval(0);
	refCurrent = 0.0;
	state = 0;
	heater = 0;
	connect(&commandTimer, SIGNAL(timeout()), this, SLOT(commandTimerTimeout()));
}

//---------------------------------------------------------------------------
Socket::Socket(Model430 *settings, QObject *parent)
{
	model430 = settings;
	magnetdaqParent = dynamic_cast<magnetdaq *>(parent);
	socket = NULL;
	unitConnected = false;
	queryState.store(QueryState::WELCOME_STRING);
	commandTimer.setInterval(0);
	connect(&commandTimer, SIGNAL(timeout()), this, SLOT(commandTimerTimeout()));
}

//---------------------------------------------------------------------------
Socket::~Socket()
{
	if(socket)
	{
		if (unitConnected)
			socket->close();

		delete socket;
	}
}

//---------------------------------------------------------------------------
void Socket::connectToModel430(QString ipaddress, quint16 port, QNetworkProxy::ProxyType aProxyType)
{
	socket = new QTcpSocket(this);
	socket->setProxy(aProxyType);

	connect(socket, SIGNAL(connected()), this, SLOT(connected()));
	connect(socket, SIGNAL(disconnected()), this, SLOT(disconnected()));
	connect(socket, SIGNAL(readyRead()), this, SLOT(readyRead()));
	connect(socket, SIGNAL(bytesWritten(qint64)), this, SLOT(bytesWritten(qint64)));

	ipAddress = ipaddress;
	ipPort = port;
	socket->connectToHost(ipaddress, port);

	if (!socket->waitForConnected(5000))
	{
		qDebug() << "Error: " << socket->errorString();
		unitConnected = false;
	}
}

//---------------------------------------------------------------------------
void Socket::connected()
{
	qDebug() << "Connected to Model 430 @" + ipAddress + ":" + QString::number(ipPort);
	unitConnected = true;

	socket->waitForReadyRead(1000);	// consume WELCOME_STRING
}

//---------------------------------------------------------------------------
void Socket::disconnected()
{
	unitConnected = false;
	qDebug() << "Disconnected from Model 430 @" + ipAddress + ":" + QString::number(ipPort);
	emit model430Disconnected();
}

//---------------------------------------------------------------------------
void Socket::bytesWritten(qint64 bytes)
{
#ifdef DEBUG
	//qDebug() << "Bytes written: " << bytes;
#endif
}

//---------------------------------------------------------------------------
void Socket::readyRead()
{
	QString reply = QString::fromLatin1(socket->readAll());

	if (queryState.load() == QueryState::WELCOME_STRING)
	{
		qDebug() << "WELCOME_STRING: " << reply;

		if (ipPort == 23 || ipPort > 7189 /* > 7189 for simulation use only */)
			queryState.store(QueryState::MSG_UPDATE);	// receive broadcast MSG's on telnet port only, no commands or queries!
		else
			queryState.store(QueryState::IDLE_STATE);	// commands and queries to port 7180
	}

	else if (queryState.load() == QueryState::FIRMWARE_VERSION)
	{
		qDebug() << "*IDN? Reply: " << reply;

		// read return data
		// split at the , delimiters
		QStringList strList = reply.split(",");

		// parse serial number
		model430->serialNumber.set(strList.at(2));

		// firmware revision is always last string
		QString versionStr = strList.last();
		QString suffix;

		// parse only the version number
		for (int i = 0; i < versionStr.length(); i++)
		{
			if (versionStr[i].isDigit() || versionStr[i] == '.')
				continue;
			else
			{
				suffix = versionStr.mid(i);
				suffix.truncate(suffix.length() - 2);	// remove terminators
				versionStr.truncate(i);	// numerical version id
			}
		}

		// convert to double
		bool ok;
		double temp = versionStr.toDouble(&ok);

		if (ok)
		{
			model430->setFirmwareVersion(temp);
			model430->setFirmwareSuffix(suffix);
		}

		queryState.store(QueryState::IDLE_STATE);
	}

	else if (queryState.load() == QueryState::TRG_SAMPLE || queryState.load() == QueryState::AMI_TRG_SAMPLE)
	{
		// this assumes *ETE 151 or *AMITRIG command
		#ifdef DEBUG
		if (queryState.load() == AMI_TRG_SAMPLE)
			qDebug() << "*AMITRG Reply: " << reply;
		else
			qDebug() << "*TRG Reply: " << reply;
		#endif

		// read return data
		// split at the , delimiters
		QStringList strList = reply.split(",");

		for (int i = 0; i < strList.size(); i++)
		{
			bool ok;
			double temp = strList.at(i).toDouble(&ok);

			if (ok)	// if successfully converted to a double
			{
				switch (i)
				{
				case 0:
					magnetField = temp;
					break;
				case 1:
					magnetCurrent = temp;
					break;
				case 2:
					magnetVoltage = temp;
					break;
				case 3:
					supplyCurrent = temp;
					break;
				case 4:
					supplyVoltage = temp;
					break;
				case 5:
					refCurrent = temp;
					break;
				case 6:
					state = (quint8)temp;
					break;
				case 7:
					heater = (quint8)temp;
					break;
				}
			}
		}

		queryState.store(QueryState::IDLE_STATE);
	}

	// simple boolean type queries
	else if (queryState.load() == QueryState::ABSORBER_PRESENT || queryState.load() == QueryState::SWITCH_INSTALLED   ||
			 queryState.load() == QueryState::EXT_RAMPDOWN		|| queryState.load() == QueryState::STABILITY_RESISTOR ||
			 queryState.load() == QueryState::SWITCH_HTR_STATE || queryState.load() == QueryState::SAMPLE_QUENCH_ENABLE)
	{
		#ifdef DEBUG
	if (queryState.load() == QueryState::ABSORBER_PRESENT)
		qDebug() << "AB? Reply: " << reply;
	else if (queryState.load() == QueryState::SWITCH_INSTALLED)
		qDebug() << "PS:INST? Reply: " << reply;
	else if (queryState.load() == QueryState::EXT_RAMPDOWN)
		qDebug() << "RAMPD:ENAB? Reply: " << reply;
	else if (queryState.load() == QueryState::STABILITY_RESISTOR)
		qDebug() << "STAB:RES? Reply: " << reply;
	else if (queryState.load() == QueryState::SWITCH_HTR_STATE)
		qDebug() << "PS? Reply: " << reply;
	else if (queryState.load() == QueryState::SAMPLE_QUENCH_ENABLE)
		qDebug() << "QU:SAM? Reply: " << reply;
		#endif

		// read return data
		bool ok;
		bool temp = (bool)reply.toInt(&ok);

		if (ok)	// if converted to an boolean
		{
			if (queryState.load() == QueryState::ABSORBER_PRESENT)
				model430->absorberPresent = temp;
			else if (queryState.load() == QueryState::SWITCH_INSTALLED)
				model430->switchInstalled = temp;
			else if (queryState.load() == QueryState::EXT_RAMPDOWN)
				model430->extRampdownEnabled = temp;
			else if (queryState.load() == QueryState::STABILITY_RESISTOR)
				model430->stabilityResistor = temp;
			else if (queryState.load() == QueryState::SWITCH_HTR_STATE)
				model430->switchHeaterState = temp;
			else if (queryState.load() == QueryState::SAMPLE_QUENCH_ENABLE)
				model430->sampleQuenchDetection = temp;
		}
		queryState.store(QueryState::IDLE_STATE);
	}

	// simple integer type queries
	else if (queryState.load() == QueryState::SUPPLY_TYPE		  || queryState.load() == QueryState::SUPPLY_VV_INPUT    ||
			 queryState.load() == QueryState::STABILITY_MODE	  || queryState.load() == QueryState::SWITCH_TRANSITION  ||
			 queryState.load() == QueryState::SWITCH_HEATED_TIME || queryState.load() == QueryState::SWITCH_COOLED_TIME ||
			 queryState.load() == QueryState::QUENCH_ENABLE	  || queryState.load() == QueryState::QUENCH_SENSITIVITY ||
			 queryState.load() == QueryState::PROTECTION_MODE	  || queryState.load() == QueryState::MODE				  ||
			 queryState.load() == QueryState::RAMP_TIMEBASE	  || queryState.load() == QueryState::FIELD_UNITS		  ||
			 queryState.load() == QueryState::RAMP_SEGMENTS	  || queryState.load() == QueryState::RAMPDOWN_SEGMENTS  ||
			 queryState.load() == QueryState::RAMPDOWN_COUNT	  || queryState	== QueryState::QUENCH_COUNT		  ||
			 queryState.load() == QueryState::STATE			  || queryState.load() == QueryState::CURRENT_RANGE	  ||
			 queryState.load() == QueryState::SAMPLE_QUENCH_LIMIT)
	{
		#ifdef DEBUG
		if (queryState.load() == QueryState::SUPPLY_TYPE)
			qDebug() << "SUPP:TYPE? Reply: " << reply;
		else if (queryState.load() == QueryState::SUPPLY_VV_INPUT)
			qDebug() << "SUPP:MODE? Reply: " << reply;
		else if (queryState.load() == QueryState::STABILITY_MODE)
			qDebug() << "STAB:MODE? Reply: " << reply;
		else if (queryState.load() == QueryState::SWITCH_TRANSITION)
			qDebug() << "PS:TRAN? Reply: " << reply;
		else if (queryState.load() == QueryState::SWITCH_HEATED_TIME)
			qDebug() << "PS:HTIME? Reply: " << reply;
		else if (queryState.load() == QueryState::SWITCH_COOLED_TIME)
			qDebug() << "PS:CTIME? Reply: " << reply;
		else if (queryState.load() == QueryState::QUENCH_ENABLE)
			qDebug() << "QU:DET? Reply: " << reply;
		else if (queryState.load() == QueryState::QUENCH_SENSITIVITY)
			qDebug() << "QU:RATE? Reply: " << reply;
		else if (queryState.load() == QueryState::PROTECTION_MODE)
			qDebug() << "OPLIMIT:MODE? Reply: " << reply;
		else if (queryState.load() == QueryState::MODE)
			qDebug() << "MODE? Reply: " << reply;
		else if (queryState.load() == QueryState::RAMP_TIMEBASE)
			qDebug() << "RAMP:RATE:UNITS? Reply: " << reply;
		else if (queryState.load() == QueryState::FIELD_UNITS)
			qDebug() << "FIELD:UNITS? Reply: " << reply;
		else if (queryState.load() == QueryState::RAMP_SEGMENTS)
			qDebug() << "RAMP:RATE:SEG? Reply: " << reply;
		else if (queryState.load() == QueryState::RAMPDOWN_SEGMENTS)
			qDebug() << "RAMPD:RATE:SEG? Reply : " << reply;
		else if (queryState.load() == QueryState::RAMPDOWN_COUNT)
			qDebug() << "RAMPD:COUNT? Reply : " << reply;
		else if (queryState.load() == QueryState::QUENCH_COUNT)
			qDebug() << "QU:COUNT? Reply : " << reply;
		else if (queryState.load() == QueryState::STATE)
			qDebug() << "STATE? Reply : " << reply;
		else if (queryState.load() == QueryState::CURRENT_RANGE)
			qDebug() << "SUPP:RANGE? Reply : " << reply;
		else if (queryState.load() == QueryState::SAMPLE_QUENCH_LIMIT)
			qDebug() << "QU:SAM? Reply : " << reply;
		#endif

		// read return data
		bool ok;
		int temp = reply.toInt(&ok);

		if (ok)	// if converted to an integer
		{
			if (queryState.load() == QueryState::SUPPLY_TYPE)
				model430->powerSupplySelection = temp;
			else if (queryState.load() == QueryState::SUPPLY_VV_INPUT)
				model430->inputVoltageRange = temp;
			else if (queryState.load() == QueryState::STABILITY_MODE)
				model430->stabilityMode = temp;
			else if (queryState.load() == QueryState::SWITCH_TRANSITION)
				model430->switchTransition = temp;
			else if (queryState.load() == QueryState::SWITCH_HEATED_TIME)
				model430->switchHeatedTime = temp;
			else if (queryState.load() == QueryState::SWITCH_COOLED_TIME)
				model430->switchCooledTime = temp;
			else if (queryState.load() == QueryState::QUENCH_ENABLE)
				model430->quenchDetection = temp;
			else if (queryState.load() == QueryState::QUENCH_SENSITIVITY)
				model430->quenchSensitivity = temp;
			else if (queryState.load() == QueryState::PROTECTION_MODE)
				model430->protectionMode = temp;
			else if (queryState.load() == QueryState::MODE)
				model430->mode = temp;
			else if (queryState.load() == QueryState::RAMP_TIMEBASE)
				model430->rampRateTimeUnits = temp;
			else if (queryState.load() == QueryState::FIELD_UNITS)
				model430->fieldUnits = temp;
			else if (queryState.load() == QueryState::RAMP_SEGMENTS)
				model430->rampRateSegments = temp;
			else if (queryState.load() == QueryState::RAMPDOWN_SEGMENTS)
				model430->rampdownSegments = temp;
			else if (queryState.load() == QueryState::RAMPDOWN_COUNT)
				model430->rampdownEventsCount = temp;
			else if (queryState.load() == QueryState::QUENCH_COUNT)
				model430->quenchEventsCount = temp;
			else if (queryState.load() == QueryState::STATE)
				model430->state = (State)temp;
			else if (queryState.load() == QueryState::CURRENT_RANGE)
				model430->currentRange = temp;
			else if (queryState.load() == QueryState::SAMPLE_QUENCH_LIMIT)
				model430->sampleQuenchLimit = temp;
		}
		queryState.store(QueryState::IDLE_STATE);
	}

	// simple floating point queries
	else if (queryState.load() == QueryState::SUPPLY_MIN_VOLTAGE  || queryState.load() == QueryState::SUPPLY_MAX_VOLTAGE	  ||
			 queryState.load() == QueryState::SUPPLY_MIN_CURRENT  || queryState.load() == QueryState::SUPPLY_MAX_CURRENT	  ||
			 queryState.load() == QueryState::STABILITY_SETTING   || queryState.load() == QueryState::COIL_CONSTANT		  ||
			 queryState.load() == QueryState::INDUCTANCE		   || queryState.load() == QueryState::SENSE_INDUCTANCE      ||
			 queryState.load() == QueryState::SWITCH_CURRENT	   || queryState.load() == QueryState::PS_RAMP_RATE		  ||
			 queryState.load() == QueryState::SWITCH_COOLING_GAIN || queryState.load() == QueryState::CURRENT_LIMIT		  ||
			 queryState.load() == QueryState::IC_SLOPE			   || queryState.load() == QueryState::IC_OFFSET			  ||
			 queryState.load() == QueryState::TSCALE			   || queryState.load() == QueryState::TOFFSET				  ||
			 queryState.load() == QueryState::TMAX				   || queryState.load() == QueryState::TARGET_CURRENT		  ||
			 queryState.load() == QueryState::TARGET_FIELD		   || queryState.load() == QueryState::VOLTAGE_LIMIT)
	{
		#ifdef DEBUG
		if (queryState.load() == QueryState::SUPPLY_MIN_VOLTAGE)
			qDebug() << "SUPP:VOLT:MIN? Reply: " << reply;
		else if (queryState.load() == QueryState::SUPPLY_MAX_VOLTAGE)
			qDebug() << "SUPP:VOLT:MAX? Reply: " << reply;
		else if (queryState.load() == QueryState::SUPPLY_MIN_CURRENT)
			qDebug() << "SUPP:CURR:MIN? Reply: " << reply;
		else if (queryState.load() == QueryState::SUPPLY_MAX_CURRENT)
			qDebug() << "SUPP:CURR:MIN? Reply: " << reply;
		else if (queryState.load() == QueryState::STABILITY_SETTING)
			qDebug() << "STAB? Reply: " << reply;
		else if (queryState.load() == QueryState::COIL_CONSTANT)
			qDebug() << "COIL? Reply: " << reply;
		else if (queryState.load() == QueryState::INDUCTANCE)
			qDebug() << "IND? Reply: " << reply;
		else if (queryState.load() == QueryState::SWITCH_CURRENT)
			qDebug() << "PS:CURR? Reply: " << reply;
		else if (queryState.load() == QueryState::PS_RAMP_RATE)
			qDebug() << "PS:PSRR? Reply: " << reply;
		else if (queryState.load() == QueryState::SWITCH_COOLING_GAIN)
			qDebug() << "PS:CGAIN? Reply: " << reply;
		else if (queryState.load() == QueryState::CURRENT_LIMIT)
			qDebug() << "CURR:LIM? Reply: " << reply;
		else if (queryState.load() == QueryState::IC_SLOPE)
			qDebug() << "OPL:ICSLOPE? Reply: " << reply;
		else if (queryState.load() == QueryState::IC_OFFSET)
			qDebug() << "OPL:ICOFFSET? Reply: " << reply;
		else if (queryState.load() == QueryState::TSCALE)
			qDebug() << "OPL:TSCALE? Reply: " << reply;
		else if (queryState.load() == QueryState::IC_OFFSET)
			qDebug() << "OPL:TOFFSET? Reply: " << reply;
		else if (queryState.load() == QueryState::TMAX)
			qDebug() << "OPL:TMAX? Reply: " << reply;
		else if (queryState.load() == QueryState::SENSE_INDUCTANCE)
			qDebug() << "IND:SENSE? Reply: " << reply;
		else if (queryState.load() == QueryState::TARGET_CURRENT)
			qDebug() << "CURR:TARG? Reply: " << reply;
		else if (queryState.load() == QueryState::TARGET_FIELD)
			qDebug() << "FIELD:TARG? Reply: " << reply;
		else if (queryState.load() == QueryState::VOLTAGE_LIMIT)
			qDebug() << "VOLT:LIM? Reply: " << reply;
		#endif

		// read return data
		bool ok;
		double temp = reply.toDouble(&ok);

		if (ok)	// if converted to a double
		{
			if (queryState.load() == QueryState::SUPPLY_MIN_VOLTAGE)
				model430->minSupplyVoltage = temp;
			else if (queryState.load() == QueryState::SUPPLY_MAX_VOLTAGE)
				model430->maxSupplyVoltage = temp;
			else if (queryState.load() == QueryState::SUPPLY_MIN_CURRENT)
				model430->minSupplyCurrent = temp;
			else if (queryState.load() == QueryState::SUPPLY_MAX_CURRENT)
				model430->maxSupplyCurrent = temp;
			else if (queryState.load() == QueryState::STABILITY_SETTING)
				model430->stabilitySetting = temp;
			else if (queryState.load() == QueryState::COIL_CONSTANT)
				model430->coilConstant = temp;
			else if (queryState.load() == QueryState::INDUCTANCE)
				model430->inductance = temp;
			else if (queryState.load() == QueryState::SWITCH_CURRENT)
				model430->switchCurrent = temp;
			else if (queryState.load() == QueryState::PS_RAMP_RATE)
				model430->cooledSwitchRampRate = temp;
			else if (queryState.load() == QueryState::SWITCH_COOLING_GAIN)
				model430->switchCoolingGain = temp;
			else if (queryState.load() == QueryState::CURRENT_LIMIT)
				model430->currentLimit = temp;
			else if (queryState.load() == QueryState::IC_SLOPE)
				model430->IcSlope = temp;
			else if (queryState.load() == QueryState::IC_OFFSET)
				model430->IcOffset = temp;
			else if (queryState.load() == QueryState::TSCALE)
				model430->Tscale = temp;
			else if (queryState.load() == QueryState::IC_OFFSET)
				model430->Toffset = temp;
			else if (queryState.load() == QueryState::TMAX)
				model430->Tmax = temp;
			else if (queryState.load() == QueryState::SENSE_INDUCTANCE)
				model430->inductance = temp;
			else if (queryState.load() == QueryState::TARGET_CURRENT)
				model430->targetCurrent = temp;
			else if (queryState.load() == QueryState::TARGET_FIELD)
				model430->targetField = temp;
			else if (queryState.load() == QueryState::VOLTAGE_LIMIT)
				model430->voltageLimit = temp;
		}

		queryState.store(QueryState::IDLE_STATE);
	}

	// two doubles returned queries
	else if (queryState.load() == QueryState::RAMP_RATE_CURRENT || queryState.load() == QueryState::RAMP_RATE_FIELD ||
			 queryState.load() == QueryState::RAMPDOWN_CURRENT  || queryState.load() == QueryState::RAMPDOWN_FIELD)
	{
		#ifdef DEBUG
		if (queryState.load() == QueryState::RAMP_RATE_CURRENT)
			qDebug() << "RAMP:RATE:CURR:" + QString::number(rampSegment) + "? Reply: " << reply;
		else if (queryState.load() == QueryState::RAMP_RATE_FIELD)
			qDebug() << "RAMP:RATE:FIELD:" + QString::number(rampSegment) + "? Reply: " << reply;
		else if (queryState.load() == QueryState::RAMPDOWN_CURRENT)
			qDebug() << "RAMPD:RATE:CURR:" + QString::number(rampSegment) + "? Reply: " << reply;
		else if (queryState.load() == QueryState::RAMPDOWN_FIELD)
			qDebug() << "RAMPD:RATE:FIELD:" + QString::number(rampSegment) + "? Reply: " << reply;
		#endif

		// read return data
		// split at the , delimiters
		QStringList strList = reply.split(",");

		for (int i = 0; i < strList.size(); i++)
		{
			bool ok;
			double temp = strList.at(i).toDouble(&ok);

			if (ok)	// if successfully converted to a double
			{
				if (i == 0)	// ramp rate value
				{
					if (queryState.load() == QueryState::RAMP_RATE_CURRENT)
						model430->currentRampRates[rampSegment - 1] = temp;
					else if (queryState.load() == QueryState::RAMP_RATE_FIELD)
						model430->fieldRampRates[rampSegment - 1] = temp;
					else if (queryState.load() == QueryState::RAMPDOWN_CURRENT)
						model430->currentRampdownRates[rampSegment - 1] = temp;
					else if (queryState.load() == QueryState::RAMPDOWN_FIELD)
						model430->fieldRampdownRates[rampSegment - 1] = temp;
				}
				else if (i == 1) // ramp limit value
				{
					if (queryState.load() == QueryState::RAMP_RATE_CURRENT)
						model430->currentRampLimits[rampSegment - 1] = temp;
					else if (queryState.load() == QueryState::RAMP_RATE_FIELD)
						model430->fieldRampLimits[rampSegment - 1] = temp;
					else if (queryState.load() == QueryState::RAMPDOWN_CURRENT)
						model430->currentRampdownLimits[rampSegment - 1] = temp;
					else if (queryState.load() == QueryState::RAMPDOWN_FIELD)
						model430->fieldRampdownLimits[rampSegment - 1] = temp;
				}
			}
		}

		queryState.store(QueryState::IDLE_STATE);
	}

	else if (queryState.load() == QueryState::SAMPLE_CURRENT)
	{
		#ifdef DEBUG
		qDebug() << "CURRENT: " << reply;
		#endif

		// read return data
		magnetCurrent = reply.toDouble();

		queryState.store(QueryState::IDLE_STATE);
	}

	else if (queryState.load() == QueryState::SAMPLE_VOLTAGE)
	{
		#ifdef DEBUG
		qDebug() << "VOLTAGE: " << reply;
		#endif

		// read return data
		magnetVoltage = reply.toDouble();

		queryState.store(QueryState::IDLE_STATE);
	}

	else if (queryState.load() == QueryState::SYSTEM_ERROR)
	{
		queryState.store(QueryState::IDLE_STATE);

		#ifdef DEBUG
		qDebug() << "SYST:ERR? Reply: " << reply;
		#endif

		// parse error message code and save
		QStringList strList = reply.split(',');

		// first string in list should be error code
		bool ok;
		int temp = strList.at(0).toInt(&ok);

		if (ok)
			model430->errorCode = -(temp);	// flip sign to positive value

		emit systemErrorMessage(reply, nullptr);
	}

	else if (queryState.load() == QueryState::MSG_UPDATE)	// port 23 only
	{
		// parse MSG_UPDATE messages with display and keypad info
		if (reply.contains("MSG_DISP_UPDATE::"))
		{
			// split at the :: delimiters
			QStringList strList = reply.split("::");
			#ifdef DEBUG
			qDebug() << "MSG_DISP_UPDATE::" + strList[1] + "::" + strList[2];
			#endif

			// expect 9 substrings -- if incomplete, ignore
			if (strList.count() < 9)
				return;

			QString displayStr = strList[1] + "\n" + strList[2];
			bool leds[6] = { false, false, false, false, false, false };

			for (int i = 0; i < 6; i++)
			{
				if (strList[i + 3] == "1")
					leds[i] = true;
			}

			if (model430->state() == State::QUENCH)
			{
				// parse quench current from display string
				QString quenchStr;
				int start = displayStr.indexOf("Quench Detect @");
				start += 15;
				quenchStr = displayStr.mid(start, 12);
				quenchStr.replace("A", " ");

				bool ok;
				double temp = quenchStr.toDouble(&ok);
				if (ok)
					model430->quenchCurrent = temp;
			}

			emit updateFrontPanel(displayStr, leds[0], leds[1], leds[2], leds[3], leds[4]);
		}

		if (reply.contains("MSG_VOLTMETER_UPDATE::"))
		{
			// split at the :: delimiters
			QStringList strList = reply.split("::");
#ifdef DEBUG
			qDebug() << "MSG_VOLTMETER_UPDATE::" + strList[1] + "::" + strList[2];
#endif

			// expect 11 substrings -- if incomplete, ignore
			if (strList.count() < 11)
				return;

			QString displayStr = strList[1] + "  (Showing " + strList[9] + " Bar Graph)  " + "\n" + strList[2] + "      " + strList[9] + " = " + strList[10] + "       ";
			bool leds[6] = { false, false, false, false, false, false };

			for (int i = 0; i < 6; i++)
			{
				if (strList[i + 3] == "1")
					leds[i] = true;
			}

			emit updateFrontPanel(displayStr, leds[0], leds[1], leds[2], leds[3], leds[4]);
		}

		if (reply.contains("FIELD_UNITS_CHANGED"))
		{
			#ifdef DEBUG
			qDebug() << "MSG_FIELD_UNITS_CHANGED";
			#endif

			// field units change notification
			emit fieldUnitsChanged();
		}

		if (reply.contains("BEEP"))
		{
			#ifdef DEBUG
			qDebug() << "MSG_BEEP";
			#endif

			// an error beep occurred, set flag for possible SYST:ERR? status display
			emit systemError();
		}

		// the following assist in syncing Magnet-DAQ panels with remote changes by operator
		if (reply.contains("SYNC:SUPPLY"))
		{
			#ifdef DEBUG
			qDebug() << "MSG_SYNC:SUPPLY";
			#endif
			magnetdaqParent->remoteConfigurationChanged(SUPPLY_PAGE);
		}
		else if (reply.contains("SYNC:LOAD"))
		{
			#ifdef DEBUG
			qDebug() << "MSG_SYNC:LOAD";
			#endif
			magnetdaqParent->remoteConfigurationChanged(LOAD_PAGE);
		}
		else if (reply.contains("SYNC:SWITCH"))
		{
			#ifdef DEBUG
			qDebug() << "MSG_SYNC:SWITCH";
			#endif
			magnetdaqParent->remoteConfigurationChanged(SWITCH_PAGE);
		}
		else if (reply.contains("SYNC:PROT"))
		{
			#ifdef DEBUG
			qDebug() << "MSG_SYNC:PROT";
			#endif
			magnetdaqParent->remoteConfigurationChanged(PROTECTION_PAGE);
		}
		else if (reply.contains("SYNC:RAMP"))
		{
			#ifdef DEBUG
			qDebug() << "MSG_SYNC:RAMP";
			#endif
			magnetdaqParent->remoteConfigurationChanged(RAMP_PAGE);
		}

		if (reply.contains("EXT_RAMPDOWN_START"))
		{
			#ifdef DEBUG
			qDebug() << "MSG_EXT_RAMPDOWN_START";
			#endif

			// external rampdown start notification
			emit startExternalRampdown();
		}

		if (reply.contains("EXT_RAMPDOWN_END"))
		{
			#ifdef DEBUG
			qDebug() << "MSG_EXT_RAMPDOWN_END";
			#endif

			// external rampdown end notification
			emit endExternalRampdown();
		}
	}

	else if (queryState.load() == QueryState::STATUS_BYTE)
	{
		#ifdef DEBUG
		qDebug() << "*STB? Reply: " << reply;
		#endif

		bool ok;
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
	   unsigned char temp = (unsigned char)(reply.toInt(&ok));
#else
		byte temp = (byte)(reply.toInt(&ok));
#endif

		if (ok)
			model430->statusByte = temp;

		queryState.store(QueryState::IDLE_STATE);
	}

	else if (queryState.load() == QueryState::RAMPDOWN_FILE)
	{
		replyBuffer.append(reply);

		// check for contiguous <CR><LF> for transmission finished
		if (replyBuffer.contains("\r\n\r\n"))
		{
			queryState.store(QueryState::IDLE_STATE);
			model430->setRampdownFile(replyBuffer);
		}
	}

	else if (queryState.load() == QueryState::QUENCH_FILE)
	{
		replyBuffer.append(reply);

		// check for contiguous <CR><LF> for transmission finished
		if (replyBuffer.contains("\r\n\r\n"))
		{
			queryState.store(QueryState::IDLE_STATE);
			model430->setQuenchFile(replyBuffer);
		}
	}

	else if (queryState.load() == QueryState::SETTINGS)
	{
		replyBuffer.append(reply);

		// check for contiguous <CR><LF> for transmission finished
		if (replyBuffer.contains("\r\n\r\n"))
		{
			queryState.store(QueryState::IDLE_STATE);
			model430->setSettings(replyBuffer);
		}
	}

	else if (queryState.load() == QueryState::IPNAME)
	{
		qDebug() << "IPNAME? reply: " << reply;

		reply.truncate(reply.length() - 2);	// remove terminators
		model430->setIpName(reply);
		queryState.store(QueryState::IDLE_STATE);
	}
}

//---------------------------------------------------------------------------
void Socket::getNextDataPoint()
{
	if (unitConnected)
	{
		if (queryState.load() == QueryState::IDLE_STATE)
		{
			qint64 currentTime = QDateTime::currentMSecsSinceEpoch();

			if (magnetdaqParent->supports_AMITRG())	// firmware 2.64/3.14 or later supports private trigger
			{
				queryState.store(QueryState::AMI_TRG_SAMPLE);
				socket->write("*AMITRG\r\n");
			}
			else
			{
				queryState.store(QueryState::TRG_SAMPLE);
				socket->write("*TRG\r\n");
			}

			socket->waitForReadyRead(500);

			// NOTE: last three values not received in firmware prior to 2.64/3.14
			emit nextDataPoint(currentTime, magnetField, magnetCurrent, magnetVoltage, supplyCurrent, supplyVoltage, refCurrent, state, heater);

			queryState.store(QueryState::IDLE_STATE);
		}
	}
}

//---------------------------------------------------------------------------
void Socket::commandTimerTimeout(void)
{
	if (unitConnected)
	{
		if (commandQueue.isEmpty())
		{
			commandTimer.stop();
			commandTimer.setInterval(0);
		}
		else
		{
			while (queryState != QueryState::IDLE_STATE && queryState != QueryState::MSG_UPDATE)
			{
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
				usleep(50000);
#else
				Sleep(50);
#endif
			}

			QString cmd = commandQueue.dequeue();
			socket->write(cmd.toLocal8Bit().data(), cmd.size());

			#ifdef DEBUG
			qDebug() << "CMD: " << cmd;
			#endif

			if (magnetdaqParent->isARM())	// dual core ARM -- go faster
				commandTimer.setInterval(100);
			else
				commandTimer.setInterval(250);
		}
	}
}

//---------------------------------------------------------------------------
void Socket::sendCommand(QString aStr)
{
	if (unitConnected)
	{
		commandQueue.enqueue(aStr);
		commandTimer.start();
	}
}

//---------------------------------------------------------------------------
void Socket::sendBlockingCommand(QString aStr)
{
	if (unitConnected)
	{
		while (queryState != QueryState::IDLE_STATE)
		{
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
				usleep(50000);
#else
			Sleep(50);
#endif
		}

		// according to documentation, this write blocks until all data is written ??
		socket->write(aStr.toLocal8Bit().data(), aStr.size());

		#ifdef DEBUG
		qDebug() << "CMD: " << aStr;
		#endif
	}
}

//---------------------------------------------------------------------------
void Socket::sendQuery(QString queryStr, QueryState aState)
{
	if (unitConnected)
	{
		QElapsedTimer timeout;
		timeout.restart();

		while (queryState.load() != QueryState::IDLE_STATE)
		{
			if (timeout.elapsed() > TIMEOUT)
			{
				emit systemErrorMessage("Query send timeout", queryStr);
				return;
			}
		}

		queryState.store(aState);
		socket->write(queryStr.toLocal8Bit());
		socket->waitForReadyRead(1000);

		timeout.restart();
		while (queryState.load() == aState)
		{
			socket->waitForReadyRead(500);
			if (timeout.elapsed() > TIMEOUT)
			{
				queryState.store(QueryState::IDLE_STATE);
				emit systemErrorMessage("Query reply timeout", queryStr);
				break;
			}
		}
	}
}

//---------------------------------------------------------------------------
// This is termed an "extended query" because it may return a rather large
// file (such as troubleshooting.dat) and requires multiple reads to obtain
// the full reply. The reply end is marked with two contiguous <CR><LF> pairs.
// Incoming data accumulates in the replyBuffer.
// Also used for inductance sense and switch current detection
void Socket::sendExtendedQuery(QString queryStr, QueryState aState, int timelimit /*seconds*/)
{
	if (unitConnected)
	{
		QElapsedTimer timeout;
		timeout.restart();

		while (queryState != QueryState::IDLE_STATE)
		{
			if (timeout.elapsed() > TIMEOUT)
			{
				emit systemErrorMessage("Query send timeout", queryStr);
				return;
			}
		}

		replyBuffer.clear();
		queryState.store(aState);
		socket->write(queryStr.toLocal8Bit());
		socket->waitForReadyRead(1000);

		timeout.restart();
		while (queryState.load() == aState)
		{
			socket->waitForReadyRead(500);

			if (timeout.elapsed() > (timelimit * 1000))	// longer timeout because we expect a long reply
			{
				queryState.store(QueryState::IDLE_STATE);
				emit systemErrorMessage("Query reply timeout", queryStr);
				break;
			}
		}
	}
}

//---------------------------------------------------------------------------
void Socket::sendRampQuery(QString queryStr, QueryState aState, int segment)
{
	if (unitConnected)
	{
		QElapsedTimer timeout;
		timeout.restart();

		while (queryState != QueryState::IDLE_STATE)
		{
			if (timeout.elapsed() > TIMEOUT)
			{
				emit systemErrorMessage("Query send timeout", queryStr);
				return;
			}
		}

		rampSegment = segment;
		queryState.store(aState);
		socket->write(queryStr.toLocal8Bit());
		socket->waitForReadyRead(1000);

		timeout.restart();
		while (queryState.load() == aState)
		{
			socket->waitForReadyRead(500);
			if (timeout.elapsed() > TIMEOUT)
			{
				queryState.store(QueryState::IDLE_STATE);
				emit systemErrorMessage("Query reply timeout", queryStr);
				break;
			}
		}
	}
}


//---------------------------------------------------------------------------
void Socket::getFirmwareVersion(void)
{
	if (unitConnected)
	{
		QElapsedTimer timeout;
		timeout.restart();

		while (queryState != QueryState::IDLE_STATE)
		{
			if (timeout.elapsed() > TIMEOUT)
			{
				emit systemErrorMessage("*IDN? send timeout", "");
				return;
			}
		}

		queryState.store(QueryState::FIRMWARE_VERSION);
		socket->write("*IDN?\r\n");
		socket->waitForReadyRead(1000);

		timeout.restart();
		while (queryState.load() == QueryState::FIRMWARE_VERSION)
		{
			socket->waitForReadyRead(500);
			if (timeout.elapsed() > TIMEOUT)
			{
				queryState.store(QueryState::IDLE_STATE);
				emit systemErrorMessage("*IDN? reply timeout", "");
				break;
			}
		}
	}
}

//---------------------------------------------------------------------------
void Socket::getMode(void)
{
	if (unitConnected)
	{
		QElapsedTimer timeout;
		timeout.restart();

		while (queryState != QueryState::IDLE_STATE)
		{
			if (timeout.elapsed() > TIMEOUT)
			{
				emit systemErrorMessage("MODE? send timeout", "");
				return;
			}
		}

		queryState.store(QueryState::MODE);
		socket->write("MODE?\r\n");
		socket->waitForReadyRead(1000);

		timeout.restart();
		while (queryState.load() == QueryState::MODE)
		{
			socket->waitForReadyRead(500);
			if (timeout.elapsed() > TIMEOUT)
			{
				queryState.store(QueryState::IDLE_STATE);
				emit systemErrorMessage("MODE? reply timeout", "");
				break;
			}
		}
	}
}

//---------------------------------------------------------------------------
void Socket::getState(void)
{
	if (unitConnected)
	{
		QElapsedTimer timeout;
		timeout.restart();

		while (queryState != QueryState::IDLE_STATE)
		{
			if (timeout.elapsed() > TIMEOUT)
			{
				emit systemErrorMessage("STATE? send timeout", "");
				return;
			}
		}

		queryState.store(QueryState::STATE);
		socket->write("STATE?\r\n");
		socket->waitForReadyRead(1000);

		timeout.restart();
		while (queryState.load() == QueryState::STATE)
		{
			socket->waitForReadyRead(500);
			if (timeout.elapsed() > TIMEOUT)
			{
				queryState.store(QueryState::IDLE_STATE);
				emit systemErrorMessage("STATE? reply timeout", "");
				break;
			}
		}
	}
}

//---------------------------------------------------------------------------
void Socket::getStatusByte(void)
{
	if (unitConnected)
	{
		QElapsedTimer timeout;
		timeout.restart();

		while (queryState != QueryState::IDLE_STATE)
		{
			if (timeout.elapsed() > TIMEOUT)
			{
				emit systemErrorMessage("*STB? send timeout", "");
				return;
			}
		}

		queryState.store(QueryState::STATUS_BYTE);
		socket->write("*STB?\r\n");
		socket->waitForReadyRead(1000);

		timeout.restart();
		while (queryState.load() == QueryState::STATUS_BYTE)
		{
			socket->waitForReadyRead(500);
			if (timeout.elapsed() > TIMEOUT)
			{
				queryState.store(QueryState::IDLE_STATE);
				emit systemErrorMessage("*STB? reply timeout", "");
				break;
			}
		}
	}
}

//---------------------------------------------------------------------------
void Socket::getIpName(void)
{
	if (unitConnected)
	{
		QElapsedTimer timeout;
		timeout.restart();

		while (queryState != QueryState::IDLE_STATE)
		{
			if (timeout.elapsed() > TIMEOUT)
			{
				emit systemErrorMessage("IPNAME? send timeout", "");
				return;
			}
		}

		queryState.store(QueryState::IPNAME);
		socket->write("IPNAME?\r\n");
		socket->waitForReadyRead(1000);

		timeout.restart();
		while (queryState.load() == QueryState::IPNAME)
		{
			socket->waitForReadyRead(500);
			if (timeout.elapsed() > TIMEOUT)
			{
				queryState.store(QueryState::IDLE_STATE);
				emit systemErrorMessage("IPNAME? reply timeout", "");
				break;
			}
		}
	}
}

//---------------------------------------------------------------------------
void Socket::remoteLockout(bool state)
{
	if (unitConnected)
	{
		if (state)
			sendCommand("SYST:REMOTE\r\n");
		else
			sendCommand("SYST:LOCAL\r\n");
	}
}

//---------------------------------------------------------------------------

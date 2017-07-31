#ifndef MODEL430_H
#define MODEL430_H

// forward declaration to prevent circular reference
class Socket;	

#include <QObject>
#include "property.hpp"

// definitions for summary status register bits
#define EXT_RAMPDOWN_EVENT				0x02
#define QUENCH_EVENT					0x04
#define RS232_MESSAGE_AVAILABLE			0x08
#define STANDARD_EVENT					0x20
#define REQUEST_SERVICE					0x40

enum errorDefs
{
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
	NO_ERROR = 0,
#endif

	// command errors
	UNRECOGNIZED_COMMAND = 101,
	INVALID_ARGUMENT,
	NON_BOOLEAN_ARGUMENT,
	MISSING_PARAMETER,
	OUT_OF_RANGE,
	NO_COIL_CONSTANT,
	NO_SWITCH_INSTALLED,
	NOT_RAMPING,
	ERROR_IN_MODE,
	ZERO_INDUCTANCE,

	// query errors
	UNRECOGNIZED_QUERY = 201,
	QUERY_NO_COIL_CONSTANT,
	QUERY_INTERRUPTED,
	NO_RECORDED_EVENTS,

	// execution errors
	HEATING_SWITCH = 301,
	QUENCH_CONDITION,
	INPUT_BUFFER_OVERFLOW,
	ERROR_COMM_BUFFER_OVERFLOW,
	SUPPLY_IS_OFF,
	RAMPDOWN_IS_ACTIVE,
	COOLED_SWITCH_REQ,

	// device errors
	CAL_CHECKSUM_FAILED = 401,
	RS232_FRAMING_ERROR,
	RS232_PARITY_ERROR,
	RS232_DATA_OVERRUN,
	FILE_NOT_FOUND,
	CORRUPTED_FILE,
	CORRUPTED_STREAM,
	UNKNOWN_FORMAT
};

enum QueryState
{
	// initial state
	WELCOME_STRING = 0,
	FIRMWARE_VERSION,
	MODE,
	IPNAME,

	// setup
	SUPPLY_TYPE,
	SUPPLY_MIN_VOLTAGE,
	SUPPLY_MAX_VOLTAGE,
	SUPPLY_MIN_CURRENT,
	SUPPLY_MAX_CURRENT,
	SUPPLY_VV_INPUT,
	STABILITY_MODE,
	STABILITY_SETTING,
	STABILITY_RESISTOR,
	COIL_CONSTANT,
	CURRENT_LIMIT,
	INDUCTANCE,
	ABSORBER_PRESENT,
	SWITCH_INSTALLED,
	SWITCH_CURRENT,
	SWITCH_TRANSITION,
	SWITCH_HEATED_TIME,
	SWITCH_COOLED_TIME,
	PS_RAMP_RATE,
	SWITCH_COOLING_GAIN,
	QUENCH_ENABLE,
	QUENCH_SENSITIVITY,
	EXT_RAMPDOWN,
	PROTECTION_MODE,
	IC_SLOPE,
	IC_OFFSET,
	TMAX,
	TSCALE,
	TOFFSET,

	// measurements
	SENSE_INDUCTANCE,
	AUTODETECT_SWITCH_CURRENT,

	// ramping configuration
	RAMP_UNITS,		// sec or min?
	FIELD_UNITS,	// kG or T?
	RAMP_SEGMENTS,
	RAMP_RATE_CURRENT,
	RAMP_RATE_FIELD,

	// rampdown configuration
	RAMPDOWN_SEGMENTS,
	RAMPDOWN_CURRENT,
	RAMPDOWN_FIELD,

	// state
	SAMPLE_CURRENT,
	SAMPLE_VOLTAGE,

	// rampdown and quench event files
	RAMPDOWN_COUNT,
	RAMPDOWN_FILE,
	QUENCH_COUNT,
	QUENCH_FILE,

	// display/keypad messaging
	MSG_UPDATE,

	// error and status state
	SYSTEM_ERROR,
	STATUS_BYTE,
	SETTINGS,	/* ASCII dump of all settings */

	// trigger
	TRG_SAMPLE,

	// idle
	IDLE_STATE
};

class Model430 : public QObject {
	Q_OBJECT

public:
	Model430(QObject *parent = Q_NULLPTR);
	~Model430();

	// setter/getter
	void setSocket(Socket *aSocket) { socket = aSocket; }
	void setFirmwareVersion(double value) { firmwareVersion = value; }
	void setFirmwareSuffix(QString str) { firmwareSuffix = str; }
	QString getFirmwareSuffix(void) { return firmwareSuffix; }
	void setRampdownFile(QString str);
	void setQuenchFile(QString str);
	void setSettings(QString str);
	void setIpName(QString str) { ipName = str; }
	QString getIpName(void) { return ipName; }

	// public properties

	Property<double> firmwareVersion;
	Property<int> mode;
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
	Property<unsigned char> statusByte;
#else
	Property<byte> statusByte;
#endif
	Property<int> errorCode;

	// SETUP -> Supply
	Property<int> powerSupplySelection;
	Property<double> minSupplyVoltage;
	Property<double> maxSupplyVoltage;
	Property<double> minSupplyCurrent;
	Property<double> maxSupplyCurrent;
	Property<int> inputVoltageRange;

	// SETUP -> Load
	Property<int> stabilityMode;
	Property<double> stabilitySetting;
	Property<bool> stabilityResistor;
	Property<double> coilConstant;
	Property<double> currentLimit;
	Property<double> inductance;
	Property<bool> absorberPresent;

	// SETUP -> Switch
	Property<bool> switchInstalled;
	Property<bool> stabilizingResistor;
	Property<double> switchCurrent;
	Property<int> switchTransition;
	Property<int> switchHeatedTime;
	Property<int> switchCooledTime;
	Property<double> cooledSwitchRampRate;
	Property<double> switchCoolingGain;

	// SETUP -> Protection
	Property<int> quenchDetection;
	Property<int> quenchSensitivity;
	Property<int> protectionMode;
	Property<double> IcSlope;
	Property<double> IcOffset;
	Property<double> Tmax;
	Property<double> Tscale;
	Property<double> Toffset;
	Property<bool> extRampdownEnabled;

	// RAMP RATE
	Property<int> rampRateUnits;
	Property<int> fieldUnits;
	Property<int> rampRateSegments;
	Property<double> currentRampRates[10];		// 10 rates
	Property<double> currentRampLimits[10];		// 10 limits
	Property<double> fieldRampRates[10];		// 10 rates
	Property<double> fieldRampLimits[10];		// 10 limits

	// RAMPDOWN
	Property<int> rampdownSegments;
	Property<double> currentRampdownRates[10];		// 10 rates
	Property<double> currentRampdownLimits[10];		// 10 limits
	Property<double> fieldRampdownRates[10];		// 10 rates
	Property<double> fieldRampdownLimits[10];		// 10 limits

	// EVENTS
	Property<int> rampdownEventsCount;
	Property<int> quenchEventsCount;

signals:
	void configurationChanged(QueryState aState);
	void systemError(QString errMsg);
	void syncRampPlot(void);
	void syncRampdownPlot(void);
	void syncRampdownEvents(QString str);
	void syncQuenchEvents(QString str);
	void syncTextSettings(QString str);

public slots:
	void sync(void);
	void syncFieldUnits(void);
	void syncSupplySetup(void);
	void syncLoadSetup(void);
	void syncSwitchSetup(void);
	void syncProtectionSetup(void);
	void syncEventCounts(bool isBlocking = false);
	void syncStabilityMode(void);
	void syncStabilitySetting(void);
	void syncInductance(void);
	void syncRampRates(void);
	void syncRampSegmentValues(void);
	void syncRampdownSegmentValues(void);

private:
	Socket *socket;	// communication socket to 430
	QString rampdownFile;
	QString quenchFile;
	QString textSettings;
	QString firmwareSuffix;
	QString ipName;

	void valueChanged(QueryState);
	void modeValueChanged(void) {};
	void fieldUnitsChanged(void);
};

#endif // MODEL430_H
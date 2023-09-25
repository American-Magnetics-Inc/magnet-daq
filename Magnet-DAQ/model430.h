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

// handy constants for units
constexpr auto KG = 0;
constexpr auto TESLA = 1;
constexpr auto AMPS = 2;

// handy constants for Mode (s2 switch states)
constexpr auto NO_FRONT_PANEL = 0x01;		// no keypad/display configuration
constexpr auto USE_OPCONSTS = 0x02;			// use Operational Limits menu and Aux 3 temperature input
constexpr auto SHORT_SAMPLE_MODE = 0x04;	// short-sample mode (voltage controls current)
constexpr auto FLUXGATE_1X_SCALING = 0x08;	// fluxgate ADC input scaling (1x ADC gain instead of 32x for shunts)
constexpr auto FLUXGATE_STATUS = 0x10;		// switch to enable fluxgate status pin check

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

enum class QueryState
{
	// initial state
	WELCOME_STRING = 0,
	FIRMWARE_VERSION,
	MODE,
	IPNAME,
	TARGET_CURRENT,
	TARGET_FIELD,
	VOLTAGE_LIMIT,

	// setup
	CURRENT_RANGE,
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
	SAMPLE_QUENCH_ENABLE,
	SAMPLE_QUENCH_LIMIT,
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
	RAMP_TIMEBASE,	// sec or min?
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
	STATE,

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
	AMI_TRG_SAMPLE, /* custom AMI private trigger */

	// switch state
	SWITCH_HTR_STATE,

	// idle
	IDLE_STATE
};

enum class State
{
	RAMPING = 1,
	HOLDING,
	PAUSED,
	MANUAL_UP,
	MANUAL_DOWN,
	ZEROING,
	QUENCH,
	AT_ZERO,
	SWITCH_HEATING,
	SWITCH_COOLING,
	EXTERNAL_RAMPDOWN
};


class Model430 : public QObject {
	Q_OBJECT

public:
	Model430(QObject *parent = Q_NULLPTR);
	~Model430();

	// setter/getter
	void setSocket(Socket *aSocket) { socket = aSocket; }
	Socket *getSocket(void) { return socket; }
	void setFirmwareVersion(double value) { firmwareVersion = value; }
	void setFirmwareSuffix(QString str) { firmwareSuffix = str; }
	QString getFirmwareSuffix(void) { return firmwareSuffix; }
	void setRampdownFile(QString str);
	void setQuenchFile(QString str);
	void setSettings(QString str);
	void setCurrentData(qint64 time, double magField, double magCurrent, double magVoltage, double supCurrent, double supVoltage, double refCurrent);
	void setIpName(QString str) { ipName = str; }
	QString getIpName(void) { return ipName; }

	// public data and properties
	qint64 timestamp;
	bool switchHeaterState; // is pswitch heater on?
	bool persistentState;	// is magnet in persistent mode?
	bool shortSampleMode;	// is system in short-sample mode?
	double magnetField;
	double magnetCurrent;
	double magnetVoltage;
	double supplyCurrent;
	double supplyVoltage;
	double quenchCurrent;
	double referenceCurrent;

	Property<double> firmwareVersion;
	Property<QString> serialNumber;
	Property<int> mode;	//S2 switch state
	Property<State> state;
	Property<double> targetCurrent;
	Property<double> targetField;
	Property<double> voltageLimit;

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
	Property<unsigned char> statusByte;
#else
	Property<unsigned char> statusByte;
#endif
	Property<int> errorCode;

	// SETUP -> Supply
	Property<int> currentRange;
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
	Property<bool> sampleQuenchDetection;
	Property<int> sampleQuenchLimit;
	Property<int> quenchSensitivity;
	Property<int> protectionMode;
	Property<double> IcSlope;
	Property<double> IcOffset;
	Property<double> Tmax;
	Property<double> Tscale;
	Property<double> Toffset;
	Property<bool> extRampdownEnabled;

	// RAMP RATE
	Property<int> rampRateTimeUnits;
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
	void shortSampleModeChanged(bool isSampleMode);
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
	void syncTargetCurrent(void);
	void syncTargetField(void);
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
	void modeValueChanged(void);
	void fieldUnitsChanged(void);
};

#endif // MODEL430_H
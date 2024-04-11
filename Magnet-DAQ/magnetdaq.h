#ifndef magnetdaq_H
#define magnetdaq_H

#include <QtWidgets/QMainWindow>
#include <QLabel>
#include "ui_magnetdaq.h"
#include "errorhistorydlg.h"

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
#include "qcustomplot.h"
#else
#include <QCustomPlot/qcp.h>
#endif

#include "socket.h"
#include "model430.h"
#include "parser.h"
#include "clickablelabel.h"

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
#include <QtFtp/QtFtp>
#endif

#define USE_QTPRINTER	// remove define to omit QPrinter code

#define N_SAMPLES_MOVING_AVG 60

// some definitions for readability
enum MAIN_TABS
{
	PLOT_TAB = 0,
	RAMP_TAB,
	TABLE_TAB,
	CONFIG_TAB,
	RAMPDOWN_TAB,
	RAMPDOWN_EVENTS_TAB,
	QUENCH_EVENTS_TAB,
	SUPPORT_TAB
};

enum SETUP_TOOLBOX
{
	SUPPLY_PAGE = 0,
	LOAD_PAGE,
	SWITCH_PAGE,
	PROTECTION_PAGE,
	RAMP_PAGE	// hack for remote config changes
};

enum class TableError
{
	NO_TABLE_ERROR = 0,			// no error
	NON_NUMERICAL_ENTRY,		// non-numerical parameter
	EXCEEDS_CURRENT_LIMIT,		// current/field value exceeds magnet limit
	COOLED_SWITCH				// cannot ramp to table target with cooled switch
};

// main plot trace ids
constexpr auto MAGNET_CURRENT_GRAPH = 0;
constexpr auto MAGNET_FIELD_GRAPH = 1;
constexpr auto SUPPLY_CURRENT_GRAPH = 2;
constexpr auto MAGNET_VOLTAGE_GRAPH = 3;
constexpr auto SUPPLY_VOLTAGE_GRAPH = 4;
constexpr auto RAMP_REFERENCE_GRAPH = 5;

//---------------------------------------------------------------------------
class magnetdaq : public QMainWindow
{
	Q_OBJECT

public:
	magnetdaq(QWidget *parent = 0);
	~magnetdaq();
	QString getAxisStr(void) { return axisStr; }
	bool supports_AMITRG(void);
	bool isARM(void);

public slots:
	void configurationChanged(QueryState state);
	void shortSampleModeChanged(bool isSampleMode);
	void remoteConfigurationChanged(int index);

private slots:
	void actionRun(void);
	void actionStop(void);
	void droppedTelnet(void);
	void exit_app(void);
	void actionSetup(void);
	void actionPlot_Settings(void);
	void actionShow_Keypad(void);
	void actionPrint(void);
	void actionShowErrorDialog(void);
	void actionHelp(void);
	void actionAbout(void);
	void actionUpgrade(void);
	void actionToggle_Collapse(bool);
	void setStatusMsg(QString msg);
	void pinErrorString(QString errMsg, bool highlight);
	void showErrorString(QString errMsg, bool highlight);
	void parserErrorString(QString err);
	void errorStatusTimeout(void);
	void ipAddressEdited(QString text);
	void setDeviceWindowTitle(void);
	void chooseLogfile(bool checked);
	void remoteLockoutChanged(bool checked);
	void timeout(void);
	void updateFrontPanel(QString, bool, bool, bool, bool, bool);
	void systemErrorNotification();
	void displaySystemError(QString errMsg, QString errorSourceStr);
	void clearErrorDisplay(void);
	void clearErrorHistory(void);
	void clearMiscDisplay(void);
	void startExternalRampdown(void);
	void endExternalRampdown(void);

	// device list management
	void restoreDeviceList(QSettings *settings);
	void saveDeviceList(void);
	void addOrUpdateDevice(QString ipAddress, QString ipName);
	void ipNameChanged(void);
	void selectedDeviceChanged(void);
	void deleteDeviceClicked(bool checked);

	// slots for printing support
#ifdef USE_QTPRINTER
	void renderPlot(QPrinter *printer);
	void renderRampPlot(QPrinter *printer);
	void renderRampdownPlot(QPrinter *printer);
#endif

	// slots for main plot
	void restorePlotSettings(QSettings *settings);
	void initPlot(void);
	void toggleAutoscrollXCheckBox(bool checked);
	void toggleAutoscrollButton(bool checked);
	void addDataPoint(qint64 time, double magField, double magCurrent, double magVoltage, double supCurrent, double supVoltage, double refCurrent, quint8 state, quint8 heater);
	void writeLogHeader(void);
	void resetAxes(bool checked);
	void timebaseChanged(bool checked);
	void currentAxisSelectionChanged(bool checked);
	void magnetVoltageSelectionChanged(bool checked);
	void supplyCurrentSelectionChanged(bool checked);
	void supplyVoltageSelectionChanged(bool checked);
	void referenceSelectionChanged(bool checked);
	void titleDoubleClick(QMouseEvent* event);
	void axisLabelDoubleClick(QCPAxis* axis, QCPAxis::SelectablePart part);
	void syncPlotLegend();
	void legendDoubleClick(QCPLegend *legend, QCPAbstractLegendItem *item);
	void selectionChanged();
	void mousePress();
	void mouseWheel();
	void contextMenuRequest(QPoint pos);
	void moveLegend();

	// slots for ramp rate plot
	void restoreRampPlotSettings(QSettings *settings);
	void initRampPlot(void);
	void setRampPlotCurrentAxisLabel(void);
	void syncRampPlot(void);
	void resetRampPlotAxes(bool checked);
	void rampPlotTimebaseChanged(bool checked);
	void rampPlotSelectionChanged(void);
	void rampPlotMousePress(void);
	void rampPlotMouseWheel(void);

	// slots for rampdown rate plot
	void restoreRampdownPlotSettings(QSettings *settings);
	void initRampdownPlot(void);
	void setRampdownPlotCurrentAxisLabel(void);
	void syncRampdownPlot(void);
	void resetRampdownPlotAxes(bool checked);
	void rampdownPlotTimebaseChanged(bool checked);
	void rampdownPlotSelectionChanged(void);
	void rampdownPlotMousePress(void);
	void rampdownPlotMouseWheel(void);

	// slots for event tabs
	void refreshRampdownList(void);
	void syncRampdownEvents(QString str);
	void rampdownEventSelectionChanged(int index);
	void refreshQuenchList(void);
	void saveQuenchHistory(void);
	void syncQuenchEvents(QString str);
	void quenchEventSelectionChanged(int index);

	// slots for support tab
	void restoreSupportSettings(QSettings * settings);
	void refreshSupportSettings(void);
	void syncTextSettings(QString str);
	void sendSupportEmailClicked(void);
	void copySettingsToClipboard(void);
	void saveSettingsToFile(void);

	// slots for 430 control
	void persistentSwitchButtonClicked(void);
	void targetSetpointButtonClicked(void);
	void rampPauseButtonClicked(void);
	void keypadClicked(bool);

	// slots for configuration changes
	void mainTabChanged(int index);
	void postErrorRefresh(void);
	void setupToolBoxChanged(int index);
	void menuValueChanged(int index);
	void textValueChanged(void);
	void checkBoxValueChanged(bool checked);
	void sampleQuenchLimitChanged(int value);
	void rampSegmentCountChanged(int value);
	void rampSegmentValueChanged(void);
	void rampdownSegmentCountChanged(int value);
	void rampdownSegmentValueChanged(void);
	void syncDisplay(void);
	void syncSupplyTab(void);
	void syncLoadTab(void);
	void syncSwitchTab(void);
	void switchSettingsEnable();
	void syncProtectionTab(void);
	void syncRampRates(void);
	void syncRampdownRates(void);
	void inductanceSenseButtonClicked(void);
	void switchCurrentDetectionButtonClicked(void);

	// firmware upgrade wizard
	void upgradeWizardPageChanged(int pageId);
	void errorDuringFirmwareUpload(QNetworkReply::NetworkError code);
	void finishedFirmwareUpload(QNetworkReply * reply);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
	void ftpTimeout(void);
	void ftpCommandFinished(int, bool error);
#endif
	void showFirmwareUpgradeWizard(void);
	void wizardFinished(int result);

	// slots for current/field table feature
	void restoreTableSettings(QSettings *settings);
	void actionLoad_Table(void);
	void convertFieldValues(int newUnits, bool forceConversion);
	void setTableHeader(void);
	void syncTableUnits(void);
	void actionSave_Table(void);
	void tableSelectionChanged(void);
	void tableAddRowAbove(void);
	void tableAddRowBelow(void);
	void initNewRow(int newRow);
	void updatePresentTableSelection(int row, bool removed);
	void tableRemoveRow(void);
	void tableClear(void);
	void tableTogglePersistence(void);
	void tableItemChanged(QTableWidgetItem *item);
	void actionGenerate_Excel_Report(void);
	void saveReport(QString reportFileName);
	TableError checkNextTarget(double target, QString label);
	void sendNextTarget(double target);
	int calculateRampingTime(double target, double currentValue);
	void manualCtrlTimerTick(void);
	void markTableSelectionWithOutput(int rowIndex, QString output);
	void markTableSelectionAsPass(int rowIndex);
	void markTableSelectionAsFail(int rowIndex, double quenchCurrent);
	void abortTableTarget(void);
	void goToSelectedTableEntry(void);
	void goToNextTableEntry(void);
	bool goToTableSelection(int vectorIndex, bool makeTarget);
	void autostepRangeChanged(void);
	void calculateAutostepRemainingTime(int startIndex, int endIndex);
	void displayAutostepRemainingTime(void);
	void startAutostep(void);
	void abortAutostep(QString errorString);
	void stopAutostep(void);
	void autostepTimerTick(void);
	void enableTableControls(void);
	void abortManualCtrl(QString errorString);
	void recalculateRemainingTime(void);
	void chooseAutosaveFolder(void);
	void doAutosaveReport(bool forceOutput);
	void browseForAppPath(void);
	void browseForPythonPath(void);
	bool checkExecutionTime(void);
	void executeNowClick(void);
	void executeApp(void);
	void finishedApp(int exitCode, QProcess::ExitStatus exitStatus);
	void appCheckBoxChanged(int state);
	void pythonCheckBoxChanged(int state);

private:
	Ui::magnetdaqClass ui;
	Model430 model430;	// contains the presently-connected 430 settings

	Socket *socket;	// communicates via port 7180 to 430
	Socket *telnet; // communicates via port 23 to 430
	QTimer *plotTimer;
	qint64 startTime;
	int plotCount;
	int errorCode;
	QStack<QString> errorStack;	// the error stack (LIFO)
	errorhistorydlg *errorstackDlg;		// the error history dialog
	QTimer *manualCtrlTimer;
	QString ssSampleVoltageText;

	// command line support
	QString targetIP;	// optional command line ip start
	int port;			// optional command line ip port (for simulations only)
	int tport;			// optional telnet port for display echo (for simulations only)
	bool startHidden;	// option to start hidden
	bool isXAxis;		// axes label options for 3-axis systems
	bool isYAxis;		// ...
	bool isZAxis;		// ...
	QString axisStr;	// label for saving/restoring axes window geometry
	bool parseInput;	// optional stdin message parsing
	Parser *parser;		// stdin parsing support

	// log file support
	QFile *logFile;
	QString lastPath;

	// main plot elements
	QCPTextElement *ssPlotTitle;
	QCPAxis *timeAxis;
	QCPAxis *currentAxis;
	QCPAxis *voltageAxis;
	QToolButton *autoscrollButton;

	// main plot sample rate calculation
	int selectedTrace;
	double sampleTimes[N_SAMPLES_MOVING_AVG];
	int samplePos;
	double meanSampleTime;
	qint64 lastTime;

	// main plot selected trace stat calcs
	double selTraceValues[N_SAMPLES_MOVING_AVG];
	int selTracePos;
	double selTraceAverage;
	double selTraceErrorsSum;
	double selTraceVariance;

	// main plot label text
	QString mainPlotTitle;
	QString mainPlotXTitle;
	QString mainPlotYTitle;
	QString mainPlotYTitleCurrent;
	QString mainPlotYTitleField;
	QString mainPlotY2Title;
	QString mainLegend[5];

	// ramp plot elements
	QCPTextElement *rampPlotTitle;
	QCPAxis *rampCurrentAxis;
	QCPAxis *rampVoltageAxis;

	// rampdown plot elements
	QCPTextElement *rampdownPlotTitle;
	QCPAxis *rampdownCurrentAxis;
	QCPAxis *rampdownVoltageAxis;

	// status bar items
	QLabel *statusConnectState;
	ClickableLabel *statusError;
	QLabel *statusSampleRate;
	QLabel *statusMisc;
	QLabel* statusStats;
	QString lastStatusMiscStyle;
	QString lastStatusMiscString;

	// error status items
	std::atomic<bool> errorStatusIsActive;
	std::atomic<bool> parserErrorStatusIsActive;
	TableError tableError;	// last selected table row had error?

	// ramp segments convenience arrays
	QLabel	  *rampSegLabels[10];
	QLabel	  *rampSegMinLimits[10];
	QLineEdit *rampSegMaxLimits[10];
	QLineEdit *rampSegValues[10];

	// rampdown segments convenience arrays
	QLabel	  *rampdownSegLabels[10];
	QLabel	  *rampdownSegMinLimits[10];
	QLineEdit *rampdownSegMaxLimits[10];
	QLineEdit *rampdownSegValues[10];

	// event elements
	QStringList rampdownSummaries;
	QStringList quenchSummaries;

	void setupRampRateArrays(void);
	void setupRampdownArrays(void);
	void clearStats(void);
	void avgSelectedTrace(double newValue);
	void avgSampleTimes(double newValue);
	void setTimeAxisLabel(void);
	QString getCurrentAxisLabel(void);
	void setCurrentAxisLabel(void);
	void setVoltageAxisLabel(void);

	// firmware upgrade wizard
	QWizard *upgradeWizard;
	QWizardPage *finishedPage;
	QLabel *finishedPageLabel;
	bool checkFirmwareVersion(void);
	bool checkAvailableFirmware(void);
	QString formatFirmwareUpgradeStr(void);
	QString formatFirmwareUpgradeMsg(void);
	void initializeUpgradeWizard(void);
	QWizardPage * createIntroPage(void);
	QWizardPage * createUploadPage(void);
	QWizardPage * createConclusionPage(void);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
	QFtp *ftp;	// needed for Qt6+
	bool ftpTimeoutOccurred;
#endif

	// current/field table
	QString tableFileName;
	QString lastTableLoadPath;
	QString saveTableFileName;
	QString lastTableSavePath;
	QString lastReportPath;
	QString reportFileName;
	int presentTableValue;
	int lastTableValue;		// last known good table selection
	int remainingTime;		// time remaining for arrival at target
	int tableUnits;

	// table autostepping
	QTimer *autostepTimer;
	QProcess* process;
	int elapsedTimerTicks;
	int autostepStartIndex;
	int autostepEndIndex;
	int autostepRemainingTime;
	bool haveAutosavedReport;
	TableError autostepError;
	QString lastAppFilePath;
	QString lastPythonPath;
	QString autosaveFolder;

	// support tab elements
	QString lastSettingsSavePath;
	QString saveSettingsFileName;
};

#endif // magnetdaq_H

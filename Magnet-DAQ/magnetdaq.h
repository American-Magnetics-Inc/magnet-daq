#ifndef magnetdaq_H
#define magnetdaq_H

#include <QtWidgets/QMainWindow>
#include <QLabel>
#include "ui_magnetdaq.h"
#include "errorhistorydlg.h"
#include "qcustomplot.h"
#include "socket.h"
#include "model430.h"
#include "parser.h"
#include "clickablelabel.h"

#define N_SAMPLES_MOVING_AVG 60

// some definitions for readability
enum MAIN_TABS
{
	PLOT_TAB = 0,
	RAMP_TAB,
	CONFIG_TAB,
	RAMPDOWN_TAB,
	RAMPDOWN_EVENTS_TAB,
	QUENCH_EVENTS_TAB,
	SUPPORT_TAB
};

class magnetdaq : public QMainWindow
{
	Q_OBJECT

public:
	magnetdaq(QWidget *parent = 0);
	~magnetdaq();
	QString getAxisStr(void) { return axisStr; }

public slots:
	void configurationChanged(QueryState state);

private slots:
	void actionRun(void);
	void actionStop(void);
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

	// slots for main plot
	void restorePlotSettings(QSettings *settings);
	void initPlot(void);
	void toggleAutoscrollXCheckBox(bool checked);
	void toggleAutoscrollButton(bool checked);
	void addDataPoint(qint64 time, double magField, double magCurrent, double magVoltage, double supCurrent, double supVoltage);
	void writeLogHeader(void);
	void renderPlot(QPrinter *printer);
	void resetAxes(bool checked);
	void timebaseChanged(bool checked);
	void currentAxisSelectionChanged(bool checked);
	void magnetVoltageSelectionChanged(bool checked);
	void supplyCurrentSelectionChanged(bool checked);
	void supplyVoltageSelectionChanged(bool checked);
	void titleDoubleClick(QMouseEvent* event);
	void axisLabelDoubleClick(QCPAxis* axis, QCPAxis::SelectablePart part);
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
	void renderRampPlot(QPrinter *printer);
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
	void renderRampdownPlot(QPrinter *printer);
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
	void syncQuenchEvents(QString str);
	void quenchEventSelectionChanged(int index);

	// slots for support tab
	void restoreSupportSettings(QSettings * settings);
	void refreshSupportSettings(void);
	void syncTextSettings(QString str);
	void sendSupportEmailClicked(void);
	void copySettingsToClipboard(void);

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
	void showFirmwareUpgradeWizard(void);
	void wizardFinished(int result);

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
	bool firstStatsPass;
	double sampleTimes[N_SAMPLES_MOVING_AVG];
	int samplePos;
	double meanSampleTime;
	qint64 lastTime;

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
	bool parserErrorStatusIsActive;
	QString lastStatusMiscStyle;
	QString lastStatusMiscString;

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
};

#endif // magnetdaq_H

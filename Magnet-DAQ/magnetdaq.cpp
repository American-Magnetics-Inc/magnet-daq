#include "stdafx.h"
#include "magnetdaq.h"
#include "qled.h"
#include <QtConcurrent>
#include "aboutdialog.h"

//---------------------------------------------------------------------------
// Local constants
//---------------------------------------------------------------------------
const int MIN_WINDOW_HEIGHT_EXPANDED = 730;

#if defined(Q_OS_MACOS)
const int MIN_WINDOW_HEIGHT_COLLAPSED = 220;
#else
const int MIN_WINDOW_HEIGHT_COLLAPSED = 210;
#endif


//---------------------------------------------------------------------------
// Constructor
//---------------------------------------------------------------------------
magnetdaq::magnetdaq(QWidget *parent)
	: QMainWindow(parent)
{
	// add application-specific fonts
	QFontDatabase::addApplicationFont(":/magnetdaq/Resources/BFN____.otf");	// family: Bubbledot Fine Negative
	QFontDatabase::addApplicationFont(":/magnetdaq/Resources/BFP____.otf");	// family: Bubbledot Fine Positive
#if !defined(Q_OS_MACOS)
	QFontDatabase::addApplicationFont(":/magnetdaq/Resources/WINGDNG3.TTF");	// family: Wingdings 3
#endif
	qRegisterMetaType<QueryState>("QueryState");

	ui.setupUi(this);

	// default is hidden docked widgets
	ui.setupDockWidget->hide();
	ui.plotDockWidget->hide();
	ui.keypadDockWidget->hide();

	this->setWindowTitle(QCoreApplication::applicationName() + " - v" + QCoreApplication::applicationVersion() + "   (AMI Model 430 Remote Control)");
	ui.voltageLimitLabel->setText("<html>Voltage Limit (&plusmn;V) :</html>");
	ui.currentLimitLabel->setText("<html>Current Limit (&plusmn;A) :</html>");
	ui.droppedConnectionLabel->setText("Emulated display/keypad not presently connected to a remote device.");

#if defined(Q_OS_LINUX)
	QGuiApplication::setFont(QFont("Ubuntu", 9));
	QFont::insertSubstitution("Segoe UI", "Ubuntu");
	QFont::insertSubstitution("Lucida Console", "Liberation Mono");
	ui.quenchListWidget->setFont(QFont("Liberation Mono", 10));
	ui.quenchEventTextEdit->setFont(QFont("Liberation Mono", 10));
	ui.rampdownListWidget->setFont(QFont("Liberation Mono", 10));
	ui.rampdownEventTextEdit->setFont(QFont("Liberation Mono", 10));
	ui.settingsTextEdit->setFont(QFont("Liberation Mono", 10));
	this->setStyleSheet("QMainWindow::separator {background: rgb(200, 200, 200); width: 1px; height: 1px;} QTabBar {font-size: 8pt};");
	ui.mainTabWidget->tabBar()->setStyleSheet("font-size: 9pt");

	// remove unsightly frame outlines
	ui.keypadFrame->setStyleSheet("border: 0;");
	ui.displayFrame->setStyleSheet("border: 0;");

#elif defined(Q_OS_MACOS)
	// Mac base font scaling is different than Linux and Windows
	// use ~ 96/72 = 1.333 factor for upscaling every font size
	// we must do this everywhere a font size is specified
	QGuiApplication::setFont(QFont(".SF NS Text", 13));
	QFont::insertSubstitution("Segoe UI", ".SF NS Text");
	QFont::insertSubstitution("Lucida Console", "Monaco");

	// these font sizes seem to be somehow "frozen" in the ui file
	// replace them here in code so we don't have to have a Mac-specific ui file
	this->setFont(QFont(".SF NS Text", 13));
	ui.setupDockWidget->setFont(QFont(".SF NS Text", 13));
	ui.plotDockWidget->setFont(QFont(".SF NS Text", 13));
	ui.keypadDockWidget->setFont(QFont(".SF NS Text", 13));
	ui.centralWidget->setFont(QFont(".SF NS Text", 13));
	ui.devicesTableWidget->setFont(QFont(".SF NS Text", 13));
	ui.plotSelectionsLayout->setSpacing(8);
	ui.quenchListWidget->setFont(QFont("Monaco", 13));
	ui.quenchEventTextEdit->setFont(QFont("Monaco", 13));
	ui.settingsTextEdit->setFont(QFont("Monaco", 13));
	ui.rampdownListWidget->setFont(QFont("Monaco", 13));
	ui.rampdownEventTextEdit->setFont(QFont("Monaco", 13));
	ui.fieldAtTargetLEDLabel->setFont(QFont(".SF NS Text", 13));
	ui.persistentLEDLabel->setFont(QFont(".SF NS Text", 13));
	ui.energizedLEDLabel->setFont(QFont(".SF NS Text", 13));
	ui.quenchLabel->setFont(QFont(".SF NS Text", 13));
	ui.persistSwitchControlButton->setFont(QFont(".SF NS Text", 13));
	ui.targetFieldSetpointButton->setFont(QFont(".SF NS Text", 13));
	ui.rampPauseButton->setFont(QFont(".SF NS Text", 13));
	ui.serialNumLabel->setFont(QFont(".SF NS Text", 11));
	ui.serialNumEdit->setFont(QFont(".SF NS Text", 11));

	// remove unsightly frame outlines
	ui.keypadFrame->setStyleSheet("border: 0;");
	ui.displayFrame->setStyleSheet("border: 0;");

#else	// Windows assumed
	QGuiApplication::setFont(QFont("Segoe UI", 9));
	this->setStyleSheet("QMainWindow::separator {background: rgb(200, 200, 200); width: 1px; height: 1px;} QTabBar {font-family: Arial; font-weight: bold; font-size: 8pt};");
	ui.mainTabWidget->tabBar()->setStyleSheet("font-family: \"Segoe UI\"; font-size: 9pt");
#endif

	/************************************************************
	The command line parser allows several options:

	-h	Hide the Magnet-DAQ interface on launch.
	-x	Label the Magnet-DAQ window as "X Axis" and superimpose
		an X on the app icon.
	-y	Label the Magnet-DAQ window as "Y Axis" and superimpose
		a Y on the app icon.
	-z	Label the Magnet-DAQ window as "Z Axis" and superimpose
		a Z on the app icon.
	-p	Start the stdin/stdout parser function (for QProcess use).

	-a address		Start and auto-connect to IP address.
	--port xxxx		Connect to specified port (for simulation use only)
	--telnet xxxx	Echo display to specified port (for simulation use only)
	************************************************************/

	// init states
	parserErrorStatusIsActive = false;
	errorStatusIsActive = false;
	parser = NULL;	// stdin parser
	isXAxis = false;
	isYAxis = false;
	isZAxis = false;
	startHidden = false;
	parseInput = false;
	port = 7180;
	tport = 23;

	// define the optional command line argument(s)
	QCommandLineParser cmdLineParse;

	// An ip address option with a value (-a, --address)
	QCommandLineOption targetIPOption(QStringList() << "a" << "address",
		QCoreApplication::translate("main", "Automatically connect to <IP address>."),
		QCoreApplication::translate("main", "address"));
	cmdLineParse.addOption(targetIPOption);

	// A specific port to connect (--port) (for simulation use only)
	QCommandLineOption portOption("port",
		QCoreApplication::translate("main", "Connect to specified <ip-port>."),
		QCoreApplication::translate("main", "ip-port"));
	cmdLineParse.addOption(portOption);

	// A specific telnet port to connect (--telnet) (for simulation use only)
	QCommandLineOption telnetOption("telnet",
		QCoreApplication::translate("main", "Echo display to specified <port>."),
		QCoreApplication::translate("main", "ip-port"));
	cmdLineParse.addOption(telnetOption);

	// A boolean option (-x)
	QCommandLineOption xAxisOption("x", QCoreApplication::translate("main", "Label as X axis."));
	cmdLineParse.addOption(xAxisOption);

	// A boolean option (-y)
	QCommandLineOption yAxisOption("y", QCoreApplication::translate("main", "Label as Y axis."));
	cmdLineParse.addOption(yAxisOption);

	// A boolean option (-z)
	QCommandLineOption zAxisOption("z", QCoreApplication::translate("main", "Label as Z axis."));
	cmdLineParse.addOption(zAxisOption);

	// A boolean option with multiple names (-h, --hidden)
	QCommandLineOption hiddenOption(QStringList() << "h" << "hidden",
		QCoreApplication::translate("main", "Start in minimized (hidden) mode."));
	cmdLineParse.addOption(hiddenOption);

	// A boolean option with multiple names (-p, --parser)
	QCommandLineOption parsingOption(QStringList() << "p" << "parser",
		QCoreApplication::translate("main", "Enable stdin parsing for interprocess communication."));
	cmdLineParse.addOption(parsingOption);

	// Process the actual command line arguments given by the user
	cmdLineParse.process(*(QCoreApplication::instance()));

	const QStringList args = cmdLineParse.positionalArguments();
	// source is args.at(0), destination is args.at(1)

	// options for labeling as X, Y, or Z axis in three axis systems
	// use axisStr as prefix for saving/restoring app state
	if (isXAxis = cmdLineParse.isSet(xAxisOption))
	{
		this->setWindowIcon(QIcon(":magnetdaq/Resources/Magnet-DAQ-x.ico"));
		axisStr = "XAxis/";
		ui.tableTab->setEnabled(false);	// no table function for multi-axis operation
	}
	else if (isYAxis = cmdLineParse.isSet(yAxisOption))
	{
		this->setWindowIcon(QIcon(":magnetdaq/Resources/Magnet-DAQ-y.ico"));
		axisStr = "YAxis/";
		ui.tableTab->setEnabled(false);	// no table function for multi-axis operation
	}
	else if (isZAxis = cmdLineParse.isSet(zAxisOption))
	{
		this->setWindowIcon(QIcon(":magnetdaq/Resources/Magnet-DAQ-z.ico"));
		axisStr = "ZAxis/";
		ui.tableTab->setEnabled(false);	// no table function for multi-axis operation
	}
	else
	{
#if defined(Q_OS_LINUX)
		this->setWindowIcon(QIcon(":magnetdaq/Resources/Magnet-DAQ.ico"));
#endif
		axisStr = "";
	}

	// hidden start and parsing function options
	startHidden = cmdLineParse.isSet(hiddenOption);
	parseInput = cmdLineParse.isSet(parsingOption);

	// restore window position and gui state
	QSettings settings;
	ui.rampUnitsComboBox->setCurrentIndex(settings.value("RampUnits").toInt());

	// restore different geometry for different DPI screens
	QScreen* screen = QApplication::primaryScreen();
	QString dpiStr = QString::number(screen->logicalDotsPerInch());

	// restore main window according to collapsed preference
	if (settings.value(axisStr + "MainWindow/Collapsed/Selected" + dpiStr, false).toBool())
	{
		ui.collapseButton->setChecked(true);

		// collapse window to hide mainTabWidget and disable toolbar items
		ui.mainTabWidget->setHidden(true);
		ui.actionPrint->setEnabled(false);
		ui.actionSetup->setEnabled(false);
		ui.actionPlot_Settings->setEnabled(false);
		ui.actionShow_Keypad->setEnabled(false);
		this->setFixedHeight(MIN_WINDOW_HEIGHT_COLLAPSED);

		restoreGeometry(settings.value(axisStr + "MainWindow/Collapsed/Geometry/" + dpiStr).toByteArray());
		restoreState(settings.value(axisStr + "MainWindow/Collapsed/WindowState/" + dpiStr).toByteArray());

		ui.collapseButton->setToolTip("Expand Window");
		//ui.collapseButton->setArrowType(Qt::ArrowType::DownArrow);
	}
	else
	{
		this->setMinimumHeight(MIN_WINDOW_HEIGHT_EXPANDED);

		restoreGeometry(settings.value(axisStr + "MainWindow/Geometry/" + dpiStr).toByteArray());
		restoreState(settings.value(axisStr + "MainWindow/WindowState/" + dpiStr).toByteArray());
	}

	ui.rampRateTabSplitter->restoreGeometry(settings.value(axisStr + "RampSplitter/Geometry/" + dpiStr).toByteArray());
	ui.rampRateTabSplitter->restoreState(settings.value(axisStr + "RampSplitter/WindowState/" + dpiStr).toByteArray());
	ui.rampdownTabSplitter->restoreGeometry(settings.value(axisStr + "RampdownSplitter/Geometry/" + dpiStr).toByteArray());
	ui.rampdownTabSplitter->restoreState(settings.value(axisStr + "RampdownSplitter/WindowState/" + dpiStr).toByteArray());
	ui.systemProxyRadioButton->setChecked(settings.value(axisStr + "SystemProxy").toBool());
	ui.ipAddressEdit->setText(settings.value(axisStr + "IPAddr", "").toString());
	ui.ipNameEdit->setText(settings.value(axisStr + "IPName", "").toString());
	ui.logFileEdit->setText(settings.value(axisStr + "Logfile", "").toString());
	ui.remoteLockoutCheckBox->setChecked(settings.value(axisStr + "RemoteLockout", false).toBool());

	// no context menu for toolbar or dock widgets
	ui.mainToolBar->setContextMenuPolicy(Qt::PreventContextMenu);
	ui.setupDockWidget->setContextMenuPolicy(Qt::PreventContextMenu);

	// init members and ui state
	socket = NULL;
	telnet = NULL;
	lastPath = "";
	logFile = NULL;
	upgradeWizard = NULL;
	errorCode = NO_ERROR;
	errorstackDlg = nullptr;
	ui.actionRun->setEnabled(true);
	ui.actionStop->setEnabled(false);

	// save microvolt symbol from UI file
	ssSampleVoltageText = ui.magnetVoltageCheckBox->text();
	ui.magnetVoltageCheckBox->setText("Magnet Voltage (V)");

	// create convenience arrays for ramp rate values
	setupRampRateArrays();
	setupRampdownArrays();

	// connect toolbar actions
	connect(ui.actionRun, SIGNAL(triggered()), this, SLOT(actionRun()));
	connect(ui.actionStop, SIGNAL(triggered()), this, SLOT(actionStop()));
	connect(ui.actionSetup, SIGNAL(triggered()), this, SLOT(actionSetup()));
	connect(ui.actionPrint, SIGNAL(triggered()), this, SLOT(actionPrint()));
	connect(ui.actionHelp, SIGNAL(triggered()), this, SLOT(actionHelp()));
	connect(ui.actionAbout, SIGNAL(triggered()), this, SLOT(actionAbout()));
	connect(ui.actionUpgrade, SIGNAL(triggered()), this, SLOT(actionUpgrade()));
	connect(ui.actionShow_Keypad, SIGNAL(triggered()), this, SLOT(actionShow_Keypad()));
	connect(ui.actionPlot_Settings, SIGNAL(triggered()), this, SLOT(actionPlot_Settings()));
	connect(ui.collapseButton, SIGNAL(toggled(bool)), this, SLOT(actionToggle_Collapse(bool)));

	// restore device list in Setup
	restoreDeviceList(&settings);

	// create plotTimer
	plotTimer = new QTimer(this);

	if (parseInput)
	{
		plotTimer->setInterval(1000);	// 1 update per second if this app is QProcess slave
	}
	else
	{
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
		// For Linux and macOS, setting the plotTimer data collection rate too high results
		// in a lagging user interface. On the Mac, the display can go long periods
		// without a refresh. Limit the max update rate here to keep the interface responsive.
		plotTimer->setInterval(200);	// 5 updates per second max rate on Linux/macOS
#else
		// Windows seems to give preference to the user interface updates at the
		// expense of slowing the plotTimer data collection rate, so we set the update
		// rate to a higher value and let the interface dictate the actual achieved rate.
		plotTimer->setInterval(125);	// 8 updates per second max rate on Windows
#endif
	}

	connect(plotTimer, SIGNAL(timeout()), this, SLOT(timeout()));

	// restore plot saved settings
	restorePlotSettings(&settings);

	// initialize the plot layout
	initPlot();

	// restore and initialize the ramp plot layouts
	restoreRampPlotSettings(&settings);
	initRampPlot();
	restoreRampdownPlotSettings(&settings);
	initRampdownPlot();

	// restore support settings
	restoreSupportSettings(&settings);

	// restore table tab settings
	restoreTableSettings(&settings);

	// init LED states
	ui.shiftLED->setLook(KLed::Flat);
	ui.shiftLED->setColor(Qt::green);
	ui.shiftLED->setState(KLed::Off);
	ui.targetLED->setLook(KLed::Flat);
	ui.targetLED->setColor(Qt::green);
	ui.targetLED->setState(KLed::Off);
	ui.persistentLED->setLook(KLed::Flat);
	ui.persistentLED->setColor(Qt::green);
	ui.persistentLED->setState(KLed::Off);
	ui.energizedLED->setLook(KLed::Flat);
	ui.energizedLED->setColor(Qt::cyan);
	ui.energizedLED->setState(KLed::Off);
	ui.quenchLED->setLook(KLed::Flat);
	ui.quenchLED->setColor(Qt::red);
	ui.quenchLED->setState(KLed::Off);

	// setup status bar
	statusConnectState = new QLabel("", this);
	statusConnectState->setFrameStyle(QFrame::Panel | QFrame::Sunken);
	statusConnectState->setToolTip("TCP connection status");
	statusMisc = new QLabel("", this);
	statusMisc->setFrameStyle(QFrame::Panel | QFrame::Sunken);
	statusError = new ClickableLabel(this);
	statusError->setFrameStyle(QFrame::Panel | QFrame::Sunken);
	statusError->setToolTip("Error messages, click for history");
	statusSampleRate = new QLabel("", this);
	statusSampleRate->setFrameStyle(QFrame::Panel | QFrame::Sunken);
	statusSampleRate->setToolTip("Magnet data sample rate");
	statusSampleRate->setAlignment(Qt::AlignHCenter);

	statusBar()->addPermanentWidget(statusConnectState, 1);
	statusBar()->addPermanentWidget(statusMisc, 3);
	statusBar()->addPermanentWidget(statusSampleRate, 1);
	statusBar()->addPermanentWidget(statusError, 1);
	statusConnectState->setStyleSheet("color: red; font: bold");
	statusConnectState->setText("Disconnected");

	// other UI signal/slot connections
	connect(statusError, SIGNAL(clicked()), this, SLOT(actionShowErrorDialog()));
	connect(ui.remoteLockoutCheckBox, SIGNAL(toggled(bool)), this, SLOT(remoteLockoutChanged(bool)));
	connect(ui.ipAddressEdit, SIGNAL(textEdited(QString)), this, SLOT(ipAddressEdited(QString)));
	connect(ui.resetGraphButton, SIGNAL(clicked(bool)), this, SLOT(resetAxes(bool)));
	connect(ui.secondsRadioButton, SIGNAL(toggled(bool)), this, SLOT(timebaseChanged(bool)));
	connect(ui.magnetFieldRadioButton, SIGNAL(toggled(bool)), this, SLOT(currentAxisSelectionChanged(bool)));
	connect(ui.magnetVoltageCheckBox, SIGNAL(toggled(bool)), this, SLOT(magnetVoltageSelectionChanged(bool)));
	connect(ui.supplyCurrentCheckBox, SIGNAL(toggled(bool)), this, SLOT(supplyCurrentSelectionChanged(bool)));
	connect(ui.supplyVoltageCheckBox, SIGNAL(toggled(bool)), this, SLOT(supplyVoltageSelectionChanged(bool)));
	connect(ui.logfileButton, SIGNAL(clicked(bool)), this, SLOT(chooseLogfile(bool)));
	connect(ui.persistSwitchControlButton, SIGNAL(clicked()), this, SLOT(persistentSwitchButtonClicked()));
	connect(ui.targetFieldSetpointButton, SIGNAL(clicked()), this, SLOT(targetSetpointButtonClicked()));
	connect(ui.rampPauseButton, SIGNAL(clicked()), this, SLOT(rampPauseButtonClicked()));
	connect(ui.key_1, SIGNAL(clicked(bool)), this, SLOT(keypadClicked(bool)));
	connect(ui.key_2, SIGNAL(clicked(bool)), this, SLOT(keypadClicked(bool)));
	connect(ui.key_3, SIGNAL(clicked(bool)), this, SLOT(keypadClicked(bool)));
	connect(ui.key_4, SIGNAL(clicked(bool)), this, SLOT(keypadClicked(bool)));
	connect(ui.key_5, SIGNAL(clicked(bool)), this, SLOT(keypadClicked(bool)));
	connect(ui.key_6, SIGNAL(clicked(bool)), this, SLOT(keypadClicked(bool)));
	connect(ui.key_7, SIGNAL(clicked(bool)), this, SLOT(keypadClicked(bool)));
	connect(ui.key_8, SIGNAL(clicked(bool)), this, SLOT(keypadClicked(bool)));
	connect(ui.key_9, SIGNAL(clicked(bool)), this, SLOT(keypadClicked(bool)));
	connect(ui.key_0, SIGNAL(clicked(bool)), this, SLOT(keypadClicked(bool)));
	connect(ui.key_decimal, SIGNAL(clicked(bool)), this, SLOT(keypadClicked(bool)));
	connect(ui.key_plusminus, SIGNAL(clicked(bool)), this, SLOT(keypadClicked(bool)));
	connect(ui.key_shift, SIGNAL(clicked(bool)), this, SLOT(keypadClicked(bool)));
	connect(ui.key_zero, SIGNAL(clicked(bool)), this, SLOT(keypadClicked(bool)));
	connect(ui.key_esc, SIGNAL(clicked(bool)), this, SLOT(keypadClicked(bool)));
	connect(ui.key_menu, SIGNAL(clicked(bool)), this, SLOT(keypadClicked(bool)));
	connect(ui.key_enter, SIGNAL(clicked(bool)), this, SLOT(keypadClicked(bool)));
	connect(ui.key_right, SIGNAL(clicked(bool)), this, SLOT(keypadClicked(bool)));
	connect(ui.key_left, SIGNAL(clicked(bool)), this, SLOT(keypadClicked(bool)));
	connect(ui.mainTabWidget, SIGNAL(currentChanged(int)), this, SLOT(mainTabChanged(int)));
	connect(ui.setupToolBox, SIGNAL(currentChanged(int)), this, SLOT(setupToolBoxChanged(int)));
	connect(ui.targetSetpointEdit, SIGNAL(editingFinished()), this, SLOT(textValueChanged()));
	connect(ui.voltageLimitEdit, SIGNAL(editingFinished()), this, SLOT(textValueChanged()));
	connect(ui.minOutputVoltageEdit, SIGNAL(editingFinished()), this, SLOT(textValueChanged()));
	connect(ui.maxOutputVoltageEdit, SIGNAL(editingFinished()), this, SLOT(textValueChanged()));
	connect(ui.minOutputCurrentEdit, SIGNAL(editingFinished()), this, SLOT(textValueChanged()));
	connect(ui.maxOutputCurrentEdit, SIGNAL(editingFinished()), this, SLOT(textValueChanged()));
	connect(ui.stabilitySettingEdit, SIGNAL(editingFinished()), this, SLOT(textValueChanged()));
	connect(ui.coilConstantEdit, SIGNAL(editingFinished()), this, SLOT(textValueChanged()));
	connect(ui.currentLimitEdit, SIGNAL(editingFinished()), this, SLOT(textValueChanged()));
	connect(ui.magInductanceEdit, SIGNAL(editingFinished()), this, SLOT(textValueChanged()));
	connect(ui.inductanceButton, SIGNAL(clicked()), this, SLOT(inductanceSenseButtonClicked()));
	connect(ui.switchCurrentEdit, SIGNAL(editingFinished()), this, SLOT(textValueChanged()));
	connect(ui.switchHeatedTimeEdit, SIGNAL(editingFinished()), this, SLOT(textValueChanged()));
	connect(ui.switchCooledTimeEdit, SIGNAL(editingFinished()), this, SLOT(textValueChanged()));
	connect(ui.switchCooledRampRateEdit, SIGNAL(editingFinished()), this, SLOT(textValueChanged()));
	connect(ui.switchCoolingGainEdit, SIGNAL(editingFinished()), this, SLOT(textValueChanged()));
	connect(ui.autoDetectCurrentButton, SIGNAL(clicked()), this, SLOT(switchCurrentDetectionButtonClicked()));
	connect(ui.icSlopeEdit, SIGNAL(editingFinished()), this, SLOT(textValueChanged()));
	connect(ui.icOffsetEdit, SIGNAL(editingFinished()), this, SLOT(textValueChanged()));
	connect(ui.tmaxEdit, SIGNAL(editingFinished()), this, SLOT(textValueChanged()));
	connect(ui.tscaleEdit, SIGNAL(editingFinished()), this, SLOT(textValueChanged()));
	connect(ui.toffsetEdit, SIGNAL(editingFinished()), this, SLOT(textValueChanged()));
	connect(ui.stabilityModeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(menuValueChanged(int)));
	connect(ui.absorberComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(menuValueChanged(int)));
	connect(ui.switchInstalledComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(menuValueChanged(int)));
	connect(ui.stabilityResistorCheckBox, SIGNAL(toggled(bool)), this, SLOT(checkBoxValueChanged(bool)));
	connect(ui.switchTransitionComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(menuValueChanged(int)));
	connect(ui.quenchEnableComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(menuValueChanged(int)));
	connect(ui.quenchSensitivityComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(menuValueChanged(int)));
	connect(ui.externRampdownComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(menuValueChanged(int)));
	connect(ui.protectionModeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(menuValueChanged(int)));
	connect(ui.rampUnitsComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(menuValueChanged(int)));
	connect(ui.rampTimebaseComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(menuValueChanged(int)));
	connect(ui.rampSegmentsSpinBox, SIGNAL(valueChanged(int)), this, SLOT(rampSegmentCountChanged(int)));
	connect(ui.rampdownSegmentsSpinBox, SIGNAL(valueChanged(int)), this, SLOT(rampdownSegmentCountChanged(int)));
	connect(&model430, SIGNAL(syncRampdownEvents(QString)), this, SLOT(syncRampdownEvents(QString)));
	connect(&model430, SIGNAL(syncQuenchEvents(QString)), this, SLOT(syncQuenchEvents(QString)));
	connect(&model430, SIGNAL(syncTextSettings(QString)), this, SLOT(syncTextSettings(QString)));
	connect(ui.rampdownListWidget, SIGNAL(currentRowChanged(int)), this, SLOT(rampdownEventSelectionChanged(int)));
	connect(ui.rampdownRefreshButton, SIGNAL(clicked()), this, SLOT(refreshRampdownList()));
	connect(ui.quenchListWidget, SIGNAL(currentRowChanged(int)), this, SLOT(quenchEventSelectionChanged(int)));
	connect(ui.quenchRefreshButton, SIGNAL(clicked()), this, SLOT(refreshQuenchList()));
	connect(ui.autoscrollXCheckBox, SIGNAL(clicked(bool)), this, SLOT(toggleAutoscrollXCheckBox(bool)));
	connect(&model430, SIGNAL(shortSampleModeChanged(bool)), this, SLOT(shortSampleModeChanged(bool)));

	// More command line options parsing
	QString portStr = cmdLineParse.value(portOption);
	if (!portStr.isEmpty())
	{
		bool ok;
		port = portStr.toInt(&ok);

		if (!ok)
			port = 7180;
	}

	QString telnetStr = cmdLineParse.value(telnetOption);
	if (!telnetStr.isEmpty())
	{
		bool ok;
		tport = telnetStr.toInt(&ok);

		if (!ok)
			tport = 23;
	}

	targetIP = cmdLineParse.value(targetIPOption);
	if (!targetIP.isEmpty())
	{
		ui.ipAddressEdit->setText(targetIP);

		if(startHidden)	// hide main window on start
			this->showMinimized();

		actionRun();	// automatically Connect
	}
}

//---------------------------------------------------------------------------
// Destructor
//---------------------------------------------------------------------------
magnetdaq::~magnetdaq()
{
	actionStop();

	if (plotTimer)
	{
		delete plotTimer;
		plotTimer = NULL;
	}

	if (upgradeWizard)
	{
		delete upgradeWizard;
		upgradeWizard = NULL;
	}

	// save window position and state
	QSettings settings;

	// save different geometry for different DPI screens
	QScreen* screen = QApplication::primaryScreen();
	QString dpiStr = QString::number(screen->logicalDotsPerInch());

	// save ramp value (not time) units
	settings.setValue("RampUnits", ui.rampUnitsComboBox->currentIndex());

	// save other settings and positions
	if (ui.collapseButton->isChecked())
	{
		settings.setValue(axisStr + "MainWindow/Collapsed/Geometry/" + dpiStr, saveGeometry());
		settings.setValue(axisStr + "MainWindow/Collapsed/WindowState/" + dpiStr, saveState());
	}
	else
	{
		settings.setValue(axisStr + "MainWindow/Geometry/" + dpiStr, saveGeometry());
		settings.setValue(axisStr + "MainWindow/WindowState/" + dpiStr, saveState());
	}

	settings.setValue(axisStr + "RampSplitter/Geometry/" + dpiStr, ui.rampRateTabSplitter->saveGeometry());
	settings.setValue(axisStr + "RampSplitter/WindowState/" + dpiStr, ui.rampRateTabSplitter->saveState());
	settings.setValue(axisStr + "RampdownSplitter/Geometry/" + dpiStr, ui.rampdownTabSplitter->saveGeometry());
	settings.setValue(axisStr + "RampdownSplitter/WindowState/" + dpiStr, ui.rampdownTabSplitter->saveState());
	settings.setValue(axisStr + "DeviceList/HorizontalState/" + dpiStr, ui.devicesTableWidget->horizontalHeader()->saveState());
	settings.setValue(axisStr + "DeviceList/HorizontalGeometry/" + dpiStr, ui.devicesTableWidget->horizontalHeader()->saveGeometry());
	settings.setValue(axisStr + "IPAddr", ui.ipAddressEdit->text());
	settings.setValue(axisStr + "IPName", ui.ipNameEdit->text());
	settings.setValue(axisStr + "Graph/Xmin", ui.xminEdit->text());
	settings.setValue(axisStr + "Graph/Xmax", ui.xmaxEdit->text());
	settings.setValue(axisStr + "Graph/Ymin", ui.yminEdit->text());
	settings.setValue(axisStr + "Graph/Ymax", ui.ymaxEdit->text());
	settings.setValue(axisStr + "Graph/Vmin", ui.vminEdit->text());
	settings.setValue(axisStr + "Graph/Vmax", ui.vmaxEdit->text());
	settings.setValue(axisStr + "Logfile", ui.logFileEdit->text());
	settings.setValue(axisStr + "RemoteLockout", ui.remoteLockoutCheckBox->isChecked());
	settings.setValue(axisStr + "Graph/UseSeconds", ui.secondsRadioButton->isChecked());
	settings.setValue(axisStr + "Graph/AutoscrollX", ui.autoscrollXCheckBox->isChecked());

	// save titles (minus units)
	settings.setValue(axisStr + "Graph/TitleX", mainPlotXTitle);
	settings.setValue(axisStr + "Graph/TitleYCurrent", mainPlotYTitleCurrent);
	settings.setValue(axisStr + "Graph/TitleYField", mainPlotYTitleField);
	settings.setValue(axisStr + "Graph/TitleY2", mainPlotY2Title);

	if (model430.shortSampleMode)
	{
		settings.setValue("ssGraph/Title", mainPlotTitle);

		settings.setValue("ssGraph/Legend0", mainLegend[0]);
		settings.setValue("ssGraph/Legend1", mainLegend[1]);
		settings.setValue("ssGraph/Legend2", mainLegend[2]);
		settings.setValue("ssGraph/Legend3", mainLegend[3]);
		settings.setValue("ssGraph/Legend4", mainLegend[4]);
	}
	else
	{
		settings.setValue(axisStr + "Graph/Title", mainPlotTitle);

		settings.setValue(axisStr + "Graph/Legend0", mainLegend[0]);
		settings.setValue(axisStr + "Graph/Legend1", mainLegend[1]);
		settings.setValue(axisStr + "Graph/Legend2", mainLegend[2]);
		settings.setValue(axisStr + "Graph/Legend3", mainLegend[3]);
		settings.setValue(axisStr + "Graph/Legend4", mainLegend[4]);
	}

	// save main plot settings
	settings.setValue(axisStr + "Graph/PlotField", ui.magnetFieldRadioButton->isChecked());
	settings.setValue(axisStr + "Graph/PlotMagnetVoltage", ui.magnetVoltageCheckBox->isChecked());
	settings.setValue(axisStr + "Graph/PlotSupplyCurrent", ui.supplyCurrentCheckBox->isChecked());
	settings.setValue(axisStr + "Graph/PlotSupplyVoltage", ui.supplyVoltageCheckBox->isChecked());

	// save support settings
	settings.setValue(axisStr + "Support/Subject", ui.caseEdit->text());

	// save proxy settings
	settings.setValue(axisStr + "SystemProxy", ui.systemProxyRadioButton->isChecked());

	// save table settings
	settings.setValue("Table/AutosaveReport", ui.autosaveReportCheckBox->isChecked());
	settings.setValue("Table/EnableExecution", ui.executeCheckBox->isChecked());
	settings.setValue("Table/AppPath", ui.appLocationEdit->text());
	settings.setValue("Table/AppArgs", ui.appArgsEdit->text());
	settings.setValue("Table/PythonPath", ui.pythonPathEdit->text());
	settings.setValue("Table/ExecutionTime", ui.appStartEdit->text());
	settings.setValue("Table/PythonScript", ui.pythonCheckBox->isChecked());
}

//---------------------------------------------------------------------------
void magnetdaq::actionRun(void)
{
	QProgressDialog progressDialog;
#if defined(Q_OS_MACOS)
	// why does MacOS cutoff the text at the margins without the extra spaces?
	progressDialog.setLabelText(QString("   Reading remote 430 configuration...   "));
#else
	progressDialog.setLabelText(QString("Reading remote 430 configuration..."));
#endif
	statusConnectState->clear();
	ui.droppedConnectionLabel->clear();
	clearErrorHistory();

	if (errorstackDlg)
		errorstackDlg->clearErrorListWidget();

	// Display the dialog and start the event loop.
	if (!startHidden)
	{
		progressDialog.show();
		progressDialog.setWindowModality(Qt::WindowModal);
		progressDialog.setValue(0);
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
	}

	// use the port 7180 (default) socket to collect high-speed data queries
	socket = new Socket(&model430, this);

	if (ui.noProxyRadioButton->isChecked())
		socket->connectToModel430(ui.ipAddressEdit->text(), port, QNetworkProxy::NoProxy);
	else if (ui.systemProxyRadioButton->isChecked())
		socket->connectToModel430(ui.ipAddressEdit->text(), port, QNetworkProxy::applicationProxy().type());

	if (socket->isConnected())
	{
		// clear interface items
		this->setWindowTitle(QCoreApplication::applicationName() + " - v" + QCoreApplication::applicationVersion());
		updateFrontPanel("", false, false, false, false, false);

		plotCount = 0;
		// clear data
		for (int i = 0; i < 5; i++)
			ui.plotWidget->graph(i)->data()->clear();
		ui.plotWidget->replot();

		// query firmware version and suffix
		socket->getFirmwareVersion();
		ui.serialNumEdit->setText(model430.serialNumber());

		// check for required firmware update
		if (checkFirmwareVersion())
		{
			// connect the socket data ready signal to the plot
			connect(socket, SIGNAL(nextDataPoint(qint64, double, double, double, double, double, double, quint8, quint8)),
				this, SLOT(addDataPoint(qint64, double, double, double, double, double, double, quint8, quint8)));
			
			// connect error signals
			connect(socket, SIGNAL(model430Disconnected()), this, SLOT(actionStop()));
			connect(socket, SIGNAL(systemErrorMessage(QString, QString)), this, SLOT(displaySystemError(QString, QString)), Qt::ConnectionType::QueuedConnection);

			// query 430 mode (s2 state), sets up interface for short-sample mode if needed
			socket->getMode();

			// query 430 status
			socket->getStatusByte();

			// query 430 ipName
			socket->getIpName();

			// lockout front panel if preferred
			if (ui.remoteLockoutCheckBox->isChecked())
				socket->remoteLockout(true);

			// connect socket to 430 settings
			model430.setSocket(socket);

			// connect signal for configuration changes
			connect(&model430, SIGNAL(configurationChanged(QueryState)), this, SLOT(configurationChanged(QueryState)));

			// initialize the 430 configuration GUI, show progress dialog and checking for cancellation
			model430.syncSupplySetup();

			if (!startHidden)
			{
				progressDialog.setValue(20);
				if (progressDialog.wasCanceled())
					actionStop();
				QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
			}

			model430.syncLoadSetup();

			if (!startHidden)
			{
				progressDialog.setValue(40);
				if (progressDialog.wasCanceled())
					actionStop();
				QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
			}

			model430.syncSwitchSetup();

			if (!startHidden)
			{
				progressDialog.setValue(60);
				if (progressDialog.wasCanceled())
					actionStop();
				QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
			}

			model430.syncProtectionSetup();

			if (!startHidden)
			{
				progressDialog.setValue(80);
				if (progressDialog.wasCanceled())
					actionStop();
				QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
			}

			model430.syncEventCounts();

			if (!startHidden)
			{
				progressDialog.setValue(85);
				if (progressDialog.wasCanceled())
					actionStop();
				QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
			}

			model430.syncRampRates();

			if (!startHidden)
			{
				progressDialog.setValue(100);
				if (progressDialog.wasCanceled())
					actionStop();
			}

			// use the port 23 (default) telnet socket for configuration and keypad simulation
			telnet = new Socket(&model430, this);

			// capture display/keypad broadcasts
			if (ui.noProxyRadioButton->isChecked())
				telnet->connectToModel430(ui.ipAddressEdit->text(), tport, QNetworkProxy::NoProxy);
			else if (ui.systemProxyRadioButton->isChecked())
				telnet->connectToModel430(ui.ipAddressEdit->text(), tport, QNetworkProxy::applicationProxy().type());

			if (telnet->isConnected())
			{
				// connect the socket data ready signal to the front panel display
				connect(telnet, SIGNAL(updateFrontPanel(QString, bool, bool, bool, bool, bool)), this,
					SLOT(updateFrontPanel(QString, bool, bool, bool, bool, bool)), Qt::ConnectionType::QueuedConnection);

				// connect error signal
				connect(telnet, SIGNAL(systemError()), this, SLOT(systemErrorNotification()));
				connect(telnet, SIGNAL(model430Disconnected()), this, SLOT(droppedTelnet()));

				// connect change notifications
				connect(telnet, SIGNAL(fieldUnitsChanged()), &model430, SLOT(syncFieldUnits()));
				connect(telnet, SIGNAL(startExternalRampdown()), this, SLOT(startExternalRampdown()));
				connect(telnet, SIGNAL(endExternalRampdown()), this, SLOT(endExternalRampdown()));

				// all good, finish initializing interface
				ui.actionRun->setEnabled(false);
				ui.actionStop->setEnabled(true);
				ui.actionUpgrade->setEnabled(checkAvailableFirmware());
				ui.ipAddressEdit->setEnabled(false);
				ui.ipAddressLabel->setEnabled(false);
				ui.proxyGroupBox->setEnabled(false);
				ui.logFileEdit->setEnabled(false);
				ui.logFileLabel->setEnabled(false);
				ui.logfileButton->setEnabled(false);
				ui.secondsRadioButton->setEnabled(false);
				ui.minutesRadioButton->setEnabled(false);
				ui.devicesTableWidget->setEnabled(false);
				ui.deleteDeviceButton->setEnabled(false);
				mainTabChanged(-1);

				// start the plot timer to get data points at regular interval
				firstStatsPass = true;	// restarts sample rate averaging
				samplePos = 0;
				startTime = QDateTime::currentMSecsSinceEpoch();
				lastTime = startTime;

				// reset timebase
				if (ui.autoscrollXCheckBox->isChecked())
					timeAxis->setRange(ui.xminEdit->text().toDouble(), ui.xmaxEdit->text().toDouble());

				plotTimer->start();

				// enable table functions
				ui.manualControlGroupBox->setEnabled(true);
				ui.autoStepGroupBox->setEnabled(true);

				statusConnectState->setStyleSheet("color: green; font: bold");
				statusConnectState->setText("Connected to " + ui.ipAddressEdit->text());

				if (model430.statusByte() & EXT_RAMPDOWN_EVENT)
					startExternalRampdown();

				if (!ui.logFileEdit->text().isEmpty())
				{
					// good connections to this point, create log file
					logFile = new QFile(ui.logFileEdit->text());
					logFile->open(QFile::Append);

					// write date/time of start
					logFile->write("\n");
					logFile->write(QDateTime::currentDateTime().toString("MM/dd/yyyy: hh:mm:ss ap").toLatin1());
					logFile->write("\n");
					logFile->flush();
				}

				// add/update device in list and update window title bar
				ui.ipNameEdit->setText(model430.getIpName());
				addOrUpdateDevice(ui.ipAddressEdit->text(), model430.getIpName());
				setDeviceWindowTitle();

				// start Parser if enabled
				if (parseInput)
				{
					QThread* parserThread = new QThread;
					parser = new Parser(NULL);

					parser->_setParent(this);	// have to do this because parserThread wants to move member parent reference
					parser->setDataSource(&model430);
					parser->moveToThread(parserThread);
					connect(parser, SIGNAL(error_msg(QString)), this, SLOT(parserErrorString(QString)));
					connect(parser, SIGNAL(exit_app()), this, SLOT(exit_app()));
					connect(parserThread, SIGNAL(started()), parser, SLOT(process()));
					connect(parser, SIGNAL(finished()), parserThread, SLOT(quit()));
					connect(parser, SIGNAL(finished()), parser, SLOT(deleteLater()));
					connect(parserThread, SIGNAL(finished()), parserThread, SLOT(deleteLater()));
					parserThread->start();
				}

				// check switch installed state change and clear table if needed
				{
					static bool lastSwitchInstalledValue = false;	// retain between calls

					if (model430.switchInstalled() != lastSwitchInstalledValue)
						tableClear();

					lastSwitchInstalledValue = model430.switchInstalled();
					setTableHeader();
				}
			}
			else
			{
				ui.droppedConnectionLabel->setText("Display/keypad connection interrupted, Stop and re-Connect to re-establish communication");
				statusConnectState->setStyleSheet("color: red; font: bold");
				statusConnectState->setText("Failed to connect!");
				socket->deleteLater();
				socket = NULL;

				telnet->deleteLater();
				telnet = NULL;
			}
		}
		else
		{
			// firmware needs updating
			progressDialog.close();
			socket->deleteLater();
			socket = NULL;
			telnet = NULL;

			if (parseInput)	// cannot upgrade firmware when under remote control as upgrade process should not be interrupted
			{
				QMessageBox msgBox;

				msgBox.setText("A firmware upgrade is required to use this application with the device at network address " + ui.ipAddressEdit->text() + ".");
				msgBox.setInformativeText("Cannot upgrade the firmware during a remote control session. Manually restart Magnet-DAQ and connect to the device to perform a firmware upgrade.");
				msgBox.setStandardButtons(QMessageBox::Ok);
				msgBox.setDefaultButton(QMessageBox::Ok);

				msgBox.setIcon(QMessageBox::Critical);
				int ret = msgBox.exec();

				statusConnectState->setStyleSheet("color: red; font: bold");
				statusConnectState->setText("Firmware update required!");
			}
			else
			{
				QMessageBox msgBox;

				if (model430.firmwareVersion() == 0.0)
				{
					// likely there was no response
					msgBox.setText("No firmware version information returned from device.");
					msgBox.setInformativeText("Please check the device for proper network connections.");
					msgBox.setStandardButtons(QMessageBox::Ok);
					msgBox.setDefaultButton(QMessageBox::Ok);
				}
				else
				{
					msgBox.setText(formatFirmwareUpgradeMsg());

					// firmware versions older than v1.62 may use non-compatible WinCE kernel
					if (model430.firmwareVersion() >= 1.62)
					{
						msgBox.setInformativeText("Do you wish to upgrade the firmware?");
						msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
						msgBox.setDefaultButton(QMessageBox::No);
					}
					else
					{
						msgBox.setInformativeText("Please contact AMI Technical Support for further instructions if you would like to upgrade the firmware.");
						msgBox.setStandardButtons(QMessageBox::Ok);
						msgBox.setDefaultButton(QMessageBox::Ok);
					}
				}

				msgBox.setIcon(QMessageBox::Critical);
				int ret = msgBox.exec();

				// offer firmware upgrade here!
				if (ret == QMessageBox::Yes)
					QMetaObject::invokeMethod(this, "showFirmwareUpgradeWizard", Qt::QueuedConnection);
			}
		}
	}
	else
	{
		statusConnectState->setStyleSheet("color: red; font: bold");
		statusConnectState->setText("Failed to connect!");
		ui.droppedConnectionLabel->setText("Emulated display/keypad not presently connected to a remote device.");
		socket->deleteLater();
		socket = NULL;
		telnet = NULL;
	}
}

//---------------------------------------------------------------------------
void magnetdaq::actionStop(void)
{
	// stop and destroy parser and associated thread if it exists
	if (parser)
	{
		parser->stop();
		parser = NULL;
	}

	// stop any active table auto-stepping
	stopAutostep();

	// stop plotting
	plotTimer->stop();

	// close all 430 connections
	if (socket)
	{
		model430.setSocket(NULL);
		socket->deleteLater();
		socket = NULL;
		setDeviceWindowTitle();
	}

	if (telnet)
	{
		telnet->deleteLater();
		telnet = NULL;
	}

	if (logFile)
	{
		logFile->flush();
		logFile->close();
		delete logFile;
		logFile = NULL;
	}

	ui.actionRun->setEnabled(true);
	ui.actionStop->setEnabled(false);
	ui.actionUpgrade->setEnabled(false);
	ui.ipAddressEdit->setEnabled(true);
	ui.ipAddressLabel->setEnabled(true);
	ui.proxyGroupBox->setEnabled(true);
	ui.logFileEdit->setEnabled(true);
	ui.logFileLabel->setEnabled(true);
	ui.logfileButton->setEnabled(true);
	ui.secondsRadioButton->setEnabled(true);
	ui.minutesRadioButton->setEnabled(true);
	ui.devicesTableWidget->setEnabled(true);

	// re-enable the delete button if there is a selection
	if (ui.devicesTableWidget->selectionModel()->selectedRows().count())
		ui.deleteDeviceButton->setEnabled(true);

	// table execution disabled until connect
	ui.manualControlGroupBox->setEnabled(false);
	ui.autoStepGroupBox->setEnabled(false);

	statusConnectState->setStyleSheet("color: red; font: bold");
	statusConnectState->setText("Disconnected");
	statusSampleRate->clear();

	ui.droppedConnectionLabel->setText("Emulated display/keypad not presently connected to a remote device.");
}

//---------------------------------------------------------------------------
void magnetdaq::droppedTelnet(void)
{
	if (ui.actionStop->isEnabled())
		ui.droppedConnectionLabel->setText("Display/keypad connection interrupted, Stop and re-Connect to re-establish communication");
}

//---------------------------------------------------------------------------
void magnetdaq::exit_app(void)
{
	// first disconnect
	actionStop();

	// then close app
	close();
}

//---------------------------------------------------------------------------
void magnetdaq::actionSetup(void)
{
	if (ui.setupDockWidget->isVisible() && !ui.setupDockWidget->visibleRegion().isEmpty())
	{
		ui.setupDockWidget->hide();
	}
	else
	{
		// initially open into a docked tab item
		if (ui.plotDockWidget->isVisible())
			QMainWindow::tabifyDockWidget(ui.setupDockWidget, ui.plotDockWidget);
		else if (ui.keypadDockWidget->isVisible())
			QMainWindow::tabifyDockWidget(ui.setupDockWidget, ui.keypadDockWidget);

		ui.setupDockWidget->show();
		ui.setupDockWidget->raise();
	}
}

//---------------------------------------------------------------------------
void magnetdaq::actionPlot_Settings(void)
{
	if (ui.plotDockWidget->isVisible() && !ui.plotDockWidget->visibleRegion().isEmpty())
	{
		ui.plotDockWidget->hide();
	}
	else
	{
		// initially open into a docked tab item
		if (ui.setupDockWidget->isVisible())
			QMainWindow::tabifyDockWidget(ui.setupDockWidget, ui.plotDockWidget);
		else if (ui.keypadDockWidget->isVisible())
			QMainWindow::tabifyDockWidget(ui.plotDockWidget, ui.keypadDockWidget);

		ui.plotDockWidget->show();
		ui.plotDockWidget->raise();
	}
}

//---------------------------------------------------------------------------
void magnetdaq::actionShow_Keypad(void)
{
	if (ui.keypadDockWidget->isVisible() && !ui.keypadDockWidget->visibleRegion().isEmpty())
	{
		ui.keypadDockWidget->hide();
	}
	else
	{
		// initially open into a docked tab item
		if (ui.setupDockWidget->isVisible())
			QMainWindow::tabifyDockWidget(ui.setupDockWidget, ui.keypadDockWidget);
		else if (ui.plotDockWidget->isVisible())
			QMainWindow::tabifyDockWidget(ui.plotDockWidget, ui.keypadDockWidget);

		ui.keypadDockWidget->show();
		ui.keypadDockWidget->raise();
	}
}

//---------------------------------------------------------------------------
void magnetdaq::actionPrint(void)
{
#if defined(USE_QTPRINTER) || defined(USE_QTPRINTER_6)
	// print main plot
	QPrinter printer(QPrinterInfo::defaultPrinter()); // , QPrinter::ScreenResolution);
	QPrintPreviewDialog previewDialog(&printer, this);

	if (ui.mainTabWidget->currentIndex() == PLOT_TAB)
		connect(&previewDialog, SIGNAL(paintRequested(QPrinter*)), SLOT(renderPlot(QPrinter*)));
	else if (ui.mainTabWidget->currentIndex() == RAMP_TAB)
		connect(&previewDialog, SIGNAL(paintRequested(QPrinter*)), SLOT(renderRampPlot(QPrinter*)));
	else if (ui.mainTabWidget->currentIndex() == RAMPDOWN_TAB)
		connect(&previewDialog, SIGNAL(paintRequested(QPrinter*)), SLOT(renderRampdownPlot(QPrinter*)));
	previewDialog.exec();
#endif
}

//---------------------------------------------------------------------------
void magnetdaq::actionShowErrorDialog(void)
{
	if (errorstackDlg == NULL)
	{
		errorstackDlg = new errorhistorydlg(this);
		errorstackDlg->restoreDlgGeometry(axisStr);
	}

	errorstackDlg->loadErrorHistory(&errorStack);
	errorstackDlg->show();
	errorstackDlg->raise();
	errorstackDlg->activateWindow();
}

//---------------------------------------------------------------------------
void magnetdaq::actionHelp(void)
{
#if defined(Q_OS_MACOS)
	QString link = "file:///" + QCoreApplication::applicationDirPath() + "//..//Resources//Help//index.html";
#else
	QString link = "file:///" + QCoreApplication::applicationDirPath() + "//Help//index.html";
#endif

	QDesktopServices::openUrl(QUrl(link));
}

//---------------------------------------------------------------------------
void magnetdaq::actionAbout(void)
{
	aboutdialog about;
	about.exec();
}

//---------------------------------------------------------------------------
void magnetdaq::actionUpgrade(void)
{
	if (checkAvailableFirmware())
	{
		if (parseInput)	// cannot upgrade firmware when under remote control as upgrade process should not be interrupted
		{
			QMessageBox msgBox;

			msgBox.setText("Firmware Upgrade Wizard is unavailable.");
			msgBox.setInformativeText("Firmware upgrade is not allowed during a remote control session. Manually restart Magnet-DAQ and connect to the device to perform a firmware upgrade.");
			msgBox.setStandardButtons(QMessageBox::Ok);
			msgBox.setDefaultButton(QMessageBox::Ok);

			msgBox.setIcon(QMessageBox::Critical);
			int ret = msgBox.exec();
		}
		else
		{
			QMetaObject::invokeMethod(this, "showFirmwareUpgradeWizard", Qt::QueuedConnection);
		}
	}
}

//---------------------------------------------------------------------------
void magnetdaq::actionToggle_Collapse(bool checked)
{
	// save selection
	QSettings settings;

	// save different geometry for different DPI screens
	QScreen* screen = QApplication::primaryScreen();
	QString dpiStr = QString::number(screen->logicalDotsPerInch());

	settings.setValue(axisStr + "MainWindow/Collapsed/Selected" + dpiStr, checked);

	if (checked)
	{
		// save expanded layout
		settings.setValue(axisStr + "MainWindow/Geometry/" + dpiStr, saveGeometry());
		settings.setValue(axisStr + "MainWindow/WindowState/" + dpiStr, saveState());

		// collapse window to hide mainTabWidget and hide all docked panels and disable
		// toolbar items that show panels
		ui.mainTabWidget->setHidden(true);
		ui.setupDockWidget->hide();
		ui.plotDockWidget->hide();
		ui.keypadDockWidget->hide();
		ui.actionPrint->setEnabled(false);
		ui.actionSetup->setEnabled(false);
		ui.actionPlot_Settings->setEnabled(false);
		ui.actionShow_Keypad->setEnabled(false);
		this->setFixedHeight(MIN_WINDOW_HEIGHT_COLLAPSED);

		restoreGeometry(settings.value(axisStr + "MainWindow/Collapsed/Geometry/" + dpiStr).toByteArray());
		restoreState(settings.value(axisStr + "MainWindow/Collapsed/WindowState/" + dpiStr).toByteArray());
		ui.collapseButton->setToolTip("Expand Window");
		//ui.collapseButton->setArrowType(Qt::ArrowType::DownArrow);
	}
	else
	{
		// save collapsed layout
		settings.setValue(axisStr + "MainWindow/Collapsed/Geometry/" + dpiStr, saveGeometry());
		settings.setValue(axisStr + "MainWindow/Collapsed/WindowState/" + dpiStr, saveState());
		this->setMinimumHeight(MIN_WINDOW_HEIGHT_EXPANDED);
		this->setMaximumHeight(16777215);

		// expand window to show mainTabWidget and enable toolbar items
		ui.mainTabWidget->setHidden(false);
		ui.actionPrint->setEnabled(true);
		ui.actionSetup->setEnabled(true);
		ui.actionPlot_Settings->setEnabled(true);
		ui.actionShow_Keypad->setEnabled(true);

		restoreGeometry(settings.value(axisStr + "MainWindow/Geometry/" + dpiStr).toByteArray());
		restoreState(settings.value(axisStr + "MainWindow/WindowState/" + dpiStr).toByteArray());

		ui.collapseButton->setToolTip("Collapse Window");
		//ui.collapseButton->setArrowType(Qt::ArrowType::UpArrow);
	}
}

//---------------------------------------------------------------------------
// Outputs general and parser function errors
//---------------------------------------------------------------------------
void magnetdaq::setStatusMsg(QString msg)
{
	// restore style
	statusMisc->setStyleSheet(lastStatusMiscStyle);

	// always save the msg
	lastStatusMiscString = msg;

	if (!errorStatusIsActive && !parserErrorStatusIsActive)	// show it now!
		statusMisc->setText(msg);
}

//---------------------------------------------------------------------------
void magnetdaq::pinErrorString(QString errMsg, bool highlight)
{
	QString err;

	lastStatusMiscStyle = statusMisc->styleSheet();

	if (highlight)
	{
		statusMisc->setStyleSheet("color: red; font: bold");
		err = "Error: " + errMsg;
	}
	else
		err = errMsg;

	statusMisc->setText(err);

	qDebug() << err;	// send to log
}

//---------------------------------------------------------------------------
void magnetdaq::showErrorString(QString errMsg, bool highlight)
{
	QString err;

	if (!errorStatusIsActive)
	{
		errorStatusIsActive = true;
		lastStatusMiscStyle = statusMisc->styleSheet();
		lastStatusMiscString = statusMisc->text();
	}

	if (highlight)
	{
		statusMisc->setStyleSheet("color: red; font: bold");
		err = "Error: " + errMsg;
	}
	else
		err = errMsg;

	statusMisc->setText(err);
	QTimer::singleShot(5000, this, SLOT(errorStatusTimeout()));

	qDebug() << err;	// send to log
}

//---------------------------------------------------------------------------
void magnetdaq::parserErrorString(QString errMsg)
{
	if (!parserErrorStatusIsActive)
	{
		parserErrorStatusIsActive = true;
		lastStatusMiscStyle = statusMisc->styleSheet();
		lastStatusMiscString = statusMisc->text();
	}

	statusMisc->setStyleSheet("color: red; font: bold");
	QString err = "Parser Error: " + errMsg;
	statusMisc->setText(err);
	QTimer::singleShot(5000, this, SLOT(errorStatusTimeout()));
	qDebug() << err;	// log remote parser errors
}

//---------------------------------------------------------------------------
void magnetdaq::errorStatusTimeout(void)
{
	if (errorStatusIsActive)
		errorStatusIsActive = false;

	if (parserErrorStatusIsActive)
		parserErrorStatusIsActive = false;

	statusMisc->setStyleSheet(lastStatusMiscStyle);
	statusMisc->setText(lastStatusMiscString);	// restore normal messages
}

//---------------------------------------------------------------------------
void magnetdaq::ipAddressEdited(QString text)
{
	ui.ipNameEdit->clear();
	ui.devicesTableWidget->clearSelection();
}

//---------------------------------------------------------------------------
void magnetdaq::setDeviceWindowTitle(void)
{
	QString optionalAxisLabel = "";
	if (isXAxis)
		optionalAxisLabel = "X Axis - ";
	else if (isYAxis)
		optionalAxisLabel = "Y Axis - ";
	else if (isZAxis)
		optionalAxisLabel = "Z Axis - ";

	if (socket)
	{
		this->setWindowTitle(optionalAxisLabel + QCoreApplication::applicationName() + " - v" + QCoreApplication::applicationVersion() + "      Connected to: " +
			ui.ipNameEdit->text() + " @ " + ui.ipAddressEdit->text());
	}
	else
	{
		this->setWindowTitle(optionalAxisLabel + QCoreApplication::applicationName() + " - v" + QCoreApplication::applicationVersion() + "      " +
			ui.ipNameEdit->text() + " @ " + ui.ipAddressEdit->text());
	}
}

//---------------------------------------------------------------------------
void magnetdaq::chooseLogfile(bool checked)
{
	QString logFileName = QFileDialog::getSaveFileName(this, "Choose Log File", lastPath, "Log Files (*.txt *.log *.csv)");

	if (!logFileName.isEmpty())
	{
		ui.logFileEdit->setText(logFileName);
		lastPath = logFileName;
	}
}

//---------------------------------------------------------------------------
void magnetdaq::remoteLockoutChanged(bool checked)
{
	if (socket)
	{
		socket->remoteLockout(checked);
	}
}

//---------------------------------------------------------------------------
void magnetdaq::timeout(void)
{
	// plotTimer fired, request next data point from socket
	socket->getNextDataPoint();

	// fetch STATE? every sample if a QProcess slave
	if (parseInput)
	{
		// if parser input we reduce data collection rate to 1 Hz
		socket->getState();
	}
}

//---------------------------------------------------------------------------
void magnetdaq::updateFrontPanel(QString displayString, bool shiftLED, bool fieldLED, bool persistentLED, bool energizedLED, bool quenchLED)
{
	// first format the displayString for HTML display
	QString htmlDisplay = "<!DOCTYPE HTML PUBLIC \" -//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">\n";

	htmlDisplay += "<html><head><meta name =\"qrichtext\" content=\"1\"/><style type=\"text/css\">p, li{white-space:pre-wrap;}\n";

	// font scaling, 14pt is the base font size at 120 dpi
	QScreen* screen = this->window()->windowHandle()->screen();
	qreal dpi = screen->logicalDotsPerInch();
	double font_size = 120.0 / dpi * 14.0;

	htmlDisplay += "</style></head><body style =\"font-family:'Bubbledot Fine Positive'; font-size:" + QString::number(font_size, 'f', 1) + "pt; font-style:normal; \"bgcolor=\"#000000\">\n";

	htmlDisplay += "<p style =\"color:#a2d1ff;\">";

	// *********************************************
	// HTML code replacements for special characters

	// replace question mark
	displayString.replace("?", "&#x3F;");

	// replace less than symbol
	displayString.replace(QChar(60), "&lt;");

	// replace greater than symbol
	displayString.replace(QChar(63), "&gt;");

	// replace linefeed and special characters
	displayString.replace("\n", "<br/>");	// linefeed

	// plus/minus symbol
	displayString.replace(QChar(241), "&plusmn;");

	// DEGREE address: 0x86
	displayString.replace(QChar(0x86), "&deg;");

	// *********************************************
	// Platform/font specific code replacements for special characters

#if defined(Q_OS_MACOS)	// Mac specific, uses the "Edit->Emoticons and Symbols" definitions for MacOS
	font_size = 120.0 / dpi * 12.0;

	// reverse "P" address: 0x80
	displayString.replace(QChar(0x80), "<span style = \"font-family:'Bubbledot Fine Negative';font-size:" + QString::number(font_size, 'f', 1) + "pt;\">P</span>");

	// "UP" address: 0x81
	displayString.replace(QChar(0x81), "<span style=\"font-size:" + QString::number(font_size, 'f', 1) + "pt;\">" + QString("\u2191") + "</span>");

	// "DOWN" address: 0x82
	displayString.replace(QChar(0x82), "<span style=\"font-size:" + QString::number(font_size, 'f', 1) + "pt;\">" + QString("\u2193") + "</span>");

	// "->" address: 0x83
	displayString.replace(QChar(0x83), "<span style=\"font-size:" + QString::number(font_size, 'f', 1) + "pt;\">" + QString("\u261B") + "</span>");

	// "<-" address: 0x84
	displayString.replace(QChar(0x84), "<span style=\"font-size:" + QString::number(font_size, 'f', 1) + "pt;\">" + QString("\u261A") + "</span>");

	// MENU pointer address: 0x85
	displayString.replace(QChar(0x85), "<span style=\"font-size:" + QString::number(font_size, 'f', 1) + "pt;\">" + QString("\u25BA") + "</span>");

	// ENCODER IN USE address: 0x87
	displayString.replace(QChar(0x87), "<span style=\"font-size:" + QString::number(font_size, 'f', 1) + "pt;\">" + QString("\u21D5") + "</span>");

	// reverse "V" address: 0x88
	displayString.replace(QChar(0x88), "<span style=\"font-family:'Bubbledot Fine Negative';\">V</span>");

	// inverse "C" address: 0x89
	displayString.replace(QChar(0x89), "<span style=\"font-family:'Bubbledot Fine Negative';\">C</span>");

	// inverse "T" address: 0x8A
	displayString.replace(QChar(0x8A), "<span style=\"font-family:'Bubbledot Fine Negative';\">T</span>");

#else	// Windows and Linux
	font_size = 120.0 / dpi * 13.0;	// slightly smaller

	// reverse "P" address: 0x80
	displayString.replace(QChar(0x80), "<span style = \"font-family:'Bubbledot Fine Negative'; font-size:" + QString::number(font_size, 'f', 1) + "pt;\">P</span>");

	// "UP" address: 0x81
	displayString.replace(QChar(0x81), "<span style=\"font-family:'Wingdings 3'; font-size:" + QString::number(font_size, 'f', 1) + "pt;\">h</span>");

	// "DOWN" address: 0x82
	displayString.replace(QChar(0x82), "<span style=\"font-family:'Wingdings 3'; font-size:" + QString::number(font_size, 'f', 1) + "pt;\">i</span>");

	// reverse "V" address: 0x88
	displayString.replace(QChar(0x88), "<span style=\"font-family:'Bubbledot Fine Negative'; font-size:" + QString::number(font_size, 'f', 1) + "pt;\">V</span>");

	// inverse "C" address: 0x89
	displayString.replace(QChar(0x89), "<span style=\"font-family:'Bubbledot Fine Negative'; font-size:" + QString::number(font_size, 'f', 1) + "pt;\">C</span>");

	// inverse "T" address: 0x8A
	displayString.replace(QChar(0x8A), "<span style=\"font-family:'Bubbledot Fine Negative'; font-size:" + QString::number(font_size, 'f', 1) + "pt;\">T</span>");

	font_size = 120.0 / dpi * 10.0;	// somewhat smaller

	// "->" address: 0x83
	displayString.replace(QChar(0x83), "<span style=\"font-family:'Wingdings 3'; font-size:" + QString::number(font_size, 'f', 1) + "pt;\">&#xE2;</span>");

	// "<-" address: 0x84
	displayString.replace(QChar(0x84), "<span style=\"font-family:'Wingdings 3'; font-size:" + QString::number(font_size, 'f', 1) + "pt;\">&#xE1;</span>");

	// MENU pointer address: 0x85
	displayString.replace(QChar(0x85), "<span style=\"font-family:'Wingdings 3'; font-size:" + QString::number(font_size, 'f', 1) + "pt;\">u</span>");

	// ENCODER IN USE address: 0x87
	displayString.replace(QChar(0x87), "<span style=\"font-family:'Wingdings 3'; font-size:" + QString::number(font_size, 'f', 1) + "pt;\">o</span>");
#endif

	// complete the displayString
	htmlDisplay += displayString;
	htmlDisplay += "</p></body></html>";

	// update the display interface
	ui.displayTextEdit->setHtml(htmlDisplay);

	// set LEDs
	if (shiftLED)
		ui.shiftLED->setState(KLed::State::On);
	else
		ui.shiftLED->setState(KLed::State::Off);

	if (fieldLED)
		ui.targetLED->setState(KLed::State::On);
	else
		ui.targetLED->setState(KLed::State::Off);

	if (persistentLED)
	{
		ui.persistentLED->setState(KLed::State::On);
		model430.persistentState = true;
	}
	else
	{
		ui.persistentLED->setState(KLed::State::Off);
		model430.persistentState = false;
	}

	if (energizedLED)
		ui.energizedLED->setState(KLed::State::On);
	else
		ui.energizedLED->setState(KLed::State::Off);

	if (quenchLED)
		ui.quenchLED->setState(KLed::State::On);
	else
		ui.quenchLED->setState(KLed::State::Off);

	// set heater switch state if prior to firmware that supports heater state
	if (!supports_AMITRG())
	{
		if (displayString.contains("PSwitch Heater: ON"))
			model430.switchHeaterState = true;
		else if (displayString.contains("PSwitch Heater: OFF"))
			model430.switchHeaterState = false;
	}
}

//---------------------------------------------------------------------------
// Error and status message handling.
//---------------------------------------------------------------------------
void magnetdaq::systemErrorNotification(void)
{
	QApplication::beep();

	// try to get explanation of remote errors
	if (socket)
		socket->sendQuery("SYST:ERR?\r\n", SYSTEM_ERROR);
}

//---------------------------------------------------------------------------
void magnetdaq::displaySystemError(QString errMsg, QString errorSourceStr)
{
	if (!errMsg.contains("No errors"))
	{
		statusError->setStyleSheet("color: red; font: bold");

		// save device errors in error stacks, device errors have nullptr errorSourceStr
		if (errorSourceStr == nullptr)
		{
			// send all device errors to parser
			if (parser != nullptr)
				parser->addSystemError(errMsg.remove("\r\n"));

			// also save to device error stack
			if (!errMsg.isEmpty())
				errorStack.push(errMsg.remove("\r\n"));

			if (errorstackDlg)
			{
				if (errorstackDlg->isVisible())
					errorstackDlg->loadErrorHistory(&errorStack);
			}

			statusError->setText(errMsg.remove("\r\n"));
			qDebug() << errMsg.remove("\r\n");	// send to log

			postErrorRefresh();	// refresh currently displayed values, erasing any erroneous entry
			QTimer::singleShot(3000, this, SLOT(clearErrorDisplay()));
		}
		else
		{
			statusMisc->setStyleSheet("color: red; font: bold");
			statusMisc->setText(errMsg.remove("\r\n") + "::" + errorSourceStr.remove("\r\n"));
			qDebug() << errMsg.remove("\r\n") << "::" << errorSourceStr.remove("\r\n");	// send to log

			postErrorRefresh();	// refresh currently displayed values, erasing any erroneous entry
			QTimer::singleShot(3000, this, SLOT(clearMiscDisplay()));
		}
	}
}

//---------------------------------------------------------------------------
void magnetdaq::clearErrorDisplay(void)
{
	statusError->clear();
}

//---------------------------------------------------------------------------
void magnetdaq::clearErrorHistory(void)
{
	errorStack.clear();
}

//---------------------------------------------------------------------------
void magnetdaq::clearMiscDisplay(void)
{
	statusMisc->setStyleSheet("");
	statusMisc->clear();
}

//---------------------------------------------------------------------------
// External rampdown handling.
//---------------------------------------------------------------------------
void magnetdaq::startExternalRampdown(void)
{
	// changing parameters is not allowed during rampdown cycle
	ui.rampdownTab->setEnabled(false);
	ui.rampRateTab->setEnabled(false);

	statusMisc->setStyleSheet("color: red; font: bold");
	statusMisc->setText("External Rampdown is active. No input is allowed until completion.");
}

//---------------------------------------------------------------------------
void magnetdaq::endExternalRampdown(void)
{
	ui.rampdownTab->setEnabled(true);
	ui.rampRateTab->setEnabled(true);

	clearMiscDisplay();
}

//---------------------------------------------------------------------------
// Control and simulated keypad button actions.
//---------------------------------------------------------------------------
void magnetdaq::persistentSwitchButtonClicked(void)
{
	if (telnet)
		telnet->sendCommand("W_KEY_PSWITCH\r\n");
}

//---------------------------------------------------------------------------
void magnetdaq::targetSetpointButtonClicked(void)
{
	if (telnet)
		telnet->sendCommand("W_KEY_TARGET\r\n");
}

//---------------------------------------------------------------------------
void magnetdaq::rampPauseButtonClicked(void)
{
	if (telnet)
		telnet->sendCommand("W_KEY_RAMPPAUSE\r\n");
}

//---------------------------------------------------------------------------
void magnetdaq::keypadClicked(bool checked)
{
	if (telnet)
	{
		QToolButton* button = qobject_cast<QToolButton*>(sender());

		// use "hidden" button text to determine which key was pressed
		if (button->text() == "1")
			telnet->sendCommand("W_KEY_1\r\n");
		else if (button->text() == "2")
			telnet->sendCommand("W_KEY_2\r\n");
		else if (button->text() == "3")
			telnet->sendCommand("W_KEY_3\r\n");
		else if (button->text() == "4")
		{
			telnet->sendCommand("W_KEY_4\r\n");

			if (ui.shiftLED->state())
			{
				// abort any automatic table functions
				abortTableTarget();
			}
		}
		else if (button->text() == "5")
			telnet->sendCommand("W_KEY_5\r\n");
		else if (button->text() == "6")
		{
			telnet->sendCommand("W_KEY_6\r\n");

			if (ui.shiftLED->state())
			{
				// abort any automatic table functions
				abortTableTarget();
			}
		}
		else if (button->text() == "7")
			telnet->sendCommand("W_KEY_7\r\n");
		else if (button->text() == "8")
			telnet->sendCommand("W_KEY_8\r\n");
		else if (button->text() == "9")
			telnet->sendCommand("W_KEY_9\r\n");
		else if (button->text() == "0")
			telnet->sendCommand("W_KEY_0\r\n");
		else if (button->text() == ".")
			telnet->sendCommand("W_KEY_PERIOD\r\n");
		else if (button->text() == "+")
			telnet->sendCommand("W_KEY_PLUSMINUS\r\n");
		else if (button->text() == "shift")
			telnet->sendCommand("W_KEY_SHIFT\r\n");
		else if (button->text() == "zero")
		{
			telnet->sendCommand("W_KEY_ZEROFIELD\r\n");

			// abort any automatic table functions
			abortTableTarget();
		}
		else if (button->text() == "esc")
			telnet->sendCommand("W_KEY_ESC\r\n");
		else if (button->text() == "menu")
			telnet->sendCommand("W_KEY_MENU\r\n");
		else if (button->text() == "enter")
			telnet->sendCommand("W_KEY_ENTER\r\n");
		else if (button->text() == "right")
			telnet->sendCommand("W_KEY_RIGHT\r\n");
		else if (button->text() == "left")
			telnet->sendCommand("W_KEY_LEFT\r\n");
	}
}

//---------------------------------------------------------------------------
//	Miscellaneous convenience functions
//---------------------------------------------------------------------------
void magnetdaq::setupRampRateArrays(void)
{
	// create convenience arrays for ramp rate values
	rampSegLabels[0] = ui.rampSegLabel_1;
	rampSegLabels[1] = ui.rampSegLabel_2;
	rampSegLabels[2] = ui.rampSegLabel_3;
	rampSegLabels[3] = ui.rampSegLabel_4;
	rampSegLabels[4] = ui.rampSegLabel_5;
	rampSegLabels[5] = ui.rampSegLabel_6;
	rampSegLabels[6] = ui.rampSegLabel_7;
	rampSegLabels[7] = ui.rampSegLabel_8;
	rampSegLabels[8] = ui.rampSegLabel_9;
	rampSegLabels[9] = ui.rampSegLabel_10;
	rampSegMinLimits[0] = ui.rampSegMinLabel_1;
	rampSegMinLimits[1] = ui.rampSegMinLabel_2;
	rampSegMinLimits[2] = ui.rampSegMinLabel_3;
	rampSegMinLimits[3] = ui.rampSegMinLabel_4;
	rampSegMinLimits[4] = ui.rampSegMinLabel_5;
	rampSegMinLimits[5] = ui.rampSegMinLabel_6;
	rampSegMinLimits[6] = ui.rampSegMinLabel_7;
	rampSegMinLimits[7] = ui.rampSegMinLabel_8;
	rampSegMinLimits[8] = ui.rampSegMinLabel_9;
	rampSegMinLimits[9] = ui.rampSegMinLabel_10;
	rampSegMaxLimits[0] = ui.rampSegMaxEdit_1;
	rampSegMaxLimits[1] = ui.rampSegMaxEdit_2;
	rampSegMaxLimits[2] = ui.rampSegMaxEdit_3;
	rampSegMaxLimits[3] = ui.rampSegMaxEdit_4;
	rampSegMaxLimits[4] = ui.rampSegMaxEdit_5;
	rampSegMaxLimits[5] = ui.rampSegMaxEdit_6;
	rampSegMaxLimits[6] = ui.rampSegMaxEdit_7;
	rampSegMaxLimits[7] = ui.rampSegMaxEdit_8;
	rampSegMaxLimits[8] = ui.rampSegMaxEdit_9;
	rampSegMaxLimits[9] = ui.rampSegMaxEdit_10;
	rampSegValues[0] = ui.rampSegRate_1;
	rampSegValues[1] = ui.rampSegRate_2;
	rampSegValues[2] = ui.rampSegRate_3;
	rampSegValues[3] = ui.rampSegRate_4;
	rampSegValues[4] = ui.rampSegRate_5;
	rampSegValues[5] = ui.rampSegRate_6;
	rampSegValues[6] = ui.rampSegRate_7;
	rampSegValues[7] = ui.rampSegRate_8;
	rampSegValues[8] = ui.rampSegRate_9;
	rampSegValues[9] = ui.rampSegRate_10;

	for (int i = 0; i < 10; i++)
	{
		connect(rampSegMaxLimits[i], SIGNAL(editingFinished()), this, SLOT(rampSegmentValueChanged()));
		connect(rampSegValues[i], SIGNAL(editingFinished()), this, SLOT(rampSegmentValueChanged()));
	}
}

//---------------------------------------------------------------------------
void magnetdaq::setupRampdownArrays(void)
{
	// create convenience arrays for rampdown values
	rampdownSegLabels[0] = ui.rampdownSegLabel_1;
	rampdownSegLabels[1] = ui.rampdownSegLabel_2;
	rampdownSegLabels[2] = ui.rampdownSegLabel_3;
	rampdownSegLabels[3] = ui.rampdownSegLabel_4;
	rampdownSegLabels[4] = ui.rampdownSegLabel_5;
	rampdownSegLabels[5] = ui.rampdownSegLabel_6;
	rampdownSegLabels[6] = ui.rampdownSegLabel_7;
	rampdownSegLabels[7] = ui.rampdownSegLabel_8;
	rampdownSegLabels[8] = ui.rampdownSegLabel_9;
	rampdownSegLabels[9] = ui.rampdownSegLabel_10;
	rampdownSegMinLimits[0] = ui.rampdownSegMinLabel_1;
	rampdownSegMinLimits[1] = ui.rampdownSegMinLabel_2;
	rampdownSegMinLimits[2] = ui.rampdownSegMinLabel_3;
	rampdownSegMinLimits[3] = ui.rampdownSegMinLabel_4;
	rampdownSegMinLimits[4] = ui.rampdownSegMinLabel_5;
	rampdownSegMinLimits[5] = ui.rampdownSegMinLabel_6;
	rampdownSegMinLimits[6] = ui.rampdownSegMinLabel_7;
	rampdownSegMinLimits[7] = ui.rampdownSegMinLabel_8;
	rampdownSegMinLimits[8] = ui.rampdownSegMinLabel_9;
	rampdownSegMinLimits[9] = ui.rampdownSegMinLabel_10;
	rampdownSegMaxLimits[0] = ui.rampdownSegMaxEdit_1;
	rampdownSegMaxLimits[1] = ui.rampdownSegMaxEdit_2;
	rampdownSegMaxLimits[2] = ui.rampdownSegMaxEdit_3;
	rampdownSegMaxLimits[3] = ui.rampdownSegMaxEdit_4;
	rampdownSegMaxLimits[4] = ui.rampdownSegMaxEdit_5;
	rampdownSegMaxLimits[5] = ui.rampdownSegMaxEdit_6;
	rampdownSegMaxLimits[6] = ui.rampdownSegMaxEdit_7;
	rampdownSegMaxLimits[7] = ui.rampdownSegMaxEdit_8;
	rampdownSegMaxLimits[8] = ui.rampdownSegMaxEdit_9;
	rampdownSegMaxLimits[9] = ui.rampdownSegMaxEdit_10;
	rampdownSegValues[0] = ui.rampdownSegRate_1;
	rampdownSegValues[1] = ui.rampdownSegRate_2;
	rampdownSegValues[2] = ui.rampdownSegRate_3;
	rampdownSegValues[3] = ui.rampdownSegRate_4;
	rampdownSegValues[4] = ui.rampdownSegRate_5;
	rampdownSegValues[5] = ui.rampdownSegRate_6;
	rampdownSegValues[6] = ui.rampdownSegRate_7;
	rampdownSegValues[7] = ui.rampdownSegRate_8;
	rampdownSegValues[8] = ui.rampdownSegRate_9;
	rampdownSegValues[9] = ui.rampdownSegRate_10;

	for (int i = 0; i < 10; i++)
	{
		connect(rampdownSegMaxLimits[i], SIGNAL(editingFinished()), this, SLOT(rampdownSegmentValueChanged()));
		connect(rampdownSegValues[i], SIGNAL(editingFinished()), this, SLOT(rampdownSegmentValueChanged()));
	}
}

//---------------------------------------------------------------------------
void magnetdaq::shortSampleModeChanged(bool isSampleMode)
{
	// reconfigure UI for sample mode
	if (isSampleMode)
	{
		model430.switchInstalled = false;

		// set table headings as appropriate
		setTableHeader();

		if (tableUnits != AMPS)
			tableClear();	// can't use field values with short-sample mode

		// set several items as invisible that do not apply
		ui.targetFieldSetpointButton->setText("Target Current Setpoint");
		ui.rampdownTabSplitter->setVisible(false);
		ui.voltageModeInputLabel->setText("V-C Mode Input Range (V) :");
		ui.switchPage->setVisible(false);
		ui.stabilityModeLabel->setVisible(false);
		ui.stabilityModeComboBox->setVisible(false);
		ui.coilConstantEdit->setVisible(false);
		ui.coilConstantLabel->setVisible(false);
		ui.inductanceButton->setVisible(false);
		ui.magInductanceEdit->setVisible(false);
		ui.magInductanceLabel->setVisible(false);
		ui.absorberComboBox->setVisible(false);
		ui.absorberLabel->setVisible(false);
		ui.quenchSensitivityComboBox->setVisible(false);
		ui.quenchSensitivityLabel->setVisible(false);
		ui.externRampdownComboBox->setVisible(false);
		ui.externRampdownLabel->setVisible(false);
		ui.opcGroupBox->setVisible(false);
		ui.rampPlotWidget->setVisible(false);
		ui.voltageLimitEdit->setVisible(false);
		ui.voltageLimitLabel->setVisible(false);
		ui.rampUnitsComboBox->setCurrentIndex(0);
		ui.rampUnitsComboBox->setVisible(false);
		ui.rampUnitsLabel->setVisible(false);
		ui.magnetCurrentRadioButton->setText("Sample Current (A)");
		ui.supplyVoltageCheckBox->setText("Program Out (V)");
		ui.magnetVoltageCheckBox->setText(ssSampleVoltageText);
		ui.magnetFieldRadioButton->setVisible(false);

		// restore legend names
		// Note: Should not have "axisStr" for multi-axis control in current mode
		QSettings settings;
		mainLegend[MAGNET_CURRENT_GRAPH] = settings.value("ssGraph/Legend0", "Sample Current").toString();
		mainLegend[MAGNET_FIELD_GRAPH] = settings.value("ssGraph/Legend1", "Magnet Field (N/A)").toString();
		mainLegend[SUPPLY_CURRENT_GRAPH] = settings.value("ssGraph/Legend2", "Supply Current").toString();
		mainLegend[MAGNET_VOLTAGE_GRAPH] = settings.value("ssGraph/Legend3", "Sample Voltage").toString();
		mainLegend[SUPPLY_VOLTAGE_GRAPH] = settings.value("ssGraph/Legend4", "Program Out").toString();

		mainPlotTitle = settings.value("ssGraph/Title", "Sample Current/Voltage vs. Time").toString();

		ui.plotWidget->graph(MAGNET_CURRENT_GRAPH)->setName(mainLegend[MAGNET_CURRENT_GRAPH]);
		ui.plotWidget->graph(MAGNET_FIELD_GRAPH)->setName(mainLegend[MAGNET_FIELD_GRAPH]);
		ui.plotWidget->graph(SUPPLY_CURRENT_GRAPH)->setName(mainLegend[SUPPLY_CURRENT_GRAPH]);
		ui.plotWidget->graph(MAGNET_VOLTAGE_GRAPH)->setName(mainLegend[MAGNET_VOLTAGE_GRAPH]);
		ui.plotWidget->graph(SUPPLY_VOLTAGE_GRAPH)->setName(mainLegend[SUPPLY_VOLTAGE_GRAPH]);
		ssPlotTitle->setText(mainPlotTitle);
	}
	else
	{
		ui.targetFieldSetpointButton->setText("Target Field Setpoint");
		ui.rampdownTabSplitter->setVisible(true);
		ui.voltageModeInputLabel->setText("V-V Mode Input Range (V) :");
		ui.switchPage->setVisible(true);
		ui.stabilityModeLabel->setVisible(true);
		ui.stabilityModeComboBox->setVisible(true);
		ui.coilConstantEdit->setVisible(true);
		ui.coilConstantLabel->setVisible(true);
		ui.inductanceButton->setVisible(true);
		ui.magInductanceEdit->setVisible(true);
		ui.magInductanceLabel->setVisible(true);
		ui.absorberComboBox->setVisible(true);
		ui.absorberLabel->setVisible(true);
		ui.quenchSensitivityComboBox->setVisible(true);
		ui.quenchSensitivityLabel->setVisible(true);
		ui.externRampdownComboBox->setVisible(true);
		ui.externRampdownLabel->setVisible(true);
		ui.opcGroupBox->setVisible(true);
		ui.rampPlotWidget->setVisible(true);
		ui.voltageLimitEdit->setVisible(true);
		ui.voltageLimitLabel->setVisible(true);
		ui.rampUnitsComboBox->setVisible(true);
		ui.rampUnitsLabel->setVisible(true);
		ui.magnetCurrentRadioButton->setText("Magnet Current (A)");
		ui.magnetVoltageCheckBox->setText("Magnet Voltage (V)");
		ui.supplyVoltageCheckBox->setText("Supply Voltage (V)");
		ui.magnetFieldRadioButton->setVisible(true);

		// restore legend names
		QSettings settings;
		mainLegend[MAGNET_CURRENT_GRAPH] = settings.value(axisStr + "Graph/Legend0", "Magnet Current").toString();
		mainLegend[MAGNET_FIELD_GRAPH] = settings.value(axisStr + "Graph/Legend1", "Magnet Field").toString();
		mainLegend[SUPPLY_CURRENT_GRAPH] = settings.value(axisStr + "Graph/Legend2", "Supply Current").toString();
		mainLegend[MAGNET_VOLTAGE_GRAPH] = settings.value(axisStr + "Graph/Legend3", "Magnet Voltage").toString();
		mainLegend[SUPPLY_VOLTAGE_GRAPH] = settings.value(axisStr + "Graph/Legend4", "Supply Voltage").toString();

		mainPlotTitle = settings.value(axisStr + "Graph/Title", "Magnet Current/Voltage vs. Time").toString();

		ui.plotWidget->graph(MAGNET_CURRENT_GRAPH)->setName(mainLegend[MAGNET_CURRENT_GRAPH]);
		ui.plotWidget->graph(MAGNET_FIELD_GRAPH)->setName(mainLegend[MAGNET_FIELD_GRAPH]);
		ui.plotWidget->graph(SUPPLY_CURRENT_GRAPH)->setName(mainLegend[SUPPLY_CURRENT_GRAPH]);
		ui.plotWidget->graph(MAGNET_VOLTAGE_GRAPH)->setName(mainLegend[MAGNET_VOLTAGE_GRAPH]);
		ui.plotWidget->graph(SUPPLY_VOLTAGE_GRAPH)->setName(mainLegend[SUPPLY_VOLTAGE_GRAPH]);
		ssPlotTitle->setText(mainPlotTitle);
	}
}

//---------------------------------------------------------------------------

//---------------------------------------------------------------------------

#include "stdafx.h"
#include "magnetdaq.h"
#include "qled.h"
#include <QtConcurrent>
#include "aboutdialog.h"

//---------------------------------------------------------------------------
// Constructor
//---------------------------------------------------------------------------
magnetdaq::magnetdaq(QWidget *parent)
	: QMainWindow(parent)
{
	QCoreApplication::setOrganizationName("American Magnetics Inc.");
	QCoreApplication::setApplicationName("MagnetDAQ");
	QCoreApplication::setApplicationVersion("0.98");
	QCoreApplication::setOrganizationDomain("AmericanMagnetics.com");

	// add application-specific fonts
	QFontDatabase::addApplicationFont(":/magnetdaq/Resources/BFN____.otf");	// family: Bubbledot Fine Negative
	QFontDatabase::addApplicationFont(":/magnetdaq/Resources/BFP____.otf");	// family: Bubbledot Fine Positive
#if !defined(Q_OS_MACOS)
	QFontDatabase::addApplicationFont(":/magnetdaq/Resources/WINGDNG3.TTF");	// family: Wingdings 3
#endif

	ui.setupUi(this);

	// default is hidden docked widgets
	ui.setupDockWidget->hide();
	ui.plotDockWidget->hide();
	ui.keypadDockWidget->hide();

	this->setWindowTitle(QCoreApplication::applicationName() + " - v" + QCoreApplication::applicationVersion() + "   (AMI Model 430 Remote Control)");

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

	// remove unsightly frame outlines
	ui.keypadFrame->setStyleSheet("border: 0;");

#else	// Windows assumed
	QGuiApplication::setFont(QFont("Segoe UI", 9));
	this->setStyleSheet("QMainWindow::separator {background: rgb(200, 200, 200); width: 1px; height: 1px;} QTabBar {font-size: 8pt};");
	ui.mainTabWidget->tabBar()->setStyleSheet("font-size: 9pt");
#endif

	// restore window position and gui state
	QSettings settings;

	// restore different geometry for different DPI screens
	QString dpiStr = QString::number(QApplication::desktop()->screen()->logicalDpiX());
	restoreGeometry(settings.value("MainWindow/Geometry/" + dpiStr).toByteArray());
	restoreState(settings.value("MainWindow/WindowState").toByteArray());
	ui.rampRateTabSplitter->restoreGeometry(settings.value("RampSplitter/Geometry/" + dpiStr).toByteArray());
	ui.rampRateTabSplitter->restoreState(settings.value("RampSplitter/WindowState").toByteArray());
	ui.rampdownTabSplitter->restoreGeometry(settings.value("RampdownSplitter/Geometry/" + dpiStr).toByteArray());
	ui.rampdownTabSplitter->restoreState(settings.value("RampdownSplitter/WindowState").toByteArray());
	ui.ipAddressEdit->setText(settings.value("IPAddr", "").toString());
	ui.ipNameEdit->setText(settings.value("IPName", "").toString());
	ui.logFileEdit->setText(settings.value("Logfile", "").toString());
	ui.remoteLockoutCheckBox->setChecked(settings.value("RemoteLockout", false).toBool());

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
	ui.actionRun->setEnabled(true);
	ui.actionStop->setEnabled(false);

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
	connect(ui.actionShow_Keypad, SIGNAL(triggered()), this, SLOT(actionShow_Keypad()));
	connect(ui.actionPlot_Settings, SIGNAL(triggered()), this, SLOT(actionPlot_Settings()));

	// restore device list in Setup
	restoreDeviceList(&settings);

	// create plotTimer
	plotTimer = new QTimer(this);

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
	// For Linux and macOS, setting the plotTimer data collection rate too high results 
	// in a lagging user interface. On the Mac, the display can go long periods
	// without a refresh. Limit the max update rate here to keep the interface responsive.
	plotTimer->setInterval(200);	// 5 updates per second max rate on Linux/macOS
#else
	// Windows seems to give preference to the user interface updates at the
	// expense of slowing the plotTimer data collection rate, so we set the update 
	// rate to a higher value and let the interface dictate the actual achieved rate.
	plotTimer->setInterval(100);	// 10 updates per second max rate on Windows
#endif
	
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
	statusError = new QLabel("", this);
	statusError->setFrameStyle(QFrame::Panel | QFrame::Sunken);
	statusError->setToolTip("Error messages");
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
	QString dpiStr = QString::number(QApplication::desktop()->screen()->logicalDpiX());

	settings.setValue("MainWindow/Geometry/" + dpiStr, saveGeometry());
	settings.setValue("MainWindow/WindowState", saveState());
	settings.setValue("RampSplitter/Geometry/" + dpiStr, ui.rampRateTabSplitter->saveGeometry());
	settings.setValue("RampSplitter/WindowState", ui.rampRateTabSplitter->saveState());
	settings.setValue("RampdownSplitter/Geometry/" + dpiStr, ui.rampdownTabSplitter->saveGeometry());
	settings.setValue("RampdownSplitter/WindowState", ui.rampdownTabSplitter->saveState());
	settings.setValue("DeviceList/HorizontalState", ui.devicesTableWidget->horizontalHeader()->saveState());
	settings.setValue("DeviceList/HorizontalGeometry/" + dpiStr, ui.devicesTableWidget->horizontalHeader()->saveGeometry());
	settings.setValue("IPAddr", ui.ipAddressEdit->text());
	settings.setValue("IPName", ui.ipNameEdit->text());
	settings.setValue("Graph/Xmin", ui.xminEdit->text());
	settings.setValue("Graph/Xmax", ui.xmaxEdit->text());
	settings.setValue("Graph/Ymin", ui.yminEdit->text());
	settings.setValue("Graph/Ymax", ui.ymaxEdit->text());
	settings.setValue("Graph/Vmin", ui.vminEdit->text());
	settings.setValue("Graph/Vmax", ui.vmaxEdit->text());
	settings.setValue("Logfile", ui.logFileEdit->text());
	settings.setValue("RemoteLockout", ui.remoteLockoutCheckBox->isChecked());
	settings.setValue("Graph/UseSeconds", ui.secondsRadioButton->isChecked());
	settings.setValue("Graph/AutoscrollX", ui.autoscrollXCheckBox->isChecked());

	// save titles (minus units)
	settings.setValue("Graph/Title", mainPlotTitle);
	settings.setValue("Graph/TitleX", mainPlotXTitle);
	settings.setValue("Graph/TitleYCurrent", mainPlotYTitleCurrent);
	settings.setValue("Graph/TitleYField", mainPlotYTitleField);
	settings.setValue("Graph/TitleY2", mainPlotY2Title);
	settings.setValue("Graph/Legend0", mainLegend[0]);
	settings.setValue("Graph/Legend1", mainLegend[1]);
	settings.setValue("Graph/Legend2", mainLegend[2]);
	settings.setValue("Graph/Legend3", mainLegend[3]);
	settings.setValue("Graph/Legend4", mainLegend[4]);

	// save main plot settings
	settings.setValue("Graph/PlotField", ui.magnetFieldRadioButton->isChecked());
	settings.setValue("Graph/PlotMagnetVoltage", ui.magnetVoltageCheckBox->isChecked());
	settings.setValue("Graph/PlotSupplyCurrent", ui.supplyCurrentCheckBox->isChecked());
	settings.setValue("Graph/PlotSupplyVoltage", ui.supplyVoltageCheckBox->isChecked());

	// save support settings
	settings.setValue("Support/Subject", ui.caseEdit->text());
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

	// Display the dialog and start the event loop.
	progressDialog.show();
	progressDialog.setWindowModality(Qt::WindowModal);
	progressDialog.setValue(0);
	QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

	// use the port 7180 socket to collect high-speed data queries
	socket = new Socket(&model430, this);
	socket->connectToModel430(ui.ipAddressEdit->text(), 7180);

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

		// requires firmware version 2.51 or later, or 3.01 or later
		if (checkFirmwareVersion())
		{
			// connect the socket data ready signal to the plot
			connect(socket, SIGNAL(nextDataPoint(qint64, double, double, double, double, double)), this, SLOT(addDataPoint(qint64, double, double, double, double, double)));

			// connect error signals
			connect(socket, SIGNAL(model430Disconnected()), this, SLOT(actionStop()));
			connect(socket, SIGNAL(systemErrorMessage(QString)), this, SLOT(displaySystemError(QString)), Qt::ConnectionType::QueuedConnection);

			// query 430 mode (s2 state)
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
			progressDialog.setValue(20);
			if (progressDialog.wasCanceled())
				actionStop();
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

			model430.syncLoadSetup();
			progressDialog.setValue(40);
			if (progressDialog.wasCanceled())
				actionStop();
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

			model430.syncSwitchSetup();
			progressDialog.setValue(60);
			if (progressDialog.wasCanceled())
				actionStop();
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

			model430.syncProtectionSetup();
			progressDialog.setValue(80);
			if (progressDialog.wasCanceled())
				actionStop();
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

			model430.syncEventCounts();
			progressDialog.setValue(85);
			if (progressDialog.wasCanceled())
				actionStop();
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

			model430.syncRampRates();
			progressDialog.setValue(100);
			if (progressDialog.wasCanceled())
				actionStop();

			// use the port 23 telnet socket for configuration and keypad simulation
			telnet = new Socket(&model430, this);
			telnet->connectToModel430(ui.ipAddressEdit->text(), 23);	// capture display/keypad broadcasts

			if (telnet->isConnected())
			{
				// connect the socket data ready signal to the front panel display
				connect(telnet, SIGNAL(updateFrontPanel(QString, bool, bool, bool, bool, bool)), this,
					SLOT(updateFrontPanel(QString, bool, bool, bool, bool, bool)), Qt::ConnectionType::QueuedConnection);

				// connect error signal
				connect(telnet, SIGNAL(systemError()), this, SLOT(systemErrorNotification()));

				// connect change notifications
				connect(telnet, SIGNAL(fieldUnitsChanged()), &model430, SLOT(syncFieldUnits()));
				connect(telnet, SIGNAL(startExternalRampdown()), this, SLOT(startExternalRampdown()));
				connect(telnet, SIGNAL(endExternalRampdown()), this, SLOT(endExternalRampdown()));

				// all good, finish initializing interface
				ui.actionRun->setEnabled(false);
				ui.actionStop->setEnabled(true);
				ui.ipAddressEdit->setEnabled(false);
				ui.ipAddressLabel->setEnabled(false);
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
			}
			else
			{
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
			// firmware too old, offer upgrade
			progressDialog.close();
			socket->deleteLater();
			socket = NULL;
			telnet = NULL;

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

					// firmware versions older than v1.60 may use non-compatible kernel
					if (model430.firmwareVersion() >= 1.60)
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
		socket->deleteLater();
		socket = NULL;
		telnet = NULL;
	}
}

//---------------------------------------------------------------------------
void magnetdaq::actionStop(void)
{
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
	ui.ipAddressEdit->setEnabled(true);
	ui.ipAddressLabel->setEnabled(true);
	ui.logFileEdit->setEnabled(true);
	ui.logFileLabel->setEnabled(true);
	ui.logfileButton->setEnabled(true);
	ui.secondsRadioButton->setEnabled(true);
	ui.minutesRadioButton->setEnabled(true);
	ui.devicesTableWidget->setEnabled(true);

	// re-enable the delete button if there is a selection
	if (ui.devicesTableWidget->selectionModel()->selectedRows().count())
		ui.deleteDeviceButton->setEnabled(true);

	statusConnectState->setStyleSheet("color: red; font: bold");
	statusConnectState->setText("Disconnected");
	statusSampleRate->clear();
}

//---------------------------------------------------------------------------
void magnetdaq::actionSetup(void)
{
	// initially open into a docked tab item
	if (ui.plotDockWidget->isVisible())
		QMainWindow::tabifyDockWidget(ui.setupDockWidget, ui.plotDockWidget);
	else if (ui.keypadDockWidget->isVisible())
		QMainWindow::tabifyDockWidget(ui.setupDockWidget, ui.keypadDockWidget);

	ui.setupDockWidget->show();
	ui.setupDockWidget->raise();
}

//---------------------------------------------------------------------------
void magnetdaq::actionPlot_Settings(void)
{
	// initially open into a docked tab item
	if (ui.setupDockWidget->isVisible())
		QMainWindow::tabifyDockWidget(ui.setupDockWidget, ui.plotDockWidget);
	else if (ui.keypadDockWidget->isVisible())
		QMainWindow::tabifyDockWidget(ui.plotDockWidget, ui.keypadDockWidget);

	ui.plotDockWidget->show();
	ui.plotDockWidget->raise();
}

//---------------------------------------------------------------------------
void magnetdaq::actionShow_Keypad(void)
{
	// initially open into a docked tab item
	if (ui.setupDockWidget->isVisible())
		QMainWindow::tabifyDockWidget(ui.setupDockWidget, ui.keypadDockWidget);
	else if (ui.plotDockWidget->isVisible())
		QMainWindow::tabifyDockWidget(ui.plotDockWidget, ui.keypadDockWidget);

	ui.keypadDockWidget->show();
	ui.keypadDockWidget->raise();
}

//---------------------------------------------------------------------------
void magnetdaq::actionPrint(void)
{
	// print main plot
	QPrinter printer;
	QPrintPreviewDialog previewDialog(&printer, this);

	if (ui.mainTabWidget->currentIndex() == PLOT_TAB)
		connect(&previewDialog, SIGNAL(paintRequested(QPrinter*)), SLOT(renderPlot(QPrinter*)));
	else if (ui.mainTabWidget->currentIndex() == RAMP_TAB)
		connect(&previewDialog, SIGNAL(paintRequested(QPrinter*)), SLOT(renderRampPlot(QPrinter*)));
	else if (ui.mainTabWidget->currentIndex() == RAMPDOWN_TAB)
		connect(&previewDialog, SIGNAL(paintRequested(QPrinter*)), SLOT(renderRampdownPlot(QPrinter*)));
	previewDialog.exec();
}

//---------------------------------------------------------------------------
void magnetdaq::actionHelp(void)
{

}

//---------------------------------------------------------------------------
void magnetdaq::actionAbout(void)
{
	aboutdialog about;
	about.exec();
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
	if (socket)
	{
		this->setWindowTitle(QCoreApplication::applicationName() + " - v" + QCoreApplication::applicationVersion() + "      Connected to: " +
			ui.ipNameEdit->text() + " @ " + ui.ipAddressEdit->text());
	}
	else
	{
		this->setWindowTitle(QCoreApplication::applicationName() + " - v" + QCoreApplication::applicationVersion() + "      " +
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
}

//---------------------------------------------------------------------------
void magnetdaq::updateFrontPanel(QString displayString, bool shiftLED, bool fieldLED, bool persistentLED, bool energizedLED, bool quenchLED)
{
	// first format the displayString for HTML display
	QString htmlDisplay = "<!DOCTYPE HTML PUBLIC \" -//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">\n";
	
	htmlDisplay += "<html><head><meta name =\"qrichtext\" content=\"1\"/><style type=\"text/css\">p, li{white-space:pre-wrap;}\n";

	// font scaling, 14pt is the base font size at 120 dpi
	qreal dpi = QApplication::desktop()->screen()->logicalDpiX();
	double font_size = 120.0 / dpi * 14.0;

	htmlDisplay += "</style></head><body style =\"font-family:'Bubbledot Fine Positive'; font-size:" + QString::number(font_size, 'f', 1) + "pt; font-style:normal; \"bgcolor=\"#000000\">\n";

	htmlDisplay += "<p style =\"color:#a2d1ff;\">";

	// *********************************************
	// HTML code replacements for special characters

	// replace question mark
	displayString.replace("?", "&#x3F;");

	// replace less than symbol
	displayString.replace(60, "&lt;");

	// replace greater than symbol
	displayString.replace(63, "&gt;");

	// replace linefeed and special characters
	displayString.replace("\n", "<br/>");	// linefeed

	// plus/minus symbol
	displayString.replace(241, "&plusmn;");

	// DEGREE address: 0x86
	displayString.replace(0x86, "&deg;");

	// *********************************************
	// Platform/font specific code replacements for special characters

#if defined(Q_OS_MACOS)	// Mac specific, uses the "Edit->Emoticons and Symbols" definitions for MacOS
	font_size = 120.0 / dpi * 12.0;

	// reverse "P" address: 0x80
	displayString.replace(0x80, "<span style = \"font-family:'Bubbledot Fine Negative';font-size:" + QString::number(font_size, 'f', 1) + "pt;\">P</span>");

	// "UP" address: 0x81
	displayString.replace(0x81, "<span style=\"font-size:" + QString::number(font_size, 'f', 1) + "pt;\">" + QString("\u2191") + "</span>");

	// "DOWN" address: 0x82
	displayString.replace(0x82, "<span style=\"font-size:" + QString::number(font_size, 'f', 1) + "pt;\">" + QString("\u2193") + "</span>");

	// "->" address: 0x83
	displayString.replace(0x83, "<span style=\"font-size:" + QString::number(font_size, 'f', 1) + "pt;\">" + QString("\u261B") + "</span>");

	// "<-" address: 0x84
	displayString.replace(0x84, "<span style=\"font-size:" + QString::number(font_size, 'f', 1) + "pt;\">" + QString("\u261A") + "</span>");

	// MENU pointer address: 0x85
	displayString.replace(0x85, "<span style=\"font-size:" + QString::number(font_size, 'f', 1) + "pt;\">" + QString("\u25BA") + "</span>");

	// ENCODER IN USE address: 0x87
	displayString.replace(0x87, "<span style=\"font-size:" + QString::number(font_size, 'f', 1) + "pt;\">" + QString("\u21D5") + "</span>");

	// reverse "V" address: 0x88
	displayString.replace(0x88, "<span style=\"font-family:'Bubbledot Fine Negative';\">V</span>");

	// inverse "C" address: 0x89
	displayString.replace(0x89, "<span style=\"font-family:'Bubbledot Fine Negative';\">C</span>");

	// inverse "T" address: 0x8A
	displayString.replace(0x8A, "<span style=\"font-family:'Bubbledot Fine Negative';\">T</span>");

#else	// Windows and Linux
	font_size = 120.0 / dpi * 13.0;	// slightly smaller

	// reverse "P" address: 0x80
	displayString.replace(0x80, "<span style = \"font-family:'Bubbledot Fine Negative'; font-size:" + QString::number(font_size, 'f', 1) + "pt;\">P</span>");

	// "UP" address: 0x81
	displayString.replace(0x81, "<span style=\"font-family:'Wingdings 3'; font-size:" + QString::number(font_size, 'f', 1) + "pt;\">h</span>");

	// "DOWN" address: 0x82
	displayString.replace(0x82, "<span style=\"font-family:'Wingdings 3'; font-size:" + QString::number(font_size, 'f', 1) + "pt;\">i</span>");

	// reverse "V" address: 0x88
	displayString.replace(0x88, "<span style=\"font-family:'Bubbledot Fine Negative'; font-size:" + QString::number(font_size, 'f', 1) + "pt;\">V</span>");

	// inverse "C" address: 0x89
	displayString.replace(0x89, "<span style=\"font-family:'Bubbledot Fine Negative'; font-size:" + QString::number(font_size, 'f', 1) + "pt;\">C</span>");

	// inverse "T" address: 0x8A
	displayString.replace(0x8A, "<span style=\"font-family:'Bubbledot Fine Negative'; font-size:" + QString::number(font_size, 'f', 1) + "pt;\">T</span>");

	font_size = 120.0 / dpi * 10.0;	// somewhat smaller

	// "->" address: 0x83
	displayString.replace(0x83, "<span style=\"font-family:'Wingdings 3'; font-size:" + QString::number(font_size, 'f', 1) + "pt;\">&#xE2;</span>");

	// "<-" address: 0x84
	displayString.replace(0x84, "<span style=\"font-family:'Wingdings 3'; font-size:" + QString::number(font_size, 'f', 1) + "pt;\">&#xE1;</span>");

	// MENU pointer address: 0x85
	displayString.replace(0x85, "<span style=\"font-family:'Wingdings 3'; font-size:" + QString::number(font_size, 'f', 1) + "pt;\">u</span>");

	// ENCODER IN USE address: 0x87
	displayString.replace(0x87, "<span style=\"font-family:'Wingdings 3'; font-size:" + QString::number(font_size, 'f', 1) + "pt;\">o</span>");
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
		ui.persistentLED->setState(KLed::State::On);
	else
		ui.persistentLED->setState(KLed::State::Off);

	if (energizedLED)
		ui.energizedLED->setState(KLed::State::On);
	else
		ui.energizedLED->setState(KLed::State::Off);

	if (quenchLED)
		ui.quenchLED->setState(KLed::State::On);
	else
		ui.quenchLED->setState(KLed::State::Off);
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
void magnetdaq::displaySystemError(QString errMsg)
{
	if (!errMsg.contains("No errors"))
	{
		statusError->setStyleSheet("color: red; font: bold");
		statusError->setText(errMsg.remove("\r\n"));
		postErrorRefresh();	// refresh currently displayed tab values
		QTimer::singleShot(3000, this, SLOT(clearErrorDisplay()));
	}
}

//---------------------------------------------------------------------------
void magnetdaq::clearErrorDisplay(void)
{
	statusError->clear();
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

		// use button text to determine which key was pressed
		if (button->text() == "1")
			telnet->sendCommand("W_KEY_1\r\n");
		else if (button->text() == "2")
			telnet->sendCommand("W_KEY_2\r\n");
		else if (button->text() == "3")
			telnet->sendCommand("W_KEY_3\r\n");
		else if (button->text() == "4")
			telnet->sendCommand("W_KEY_4\r\n");
		else if (button->text() == "5")
			telnet->sendCommand("W_KEY_5\r\n");
		else if (button->text() == "6")
			telnet->sendCommand("W_KEY_6\r\n");
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
			telnet->sendCommand("W_KEY_ZEROFIELD\r\n");
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

//---------------------------------------------------------------------------
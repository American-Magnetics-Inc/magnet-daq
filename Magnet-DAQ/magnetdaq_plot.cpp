#include "stdafx.h"
#include "magnetdaq.h"

//---------------------------------------------------------------------------
// Contains methods related to the main magnet current/field/voltage vs.
// time plot. Broken out from magnetdaq.cpp for ease of editing.
//---------------------------------------------------------------------------

const int MAGNET_CURRENT_GRAPH = 0;
const int MAGNET_FIELD_GRAPH = 1;
const int SUPPLY_CURRENT_GRAPH = 2;
const int MAGNET_VOLTAGE_GRAPH = 3;
const int SUPPLY_VOLTAGE_GRAPH = 4;

//---------------------------------------------------------------------------
void magnetdaq::restorePlotSettings(QSettings *settings)
{
	// restore saved axes labels (minus units)
	mainPlotTitle = settings->value(axisStr + "Graph/Title", "Magnet Current/Voltage vs. Time").toString();
	mainPlotXTitle = settings->value(axisStr + "Graph/TitleX", "Time").toString();
	mainPlotYTitleCurrent = settings->value(axisStr + "Graph/TitleYCurrent", "Current").toString();
	mainPlotYTitleField = settings->value(axisStr + "Graph/TitleYField", "Field").toString();
	mainPlotY2Title = settings->value(axisStr + "Graph/TitleY2", "Voltage").toString();

	// restore legend names
	mainLegend[MAGNET_CURRENT_GRAPH] = settings->value(axisStr + "Graph/Legend0", "Magnet Current").toString();
	mainLegend[MAGNET_FIELD_GRAPH] = settings->value(axisStr + "Graph/Legend1", "Magnet Field").toString();
	mainLegend[SUPPLY_CURRENT_GRAPH] = settings->value(axisStr + "Graph/Legend2", "Supply Current").toString();
	mainLegend[MAGNET_VOLTAGE_GRAPH] = settings->value(axisStr + "Graph/Legend3", "Magnet Voltage").toString();
	mainLegend[SUPPLY_VOLTAGE_GRAPH] = settings->value(axisStr + "Graph/Legend4", "Supply Voltage").toString();

	// restore plot ranges
	ui.xminEdit->setText(settings->value(axisStr + "Graph/Xmin", "0").toString());
	ui.xmaxEdit->setText(settings->value(axisStr + "Graph/Xmax", "10").toString());
	ui.yminEdit->setText(settings->value(axisStr + "Graph/Ymin", "0").toString());
	ui.ymaxEdit->setText(settings->value(axisStr + "Graph/Ymax", "10").toString());
	ui.vminEdit->setText(settings->value(axisStr + "Graph/Vmin", "-6").toString());
	ui.vmaxEdit->setText(settings->value(axisStr + "Graph/Vmax", "6").toString());

	// restore plot selections
	ui.secondsRadioButton->setChecked(settings->value(axisStr + "Graph/UseSeconds", false).toBool());
	ui.autoscrollXCheckBox->setChecked(settings->value(axisStr + "Graph/AutoscrollX", true).toBool());
	ui.magnetFieldRadioButton->setChecked(settings->value(axisStr + "Graph/PlotField", false).toBool());
	ui.magnetVoltageCheckBox->setChecked(settings->value(axisStr + "Graph/PlotMagnetVoltage", true).toBool());
	ui.supplyCurrentCheckBox->setChecked(settings->value(axisStr + "Graph/PlotSupplyCurrent", false).toBool());
	ui.supplyVoltageCheckBox->setChecked(settings->value(axisStr + "Graph/PlotSupplyVoltage", false).toBool());
}

//---------------------------------------------------------------------------
void magnetdaq::initPlot(void)
{
	//ui.plotWidget->setOpenGl(false);
	ui.plotWidget->setLocale(QLocale(QLocale::English, QLocale::UnitedStates)); // period as decimal separator and comma as thousand separator
	ui.plotWidget->legend->setVisible(true);
	QFont legendFont = font();  // start out with MainWindow's font

#if defined(Q_OS_MACOS)
	legendFont.setPointSize(12); // and make a bit smaller for legend
#else
	legendFont.setPointSize(8); // and make a bit smaller for legend
#endif

	ui.plotWidget->legend->setFont(legendFont);
	ui.plotWidget->legend->setSelectedFont(legendFont);
	ui.plotWidget->legend->setBrush(QBrush(QColor(255, 255, 255, 230)));
	ui.plotWidget->legend->setIconSize(ui.plotWidget->legend->iconSize().width() + 10, ui.plotWidget->legend->iconSize().height());
	ui.plotWidget->legend->setIconTextPadding(10);
	// by default, the legend is in the inset layout of the main axis rect. So this is how we access it to change legend placement:
	ui.plotWidget->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop | Qt::AlignRight);

	// synchronize the left and right margins of the top and bottom axis rects:
	QCPMarginGroup *marginGroup = new QCPMarginGroup(ui.plotWidget);
	ui.plotWidget->axisRect()->setMarginGroup(QCP::msLeft | QCP::msRight, marginGroup);

	timeAxis = ui.plotWidget->xAxis = ui.plotWidget->axisRect()->axis(QCPAxis::atBottom);
	ui.plotWidget->xAxis2 = ui.plotWidget->axisRect()->axis(QCPAxis::atTop);
	ui.plotWidget->xAxis2->setVisible(true);
	ui.plotWidget->xAxis2->setTickLabels(false);
	currentAxis = ui.plotWidget->yAxis = ui.plotWidget->axisRect()->axis(QCPAxis::atLeft);
	voltageAxis = ui.plotWidget->yAxis2 = ui.plotWidget->axisRect()->axis(QCPAxis::atRight);
	voltageAxis->setVisible(true);
	voltageAxis->setTickLabels(true);
	voltageAxis->setTickLabelColor(Qt::red);
	voltageAxis->setLabelColor(Qt::red);

	// create graphs for data

	// magnet current graph #0
	ui.plotWidget->addGraph(timeAxis, currentAxis);
	ui.plotWidget->graph(MAGNET_CURRENT_GRAPH)->setName(mainLegend[MAGNET_CURRENT_GRAPH]);
	{
		QPen pen = QPen(Qt::blue);
		pen.setWidthF(1.0);
		ui.plotWidget->graph(MAGNET_CURRENT_GRAPH)->setPen(pen);
	}

	// magnet field graph #1
	ui.plotWidget->addGraph(timeAxis, currentAxis);
	ui.plotWidget->graph(MAGNET_FIELD_GRAPH)->setName(mainLegend[MAGNET_FIELD_GRAPH]);
	{
		QPen pen = QPen(Qt::darkGreen);
		pen.setWidthF(1.0);
		ui.plotWidget->graph(MAGNET_FIELD_GRAPH)->setPen(pen);
	}

	// supply current graph #2
	ui.plotWidget->addGraph(timeAxis, currentAxis);
	ui.plotWidget->graph(SUPPLY_CURRENT_GRAPH)->setName(mainLegend[SUPPLY_CURRENT_GRAPH]);
	{
		QPen pen = QPen(Qt::blue);
		pen.setWidthF(1.0);
		pen.setStyle(Qt::DashLine);
		ui.plotWidget->graph(SUPPLY_CURRENT_GRAPH)->setPen(pen);
	}

	// magnet voltage graph #3
	ui.plotWidget->addGraph(timeAxis, voltageAxis);
	ui.plotWidget->graph(MAGNET_VOLTAGE_GRAPH)->setName(mainLegend[MAGNET_VOLTAGE_GRAPH]);
	{
		QPen pen = QPen(Qt::red);
		pen.setWidthF(1.0);
		ui.plotWidget->graph(MAGNET_VOLTAGE_GRAPH)->setPen(pen);
	}

	// supply voltage graph #4
	ui.plotWidget->addGraph(timeAxis, voltageAxis);
	ui.plotWidget->graph(SUPPLY_VOLTAGE_GRAPH)->setName(mainLegend[SUPPLY_VOLTAGE_GRAPH]);
	{
		QPen pen = QPen(Qt::red);
		pen.setWidthF(1.0);
		pen.setStyle(Qt::DashLine);
		ui.plotWidget->graph(SUPPLY_VOLTAGE_GRAPH)->setPen(pen);
	}

	// set default scale
	ui.plotWidget->xAxis->setRange(ui.xminEdit->text().toDouble(), ui.xmaxEdit->text().toDouble());
	ui.plotWidget->yAxis->setRange(ui.yminEdit->text().toDouble(), ui.ymaxEdit->text().toDouble());
	ui.plotWidget->yAxis2->setRange(ui.vminEdit->text().toDouble(), ui.vmaxEdit->text().toDouble());

	// set labels
#if defined(Q_OS_MACOS)
	QFont titleFont(".SF NS Text", 15, QFont::Bold);
#else
	QFont titleFont("Segoe UI", 10, QFont::Bold);
#endif

	ui.plotWidget->plotLayout()->insertRow(0);
	ssPlotTitle = new QCPTextElement(ui.plotWidget, mainPlotTitle, titleFont);
	ui.plotWidget->plotLayout()->addElement(0, 0, ssPlotTitle);

#if defined(Q_OS_MACOS)
	QFont axesFont(".SF NS Text", 13, QFont::Normal);
#else
	QFont axesFont("Segoe UI", 9, QFont::Normal);
#endif

	timeAxis->setLabelFont(axesFont);
	timeAxis->grid()->setSubGridVisible(true);
	setTimeAxisLabel();

	currentAxis->setLabelFont(axesFont);
	currentAxis->grid()->setSubGridVisible(true);
	setCurrentAxisLabel();

	voltageAxis->setLabelFont(axesFont);
	setVoltageAxisLabel();

	// create quick-access autoscroll enable/disable button
	autoscrollButton = new QToolButton();
	autoscrollButton->setIconSize(QSize(24, 24));
	autoscrollButton->setCheckable(true);
	autoscrollButton->setChecked(ui.autoscrollXCheckBox->isChecked());
	QIcon stateAwareIcon;
	stateAwareIcon.addPixmap(QIcon(":/magnetdaq/Resources/move_right.png").pixmap(24), QIcon::Normal, QIcon::On);
	stateAwareIcon.addPixmap(QIcon(":/magnetdaq/Resources/move_right_off.png").pixmap(24), QIcon::Normal, QIcon::Off);
	autoscrollButton->setIcon(stateAwareIcon);
	autoscrollButton->setToolTip("Toggle X-axis Autoscrolling");

	// add autoscroll toolbar button to graph space
	{
		QVBoxLayout* innerlayout = new QVBoxLayout;
		QSpacerItem* innerspacer = new QSpacerItem(0, 200, QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
		innerlayout->setContentsMargins(QMargins(0, 0, 0, 0));
		innerlayout->setMargin(0);
		innerlayout->addSpacerItem(innerspacer);
		innerlayout->addWidget(autoscrollButton);
		ui.plotWidget->setLayout(innerlayout);
	}

	connect(autoscrollButton, SIGNAL(clicked(bool)), this, SLOT(toggleAutoscrollButton(bool)));

	// set initial interactions
	ui.plotWidget->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectAxes | QCP::iSelectLegend | QCP::iSelectPlottables);

	// connect slot that ties some axis selections together (especially opposite axes):
	connect(ui.plotWidget, SIGNAL(selectionChangedByUser()), this, SLOT(selectionChanged()));

	// connect slots that takes care that when an axis is selected, only that direction can be dragged and zoomed:
	connect(ui.plotWidget, SIGNAL(mousePress(QMouseEvent*)), this, SLOT(mousePress()));
	connect(ui.plotWidget, SIGNAL(mouseWheel(QWheelEvent*)), this, SLOT(mouseWheel()));

	// make bottom and top axes sync:
	connect(ui.plotWidget->xAxis, SIGNAL(rangeChanged(QCPRange)), ui.plotWidget->xAxis2, SLOT(setRange(QCPRange)));

	// connect some interaction slots:
	connect(ssPlotTitle, SIGNAL(doubleClicked(QMouseEvent*)), this, SLOT(titleDoubleClick(QMouseEvent*)));
	connect(ui.plotWidget, SIGNAL(axisDoubleClick(QCPAxis*, QCPAxis::SelectablePart, QMouseEvent*)), this, SLOT(axisLabelDoubleClick(QCPAxis*, QCPAxis::SelectablePart)));
	connect(ui.plotWidget, SIGNAL(legendDoubleClick(QCPLegend*, QCPAbstractLegendItem*, QMouseEvent*)), this, SLOT(legendDoubleClick(QCPLegend*, QCPAbstractLegendItem*)));

	// setup policy and connect slot for context menu popup:
	ui.plotWidget->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui.plotWidget, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextMenuRequest(QPoint)));
}

//---------------------------------------------------------------------------
void magnetdaq::toggleAutoscrollXCheckBox(bool checked)
{
	if (ui.autoscrollXCheckBox->isChecked())
	{
		autoscrollButton->setChecked(true);
	}
	else
	{
		autoscrollButton->setChecked(false);
	}
}

//---------------------------------------------------------------------------
void magnetdaq::toggleAutoscrollButton(bool checked)
{
	if (autoscrollButton->isChecked())
	{
		ui.autoscrollXCheckBox->setChecked(true);
	}
	else
	{
		ui.autoscrollXCheckBox->setChecked(false);
	}
}

//---------------------------------------------------------------------------
void magnetdaq::setTimeAxisLabel(void)
{
	if (ui.secondsRadioButton->isChecked())
		timeAxis->setLabel(mainPlotXTitle + " (seconds)");
	else
		timeAxis->setLabel(mainPlotXTitle + " (minutes)");
}

//---------------------------------------------------------------------------
QString magnetdaq::getCurrentAxisLabel(void)	// minus units
{
	if (ui.magnetFieldRadioButton->isChecked() && ui.supplyCurrentCheckBox->isChecked())
		return (mainPlotYTitleField + ", " + mainPlotYTitleCurrent);
	else if (ui.magnetFieldRadioButton->isChecked())
		return mainPlotYTitleField;
	else
		return mainPlotYTitleCurrent;
}

//---------------------------------------------------------------------------
void magnetdaq::setCurrentAxisLabel(void)
{
	QString tempStrCurrent, tempStrField;
	static int lastFieldUnits = 0;

	if (lastFieldUnits != model430.fieldUnits())
	{
		// does magnet field already have a history?
		if (!ui.plotWidget->graph(MAGNET_FIELD_GRAPH)->data()->isEmpty())
			ui.plotWidget->graph(MAGNET_FIELD_GRAPH)->data()->clear();	// clear it
	}

	// add units
	tempStrCurrent = mainPlotYTitleCurrent + " (A)";

	if (model430.fieldUnits() == 0)
		tempStrField = mainPlotYTitleField + " (kG)";
	else
		tempStrField = mainPlotYTitleField + " (T)";

	// build label
	if (ui.magnetFieldRadioButton->isChecked() && ui.supplyCurrentCheckBox->isChecked())
		mainPlotYTitle = tempStrField + ", " + tempStrCurrent;
	else if (ui.magnetFieldRadioButton->isChecked())
		mainPlotYTitle = tempStrField;
	else
		mainPlotYTitle = tempStrCurrent;

	currentAxis->setLabel(mainPlotYTitle);
	lastFieldUnits = model430.fieldUnits();
}

//---------------------------------------------------------------------------
void magnetdaq::setVoltageAxisLabel(void)
{
	voltageAxis->setLabel(mainPlotY2Title + " (V)");
}

//---------------------------------------------------------------------------
void magnetdaq::addDataPoint(qint64 time, double magField, double magCurrent, double magVoltage, double supCurrent, double supVoltage)
{
	double timebase = (double)(time - startTime) / 1000.0;

	// save current data to model430 object
	model430.setCurrentData(time, magField, magCurrent, magVoltage, supCurrent, supVoltage);

	// sample rate calculation
	double deltaTime = (double)(time - lastTime) / 1000.0;
	avgSampleTimes(deltaTime);
	lastTime = time;

	if (ui.minutesRadioButton->isChecked())
		timebase /= 60.0;	// convert to minutes

	if (ui.magnetCurrentRadioButton->isChecked())
		ui.plotWidget->graph(MAGNET_CURRENT_GRAPH)->addData(timebase, magCurrent);
	else if (ui.magnetFieldRadioButton->isChecked())
		ui.plotWidget->graph(MAGNET_FIELD_GRAPH)->addData(timebase, magField);

	if (ui.magnetVoltageCheckBox->isChecked())
		ui.plotWidget->graph(MAGNET_VOLTAGE_GRAPH)->addData(timebase, magVoltage);

	if (ui.supplyCurrentCheckBox->isChecked())
		ui.plotWidget->graph(SUPPLY_CURRENT_GRAPH)->addData(timebase, supCurrent);

	if (ui.supplyVoltageCheckBox->isChecked())
		ui.plotWidget->graph(SUPPLY_VOLTAGE_GRAPH)->addData(timebase, supVoltage);

	if (logFile)
	{
		if (plotCount == 0)	// write header
			writeLogHeader();

		// write data to log file
		char buffer[256];

		if (model430.switchInstalled())
			sprintf(buffer, "%lld,%0.8lf,%0.9lf,%0.8lf,%0.3lf,%0.8lf,%0.6lf,%d\n", time, timebase, magField, magCurrent, magVoltage, supCurrent, supVoltage, model430.switchHeaterState);
		else
			sprintf(buffer, "%lld,%0.8lf,%0.9lf,%0.8lf,%0.3lf,%0.8lf,%0.6lf\n", time, timebase, magField, magCurrent, magVoltage, supCurrent, supVoltage);

		logFile->write(buffer);

		// flush every 300 lines (~1 minute)
		if (plotCount % 300 == 0)
			logFile->flush();
	}

	plotCount++;

	// check for out of range timebase
	if (ui.autoscrollXCheckBox->isChecked())
	{
		while (timebase > timeAxis->range().upper)
		{
			double upper = timeAxis->range().upper;
			double range = upper - timeAxis->range().lower;

			timeAxis->setRange(upper, upper + range);
		}
	}

	ui.plotWidget->replot();
}

//---------------------------------------------------------------------------
// this function implements a moving average of the data sampling rate
void magnetdaq::avgSampleTimes(double newValue)
{
	if (firstStatsPass)
	{
		if (samplePos == 0)
			meanSampleTime = 0.0;

		// save new value
		sampleTimes[samplePos] = newValue;

		// running mean for first window of data
		meanSampleTime += (newValue - meanSampleTime) / (double)(samplePos + 1);
	}
	else
	{
		double valueToReplace = sampleTimes[samplePos];

		// save new value
		sampleTimes[samplePos] = newValue;

		// running mean with value replacement
		meanSampleTime += (newValue - valueToReplace) / (double)N_SAMPLES_MOVING_AVG;
	}

	samplePos += 1;

	if (samplePos >= N_SAMPLES_MOVING_AVG)
	{
		if (meanSampleTime > 0)
			statusSampleRate->setText(QString::number(1.0 / meanSampleTime, 'f', 1) + " samples/sec");

		samplePos = 0;
		firstStatsPass = false;
	}
}

//---------------------------------------------------------------------------
void magnetdaq::writeLogHeader(void)
{
	if (logFile)
	{
		QString unitsStr;
		QString heaterStr;

		// indicate field units
		if (model430.fieldUnits() == 0)
			unitsStr = "(kG)";
		else
			unitsStr = "(T)";

		// if switch installed, add heater state column
		if (model430.switchInstalled())
			heaterStr = ",Heater State";
		else
			heaterStr = "";

		// write data column header
		if (ui.secondsRadioButton->isChecked())
			logFile->write(QString("Unix time,Elapsed Time(sec),Magnet Field" + unitsStr + ",Magnet Current(A),Magnet Voltage(V),Supply Current(A),Supply Voltage(V)" + heaterStr + "\n").toLocal8Bit());
		else
			logFile->write(QString("Unix time,Elapsed Time(min),Magnet Field" + unitsStr + ",Magnet Current(A),Magnet Voltage(V),Supply Current(A),Supply Voltage(V)" + heaterStr + "\n").toLocal8Bit());
	}
}

//---------------------------------------------------------------------------
#ifdef USE_QTPRINTER
void magnetdaq::renderPlot(QPrinter *printer)
{
	printer->setPageSize(QPrinter::Letter);
	printer->setOrientation(QPrinter::Landscape);
	printer->setPageMargins(0.75, 0.75, 0.75, 0.75, QPrinter::Unit::Inch);
	QCPPainter painter(printer);
	QRectF pageRect = printer->pageRect(QPrinter::DevicePixel);

	// set size on page
	int plotWidth = ui.plotWidget->viewport().width();
	int plotHeight = ui.plotWidget->viewport().height();

	double scale = pageRect.width() / (double)plotWidth;
	double scale2 = pageRect.height() / (double)plotHeight;

	if (scale2 < scale)
		scale = scale2;

	painter.setMode(QCPPainter::pmVectorized);
	painter.setMode(QCPPainter::pmNoCaching);

	// comment this out if you want cosmetic thin lines (always 1 pixel thick independent of pdf zoom level)
	painter.setMode(QCPPainter::pmNonCosmetic);

	painter.scale(scale, scale);
	ui.plotWidget->toPainter(&painter, plotWidth, plotHeight);
	painter.drawText(-10, -10, QDateTime::currentDateTime().toString());
}
#endif

//---------------------------------------------------------------------------
void magnetdaq::resetAxes(bool checked)
{
	ui.plotWidget->xAxis->setRange(ui.xminEdit->text().toDouble(), ui.xmaxEdit->text().toDouble());
	ui.plotWidget->yAxis->setRange(ui.yminEdit->text().toDouble(), ui.ymaxEdit->text().toDouble());
	ui.plotWidget->yAxis2->setRange(ui.vminEdit->text().toDouble(), ui.vmaxEdit->text().toDouble());

	ui.plotWidget->replot();
}

//---------------------------------------------------------------------------
void magnetdaq::timebaseChanged(bool checked)
{
	// only the seconds button is connected to this slot,
	// so we can assume we changed the units
	// clear the past data of all graphs as the timebase is changed
	for (int i = 0; i < 5; i++)
		ui.plotWidget->graph(i)->data()->clear();

	// reset time to zero
	startTime = QDateTime::currentMSecsSinceEpoch();

	setTimeAxisLabel();
	ui.plotWidget->replot();
}

//---------------------------------------------------------------------------
void magnetdaq::currentAxisSelectionChanged(bool checked)
{
	// only the field button is connected to this slot,
	// so we can assume we changed the plot selection
	if (ui.magnetFieldRadioButton->isChecked())
	{
		// does magnet field already have a history?
		if (!ui.plotWidget->graph(MAGNET_FIELD_GRAPH)->data()->isEmpty())
			ui.plotWidget->graph(MAGNET_FIELD_GRAPH)->data()->clear();	// clear it
	}
	else
	{
		// does magnet current already have a history?
		if (!ui.plotWidget->graph(MAGNET_CURRENT_GRAPH)->data()->isEmpty())
			ui.plotWidget->graph(MAGNET_CURRENT_GRAPH)->data()->clear();	// clear it
	}

	setCurrentAxisLabel();
	ui.plotWidget->replot();
}

//---------------------------------------------------------------------------
void magnetdaq::magnetVoltageSelectionChanged(bool checked)
{
	if (ui.magnetVoltageCheckBox->isChecked())
	{
		// does supply current already have a history?
		if (!ui.plotWidget->graph(MAGNET_VOLTAGE_GRAPH)->data()->isEmpty())
			ui.plotWidget->graph(MAGNET_VOLTAGE_GRAPH)->data()->clear();	// clear it
	}

	ui.plotWidget->replot();
}


//---------------------------------------------------------------------------
void magnetdaq::supplyCurrentSelectionChanged(bool checked)
{
	if (ui.supplyCurrentCheckBox->isChecked())
	{
		// does supply current already have a history?
		if (!ui.plotWidget->graph(SUPPLY_CURRENT_GRAPH)->data()->isEmpty())
			ui.plotWidget->graph(SUPPLY_CURRENT_GRAPH)->data()->clear();	// clear it
	}

	setCurrentAxisLabel();
	ui.plotWidget->replot();
}

//---------------------------------------------------------------------------
void magnetdaq::supplyVoltageSelectionChanged(bool checked)
{
	if (ui.supplyVoltageCheckBox->isChecked())
	{
		// does supply voltage already have a history?
		if (!ui.plotWidget->graph(SUPPLY_VOLTAGE_GRAPH)->data()->isEmpty())
			ui.plotWidget->graph(SUPPLY_VOLTAGE_GRAPH)->data()->clear();	// clear it
	}

	ui.plotWidget->replot();
}

//---------------------------------------------------------------------------
void magnetdaq::titleDoubleClick(QMouseEvent* event)
{
	Q_UNUSED(event)

	if (QCPTextElement *title = qobject_cast<QCPTextElement*>(sender()))
	{
		// Set the plot title by double clicking on it
		bool ok;
		QSettings settings;

		QInputDialog dlg(this);
		dlg.setInputMode(QInputDialog::TextInput);
		dlg.setWindowTitle("Set Plot Title");
		dlg.setLabelText("New plot title:");
		dlg.setTextValue(mainPlotTitle);

		// save/restore different geometry for different DPI screens
		QString dpiStr = QString::number(QApplication::desktop()->screen()->logicalDpiX());
		dlg.restoreGeometry(settings.value(axisStr + "LabelDialog/Geometry/" + dpiStr).toByteArray());
		ok = dlg.exec();

		if (ok)
		{
			mainPlotTitle = dlg.textValue();
			ssPlotTitle->setText(mainPlotTitle);
			ui.plotWidget->replot();
		}

		settings.setValue(axisStr + "LabelDialog/Geometry/" + dpiStr, dlg.saveGeometry());
	}
}

//---------------------------------------------------------------------------
void magnetdaq::axisLabelDoubleClick(QCPAxis *axis, QCPAxis::SelectablePart part)
{
	// Set an axis label by double clicking on it
	if (part == QCPAxis::spAxisLabel) // only react when the actual axis label is clicked, not tick label or axis backbone
	{
		bool ok;
		QSettings settings;

		QInputDialog dlg(this);
		dlg.setInputMode(QInputDialog::TextInput);
		dlg.setWindowTitle("Set Axis Title");
		dlg.setLabelText("New axis label:");

		if (axis == timeAxis)
			dlg.setTextValue(mainPlotXTitle);
		else if (axis == currentAxis)
			dlg.setTextValue(getCurrentAxisLabel());
		else if (axis == voltageAxis)
			dlg.setTextValue(mainPlotY2Title);

		// save/restore different geometry for different DPI screens
		QString dpiStr = QString::number(QApplication::desktop()->screen()->logicalDpiX());
		dlg.restoreGeometry(settings.value(axisStr + "LabelDialog/Geometry/" + dpiStr).toByteArray());
		ok = dlg.exec();

		if (ok)
		{
			if (axis == timeAxis)
			{
				mainPlotXTitle = dlg.textValue();
				setTimeAxisLabel();
			}
			else if (axis == currentAxis)
			{
				if (ui.magnetFieldRadioButton->isChecked() && ui.supplyCurrentCheckBox->isChecked())
				{
					QString tempStr = dlg.textValue();
					QStringList strList = tempStr.split(",");

					mainPlotYTitleField = strList[0];

					if (strList.count() > 0)
						mainPlotYTitleCurrent = strList[1];
				}
				else if (ui.magnetFieldRadioButton->isChecked())
					mainPlotYTitleField = dlg.textValue();
				else
					mainPlotYTitleCurrent = dlg.textValue();

				setCurrentAxisLabel();
			}
			else if (axis == voltageAxis)
			{
				mainPlotY2Title = dlg.textValue();
				setVoltageAxisLabel();
			}

			ui.plotWidget->replot();
		}

		settings.setValue(axisStr + "LabelDialog/Geometry/" + dpiStr, dlg.saveGeometry());
	}
}

//---------------------------------------------------------------------------
void magnetdaq::legendDoubleClick(QCPLegend *legend, QCPAbstractLegendItem *item)
{
	// Rename a graph by double clicking on its legend item
	Q_UNUSED(legend)
	if (item) // only react if item was clicked (user could have clicked on border padding of legend where there is no item, then item is 0)
	{
		QCPPlottableLegendItem *plItem = qobject_cast<QCPPlottableLegendItem*>(item);

		bool ok;
		QSettings settings;

		QInputDialog dlg(this);
		dlg.setInputMode(QInputDialog::TextInput);
		dlg.setWindowTitle("Set Graph Name");
		dlg.setLabelText("New graph name:");
		dlg.setTextValue(plItem->plottable()->name());

		// save/restore different geometry for different DPI screens
		QString dpiStr = QString::number(QApplication::desktop()->screen()->logicalDpiX());
		dlg.restoreGeometry(settings.value(axisStr + "LabelDialog/Geometry/" + dpiStr).toByteArray());
		ok = dlg.exec();

		if (ok)
		{
			plItem->plottable()->setName(dlg.textValue());
			ui.plotWidget->replot();
		}

		settings.setValue(axisStr + "LabelDialog/Geometry/" + dpiStr, dlg.saveGeometry());
	}
}

//---------------------------------------------------------------------------
void magnetdaq::selectionChanged()
{
	/*
	normally, axis base line, axis tick labels and axis labels are selectable separately, but we want
	the user only to be able to select the axis as a whole, so we tie the selected states of the tick labels
	and the axis base line together. However, the axis label shall be selectable individually.

	The selection state of the left and right axes shall be synchronized as well as the state of the
	bottom and top axes.
	*/

	// handle axis and tick labels as one selectable object:
	if (timeAxis->selectedParts().testFlag(QCPAxis::spAxis) || timeAxis->selectedParts().testFlag(QCPAxis::spTickLabels))
	{
		timeAxis->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels);
	}

	// handle axis and tick labels as one selectable object:
	if (currentAxis->selectedParts().testFlag(QCPAxis::spAxis) || currentAxis->selectedParts().testFlag(QCPAxis::spTickLabels))
	{
		currentAxis->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels);
	}

	if (voltageAxis->selectedParts().testFlag(QCPAxis::spAxis) || voltageAxis->selectedParts().testFlag(QCPAxis::spTickLabels))
	{
		voltageAxis->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels);
	}

	// synchronize selection of graphs with selection of corresponding legend items:
	for (int i = 0; i < ui.plotWidget->graphCount(); ++i)
	{
		QCPGraph *graph = ui.plotWidget->graph(i);
		QCPPlottableLegendItem *item = ui.plotWidget->legend->itemWithPlottable(graph);
		if (item->selected() || graph->selected())
		{
			item->setSelected(true);
			graph->setSelection(QCPDataSelection(graph->data()->dataRange()));
		}
	}
}

//---------------------------------------------------------------------------
void magnetdaq::mousePress()
{
	QList<QCPAxis*> axesList;

	// if an axis is selected, only allow the direction of that axis to be dragged
	// if no axis is selected, both directions may be dragged

	if (timeAxis->selectedParts().testFlag(QCPAxis::spAxis))
		axesList.append(timeAxis);
	else if (currentAxis->selectedParts().testFlag(QCPAxis::spAxis))
		axesList.append(currentAxis);
	else if (voltageAxis->selectedParts().testFlag(QCPAxis::spAxis))
		axesList.append(voltageAxis);
	else
	{
		bool selectedGraph = false;

		// is a graph selected?
		for (int i = 0; i < ui.plotWidget->graphCount(); ++i)
		{
			QCPGraph *graph = ui.plotWidget->graph(i);
			if (graph->selected())
			{
				selectedGraph = true;
				axesList.append(graph->keyAxis());
				axesList.append(graph->valueAxis());
				break;
			}
		}

		if (!selectedGraph)
		{
			axesList.append(timeAxis);
			axesList.append(currentAxis);
		}
	}

	// set the axes to drag
	ui.plotWidget->axisRect()->setRangeDragAxes(axesList);
}

//---------------------------------------------------------------------------
void magnetdaq::mouseWheel()
{
	QList<QCPAxis*> axesList;

	// if an axis is selected, only allow the direction of that axis to be zoomed
	// if no axis is selected, both directions may be zoomed

	if (timeAxis->selectedParts().testFlag(QCPAxis::spAxis))
		axesList.append(timeAxis);
	else if (currentAxis->selectedParts().testFlag(QCPAxis::spAxis))
		axesList.append(currentAxis);
	else if (voltageAxis->selectedParts().testFlag(QCPAxis::spAxis))
		axesList.append(voltageAxis);
	else
	{
		bool selectedGraph = false;

		// is a graph selected?
		for (int i = 0; i < ui.plotWidget->graphCount(); ++i)
		{
			QCPGraph *graph = ui.plotWidget->graph(i);
			if (graph->selected())
			{
				selectedGraph = true;
				axesList.append(graph->keyAxis());
				axesList.append(graph->valueAxis());
				break;
			}
		}

		if (!selectedGraph)
		{
			axesList.append(timeAxis);
			axesList.append(currentAxis);
		}
	}

	ui.plotWidget->axisRect()->setRangeZoomAxes(axesList);
}

//---------------------------------------------------------------------------
void magnetdaq::contextMenuRequest(QPoint pos)
{
	QMenu *menu = new QMenu(this);
	menu->setAttribute(Qt::WA_DeleteOnClose);

	if (ui.plotWidget->legend->selectTest(pos, false) >= 0) // context menu on legend requested
	{
		menu->addAction("Move to top left", this, SLOT(moveLegend()))->setData((int)(Qt::AlignTop | Qt::AlignLeft));
		menu->addAction("Move to top center", this, SLOT(moveLegend()))->setData((int)(Qt::AlignTop | Qt::AlignHCenter));
		menu->addAction("Move to top right", this, SLOT(moveLegend()))->setData((int)(Qt::AlignTop | Qt::AlignRight));
		menu->addAction("Move to bottom right", this, SLOT(moveLegend()))->setData((int)(Qt::AlignBottom | Qt::AlignRight));
		menu->addAction("Move to bottom left", this, SLOT(moveLegend()))->setData((int)(Qt::AlignBottom | Qt::AlignLeft));
	}

	menu->popup(ui.plotWidget->mapToGlobal(pos));
}

//---------------------------------------------------------------------------
void magnetdaq::moveLegend()
{
	if (QAction* contextAction = qobject_cast<QAction*>(sender())) // make sure this slot is really called by a context menu action, so it carries the data we need
	{
		bool ok;
		int dataInt = contextAction->data().toInt(&ok);
		if (ok)
		{
			ui.plotWidget->axisRect()->insetLayout()->setInsetAlignment(0, (Qt::Alignment)dataInt);
			ui.plotWidget->replot();
		}
	}
}

//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
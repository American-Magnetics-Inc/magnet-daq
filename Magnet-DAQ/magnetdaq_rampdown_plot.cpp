#include "stdafx.h"
#include "magnetdaq.h"

//---------------------------------------------------------------------------
// Contains methods related to the rampdown segments (in magnet voltage) vs.
// current/field plot. Broken out from magnetdaq.cpp for ease of editing.
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
void magnetdaq::restoreRampdownPlotSettings(QSettings *settings)
{

}

//---------------------------------------------------------------------------
void magnetdaq::initRampdownPlot(void)
{
	//ui.rampdownPlotWidget->setOpenGl(false);
	ui.rampdownPlotWidget->setLocale(QLocale(QLocale::English, QLocale::UnitedStates)); // period as decimal separator and comma as thousand separator

	// synchronize the left and right margins of the top and bottom axis rects:
	QCPMarginGroup *marginGroup = new QCPMarginGroup(ui.rampdownPlotWidget);
	ui.rampdownPlotWidget->axisRect()->setMarginGroup(QCP::msLeft | QCP::msRight, marginGroup);

	// make bottom and top axes, and left and right, sync:
	connect(ui.rampdownPlotWidget->xAxis, SIGNAL(rangeChanged(QCPRange)), ui.rampdownPlotWidget->xAxis2, SLOT(setRange(QCPRange)));
	connect(ui.rampdownPlotWidget->yAxis, SIGNAL(rangeChanged(QCPRange)), ui.rampdownPlotWidget->yAxis2, SLOT(setRange(QCPRange)));

	rampdownCurrentAxis = ui.rampdownPlotWidget->xAxis = ui.rampdownPlotWidget->axisRect()->axis(QCPAxis::atBottom);
	ui.rampdownPlotWidget->xAxis2 = ui.rampdownPlotWidget->axisRect()->axis(QCPAxis::atTop);
	ui.rampdownPlotWidget->xAxis2->setVisible(true);
	ui.rampdownPlotWidget->xAxis2->setTickLabels(false);
	rampdownVoltageAxis = ui.rampdownPlotWidget->yAxis = ui.rampdownPlotWidget->axisRect()->axis(QCPAxis::atLeft);
	ui.rampdownPlotWidget->yAxis2 = ui.rampdownPlotWidget->axisRect()->axis(QCPAxis::atRight);
	ui.rampdownPlotWidget->yAxis2->setVisible(true);
	ui.rampdownPlotWidget->yAxis2->setTickLabels(false);

	// create graph for data

	// magnet charge voltage vs. magnet current graph
	ui.rampdownPlotWidget->addGraph(rampdownCurrentAxis, rampdownVoltageAxis);
	ui.rampdownPlotWidget->graph()->setName("Charge Rate");
	{
		QPen pen = QPen(Qt::darkBlue);
		pen.setWidthF(1.0);
		QColor color = Qt::lightGray;
		color.setAlpha(64);
		ui.rampdownPlotWidget->graph()->setBrush(color);
		ui.rampdownPlotWidget->graph()->setPen(pen);
		//ui.rampdownPlotWidget->graph()->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 4));
	}

	// set default scale
	int rampdownSegments = ui.rampdownSegmentsSpinBox->value();
	ui.rampdownPlotWidget->xAxis->setRange(-(rampdownSegMaxLimits[rampdownSegments]->text().toDouble()), rampdownSegMaxLimits[rampdownSegments]->text().toDouble());
	ui.rampdownPlotWidget->yAxis->setRange(-10, +10);

	// set labels
#if defined(Q_OS_MACOS)
	QFont titleFont(".SF NS Text", 15, QFont::Bold);
#else
	QFont titleFont("Segoe UI", 10, QFont::Bold);
#endif

	ui.rampdownPlotWidget->plotLayout()->insertRow(0);
	rampdownPlotTitle = new QCPTextElement(ui.rampdownPlotWidget, "Discharge Voltage vs. Current/Field", titleFont);
	ui.rampdownPlotWidget->plotLayout()->addElement(0, 0, rampdownPlotTitle);

#if defined(Q_OS_MACOS)
	QFont axesFont(".SF NS Text", 13, QFont::Normal);
#else
	QFont axesFont("Segoe UI", 9, QFont::Normal);
#endif

	rampdownCurrentAxis->setLabelFont(axesFont);
	rampdownCurrentAxis->grid()->setSubGridVisible(true);
	setRampdownPlotCurrentAxisLabel();

	rampdownVoltageAxis->setLabelFont(axesFont);
	rampdownVoltageAxis->grid()->setSubGridVisible(true);
	rampdownVoltageAxis->setLabel("Discharge Voltage (V)");

	// set initial interactions
	ui.rampdownPlotWidget->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectAxes | QCP::iSelectPlottables);

	// connect slot that ties some axis selections together (especially opposite axes):
	connect(ui.rampdownPlotWidget, SIGNAL(selectionChangedByUser()), this, SLOT(rampdownPlotSelectionChanged()));

	// connect slots that takes care that when an axis is selected, only that direction can be dragged and zoomed:
	connect(ui.rampdownPlotWidget, SIGNAL(mousePress(QMouseEvent*)), this, SLOT(rampdownPlotMousePress()));
	connect(ui.rampdownPlotWidget, SIGNAL(mouseWheel(QWheelEvent*)), this, SLOT(rampdownPlotMouseWheel()));

	// connect sync with model 430
	connect(&model430, SIGNAL(syncRampdownPlot()), this, SLOT(syncRampdownPlot()), Qt::QueuedConnection);
}

//---------------------------------------------------------------------------
void magnetdaq::setRampdownPlotCurrentAxisLabel(void)
{
	if (ui.rampUnitsComboBox->currentIndex() == 0)
		rampdownCurrentAxis->setLabel("Current (A)");
	else
	{
		if (model430.fieldUnits == 0)
			rampdownCurrentAxis->setLabel("Field (kG)");
		else
			rampdownCurrentAxis->setLabel("Field (T)");
	}
}

//---------------------------------------------------------------------------
void magnetdaq::syncRampdownPlot(void)
{
	// clear existing data
	ui.rampdownPlotWidget->graph()->data()->clear();

	setRampdownPlotCurrentAxisLabel();

	// create new data storage
	int segmentCount = ui.rampdownSegmentsSpinBox->value();
	QVector<QCPGraphData> segmentData(segmentCount * 4 + 2);
	double divisor = 1.0;

	if (model430.rampRateTimeUnits == 1)
		divisor = 60.0;

	if (ui.rampUnitsComboBox->currentIndex() == 0)
	{
		// first point
		segmentData[0].key = -(model430.currentRampdownLimits[segmentCount - 1]());
		segmentData[0].value = 0;

		int j = 1;
		for (int i = (segmentCount - 1); i >= 0; i--)	// negative side of graph
		{
			segmentData[j].key = -(model430.currentRampdownLimits[i]());
			segmentData[j].value = model430.currentRampdownRates[i]() * model430.inductance() / divisor;
			j++;

			if (i == 0)
				segmentData[j].key = 0;
			else
				segmentData[j].key = -(model430.currentRampdownLimits[i - 1]());
			segmentData[j].value = model430.currentRampdownRates[i]() * model430.inductance() / divisor;
			j++;
		}

		for (int i = 0; i < segmentCount; i++)	// positive side of graph
		{
			if (i == 0)
				segmentData[j].key = 0;
			else
				segmentData[j].key = model430.currentRampdownLimits[i - 1]();
			segmentData[j].value = -(model430.currentRampdownRates[i]() * model430.inductance() / divisor);
			j++;

			segmentData[j].key = model430.currentRampdownLimits[i]();
			segmentData[j].value = -(model430.currentRampdownRates[i]() * model430.inductance() / divisor);
			j++;
		}

		// last point
		segmentData[j].key = model430.currentRampdownLimits[segmentCount - 1]();
		segmentData[j].value = 0;
	}
	else
	{
		// first point
		segmentData[0].key = -(model430.fieldRampdownLimits[segmentCount - 1]());
		segmentData[0].value = 0;

		int j = 1;
		for (int i = (segmentCount - 1); i >= 0; i--)	// negative side of graph
		{
			segmentData[j].key = -(model430.fieldRampdownLimits[i]());
			segmentData[j].value = model430.currentRampdownRates[i]() * model430.inductance() / divisor;
			j++;

			if (i == 0)
				segmentData[j].key = 0;
			else
				segmentData[j].key = -(model430.fieldRampdownLimits[i - 1]());
			segmentData[j].value = model430.currentRampdownRates[i]() * model430.inductance() / divisor;
			j++;
		}

		for (int i = 0; i < segmentCount; i++)	// positive side of graph
		{
			if (i == 0)
				segmentData[j].key = 0;
			else
				segmentData[j].key = model430.fieldRampdownLimits[i - 1]();
			segmentData[j].value = -(model430.currentRampdownRates[i]() * model430.inductance() / divisor);
			j++;

			segmentData[j].key = model430.fieldRampdownLimits[i]();
			segmentData[j].value = -(model430.currentRampdownRates[i]() * model430.inductance() / divisor);
			j++;
		}

		// last point
		segmentData[j].key = model430.fieldRampdownLimits[segmentCount - 1]();
		segmentData[j].value = 0;
	}

	ui.rampdownPlotWidget->graph()->data()->set(segmentData);
	ui.rampdownPlotWidget->rescaleAxes(true);

	//add a little whitespace around plot
	ui.rampdownPlotWidget->xAxis->scaleRange(1 / 0.85, ui.rampdownPlotWidget->xAxis->range().center());
	ui.rampdownPlotWidget->yAxis->scaleRange(1 / 0.85, ui.rampdownPlotWidget->yAxis->range().center());

	ui.rampdownPlotWidget->replot();
}

//---------------------------------------------------------------------------
#ifdef USE_QTPRINTER
void magnetdaq::renderRampdownPlot(QPrinter *printer)
{
	printer->setPageSize(QPrinter::Letter);
	printer->setOrientation(QPrinter::Landscape);
	printer->setPageMargins(0.75, 0.75, 0.75, 0.75, QPrinter::Unit::Inch);
	QCPPainter painter(printer);
	QRectF pageRect = printer->pageRect(QPrinter::DevicePixel);

	// set size on page
	int plotWidth = ui.rampdownPlotWidget->viewport().width();
	int plotHeight = ui.rampdownPlotWidget->viewport().height();

	double scale = pageRect.width() / (double)plotWidth;
	double scale2 = pageRect.height() / (double)plotHeight;

	if (scale2 < scale)
		scale = scale2;

	painter.setMode(QCPPainter::pmVectorized);
	painter.setMode(QCPPainter::pmNoCaching);

	// comment this out if you want cosmetic thin lines (always 1 pixel thick independent of pdf zoom level)
	painter.setMode(QCPPainter::pmNonCosmetic);

	painter.scale(scale, scale);
	ui.rampdownPlotWidget->toPainter(&painter, plotWidth, plotHeight);
	painter.drawText(-10, -10, QDateTime::currentDateTime().toString());
}
#endif

//---------------------------------------------------------------------------
void magnetdaq::resetRampdownPlotAxes(bool checked)
{
	int rampdownSegments = ui.rampdownSegmentsSpinBox->value();
	ui.rampdownPlotWidget->xAxis->setRange(-(rampdownSegMaxLimits[rampdownSegments]->text().toDouble()), rampdownSegMaxLimits[rampdownSegments]->text().toDouble());
	ui.rampdownPlotWidget->yAxis->setRange(-10, +10);

	ui.rampdownPlotWidget->replot();
}

//---------------------------------------------------------------------------
void magnetdaq::rampdownPlotTimebaseChanged(bool checked)
{
	// clear past data
	ui.rampdownPlotWidget->graph(0)->data()->clear();
	ui.rampdownPlotWidget->replot();
}

//---------------------------------------------------------------------------
void magnetdaq::rampdownPlotSelectionChanged(void)
{
	/*
	normally, axis base line, axis tick labels and axis labels are selectable separately, but we want
	the user only to be able to select the axis as a whole, so we tie the selected states of the tick labels
	and the axis base line together. However, the axis label shall be selectable individually.

	The selection state of the left and right axes shall be synchronized as well as the state of the
	bottom and top axes.
	*/

	// make top and bottom axes be selected synchronously, and handle axis and tick labels as one selectable object:
	if (ui.rampdownPlotWidget->xAxis->selectedParts().testFlag(QCPAxis::spAxis) || ui.rampdownPlotWidget->xAxis->selectedParts().testFlag(QCPAxis::spTickLabels) ||
		ui.rampdownPlotWidget->xAxis2->selectedParts().testFlag(QCPAxis::spAxis) || ui.rampdownPlotWidget->xAxis2->selectedParts().testFlag(QCPAxis::spTickLabels))
	{
		ui.rampdownPlotWidget->xAxis2->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels);
		ui.rampdownPlotWidget->xAxis->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels);
	}
	// make left and right axes be selected synchronously, and handle axis and tick labels as one selectable object:
	if (ui.rampdownPlotWidget->yAxis->selectedParts().testFlag(QCPAxis::spAxis) || ui.rampdownPlotWidget->yAxis->selectedParts().testFlag(QCPAxis::spTickLabels) ||
		ui.rampdownPlotWidget->yAxis2->selectedParts().testFlag(QCPAxis::spAxis) || ui.rampdownPlotWidget->yAxis2->selectedParts().testFlag(QCPAxis::spTickLabels))
	{
		ui.rampdownPlotWidget->yAxis2->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels);
		ui.rampdownPlotWidget->yAxis->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels);
	}
}

//---------------------------------------------------------------------------
void magnetdaq::rampdownPlotMousePress(void)
{
	// if an axis is selected, only allow the direction of that axis to be dragged
	// if no axis is selected, both directions may be dragged

	if (ui.rampdownPlotWidget->xAxis->selectedParts().testFlag(QCPAxis::spAxis))
		ui.rampdownPlotWidget->axisRect()->setRangeDrag(ui.rampdownPlotWidget->xAxis->orientation());
	else if (ui.rampdownPlotWidget->yAxis->selectedParts().testFlag(QCPAxis::spAxis))
		ui.rampdownPlotWidget->axisRect()->setRangeDrag(ui.rampdownPlotWidget->yAxis->orientation());
	else
		ui.rampdownPlotWidget->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
}

//---------------------------------------------------------------------------
void magnetdaq::rampdownPlotMouseWheel(void)
{
	// if an axis is selected, only allow the direction of that axis to be zoomed
	// if no axis is selected, both directions may be zoomed

	if (ui.rampdownPlotWidget->xAxis->selectedParts().testFlag(QCPAxis::spAxis))
		ui.rampdownPlotWidget->axisRect()->setRangeZoom(ui.rampdownPlotWidget->xAxis->orientation());
	else if (ui.rampdownPlotWidget->yAxis->selectedParts().testFlag(QCPAxis::spAxis))
		ui.rampdownPlotWidget->axisRect()->setRangeZoom(ui.rampdownPlotWidget->yAxis->orientation());
	else
		ui.rampdownPlotWidget->axisRect()->setRangeZoom(Qt::Horizontal | Qt::Vertical);
}

//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
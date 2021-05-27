#include "stdafx.h"
#include "magnetdaq.h"

//---------------------------------------------------------------------------
// Contains methods related to the ramp rate (in magnet voltage) vs.
// current/field plot. Broken out from magnetdaq.cpp for ease of editing.
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
void magnetdaq::restoreRampPlotSettings(QSettings *settings)
{

}

//---------------------------------------------------------------------------
void magnetdaq::initRampPlot(void)
{
	//ui.rampPlotWidget->setOpenGl(false);
	ui.rampPlotWidget->setLocale(QLocale(QLocale::English, QLocale::UnitedStates)); // period as decimal separator and comma as thousand separator

	// synchronize the left and right margins of the top and bottom axis rects:
	QCPMarginGroup *marginGroup = new QCPMarginGroup(ui.rampPlotWidget);
	ui.rampPlotWidget->axisRect()->setMarginGroup(QCP::msLeft | QCP::msRight, marginGroup);

	// make bottom and top axes, and left and right, sync:
	connect(ui.rampPlotWidget->xAxis, SIGNAL(rangeChanged(QCPRange)), ui.rampPlotWidget->xAxis2, SLOT(setRange(QCPRange)));
	connect(ui.rampPlotWidget->yAxis, SIGNAL(rangeChanged(QCPRange)), ui.rampPlotWidget->yAxis2, SLOT(setRange(QCPRange)));

	rampCurrentAxis = ui.rampPlotWidget->xAxis = ui.rampPlotWidget->axisRect()->axis(QCPAxis::atBottom);
	ui.rampPlotWidget->xAxis2 = ui.rampPlotWidget->axisRect()->axis(QCPAxis::atTop);
	ui.rampPlotWidget->xAxis2->setVisible(true);
	ui.rampPlotWidget->xAxis2->setTickLabels(false);
	rampVoltageAxis = ui.rampPlotWidget->yAxis = ui.rampPlotWidget->axisRect()->axis(QCPAxis::atLeft);
	ui.rampPlotWidget->yAxis2 = ui.rampPlotWidget->axisRect()->axis(QCPAxis::atRight);
	ui.rampPlotWidget->yAxis2->setVisible(true);
	ui.rampPlotWidget->yAxis2->setTickLabels(false);

	// create graph for data

	// magnet charge voltage vs. magnet current graph
	ui.rampPlotWidget->addGraph(rampCurrentAxis, rampVoltageAxis);
	ui.rampPlotWidget->graph()->setName("Charge Rate");
	{
		QPen pen = QPen(Qt::darkBlue);
		pen.setWidthF(1.0);
		QColor color = Qt::lightGray;
		color.setAlpha(64);
		ui.rampPlotWidget->graph()->setBrush(color);
		ui.rampPlotWidget->graph()->setPen(pen);
		//ui.rampPlotWidget->graph()->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 4));
	}

	// add graph for target point display
	ui.rampPlotWidget->addGraph(rampCurrentAxis, rampVoltageAxis);
	ui.rampPlotWidget->graph(1)->setScatterStyle(QCPScatterStyle::ssStar);
	ui.rampPlotWidget->graph(1)->setLineStyle(QCPGraph::lsNone);
	{
		QPen pen = QPen(Qt::red);
		pen.setWidthF(1.0);
		QColor color = Qt::red;
		color.setAlpha(64);
		ui.rampPlotWidget->graph(1)->setBrush(color);
		ui.rampPlotWidget->graph(1)->setPen(pen);
	}

	// set default scale
	int rampSegments = ui.rampSegmentsSpinBox->value();
	ui.rampPlotWidget->xAxis->setRange(-(rampSegMaxLimits[rampSegments]->text().toDouble()), rampSegMaxLimits[rampSegments]->text().toDouble());
	ui.rampPlotWidget->yAxis->setRange(-10, +10);

	// set labels
#if defined(Q_OS_MACOS)
	QFont titleFont(".SF NS Text", 15, QFont::Bold);
#else
	QFont titleFont("Segoe UI", 10, QFont::Bold);
#endif

	ui.rampPlotWidget->plotLayout()->insertRow(0);
	rampPlotTitle = new QCPTextElement(ui.rampPlotWidget, "Charge Voltage vs. Current/Field", titleFont);
	ui.rampPlotWidget->plotLayout()->addElement(0, 0, rampPlotTitle);
	ui.rampPlotWidget->plotLayout()->elementAt(0)->setMaximumSize(16777215, 26);
	ui.rampPlotWidget->plotLayout()->elementAt(0)->setMinimumSize(200, 26);
	ui.rampPlotWidget->plotLayout()->elementAt(1)->setMaximumSize(16777215, 16777215);

#if defined(Q_OS_MACOS)
	QFont axesFont(".SF NS Text", 13, QFont::Normal);
#else
	QFont axesFont("Segoe UI", 9, QFont::Normal);
#endif

	rampCurrentAxis->setLabelFont(axesFont);
	rampCurrentAxis->grid()->setSubGridVisible(true);
	setRampPlotCurrentAxisLabel();

	rampVoltageAxis->setLabelFont(axesFont);
	rampVoltageAxis->grid()->setSubGridVisible(true);
	rampVoltageAxis->setLabel("Charge Voltage (V)");

	// set initial interactions
	ui.rampPlotWidget->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectAxes | QCP::iSelectPlottables);

	// connect slot that ties some axis selections together (especially opposite axes):
	connect(ui.rampPlotWidget, SIGNAL(selectionChangedByUser()), this, SLOT(rampPlotSelectionChanged()));

	// connect slots that takes care that when an axis is selected, only that direction can be dragged and zoomed:
	connect(ui.rampPlotWidget, SIGNAL(mousePress(QMouseEvent*)), this, SLOT(rampPlotMousePress()));
	connect(ui.rampPlotWidget, SIGNAL(mouseWheel(QWheelEvent*)), this, SLOT(rampPlotMouseWheel()));

	// connect sync with model 430
	connect(&model430, SIGNAL(syncRampPlot()), this, SLOT(syncRampPlot()), Qt::QueuedConnection);
}

//---------------------------------------------------------------------------
void magnetdaq::setRampPlotCurrentAxisLabel(void)
{
	if (ui.rampUnitsComboBox->currentIndex() == 0)
		rampCurrentAxis->setLabel("Current (A)");
	else
	{
		if (model430.fieldUnits == KG)
			rampCurrentAxis->setLabel("Field (kG)");
		else
			rampCurrentAxis->setLabel("Field (T)");
	}
}

//---------------------------------------------------------------------------
void magnetdaq::syncRampPlot(void)
{
	// clear existing data
	ui.rampPlotWidget->graph(0)->data()->clear();
	ui.rampPlotWidget->graph(1)->data()->clear();

	setRampPlotCurrentAxisLabel();

	// create new data storage
	int segmentCount = ui.rampSegmentsSpinBox->value();
	QVector<QCPGraphData> segmentData(segmentCount * 4 + 2);
	double divisor = 1.0;

	if (model430.rampRateTimeUnits == 1)
		divisor = 60.0;

	if (ui.rampUnitsComboBox->currentIndex() == 0)
	{
		// first point
		segmentData[0].key = -(model430.currentRampLimits[segmentCount - 1]());
		segmentData[0].value = 0;

		int j = 1;
		for (int i = (segmentCount - 1); i >= 0; i--)	// negative side of graph
		{
			segmentData[j].key = -(model430.currentRampLimits[i]());
			segmentData[j].value = -(model430.currentRampRates[i]() * model430.inductance()) / divisor;
			j++;

			if (i == 0)
				segmentData[j].key = 0;
			else
				segmentData[j].key = -(model430.currentRampLimits[i - 1]());
			segmentData[j].value = -(model430.currentRampRates[i]() * model430.inductance()) / divisor;
			j++;
		}

		for (int i = 0; i < segmentCount; i++)	// positive side of graph
		{
			if (i == 0)
				segmentData[j].key = 0;
			else
				segmentData[j].key = model430.currentRampLimits[i - 1]();
			segmentData[j].value = model430.currentRampRates[i]() * model430.inductance() / divisor;
			j++;

			segmentData[j].key = model430.currentRampLimits[i]();
			segmentData[j].value = model430.currentRampRates[i]() * model430.inductance() / divisor;
			j++;
		}

		// last point
		segmentData[j].key = model430.currentRampLimits[segmentCount - 1]();
		segmentData[j].value = 0;
	}
	else
	{
		// first point
		segmentData[0].key = -(model430.fieldRampLimits[segmentCount - 1]());
		segmentData[0].value = 0;

		int j = 1;
		for (int i = (segmentCount - 1); i >= 0; i--)	// negative side of graph
		{
			segmentData[j].key = -(model430.fieldRampLimits[i]());
			segmentData[j].value = -(model430.currentRampRates[i]() * model430.inductance()) / divisor;
			j++;

			if (i == 0)
				segmentData[j].key = 0;
			else
				segmentData[j].key = -(model430.fieldRampLimits[i - 1]());
			segmentData[j].value = -(model430.currentRampRates[i]() * model430.inductance()) / divisor;
			j++;
		}

		for (int i = 0; i < segmentCount; i++)	// positive side of graph
		{
			if (i == 0)
				segmentData[j].key = 0;
			else
				segmentData[j].key = model430.fieldRampLimits[i - 1]();
			segmentData[j].value = model430.currentRampRates[i]() * model430.inductance() / divisor;
			j++;

			segmentData[j].key = model430.fieldRampLimits[i]();
			segmentData[j].value = model430.currentRampRates[i]() * model430.inductance() / divisor;
			j++;
		}

		// last point
		segmentData[j].key = model430.fieldRampLimits[segmentCount - 1]();
		segmentData[j].value = 0;
	}

	ui.rampPlotWidget->graph(0)->data()->set(segmentData);
	ui.rampPlotWidget->rescaleAxes(true);

	//add a little whitespace around plot
	ui.rampPlotWidget->xAxis->scaleRange(1 / 0.85, ui.rampPlotWidget->xAxis->range().center());
	ui.rampPlotWidget->yAxis->scaleRange(1 / 0.85, ui.rampPlotWidget->yAxis->range().center());

	// add star symbol for target
	QVector<QCPGraphData> targetPoint(1);
	targetPoint[0].value = 0.0;

	if (ui.rampUnitsComboBox->currentIndex() == 0)
		targetPoint[0].key = model430.targetCurrent();
	else
		targetPoint[0].key = model430.targetField();

	ui.rampPlotWidget->graph(1)->data()->set(targetPoint);

	ui.rampPlotWidget->replot();
}

//---------------------------------------------------------------------------
#ifdef USE_QTPRINTER
void magnetdaq::renderRampPlot(QPrinter *printer)
{
	QPageSize pageSize(QPageSize::Letter);
	QPageLayout pageLayout(pageSize, QPageLayout::Orientation::Landscape, QMarginsF(0.75, 0.75, 0.75, 0.75), QPageLayout::Unit::Inch);
	printer->setPageLayout(pageLayout);
	
	QCPPainter painter(printer);
	QRectF pageRect = printer->pageRect(QPrinter::DevicePixel);

	// set size on page
	int plotWidth = ui.rampPlotWidget->viewport().width();
	int plotHeight = ui.rampPlotWidget->viewport().height();

	double scale = pageRect.width() / (double)plotWidth;
	double scale2 = pageRect.height() / (double)plotHeight;

	if (scale2 < scale)
		scale = scale2;

	painter.setMode(QCPPainter::pmVectorized);
	painter.setMode(QCPPainter::pmNoCaching);

	// comment this out if you want cosmetic thin lines (always 1 pixel thick independent of pdf zoom level)
	painter.setMode(QCPPainter::pmNonCosmetic);

	painter.scale(scale, scale);
	ui.rampPlotWidget->toPainter(&painter, plotWidth, plotHeight);
	painter.drawText(-10, -10, QDateTime::currentDateTime().toString());
}
#endif

//---------------------------------------------------------------------------
void magnetdaq::resetRampPlotAxes(bool checked)
{
	int rampSegments = ui.rampSegmentsSpinBox->value();
	ui.rampPlotWidget->xAxis->setRange(-(rampSegMaxLimits[rampSegments]->text().toDouble()), rampSegMaxLimits[rampSegments]->text().toDouble());
	ui.rampPlotWidget->yAxis->setRange(-10, +10);

	ui.rampPlotWidget->replot();
}

//---------------------------------------------------------------------------
void magnetdaq::rampPlotTimebaseChanged(bool checked)
{
	// clear past data
	ui.rampPlotWidget->graph(0)->data()->clear();
	ui.rampPlotWidget->graph(1)->data()->clear();
	ui.rampPlotWidget->replot();
}

//---------------------------------------------------------------------------
void magnetdaq::rampPlotSelectionChanged(void)
{
	/*
	normally, axis base line, axis tick labels and axis labels are selectable separately, but we want
	the user only to be able to select the axis as a whole, so we tie the selected states of the tick labels
	and the axis base line together. However, the axis label shall be selectable individually.

	The selection state of the left and right axes shall be synchronized as well as the state of the
	bottom and top axes.
	*/

	// make top and bottom axes be selected synchronously, and handle axis and tick labels as one selectable object:
	if (ui.rampPlotWidget->xAxis->selectedParts().testFlag(QCPAxis::spAxis) || ui.rampPlotWidget->xAxis->selectedParts().testFlag(QCPAxis::spTickLabels) ||
		ui.rampPlotWidget->xAxis2->selectedParts().testFlag(QCPAxis::spAxis) || ui.rampPlotWidget->xAxis2->selectedParts().testFlag(QCPAxis::spTickLabels))
	{
		ui.rampPlotWidget->xAxis2->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels);
		ui.rampPlotWidget->xAxis->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels);
	}
	// make left and right axes be selected synchronously, and handle axis and tick labels as one selectable object:
	if (ui.rampPlotWidget->yAxis->selectedParts().testFlag(QCPAxis::spAxis) || ui.rampPlotWidget->yAxis->selectedParts().testFlag(QCPAxis::spTickLabels) ||
		ui.rampPlotWidget->yAxis2->selectedParts().testFlag(QCPAxis::spAxis) || ui.rampPlotWidget->yAxis2->selectedParts().testFlag(QCPAxis::spTickLabels))
	{
		ui.rampPlotWidget->yAxis2->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels);
		ui.rampPlotWidget->yAxis->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels);
	}
}

//---------------------------------------------------------------------------
void magnetdaq::rampPlotMousePress(void)
{
	// if an axis is selected, only allow the direction of that axis to be dragged
	// if no axis is selected, both directions may be dragged

	if (ui.rampPlotWidget->xAxis->selectedParts().testFlag(QCPAxis::spAxis))
		ui.rampPlotWidget->axisRect()->setRangeDrag(ui.rampPlotWidget->xAxis->orientation());
	else if (ui.rampPlotWidget->yAxis->selectedParts().testFlag(QCPAxis::spAxis))
		ui.rampPlotWidget->axisRect()->setRangeDrag(ui.rampPlotWidget->yAxis->orientation());
	else
		ui.rampPlotWidget->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
}

//---------------------------------------------------------------------------
void magnetdaq::rampPlotMouseWheel(void)
{
	// if an axis is selected, only allow the direction of that axis to be zoomed
	// if no axis is selected, both directions may be zoomed

	if (ui.rampPlotWidget->xAxis->selectedParts().testFlag(QCPAxis::spAxis))
		ui.rampPlotWidget->axisRect()->setRangeZoom(ui.rampPlotWidget->xAxis->orientation());
	else if (ui.rampPlotWidget->yAxis->selectedParts().testFlag(QCPAxis::spAxis))
		ui.rampPlotWidget->axisRect()->setRangeZoom(ui.rampPlotWidget->yAxis->orientation());
	else
		ui.rampPlotWidget->axisRect()->setRangeZoom(Qt::Horizontal | Qt::Vertical);
}

//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
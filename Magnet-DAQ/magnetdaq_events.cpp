#include "stdafx.h"
#include "magnetdaq.h"

//---------------------------------------------------------------------------
// Contains methods related to the event tabs.
// Broken out from magnetdaq.cpp for ease of editing.
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
void magnetdaq::refreshRampdownList(void)
{
	if (socket)
	{
		model430.syncEventCounts(true);

		if (model430.rampdownEventsCount() > 0)
		{
			QApplication::setOverrideCursor(Qt::WaitCursor);
			socket->sendExtendedQuery("RAMPDF?\r\n", RAMPDOWN_FILE, 10); // 10 second time limit on reply
			QApplication::restoreOverrideCursor();
		}
		else
		{
			rampdownSummaries.clear();
		}
	}
}

//---------------------------------------------------------------------------
void magnetdaq::syncRampdownEvents(QString str)
{
	// parse rampdown file into individual events
	if (!str.isEmpty())
	{
		// parse rampdown events
		QStringList titles;
		rampdownSummaries.clear();
		int index = 0;
		int eventStart, eventEnd;
		int count = 1;

		// find next event header
		while (str.indexOf("****************************************************************************", index) != -1)
		{
			eventStart = index;

			// next find event date
			index = str.indexOf("Rampdown number", index);

			if (index != -1)
			{
				index = str.indexOf("detected ", index);
				index += 9;		// should be at date of event

				titles.append("Event " + QString::number(count) + ": " + str.mid(index, 10));
			}

			index = str.indexOf("****************************************************************************", index);

			if (index != -1)
				eventEnd = index;
			else
				eventEnd = index = str.count();

			rampdownSummaries.append(str.mid(eventStart, eventEnd - eventStart));
			count++;
		}

		// fill widgets
		ui.rampdownListWidget->clear();
		ui.rampdownListWidget->addItems(titles);
		ui.rampdownListWidget->setCurrentRow(0);
	}

	QApplication::restoreOverrideCursor();
}

//---------------------------------------------------------------------------
void magnetdaq::rampdownEventSelectionChanged(int index)
{
	if (index >= 0)
		ui.rampdownEventTextEdit->setPlainText(rampdownSummaries[index]);
}

//---------------------------------------------------------------------------
void magnetdaq::refreshQuenchList(void)
{
	if (socket)
	{
		model430.syncEventCounts(true);

		if (model430.quenchEventsCount() > 0)
		{
			QApplication::setOverrideCursor(Qt::WaitCursor);
			socket->sendExtendedQuery("QUF?\r\n", QUENCH_FILE, 10); // 10 second time limit on reply
			QApplication::restoreOverrideCursor();
		}
		else
		{
			quenchSummaries.clear();
		}
	}
}

//---------------------------------------------------------------------------
void magnetdaq::syncQuenchEvents(QString str)
{
	// parse rampdown file into individual events
	if (!str.isEmpty())
	{
		// parse quench events
		QStringList titles;
		quenchSummaries.clear();
		int index = 0;
		int eventStart, eventEnd;
		int count = 1;

		// find next event header
		while (str.indexOf("****************************************************************************", index) != -1)
		{
			eventStart = index;

			// next find event date
			index = str.indexOf("Quench number", index);

			if (index != -1)
			{
				index = str.indexOf("detected ", index);
				index += 9;		// should be at date of event

				titles.append("Event " + QString::number(count) + ": " + str.mid(index, 10));
			}

			index = str.indexOf("****************************************************************************", index);

			if (index != -1)
				eventEnd = index;
			else
				eventEnd = index = str.count();

			quenchSummaries.append(str.mid(eventStart, eventEnd - eventStart));
			count++;
		}

		// fill widgets
		ui.quenchListWidget->clear();
		ui.quenchListWidget->addItems(titles);
		ui.quenchListWidget->setCurrentRow(0);
	}

	QApplication::restoreOverrideCursor();
}

//---------------------------------------------------------------------------
void magnetdaq::quenchEventSelectionChanged(int index)
{
	if (index >= 0)
		ui.quenchEventTextEdit->setPlainText(quenchSummaries[index]);
}

//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
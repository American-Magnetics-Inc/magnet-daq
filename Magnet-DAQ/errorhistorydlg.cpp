#include "stdafx.h"
#include "errorhistorydlg.h"
#include "magnetdaq.h"
#include <QObject>

magnetdaq *errorSource;

//---------------------------------------------------------------------------
errorhistorydlg::errorhistorydlg(QWidget *parent)
	: QDialog(parent)
{
	ui.setupUi(this);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	connect(ui.clearHistoryButton, SIGNAL(clicked()), this, SLOT(clearHistory()));
}

//---------------------------------------------------------------------------
errorhistorydlg::~errorhistorydlg()
{
	// save position on screen
	QSettings settings;
	QString dpiStr = QString::number(QApplication::desktop()->screen()->logicalDpiX());
	settings.setValue(savedAxisStr + "ErrorHistDlg/Geometry/" + dpiStr, saveGeometry());
}

//---------------------------------------------------------------------------
void errorhistorydlg::restoreDlgGeometry(QString axisStr)
{
	errorSource = qobject_cast<magnetdaq *>(parent());
	connect(this, SIGNAL(clearErrorHistory()), errorSource, SLOT(clearErrorHistory()));

	// restore window position
	savedAxisStr = axisStr;
	QSettings settings;
	QString dpiStr = QString::number(QApplication::desktop()->screen()->logicalDpiX());
	restoreGeometry(settings.value(savedAxisStr + "ErrorHistDlg/Geometry/" + dpiStr).toByteArray());
}

//---------------------------------------------------------------------------
void errorhistorydlg::loadErrorHistory(QStack<QString> *errorStack)
{
	// populate the errorListWidget
	ui.errorListWidget->clear();

	for (int i = (errorStack->size() - 1); i >= 0; i--)
		ui.errorListWidget->addItem(errorStack->at(i));
}

//---------------------------------------------------------------------------
void errorhistorydlg::clearErrorListWidget(void)
{
	ui.errorListWidget->clear();
}

//---------------------------------------------------------------------------
void errorhistorydlg::clearHistory(void)
{
	clearErrorListWidget();
	emit clearErrorHistory();
}


//---------------------------------------------------------------------------

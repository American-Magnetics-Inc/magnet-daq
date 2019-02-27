#pragma once

#include <QWidget>
#include <QDialog>
#include <QStack>
#include "ui_errorhistorydlg.h"

class errorhistorydlg : public QDialog
{
	Q_OBJECT

public:
	errorhistorydlg(QWidget *parent = Q_NULLPTR);
	~errorhistorydlg();

	void loadErrorHistory(QStack<QString> *);
	void restoreDlgGeometry(QString axisStr);
	void clearErrorListWidget(void);

signals:
	void clearErrorHistory(void);

private slots:
	void clearHistory(void);

private:
	Ui::errorhistorydlg ui;
	QString savedAxisStr;
};

#pragma once

#include <QtWidgets/QTableWidget>

class QTableWidgetWithCopyPaste : public QTableWidget
{
public:
	QTableWidgetWithCopyPaste(int rows, int columns, QWidget* parent) :
		QTableWidget(rows, columns, parent)
	{
		m_minimumNumCols = 1;
	};

	QTableWidgetWithCopyPaste(QWidget* parent) :
		QTableWidget(parent)
	{
		m_minimumNumCols = 1;
	};

	void saveToFile(QString filename);
	void loadFromFile(QString filename, bool makeCheckable, int skipCnt);
	void setMinimumNumCols(int numCols) { m_minimumNumCols = numCols; }
	int minimumNumCols(void) { return m_minimumNumCols; }
	void addColumn(void);
	void deleteLastColumn(void);

private:
	int m_minimumNumCols;
	void copy();
	void paste();
	void performDelete();

protected:
	virtual void keyPressEvent(QKeyEvent * event);
};

#ifndef SOCKET_H
#define SOCKET_H

#include <QObject>
#include <QtNetwork>
#include <QDebug>
#include <QQueue>
#include "model430.h"
#include <atomic>

class Socket : public QObject
{
	Q_OBJECT

public:
	Socket(QObject *parent = Q_NULLPTR);
	Socket(Model430 *settings = 0, QObject *parent = Q_NULLPTR);
	~Socket();

	void connectToModel430(QString ipaddress, quint16 port, QNetworkProxy::ProxyType aProxyType);
	void getNextDataPoint();
	bool isConnected() {return unitConnected;}
	void sendCommand(QString);
	void sendQuery(QString queryStr, QueryState aState);
	void sendExtendedQuery(QString queryStr, QueryState aState, int timelimit /*seconds*/);
	void sendRampQuery(QString queryStr, QueryState aState, int segment);
	void getFirmwareVersion();
	void getMode();
	void getState(void);
	void getStatusByte(void);
	void getIpName(void);
	void remoteLockout(bool state);

public slots:
	void sendBlockingCommand(QString aStr);

signals:
	void nextDataPoint(qint64 time, double magField, double magCurrent, double magVoltage, double supCurrent, double supVoltage, double refCurrent, quint8 state, quint8 heater);
	void updateFrontPanel(QString displayString, bool shiftLED, bool fieldLED, bool persistentLED, bool engergizedLED, bool quenchLED);
	void systemError();
	void fieldUnitsChanged();
	void startExternalRampdown();
	void endExternalRampdown();
	void systemErrorMessage(QString errMsg, QString lastStrSent);
	void model430Disconnected(void);

private slots:
	void connected();
	void disconnected();
	void readyRead();
	void bytesWritten(qint64 bytes);
	void commandTimerTimeout(void);

private:
	QTcpSocket *socket;
	QString ipAddress;
	quint16 ipPort;
	std::atomic<QueryState> queryState;
	volatile bool cmdWritten;
	double magnetField, magnetCurrent, magnetVoltage, supplyCurrent, supplyVoltage, refCurrent;
	quint8 state, heater;
	bool unitConnected;
	QQueue<QString> commandQueue;
	QTimer commandTimer;
	QString replyBuffer;

	// Model 430 settings
	Model430 *model430;
	QString firmwareVersion;
	int rampSegment;	// present ramp segment for query/command
};

#endif // SOCKET_H

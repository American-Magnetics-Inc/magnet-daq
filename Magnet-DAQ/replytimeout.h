#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QEvent>

//---------------------------------------------------------------------------
// ReplyTimeout class (tracks timed out NetworkManager requests)
//---------------------------------------------------------------------------
class ReplyTimeout : public QObject
{
	Q_OBJECT
		QBasicTimer m_timer;

public:
	static bool timeoutOccurred;

	ReplyTimeout(QNetworkReply* reply, const int timeout) : QObject(reply)
	{
		Q_ASSERT(reply);

		timeoutOccurred = false;

		if (reply && reply->isRunning())
			m_timer.start(timeout, this);
	}

	static void set(QNetworkReply* reply, const int timeout)
	{
		new ReplyTimeout(reply, timeout);
	}

	static bool replyTimeout(void) { return timeoutOccurred; }

protected:
	void timerEvent(QTimerEvent * ev)
	{
		if (!m_timer.isActive() || ev->timerId() != m_timer.timerId())
			return;

		auto reply = static_cast<QNetworkReply*>(parent());

		if (reply->isRunning())
		{
			timeoutOccurred = true;
			reply->close();
		}

		m_timer.stop();
	}
};
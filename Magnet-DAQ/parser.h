#ifndef PARSER_H
#define PARSER_H

#include <QObject>
#include "model430.h"

class Parser : public QObject
{
	Q_OBJECT

public:
	Parser(QObject *parent);
	~Parser();
	void setDataSource(Model430 *src) { model430 = src; }
	void stop(void) { stopProcessing = true; }

public slots:
	void process(void);

signals:
	void finished();
	void error(QString err);

private:
	// add your variables here
	bool stopProcessing;
	Model430 *model430;
	QString inputStr;

	void parseInput(char *commbuf, char* outputBuffer);
	void parse_query_C(char* word, char* outputBuffer);
	void parse_query_F(char* word, char* outputBuffer);
	void parse_query_Q(char* word, char* outputBuffer);
	void parse_configure(const char* word, char *outputBuffer);
	void parse_configure_C(const char* word, char *outputBuffer);
	void parse_configure_F(const char* word, char *outputBuffer);
	void parse_configure_I(const char* word, char *outputBuffer);
	void parse_configure_P(const char* word, char *outputBuffer);
	void parse_configure_R(const char* word, char *outputBuffer);
	void parse_configure_S(const char* word, char *outputBuffer);
	void parse_configure_V(const char* word, char *outputBuffer);
};

#endif // PARSER_H
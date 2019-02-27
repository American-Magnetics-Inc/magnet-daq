#ifndef PARSER_H
#define PARSER_H

#include <QObject>
#include <QStack>
#include "model430.h"

//---------------------------------------------------------------------------
// Type declarations
//---------------------------------------------------------------------------

typedef enum
{
	ERR_NONE = 0,
	ERR_UNRECOGNIZED_COMMAND = -101,
	ERR_INVALID_ARGUMENT = -102,
	ERR_NON_BOOLEAN_ARGUMENT = -103,
	ERR_MISSING_PARAMETER = -104,
	ERR_OUT_OF_RANGE = -105,
	ERR_NO_COIL_CONSTANT = -106,
	ERR_NON_NUMERICAL_ENTRY = -151,

	ERR_UNRECOGNIZED_QUERY = -201,

} SystemError;


//---------------------------------------------------------------------------
// Parser class
//---------------------------------------------------------------------------
class Parser : public QObject
{
	Q_OBJECT

public:
	Parser(QObject *parent);
	~Parser();
	void _setParent(QObject *p) { _parent = p; }
	void setDataSource(Model430 *src) { model430 = src; }
	void stop(void);
	void addSystemError(QString errMsg);

public slots:
	void process(void);

signals:
	void finished();
	void error_msg(QString err);
	void sendBlockingCommand(QString aStr);
	void configurationChanged(QueryState aState);
	void exit_app(void);

private:
	// add your variables here
	std::atomic<bool> stopParsing;
	Model430 *model430;
	QObject *_parent;
	QString inputStr;
	QStack<QString> errorStack;

	void addToErrorQueue(SystemError error);
	void parseInput(char *commbuf, char* outputBuffer);
	void parse_query_C(char* word, char* outputBuffer);
	void parse_query_F(char* word, char* outputBuffer);
	void parse_query_I(char* word, char* outputBuffer);
	void parse_query_P(char* word, char* outputBuffer);
	void parse_query_Q(char* word, char* outputBuffer);
	void parse_query_R(char* word, char* outputBuffer);
	void parse_query_S(char* word, char* outputBuffer);
	void parse_query_V(char* word, char* outputBuffer);
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
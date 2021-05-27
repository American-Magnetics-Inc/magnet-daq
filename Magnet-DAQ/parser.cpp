#include "stdafx.h"
#include "parser.h"
#include "socket.h"
#include <iostream>

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#endif

#define DELIMITER	":"		// colon
#define SEPARATOR ": \t"	// colon, space, or tab
#define SPACE " \t"			// space or tab
#define COMMA ","			// comma

/************************************************************
	This file is designed to support using Magnet-DAQ as a
	slave QProcess to another application. It exposes a
	subset of the Model 430 remote interface commands and
	queries. This allows a master application to use Magnet-DAQ
	as the interface to the Model 430 without programming the
	TCP/IP communication directly.

	The primary purpose for this functionality to date is the
	support of 2- and 3-axis AMI magnet systems. A Magnet-DAQ
	process is opened for each axis.

	The Parser::process() method executes in a separate thread
	from the user interface. I/O uses stdin and stdout.

	To enable the parser function, use the command line
	argument "-p" on Magnet-DAQ launch.

	Please note that the QProcess functionality is not available
	for UWP (Universal Windows) apps as the sandboxing does not
	allow this type of interprocess communication.
************************************************************/

//---------------------------------------------------------------------------
// Command and query parsing tokens
//---------------------------------------------------------------------------
const char _SYST[] = "SYST";
const char _SYSTEM[] = "SYSTEM";
const char _ERR[] = "ERR";
const char _ERROR[] = "ERROR";
const char _EXIT[] = "EXIT";
const char _COUN[] = "COUN";
const char _COUNT[] = "COUNT";
const char _CLS[] = "*CLS";
const char _CONFIGURE[] = "CONFIGURE";
const char _CONF[] = "CONF";
const char _IDN[] = "*IDN";
const char _INDUCTANCE[] = "INDUCTANCE";
const char _IND[] = "IND";
const char _PAUSE[] = "PAUSE";
const char _PSWITCH[] = "PSWITCH";
const char _PS[] = "PS";
const char _RAMP[] = "RAMP";
const char _ZERO[] = "ZERO";
const char _CURR[] = "CURR";
const char _CURRENT[] = "CURRENT";
const char _VOLT[] = "VOLT";
const char _VOLTAGE[] = "VOLTAGE";
const char _LIM[] = "LIM";
const char _LIMIT[] = "LIMIT";
const char _SUPP[] = "SUPP";
const char _SUPPLY[] = "SUPPLY";
const char _MAG[] = "MAG";
const char _MAGNET[] = "MAGNET";
const char _TARG[] = "TARG";
const char _TARGET[] = "TARGET";
const char _COIL[] = "COIL";
const char _COILCONST[] = "COILCONST";
const char _FIELD[] = "FIELD";
const char _UNITS[] = "UNITS";
const char _STAB[] = "STAB";
const char _STABILITY[] = "STABILITY";
const char _RES[] = "RES";
const char _RESISTOR[] = "RESISTOR";
const char _MODE[] = "MODE";
const char _RATE[] = "RATE";
const char _SEG[] = "SEG";
const char _SEGMENTS[] = "SEGMENTS";
const char _STATE[] = "STATE";
const char _QU[] = "QU";
const char _QUENCH[] = "QUENCH";
const char _POWERSUPPLYRAMPRATE[] = "POWERSUPPLYRAMPRATE";
const char _PSRR[] = "PSRR";
const char _HTIME[] = "HTIME";
const char _HEATTIME[] = "HEATTIME";
const char _CTIME[] = "CTIME";
const char _COOLTIME[] = "COOLTIME";
const char _CGAIN[] = "CGAIN";
const char _COOLINGGAIN[] = "COOLINGGAIN";
const char _TRAN[] = "TRAN";
const char _TRANSITION[] = "TRANSITION";
const char _INST[] = "INST";
const char _INSTALLED[] = "INSTALLED";
const char _PERS[] = "PERS";
const char _PERSISTENT[] = "PERSISTENT";

//---------------------------------------------------------------------------
// Incoming data type tests and conversions
//---------------------------------------------------------------------------
char *struprt(char *str)
{
	if (str == NULL)
		return NULL;

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
	char *next = str;
	while (*next != '\0')
	{
		*next = toupper((unsigned char)*next);
		next++;
	}
#else
	str = _strupr(str);	// convert to all uppercase
#endif
	return str;
}

//---------------------------------------------------------------------------
char *trimwhitespace(char *str)
{
	char *end;

	// Trim leading space
	while (isspace((unsigned char)*str)) str++;

	if (*str == 0)  // All spaces?
		return str;

	// Trim trailing space
	end = str + strlen(str) - 1;
	while (end > str && isspace((unsigned char)*end)) end--;

	// Write new null terminator character
	end[1] = '\0';

	return str;
}

//---------------------------------------------------------------------------
bool isValue(const char* buf)
{
	short digits = 0;   // digits
	short sign = 0;     // optional sign
	short dec = 0;      // optional mantissa decimal
	short exp = 0;      // optional exponent indicator 'E'

	// must check for this first!
	if (buf == NULL)
		return false;

	// skip all leading whitespace
	// no whitespace can occur within rest of string
	// string is null terminated
	while (isspace(*buf))
		buf++;	// next character, NULL not considered whitespace

	// search for mantissa sign and digits
	while (*buf != '\0' && !exp)
	{
		if (*buf == '+' || *buf == '-')
		{
			sign++;

			if (sign > 1)	// more than one sign or it followed a digit/decimal
				return false;

			buf++;			// next character
		}
		else if (isdigit(*buf))
		{
			digits++;
			sign++;			// sign can't follow digit
			buf++;			// next character
		}
		else if (*buf == '.')
		{
			dec++;
			sign++;			// sign can't follow decimal

			if (dec > 1)	// more than one decimal
				return false;

			buf++;			// next character
		}
		else if (*buf == 'E')
		{	// found exponent
			if (!digits)	// need at least one mantissa digit before an exponent
				return false;

			exp++;
			buf++;
		}
		else
		{
			// unacceptable character or symbol
			return false;
		}
	}	// end while()

	if (!digits)		// must have at least one mantissa digit
		return false;

	// possibly in exponent portion, search for exponent sign and digits
	sign = 0;
	digits = 0;
	while (*buf != '\0' && exp)
	{
		if (*buf == '+' || *buf == '-')
		{
			sign++;
			if (sign > 1)	// more than one sign or it followed a digit/decimal
				return false;
			buf++;			// next character
		}
		else if (isdigit(*buf))
		{
			digits++;
			sign++;			// sign can't follow digit
			buf++;			// next character
		}
		else
		{
			// unacceptable character or symbol
			return false;
		}
	}	// end while()

	if (exp && !digits)	// if there's an exponent, need at least one digit
		return false;

	return true;	// all tests satisfied, valid number
}

//---------------------------------------------------------------------------
// Class methods
//---------------------------------------------------------------------------
Parser::Parser(QObject *parent)
	: QObject(parent)
{
	stopParsing = false;
	model430 = NULL;
	_parent = NULL;
}

//---------------------------------------------------------------------------
Parser::~Parser()
{
	stopParsing = true;
	qDebug("Magnet-DAQ stdin Parser End");
}

//---------------------------------------------------------------------------
void Parser::stop(void)
{
	// NOTE: Due to the getline() call in process(), this flag won't immediately
	// stop the thread because getline() blocks until receipt of input on stdin.
	// This is generally not a problem if the entire app exits when an instrument
	// connection is closed. If the app does not exit, the first command after
	// reconnect is ignored because it is consumed by the blocking getline()
	// still running in the previous thread -- after this consumption, the
	// old thread immediately exits and everything proceeds normally.
	stopParsing = true;
}

//---------------------------------------------------------------------------
// --- PROCESS THREAD ---
// Start processing data.
void Parser::process(void)
{
	if (model430 == NULL)
	{
		qDebug("Magnet-DAQ parser aborted; no data source specified");
	}
	else
	{
		// connect send command slot
		connect(this, SIGNAL(sendBlockingCommand(QString)), model430->getSocket(), SLOT(sendBlockingCommand(QString)));
		connect(this, SIGNAL(configurationChanged(QueryState)), _parent, SLOT(configurationChanged(QueryState)));
		stopParsing = false;

		// allocate resources and start parsing
		qDebug("Magnet-DAQ stdin Parser Start");
		char input[1024];
		char output[1024];

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
		fd_set read_fds;
        int sfd=STDIN_FILENO, result;
#endif
		while (!stopParsing)
		{
			input[0] = '\0';
			output[0] = '\0';

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
			//we want to receive data from stdin so add these file
			//descriptors to the file descriptor set. These also have to be reset
			//within the loop since select modifies the sets.
			// MM@AMI: I have no idea why this is required to get stdin to work
			FD_ZERO(&read_fds);
            FD_SET(sfd, &read_fds);

			result = select(sfd + 1, &read_fds, NULL, NULL, NULL);

			if (result == -1 && errno != EINTR)
			{
                qDebug("Magnet-DAQ parser aborted; error in select()");
				std::cerr << "Error in select: " << strerror(errno) << "\n";
				break;
			}
			else if (result == -1 && errno == EINTR)
			{
				//we've received an interrupt - handle this
                qDebug("Magnet-DAQ parser aborted; received unknown interrupt");
				break;
			}
			else
			{
				if (FD_ISSET(STDIN_FILENO, &read_fds))
				{
					std::cin.getline(input, sizeof(input));	// this blocks until input
				}
			}
#else
			std::cin.getline(input, sizeof(input));	// this blocks until input
#endif

#ifdef DEBUG
			if (input[0] != '\0')
				qDebug() << QString(input);
#endif
			struprt(input);	// convert to all uppercase

			// save original string
			inputStr = QString(input);

			// parse stdin
			parseInput(input, output);
		}

		disconnect(this, SIGNAL(sendBlockingCommand(QString)), model430->getSocket(), SLOT(sendBlockingCommand(QString)));
		disconnect(this, SIGNAL(configurationChanged(QueryState)), _parent, SLOT(configurationChanged(QueryState)));
	}

	emit finished();
}

//---------------------------------------------------------------------------
void Parser::addToErrorQueue(SystemError error)
{
	QString errMsg;

	switch (error)
	{
	case ERR_UNRECOGNIZED_COMMAND:
		errMsg = QString::number((int)ERR_UNRECOGNIZED_COMMAND) + ",\"Unrecognized command\"";
		break;

	case ERR_INVALID_ARGUMENT:
		errMsg = QString::number((int)ERR_INVALID_ARGUMENT) + ",\"Invalid argument\"";
		break;

	case ERR_NON_BOOLEAN_ARGUMENT:
		errMsg = QString::number((int)ERR_NON_BOOLEAN_ARGUMENT) + ",\"Non-boolean argument\"";
		break;

	case ERR_MISSING_PARAMETER:
		errMsg = QString::number((int)ERR_MISSING_PARAMETER) + ",\"Missing parameter\"";
		break;

	case ERR_OUT_OF_RANGE:
		errMsg = QString::number((int)ERR_OUT_OF_RANGE) + ",\"Value out of range\"";
		break;

	case ERR_NO_COIL_CONSTANT:
		errMsg = QString::number((int)ERR_NO_COIL_CONSTANT) + ",\"Undefined coil const\"";
		break;

	case ERR_NON_NUMERICAL_ENTRY:
		errMsg = QString::number((int)ERR_NON_NUMERICAL_ENTRY) + ",\"Non-numerical entry\"";
		break;

	case ERR_UNRECOGNIZED_QUERY:
		errMsg = QString::number((int)ERR_UNRECOGNIZED_QUERY) + ",\"Unrecognized query\"";
		break;

	default:
		errMsg = QString::number((int)error) + ",\"Error\"";
		break;
	}

	if (!errMsg.isEmpty())
	{
		errorStack.push(errMsg);
		emit error_msg(errMsg);

#if defined(Q_OS_WIN)
		Beep(1000, 600);
#else
		QApplication::beep();
#endif
	}
}

//---------------------------------------------------------------------------
void Parser::addSystemError(QString errMsg)
{
	if (!errMsg.isEmpty())
		errorStack.push(errMsg);
}

//---------------------------------------------------------------------------
void Parser::parseInput(char *commbuf, char* outputBuffer)
{
	char* word;    // string tokens
	char* value;   // start of argument
	char* pos;

	pos = strchr(commbuf, '?');

	/************************************************************
	The command is a query.
	************************************************************/
	if (pos != NULL)
	{		// found a ?
			// terminate string at the question mark
		*pos = '\0';

		// get first token
		word = strtok(commbuf, DELIMITER);

		if (word == NULL)
		{
			addToErrorQueue(ERR_UNRECOGNIZED_QUERY);
			return; // no match
		}

		// switch on first character
		switch (*word)
		{
			// tests: *IDN?,
			case  '*':
				if (!strcmp(word, _IDN))
				{
					QString version(qApp->applicationName() + "," + qApp->applicationVersion() + "\n");
					std::cout.write(version.toLocal8Bit(), version.size());
				}
				else
				{
					addToErrorQueue(ERR_UNRECOGNIZED_QUERY);	// no match, error
				}
				break;

			// I* queries
			case  'I':
				parse_query_I(word, outputBuffer);
				break;

			// S* queries
			case  'S':
				parse_query_S(word, outputBuffer);
				break;

			// V* queries
			case  'V':
				parse_query_V(word, outputBuffer);
				break;

			// C* queries
			case  'C':
				parse_query_C(word, outputBuffer);
				break;

			// P* queries
			case  'P':
				parse_query_P(word, outputBuffer);
				break;

			// Q* queries
			case  'Q':
				parse_query_Q(word, outputBuffer);
				break;

			// R* queries
			case  'R':
				parse_query_R(word, outputBuffer);
				break;

			// F* queries
			case  'F':
				parse_query_F(word, outputBuffer);
				break;

			// O* queries
			case 'O':
				addToErrorQueue(ERR_UNRECOGNIZED_QUERY);	// no match, error
				//parse_query_O(word, outputBuffer);
				break;

			// no match
			default:
				addToErrorQueue(ERR_UNRECOGNIZED_QUERY);
				break;
		}	// end switch on first word

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
		std::cout.flush();	// needed on Linux
#endif
	}

	/************************************************************
	The command is not a query (no return data).
	************************************************************/
	else
	{
		// get first token
		word = strtok(commbuf, SEPARATOR);
		if (word == NULL)
		{
			addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
			return; // no match
		}

		// switch on first character
		switch (*word)
		{
			// tests *CLS
			case '*':
				if (!strcmp(word, _CLS))
				{
					errorStack.clear();
				}
				else
				{
					addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
				}
				break;

				// tests EXIT
			case 'E':
				if (strcmp(word, _EXIT) == 0)
				{
					emit exit_app();
				}
				else
				{
					addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
				}
				break;

			// tests all CONFigure commands
			case  'C':
				if (strcmp(word, _CONF) == 0 || strcmp(word, _CONFIGURE) == 0)
				{
					// get next token
					word = strtok(NULL, SEPARATOR);
					if (word == NULL)
					{
						addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
					}
					else
					{
						parse_configure(word, outputBuffer);
					}
				}
				else
				{
					addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
				}
				break;

			// tests RAMP
			case  'R':
				if (strcmp(word, _RAMP) == 0)
				{
					// get next token
					word = strtok(NULL, DELIMITER);

					// Check for the NULL condition here to make sure there are not additional args
					if (word == NULL)
					{
						emit sendBlockingCommand(inputStr + "\r\n");
					}
					else
					{
						addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
					}
				}
				else
				{
					addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
				}
				break;

			// tests PSwitch 0|1,PAUSE
			case  'P':
				if (strcmp(word, _PS) == 0 || strcmp(word, _PSWITCH) == 0)
				{
					value = strtok(NULL, SPACE);		// look for value

					if (isValue(value))
					{
						while (isspace(*value))
							value++;

						if (*value == '1' || *value == '0')
						{
							emit sendBlockingCommand(inputStr + "\r\n");
						}
						else
						{
							addToErrorQueue(ERR_NON_BOOLEAN_ARGUMENT);
						}
					}
					else
					{
						if (value == NULL)
							addToErrorQueue(ERR_MISSING_PARAMETER);
						else
							addToErrorQueue(ERR_INVALID_ARGUMENT);
					}

				}
				else if (strcmp(word, _PAUSE) == 0)
				{
					emit sendBlockingCommand(inputStr + "\r\n");
				}
				else
				{
					addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
				}
				break;

				// tests ZERO
			case  'Z':
				if (strcmp(word, _ZERO) == 0)
				{
					// get next token
					word = strtok(NULL, DELIMITER);

					// Check for the NULL condition here to make sure there are not additional args
					if (word == NULL)
					{
						emit sendBlockingCommand(inputStr + "\r\n");
					}
					else
					{
						addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
					}
				}
				else
				{
					addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
				}
				break;

			default:
				addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
				break;
		}
	}
}

//---------------------------------------------------------------------------
// tests  CURRent:TARGet?, CURRent:MAGnet?, CURRent:SUPPly?
//        CURRent:LIMit?,  COILconst?
//---------------------------------------------------------------------------
void Parser::parse_query_C(char* word, char* outputBuffer)
{
	if (strcmp(word, _CURR) == 0 || strcmp(word, _CURRENT) == 0)
	{
		word = strtok(NULL, DELIMITER);		// get next token

		if (word == NULL)
		{
			addToErrorQueue(ERR_UNRECOGNIZED_QUERY);	// no match, error
		}

		// CURRent:LIMit?
		else if (strcmp(word, _LIM) == 0 || strcmp(word, _LIMIT) == 0)
		{
			sprintf(outputBuffer, "%0.10g\n", model430->currentLimit());
			std::cout.write(outputBuffer, strlen(outputBuffer));
		}

		// CURRent:SUPPly?
		else if (strcmp(word, _SUPP) == 0 || strcmp(word, _SUPPLY) == 0)
		{
			sprintf(outputBuffer, "%0.10g\n", model430->supplyCurrent);
			std::cout.write(outputBuffer, strlen(outputBuffer));
		}

		// CURRent:MAGnet?
		else if (strcmp(word, _MAG) == 0 || strcmp(word, _MAGNET) == 0)
		{
			sprintf(outputBuffer, "%0.10g\n", model430->magnetCurrent);
			std::cout.write(outputBuffer, strlen(outputBuffer));
		}

		// CURRent:TARGet?
		else if (strcmp(word, _TARG) == 0 || strcmp(word, _TARGET) == 0)
		{
			sprintf(outputBuffer, "%0.10g\n", model430->targetCurrent());
			std::cout.write(outputBuffer, strlen(outputBuffer));
		}

		else
		{
			addToErrorQueue(ERR_UNRECOGNIZED_QUERY);	// no match, error
		}
	}
	else if (strcmp(word, _COIL) == 0 || strcmp(word, _COILCONST) == 0)
	{
		sprintf(outputBuffer, "%0.10g\n", model430->coilConstant());
		std::cout.write(outputBuffer, strlen(outputBuffer));
	}
	else
	{
		addToErrorQueue(ERR_UNRECOGNIZED_QUERY);	// no match, error
	}
}

//---------------------------------------------------------------------------
// tests FIELD:MAGnet?, FIELD:TARGet?, FIELD:UNITS?
//---------------------------------------------------------------------------
void Parser::parse_query_F(char* word, char* outputBuffer)
{
	if (strcmp(word, _FIELD) == 0)
	{
		word = strtok(NULL, DELIMITER);	// get next token

		if (word == NULL)
		{
			addToErrorQueue(ERR_UNRECOGNIZED_QUERY);	// no match, error
		}
		else if (strcmp(word, _MAG) == 0 || strcmp(word, _MAGNET) == 0)
		{
			if (model430->coilConstant() > 0.0)
			{
				sprintf(outputBuffer, "%0.10g\n", model430->magnetField);
				std::cout.write(outputBuffer, strlen(outputBuffer));
			}
			else
			{
				addToErrorQueue(ERR_NO_COIL_CONSTANT);	// error -- no coil constant defined
			}
		}
		else if (strcmp(word, _TARG) == 0 || strcmp(word, _TARGET) == 0)
		{
			if (model430->coilConstant() > 0.0)
			{
				sprintf(outputBuffer, "%0.10g\n", model430->targetField());
				std::cout.write(outputBuffer, strlen(outputBuffer));
			}
			else
			{
				addToErrorQueue(ERR_NO_COIL_CONSTANT);	// error -- no coil constant defined
			}
		}
		else if (strcmp(word, _UNITS) == 0)
		{
			sprintf(outputBuffer, "%d\n", model430->fieldUnits());
			std::cout.write(outputBuffer, strlen(outputBuffer));
		}
		else
		{
			addToErrorQueue(ERR_UNRECOGNIZED_QUERY);	// no match, error
		}
	}
	else
	{
		addToErrorQueue(ERR_UNRECOGNIZED_QUERY);	// no match, error
	}
}

//---------------------------------------------------------------------------
// tests INDuctance?
//---------------------------------------------------------------------------
void Parser::parse_query_I(char* word, char* outputBuffer)
{
	if (strcmp(word, _IND) == 0 || strcmp(word, _INDUCTANCE) == 0)
	{
		word = strtok(NULL, DELIMITER);		// get next token

		if (word == NULL)	// return present inductance
		{
			sprintf(outputBuffer, "%0.2f\n", model430->inductance());
			std::cout.write(outputBuffer, strlen(outputBuffer));
		}
		else
		{
			addToErrorQueue(ERR_UNRECOGNIZED_QUERY);	// no match, error
		}
	}
	else
	{
		addToErrorQueue(ERR_UNRECOGNIZED_QUERY);	// no match, error
	}
}

//---------------------------------------------------------------------------
// tests PSwitch:INSTalled?, PSwitch:CURRent?, PSwitch:TRANsition?,
// PSwitch:HeatTIME?, PSwitch:CoolTime?, PSwitch:PowerSupplyRampeRate?
// PSwitch:CoolingGAIN?
//---------------------------------------------------------------------------
void Parser::parse_query_P(char* word, char* outputBuffer)
{
	if (strcmp(word, _PS) == 0 || strcmp(word, _PSWITCH) == 0)
	{
		word = strtok(NULL, DELIMITER);		// get next token

		if (word == NULL)
		{
			sprintf(outputBuffer, "%d\n", (int)model430->switchHeaterState);
			std::cout.write(outputBuffer, strlen(outputBuffer));
		}
		else if (strcmp(word, _CURR) == 0 || strcmp(word, _CURRENT) == 0)
		{
			sprintf(outputBuffer, "%0.1f\n", model430->switchCurrent());
			std::cout.write(outputBuffer, strlen(outputBuffer));
		}
		else if (strcmp(word, _HTIME) == 0 || strcmp(word, _HEATTIME) == 0)
		{
			sprintf(outputBuffer, "%d\n", model430->switchHeatedTime());
			std::cout.write(outputBuffer, strlen(outputBuffer));
		}
		else if (strcmp(word, _CTIME) == 0 || strcmp(word, _COOLTIME) == 0)
		{
			sprintf(outputBuffer, "%d\n", model430->switchCooledTime());
			std::cout.write(outputBuffer, strlen(outputBuffer));
		}
		else if (strcmp(word, _CGAIN) == 0 || strcmp(word, _COOLINGGAIN) == 0)
		{
			sprintf(outputBuffer, "%0.1f\n", model430->switchCoolingGain());
			std::cout.write(outputBuffer, strlen(outputBuffer));
		}
		else if (strcmp(word, _INST) == 0 || strcmp(word, _INSTALLED) == 0)
		{
			sprintf(outputBuffer, "%d\n", (int)model430->switchInstalled());
			std::cout.write(outputBuffer, strlen(outputBuffer));
		}
		else if (strcmp(word, _PSRR) == 0 || strcmp(word, _POWERSUPPLYRAMPRATE) == 0)
		{
			sprintf(outputBuffer, "%0.1f\n", model430->cooledSwitchRampRate());
			std::cout.write(outputBuffer, strlen(outputBuffer));
		}
		else if (strcmp(word, _TRAN) == 0 || strcmp(word, _TRANSITION) == 0)
		{
			sprintf(outputBuffer, "%d\n", model430->switchTransition());
			std::cout.write(outputBuffer, strlen(outputBuffer));
		}
		else
		{
			addToErrorQueue(ERR_UNRECOGNIZED_QUERY);	// no match, error
		}
	}
	else if (strcmp(word, _PERS) == 0 || strcmp(word, _PERSISTENT) == 0)
	{
		sprintf(outputBuffer, "%d\n", (int)model430->persistentState);
		std::cout.write(outputBuffer, strlen(outputBuffer));
	}
	else
	{
		addToErrorQueue(ERR_UNRECOGNIZED_QUERY);	// no match, error
	}
}

//---------------------------------------------------------------------------
// tests QUench:CURRent?
//---------------------------------------------------------------------------
void Parser::parse_query_Q(char* word, char* outputBuffer)
{
	if (strcmp(word, _QU) == 0 || strcmp(word, _QUENCH) == 0)
	{
		word = strtok(NULL, DELIMITER);	// get next token

		if (word == NULL)
		{
			addToErrorQueue(ERR_UNRECOGNIZED_QUERY);	// no match, error
		}
		else if (strcmp(word, _CURR) == 0 || strcmp(word, _CURRENT) == 0)
		{
			sprintf(outputBuffer, "%0.10g\n", model430->quenchCurrent);
			std::cout.write(outputBuffer, strlen(outputBuffer));
		}
		else
		{
			addToErrorQueue(ERR_UNRECOGNIZED_QUERY);	// no match, error
		}
	}
	else
	{
		addToErrorQueue(ERR_UNRECOGNIZED_QUERY);	// no match, error
	}
}

//---------------------------------------------------------------------------
// tests RAMP:RATE:SEGments?, RAMP:RATE:UNITS?, RATE:RATE:CURRent:<segment>?,
// RAMP:RATE:FIELD:<segment>?
//---------------------------------------------------------------------------
void Parser::parse_query_R(char* word, char* outputBuffer)
{
	char* value;   // start of argument

	if (strcmp(word, _RAMP) == 0)
	{
		word = strtok(NULL, DELIMITER);	// get next token

		if (word == NULL)
		{
			addToErrorQueue(ERR_UNRECOGNIZED_QUERY);	// no match, error
		}

		// RAMP:RATE query group or RAMPDown:RATE query group
		if (strcmp(word, _RATE) == 0)
		{
			word = strtok(NULL, DELIMITER);	// get next token

			// we should check the NULL condition here since ramp:rate? isn't valid
			if (word == NULL)
			{
				addToErrorQueue(ERR_UNRECOGNIZED_QUERY);	// no match, error
			}

			// RAMP:RATE:CURRent query group
			else if (strcmp(word, _CURR) == 0 || strcmp(word, _CURRENT) == 0)
			{
				value = strtok(NULL, DELIMITER);		// look for value

				// we should check the NULL condition here since ramp:rate:curr? is not valid
				if (value == NULL)
				{
					addToErrorQueue(ERR_UNRECOGNIZED_QUERY);	// no match, error
					return;
				}

				if (isValue(value))
				{
					int checkValue = (int)strtod(value, NULL) - 1;

					if (checkValue >= 0 && checkValue < model430->rampRateSegments())
					{
						double current = model430->currentRampLimits[checkValue]();
						double rate = model430->currentRampRates[checkValue]();

						sprintf(outputBuffer, "%0.10g,%0.10g\n", rate, current);
						std::cout.write(outputBuffer, strlen(outputBuffer));
					}
					else
					{
						addToErrorQueue(ERR_OUT_OF_RANGE);
					}
				}
				else
				{
					addToErrorQueue(ERR_INVALID_ARGUMENT);
				}
			}

			// RAMP:RATE:FIELD query group
			else if (strcmp(word, _FIELD) == 0)
			{
				if (model430->coilConstant() > 0.0)
				{
					value = strtok(NULL, DELIMITER);		// look for value

					// we should check the NULL condition here since ramp:rate:field? isn't valid
					if (value == NULL)
					{
						addToErrorQueue(ERR_UNRECOGNIZED_QUERY);	// no match, error
						return;
					}

					if (isValue(value))
					{
						int checkValue = (int)strtod(value, NULL) - 1;

						if (checkValue >= 0 && checkValue < model430->rampRateSegments())
						{
							double current = model430->fieldRampLimits[checkValue]();
							double rate = model430->fieldRampRates[checkValue]();

							sprintf(outputBuffer, "%0.10g,%0.10g\n", rate, current);
							std::cout.write(outputBuffer, strlen(outputBuffer));
						}
						else
						{
							addToErrorQueue(ERR_OUT_OF_RANGE);	// ramp index out of range
						}
					}
					else
					{
						addToErrorQueue(ERR_INVALID_ARGUMENT);
					}
				}
				else
				{
					addToErrorQueue(ERR_NO_COIL_CONSTANT);	// error -- no coil constant defined
				}
			}

			// RAMP:RATE:UNITS?
			else if (strcmp(word, _UNITS) == 0)
			{
				sprintf(outputBuffer, "%d\n", model430->rampRateTimeUnits());
				std::cout.write(outputBuffer, strlen(outputBuffer));
			}

			// RAMP:RATE:SEGments query group
			else if (strcmp(word, _SEG) == 0 || strcmp(word, _SEGMENTS) == 0)
			{
				sprintf(outputBuffer, "%d\n", model430->rampRateSegments());
				std::cout.write(outputBuffer, strlen(outputBuffer));
			}

			else
			{
				addToErrorQueue(ERR_UNRECOGNIZED_QUERY); //no match, error
			}
		}
		else
		{
			addToErrorQueue(ERR_UNRECOGNIZED_QUERY); //no match, error
		}
	}
	else
	{
		addToErrorQueue(ERR_UNRECOGNIZED_QUERY); //no match, error
	}
}

//---------------------------------------------------------------------------
// tests STATE?, SYSTem:ERRor?, SYSTem:COUNt?, STABility?, STABility:MODE?,
// STABility:RESistor?
//---------------------------------------------------------------------------
void Parser::parse_query_S(char* word, char* outputBuffer)
{
	if (!strcmp(word, _STATE))
	{
		QString tmpStr(QString::number((int)(model430->state())) + "\n");
		std::cout.write(tmpStr.toLocal8Bit(), tmpStr.size());
	}
	else if (strcmp(word, _SYST) == 0 || strcmp(word, _SYSTEM) == 0)
	{
		// get next token
		word = strtok(NULL, SEPARATOR);

		if (word == NULL)
		{
			addToErrorQueue(ERR_UNRECOGNIZED_QUERY); // no match, error
		}
		else if (strcmp(word, _ERR) == 0 || strcmp(word, _ERROR) == 0)
		{
			// get next token
			word = strtok(NULL, SEPARATOR);

			if (word == NULL)
			{
				if (errorStack.count())
					sprintf(outputBuffer, "%s\n", errorStack.pop().toLocal8Bit().constData());
				else
					sprintf(outputBuffer, "0,\"No error\"\n");

				std::cout.write(outputBuffer, strlen(outputBuffer));
			}
			else if (strcmp(word, _COUN) == 0 || strcmp(word, _COUNT) == 0)
			{
				// get next token
				word = strtok(NULL, SEPARATOR);

				if (word == NULL)
				{
					sprintf(outputBuffer, "%d\n", errorStack.count());
					std::cout.write(outputBuffer, strlen(outputBuffer));
				}
			}
			else
			{
				addToErrorQueue(ERR_UNRECOGNIZED_QUERY); // no match, error
			}
		}
		else
		{
			addToErrorQueue(ERR_UNRECOGNIZED_QUERY); //no match, error
		}
	}
	else if (strcmp(word, _STAB) == 0 || strcmp(word, _STABILITY) == 0)
	{
		// get next token
		word = strtok(NULL, DELIMITER);

		if (word == NULL)	// return stability setting
		{
			sprintf(outputBuffer, "%0.10g\n", model430->stabilitySetting());
			std::cout.write(outputBuffer, strlen(outputBuffer));
		}
		else if (strcmp(word, _MODE) == 0)	// return stability mode
		{
			sprintf(outputBuffer, "%d\n", model430->stabilityMode());
			std::cout.write(outputBuffer, strlen(outputBuffer));
		}
		else if (strcmp(word, _RES) == 0 || strcmp(word, _RESISTOR) == 0)	// return stabilizing resistor installed?
		{
			sprintf(outputBuffer, "%d\n", (int)model430->stabilityResistor());
			std::cout.write(outputBuffer, strlen(outputBuffer));
		}
		else
		{
			addToErrorQueue(ERR_UNRECOGNIZED_QUERY);	// no match
		}
	}
	else
	{
		addToErrorQueue(ERR_UNRECOGNIZED_QUERY);	// no match, error
	}
}

//---------------------------------------------------------------------------
// tests VOLTage:LIMit?, VOLTage:MAGnet?, VOLTage:SUPPly?
//---------------------------------------------------------------------------
void Parser::parse_query_V(char* word, char* outputBuffer)
{
	if (strcmp(word, _VOLT) == 0 || strcmp(word, _VOLTAGE) == 0)
	{
		word = strtok(NULL, DELIMITER);		// get next token
		if (word == NULL)
		{
			addToErrorQueue(ERR_UNRECOGNIZED_QUERY);	// no match, error
		}
		else if (strcmp(word, _SUPP) == 0 || strcmp(word, _SUPPLY) == 0)
		{
			sprintf(outputBuffer, "%0.10g\n", model430->supplyVoltage);
			std::cout.write(outputBuffer, strlen(outputBuffer));
		}
		else if (strcmp(word, _LIM) == 0 || strcmp(word, _LIMIT) == 0)
		{
			sprintf(outputBuffer, "%0.3f\n", model430->voltageLimit());
			std::cout.write(outputBuffer, strlen(outputBuffer));
		}
		else if (strcmp(word, _MAG) == 0 || strcmp(word, _MAGNET) == 0)
		{
			sprintf(outputBuffer, "%0.10g\n", model430->magnetVoltage);
			std::cout.write(outputBuffer, strlen(outputBuffer));
		}
		else
		{
			addToErrorQueue(ERR_UNRECOGNIZED_QUERY);	// no match, error
		}
	}
	else
	{
		addToErrorQueue(ERR_UNRECOGNIZED_QUERY);	// no match, error
	}
}

//---------------------------------------------------------------------------
//	 Parses all CONFigure commands. These are broken out of
//   parseInput() for easier reading.
//---------------------------------------------------------------------------
void Parser::parse_configure(const char* word, char *outputBuffer)
{
	if (word == NULL)
	{
		addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
	}
	else
	{
		switch (*word)
		{
			// CONFigure:I* commands
			case  'I':
				parse_configure_I(word, outputBuffer);
				break;

			// CONFigure:V* commands
			case  'V':
				parse_configure_V(word, outputBuffer);
				break;

			// CONFigure:C* commands
			case  'C':
				parse_configure_C(word, outputBuffer);
				break;

			// CONFigure:R* commands
			case  'R':
				parse_configure_R(word, outputBuffer);
				break;

			// CONFigure:F* commands
			case  'F':
				parse_configure_F(word, outputBuffer);
				break;

			// CONFigure:P* commands
			case  'P':
				parse_configure_P(word, outputBuffer);
				break;

			// CONFigure:Q* commands
			case  'Q':
				addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
				//parse_configure_Q(word);
				break;

			// CONFigure:S* commands
			case  'S':
				parse_configure_S(word, outputBuffer);
				break;

			default:
				addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
				break;
		}	// end switch(*word) for CONF
	}
}

//---------------------------------------------------------------------------
// tests CONFigure:CURRent:TARGet, CONFigure:CURRent:LIMit,
//       CONFigure:COILconst
//---------------------------------------------------------------------------
void Parser::parse_configure_C(const char* word, char *outputBuffer)
{
	bool saveBBRAM = false;

	if (strcmp(word, _CURR) == 0 || strcmp(word, _CURRENT) == 0)
	{
		word = strtok(NULL, SEPARATOR);		// get next token

		if (word == NULL)
		{
			addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
		}

		// CONFigure:CURRent:LIMit...
		else if (strcmp(word, _LIM) == 0 || strcmp(word, _LIMIT) == 0)
		{
			word = strtok(NULL, SEPARATOR);	// get next token

			// CONFigure:CURRent:LIMit
			if (isValue(word))
			{
				double temp = strtod(word, NULL);
				emit sendBlockingCommand(inputStr + "\r\n");
				model430->currentLimit = temp;
			}
			else
			{
				if (word == NULL)
					addToErrorQueue(ERR_MISSING_PARAMETER);
				else
					addToErrorQueue(ERR_INVALID_ARGUMENT);
			}
		}

		// CONFigure:CURRent:TARGet
		else if (strcmp(word, _TARG) == 0 || strcmp(word, _TARGET) == 0)
		{
			char *value = strtok(NULL, SPACE);		// look for value

			if (isValue(value))
			{
				double temp = strtod(value, NULL);
				emit sendBlockingCommand(inputStr + "\r\n");
				model430->targetCurrent = temp;
			}
			else
			{
				if (value == NULL)
					addToErrorQueue(ERR_MISSING_PARAMETER);
				else
					addToErrorQueue(ERR_INVALID_ARGUMENT);
			}
		}

		else
		{
			addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
		}
	}	// end if CURR

	// CONFigure:COILconst
	else if (strcmp(word, _COIL) == 0 || strcmp(word, _COILCONST) == 0)
	{
		char *value = strtok(NULL, SPACE);		// look for value

		if (isValue(value))
		{
			double temp = strtod(value, NULL);
			emit sendBlockingCommand(inputStr + "\r\n");
			model430->coilConstant = temp;
		}
		else
		{
			if (value == NULL)
				addToErrorQueue(ERR_MISSING_PARAMETER);
			else
				addToErrorQueue(ERR_INVALID_ARGUMENT);
		}
	}	// end if COIL
	else
	{
		addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
	}
}

//---------------------------------------------------------------------------
// tests CONFigure:FIELD:TARGet, CONFigure:FIELD:UNITS
//---------------------------------------------------------------------------
void Parser::parse_configure_F(const char* word, char *outputBuffer)
{
	if (strcmp(word, _FIELD) == 0)
	{
		word = strtok(NULL, SEPARATOR);	// get next token

		if (word == NULL)
		{
			addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
		}

		// CONFigure:FIELD:TARGet
		else if (strcmp(word, _TARG) == 0 || strcmp(word, _TARGET) == 0)
		{
			char *value = strtok(NULL, SPACE);	// look for value

			if (isValue(value))
			{
				double temp = strtod(value, NULL);
				emit sendBlockingCommand(inputStr + "\r\n");
				model430->targetField = temp;
			}
			else
			{
				if (value == NULL)
					addToErrorQueue(ERR_MISSING_PARAMETER);
				else
					addToErrorQueue(ERR_INVALID_ARGUMENT);
			}
		}
		// CONFigure:FIELD:UNITS
		else if (strcmp(word, _UNITS) == 0)
		{
			char *value = strtok(NULL, SPACE);	// look for value

			if (isValue(value))
			{
				while (isspace(*value))
					value++;

				if (*value == '1' || *value == '0')
				{
					emit sendBlockingCommand(inputStr + "\r\n");
					model430->fieldUnits = atoi(value);
				}
				else
				{
					addToErrorQueue(ERR_INVALID_ARGUMENT);
				}
			}
			else
			{
				if (value == NULL)
					addToErrorQueue(ERR_MISSING_PARAMETER);
				else
					addToErrorQueue(ERR_INVALID_ARGUMENT);
			}
		}
		else
		{
			addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
		}
	}
	else
	{
		addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
	}
}

//---------------------------------------------------------------------------
// tests CONFigure:INDuctance
//---------------------------------------------------------------------------
void Parser::parse_configure_I(const char* word, char *outputBuffer)
{
	if (strcmp(word, _IND) == 0 || strcmp(word, _INDUCTANCE) == 0)
	{
		word = strtok(NULL, SEPARATOR);	// get next token

		if (isValue(word))
		{
			double temp = strtod(word, NULL);
			emit sendBlockingCommand(inputStr + "\r\n");
			model430->inductance = temp;
		}
		else
		{
			if (word == NULL)
				addToErrorQueue(ERR_MISSING_PARAMETER);
			else
				addToErrorQueue(ERR_INVALID_ARGUMENT);
		}
	}
	else
	{
		addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
	}
}

//---------------------------------------------------------------------------
// tests CONFigure:PSwitch,          CONFigure:PSwitch:CURRent,             CONFigure:PSwitch:HeatTIME,
//       CONFigure:PSwitch:CoolTIME, CONFigure:PSwitch:PowerSupplyRampRate, CONFigure:PSwitch:CoolingGAIN,
//		 CONFigure:PSwitch:TRANsition
//---------------------------------------------------------------------------
void Parser::parse_configure_P(const char* word, char *outputBuffer)
{
	if (strcmp(word, _PS) == 0 || strcmp(word, _PSWITCH) == 0)
	{
		word = strtok(NULL, SEPARATOR); // get next token

		if (word == NULL)
		{
			addToErrorQueue(ERR_MISSING_PARAMETER);	// no argument
		}

		// CONFigure:PSwitch:CURRent
		else if (strcmp(word, _CURR) == 0 || strcmp(word, _CURRENT) == 0)
		{
			char *value = strtok(NULL, SPACE);	// look for value

			if (isValue(value))
			{
				double temp = strtod(value, NULL);
				emit sendBlockingCommand(inputStr + "\r\n");
				model430->switchCurrent = temp;
			}
			else
			{
				if (value == NULL)
					addToErrorQueue(ERR_MISSING_PARAMETER);
				else
					addToErrorQueue(ERR_INVALID_ARGUMENT);
			}
		}

		// CONFigure:PSwitch:PowerSupplyRampRate
		else if (strcmp(word, _PSRR) == 0 || strcmp(word, _POWERSUPPLYRAMPRATE) == 0)
		{
			char *value = strtok(NULL, SPACE);
			if (isValue(value))
			{
				double temp = strtod(value, NULL);
				emit sendBlockingCommand(inputStr + "\r\n");
				model430->cooledSwitchRampRate = temp;
			}
			else
			{
				if (value == NULL)
					addToErrorQueue(ERR_MISSING_PARAMETER);
				else
					addToErrorQueue(ERR_INVALID_ARGUMENT);
			}
		}

		// CONFigure:PSwitch:HeatTIME
		else if (strcmp(word, _HTIME) == 0 || strcmp(word, _HEATTIME) == 0)
		{
			char *value = strtok(NULL, SPACE);	// look for value

			if (isValue(value))
			{
				double temp = strtod(value, NULL);
				emit sendBlockingCommand(inputStr + "\r\n");
				model430->switchHeatedTime = temp;
			}
			else
			{
				if (value == NULL)
					addToErrorQueue(ERR_MISSING_PARAMETER);
				else
					addToErrorQueue(ERR_INVALID_ARGUMENT);
			}
		}

		// CONFigure:PSwitch:CoolTIME
		else if (strcmp(word, _CTIME) == 0 || strcmp(word, _COOLTIME) == 0)
		{
			char *value = strtok(NULL, SPACE);	// look for value

			if (isValue(value))
			{
				double temp = strtod(value, NULL);
				emit sendBlockingCommand(inputStr + "\r\n");
				model430->switchCooledTime = temp;
			}
			else
			{
				if (value == NULL)
					addToErrorQueue(ERR_MISSING_PARAMETER);
				else
					addToErrorQueue(ERR_INVALID_ARGUMENT);
			}
		}

		// CONFigure:PSwitch:CoolingGAIN
		else if (strcmp(word, _CGAIN) == 0 || strcmp(word, _COOLINGGAIN) == 0)
		{
			char *value = strtok(NULL, SPACE);	// look for value

			if (isValue(value))
			{
				double temp = strtod(value, NULL);
				emit sendBlockingCommand(inputStr + "\r\n");
				model430->switchCoolingGain = temp;
			}
			else
			{
				if (value == NULL)
					addToErrorQueue(ERR_MISSING_PARAMETER);
				else
					addToErrorQueue(ERR_INVALID_ARGUMENT);
			}
		}

		// CONFigure:PSwitch:TRANsition
		else if (strcmp(word, _TRAN) == 0 || strcmp(word, _TRANSITION) == 0)
		{
			char *value = strtok(NULL, SPACE);	// look for value

			if (isValue(value))
			{
				while (isspace(*value))
					value++;

				if (*value == '0' || *value == '1')
				{
					emit sendBlockingCommand(inputStr + "\r\n");
					model430->switchTransition = atoi(value);
				}
				else
				{
					addToErrorQueue(ERR_INVALID_ARGUMENT);
				}
			}
			else
			{
				if (value == NULL)
					addToErrorQueue(ERR_MISSING_PARAMETER);
				else
					addToErrorQueue(ERR_INVALID_ARGUMENT);
			}
		}

		// CONFigure:PSwitch
		else if (isValue(word))
		{
			char *value = strtok((char*)word, SPACE);	// look for value

			while (isspace(*value))
				value++;

			if (*value == '0' || *value == '1')
			{
				emit sendBlockingCommand(inputStr + "\r\n");
				model430->switchInstalled = atoi(value);
			}
			else
			{
				addToErrorQueue(ERR_NON_BOOLEAN_ARGUMENT);
			}
		}

		else
		{
			addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
		}
	}
	else
	{
		addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
	}
}

//---------------------------------------------------------------------------
// tests CONFigure:RAMP:RATE:FIELD,     CONFigure:RAMP:RATE:CURRent,
//       CONFigure:RAMP:RATE:UNITS,     CONFigure:RAMP:RATE:SEGments <segs>
//---------------------------------------------------------------------------
void Parser::parse_configure_R(const char* word, char *outputBuffer)
{
	// CONFigure:RAMP command group
	if (strcmp(word, _RAMP) == 0)
	{
		word = strtok(NULL, SEPARATOR);	// get next token

		if (word == NULL)
		{
			addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
		}

		// CONFigure:RAMP:RATE group
		else if (strcmp(word, _RATE) == 0)
		{
			word = strtok(NULL, SEPARATOR);	// get next token

			if (word == NULL)
			{
				addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
			}

			// CONFigure:RAMP:RATE:FIELD
			else if (strcmp(word, _FIELD) == 0)
			{
				// Read in the segment number
				char *value = strtok(NULL, COMMA);

				if (isValue(value))
				{
					double checkValue = strtod(value, NULL);
					if (checkValue > 0 && checkValue <= 10 && checkValue == (int)checkValue)
					{
						// Read in the ramp rate
						value = strtok(NULL, COMMA);

						if (isValue(value)) // end range
						{
							// Grab the upper limit value
							value = strtok(NULL, COMMA);	// look for upper bound value
							if (isValue(value))
							{
								emit sendBlockingCommand(inputStr + "\r\n");
								emit configurationChanged(RAMP_RATE_FIELD);
							}
							else
							{
								if (value == NULL)
									addToErrorQueue(ERR_MISSING_PARAMETER);
								else
									addToErrorQueue(ERR_INVALID_ARGUMENT);
							}
						}
						else
						{
							if (value == NULL)
								addToErrorQueue(ERR_MISSING_PARAMETER);
							else
								addToErrorQueue(ERR_INVALID_ARGUMENT);
						}
					}
					else
					{
						addToErrorQueue(ERR_OUT_OF_RANGE);
					}
				}
				else
				{
					if (value == NULL)
						addToErrorQueue(ERR_MISSING_PARAMETER);
					else
						addToErrorQueue(ERR_INVALID_ARGUMENT);
				}
			}

			// CONFigure:RAMP:RATE:UNITS
			else if (strcmp(word, _UNITS) == 0)
			{
				char *value = strtok(NULL, SPACE);	// look for value

				if (isValue(value))
				{
					while (isspace(*value))
					{
						value++;
					}

					if (*value == '1' || *value == '0')
					{
						emit sendBlockingCommand(inputStr + "\r\n");
						model430->rampRateTimeUnits = atoi(value);
					}
					else
					{
						addToErrorQueue(ERR_INVALID_ARGUMENT);
					}
				}
				else
				{
					if (value == NULL)
						addToErrorQueue(ERR_MISSING_PARAMETER);
					else
						addToErrorQueue(ERR_INVALID_ARGUMENT);
				}
			}

			// CONFigure:RAMP:RATE:CURRent
			else if (strcmp(word, _CURR) == 0 || strcmp(word, _CURRENT) == 0)
			{
				// Read in the segment number
				char *value = strtok(NULL, COMMA);

				if (isValue(value))
				{
					double checkValue = strtod(value, NULL);
					if (checkValue > 0 && checkValue <= 10 && checkValue == (int)checkValue)
					{
						// Read in the ramp rate
						value = strtok(NULL, COMMA);

						if (isValue(value)) // end range
						{
							value = strtok(NULL, COMMA);	// look for upper bound value
							if (isValue(value))
							{
								emit sendBlockingCommand(inputStr + "\r\n");
								emit configurationChanged(RAMP_RATE_CURRENT);
							}
							else
							{
								if (value == NULL)
									addToErrorQueue(ERR_MISSING_PARAMETER);
								else
									addToErrorQueue(ERR_INVALID_ARGUMENT);
							}
						}
						else
						{
							if (value == NULL)
								addToErrorQueue(ERR_MISSING_PARAMETER);
							else
								addToErrorQueue(ERR_INVALID_ARGUMENT);
						}
					}
					else
					{
						addToErrorQueue(ERR_OUT_OF_RANGE);
					}
				}
				else
				{
					if (value == NULL)
						addToErrorQueue(ERR_MISSING_PARAMETER);
					else
						addToErrorQueue(ERR_INVALID_ARGUMENT);
				}
			}

			// CONFigure:RAMP:RATE:SEGments
			else if (strcmp(word, _SEG) == 0 || strcmp(word, _SEGMENTS) == 0)
			{
				char *value = strtok(NULL, SPACE);	// look for value

				if (isValue(value))
				{
					double checkValue = strtod(value, NULL);

					if (checkValue > 0 && checkValue <= 10)
					{
						emit sendBlockingCommand(inputStr + "\r\n");
						model430->rampRateSegments = checkValue;
					}
					else
					{
						addToErrorQueue(ERR_OUT_OF_RANGE);
					}
				}
				else
				{
					if (value == NULL)
						addToErrorQueue(ERR_MISSING_PARAMETER);
					else
						addToErrorQueue(ERR_INVALID_ARGUMENT);
				}
			}

			else
			{
				addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
			}
		}
		else
		{
			addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
		}
	}
	else
	{
		addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
	}
}

//---------------------------------------------------------------------------
// tests CONFigure:STABility, CONFigure:STABility:MODE,
//		 CONFigure:STABility:RESistor
//---------------------------------------------------------------------------
void Parser::parse_configure_S(const char* word, char *outputBuffer)
{
	if (strcmp(word, _STAB) == 0 || strcmp(word, _STABILITY) == 0)
	{
		word = strtok(NULL, SEPARATOR); // get next token

		if (word == NULL)
		{
			// no argument for CONFigure:STABility
			addToErrorQueue(ERR_MISSING_PARAMETER);	// no match, error
		}

		// CONFigure:STABility:MODE
		else if (strcmp(word, _MODE) == 0)
		{
			char *value = strtok(NULL, SPACE);		// look for value

			if (isValue(value))
			{
				while (isspace(*value))
					value++;

				if (*value == '0' || *value == '1' || *value == '2')
				{
					emit sendBlockingCommand(inputStr + "\r\n");
					model430->stabilityMode = atoi(value);
				}
				else
				{
					addToErrorQueue(ERR_INVALID_ARGUMENT);
				}
			}
			else
			{
				if (value == NULL)
					addToErrorQueue(ERR_MISSING_PARAMETER);
				else
					addToErrorQueue(ERR_INVALID_ARGUMENT);
			}
		}

		// CONFigure:STABility:RESistor
		else if (strcmp(word, _RES) == 0 || strcmp(word, _RESISTOR) == 0)
		{
			char *value = strtok(NULL, SPACE);		// look for value

			if (isValue(value))
			{
				while (isspace(*value))
					value++;

				if (*value == '0' || *value == '1')
				{
					emit sendBlockingCommand(inputStr + "\r\n");
					model430->stabilityResistor = atoi(value);
				}
				else
				{
					addToErrorQueue(ERR_NON_BOOLEAN_ARGUMENT);
				}
			}
			else
			{
				if (value == NULL)
					addToErrorQueue(ERR_MISSING_PARAMETER);
				else
					addToErrorQueue(ERR_INVALID_ARGUMENT);
			}
		}

		// CONFigure:STABility
		else if (isValue(word))
		{
			char *value = strtok((char*)word, SPACE);	// look for value

			while (isspace(*value))
				value++;

			double temp = strtod(value, NULL);
			emit sendBlockingCommand(inputStr + "\r\n");
			model430->stabilitySetting = temp;
		}

		else
		{
			addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
		}
	}
	else
	{
		addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
	}
}

//---------------------------------------------------------------------------
// tests CONFigure:VOLTage:LIMit
//---------------------------------------------------------------------------
void Parser::parse_configure_V(const char* word, char *outputBuffer)
{
	if (strcmp(word, _VOLT) == 0 || strcmp(word, _VOLTAGE) == 0)
	{
		word = strtok(NULL, SEPARATOR);		// get next token

		if (word == NULL)
		{
			addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
		}
		else if (strcmp(word, _LIM) == 0 || strcmp(word, _LIMIT) == 0)
		{
			word = strtok(NULL, SEPARATOR);	// get next token

			if (isValue(word))
			{
				double temp = strtod(word, NULL);
				emit sendBlockingCommand(inputStr + "\r\n");
				model430->voltageLimit = temp;
			}
			else
			{
				if (word == NULL)
					addToErrorQueue(ERR_MISSING_PARAMETER);
				else
					addToErrorQueue(ERR_INVALID_ARGUMENT);
			}
		}
		else
		{
			addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
		}
	}
	else
	{
		addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
	}
}

//---------------------------------------------------------------------------

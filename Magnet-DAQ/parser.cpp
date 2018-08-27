#include "stdafx.h"
#include "parser.h"
#include "socket.h"
#include <iostream>

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
const char _CONFIGURE[] = "CONFIGURE";
const char _CONF[] = "CONF";
const char _IDN[] = "*IDN";
const char _INDUCTANCE[] = "INDUCTANCE";
const char _IND[] = "IND";
const char _PAUSE[] = "PAUSE";
const char _PSWITCH[] = "PSWITCH";
const char _PS[] = "PS";
const char _RAMP[] = "RAMP";
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

//---------------------------------------------------------------------------
// Incoming data type tests and conversions
//---------------------------------------------------------------------------
char *struprt(char *str)
{
	char *next = str;
	while (*str != '\0')
		*str = toupper((unsigned char)*str);
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
	stopProcessing = false;
	model430 = NULL;
}

//---------------------------------------------------------------------------
Parser::~Parser()
{
	stopProcessing = true;
	qDebug("Magnet-DAQ stdin Parser End");
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
		stopProcessing = false;

		// allocate resources and start parsing
		qDebug("Magnet-DAQ stdin Parser Start");
		char input[1024];
		char output[1024];

		while (!stopProcessing)
		{
			input[0] = '\0';
			output[0] = '\0';

			std::cin.getline(input, sizeof(input));
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
			struprt(input);	// convert to all uppercase
#else
			_strupr(input);	// convert to all uppercase
#endif

#ifdef DEBUG
			if (input[0] != '\0')
				qDebug() << QString(input);
#endif

			// save original string
			inputStr = QString(input);

			// parse stdin
			parseInput(input, output);
		}

		disconnect(this, SIGNAL(sendBlockingCommand(QString)), model430->getSocket(), SLOT(sendBlockingCommand(QString)));
	}

	emit finished();
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
				break;

			// I* queries
			case  'I':
				//parse_query_I(word, outputBuffer);
				break;

			// S* queries
			case  'S':
				if (!strcmp(word, _STATE))				
				{
					QString tmpStr(QString::number((int)(model430->state())) + "\n");
					std::cout.write(tmpStr.toLocal8Bit(), tmpStr.size());
				}
				break;

			// V* queries
			case  'V':
				//parse_query_V(word, outputBuffer);
				break;

			// C* queries
			case  'C':
				parse_query_C(word, outputBuffer);
				break;

			// P* queries
			case  'P':
				//parse_query_P(word, outputBuffer);
				break;

			// L* queries
			case  'L':
				//parse_query_L(word, outputBuffer);
				break;

			// Q* queries
			case  'Q':
				parse_query_Q(word, outputBuffer);
				break;

			// R* queries
			case  'R':
				//parse_query_R(word, outputBuffer);
				break;

			// F* queries
			case  'F':
				parse_query_F(word, outputBuffer);
				break;

			// O* queries
			case 'O':
				//parse_query_O(word, outputBuffer);
				break;

			// no match
			default:
				break;
		}	// end switch on first word
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
			return; // no match
		}

		// switch on first character
		switch (*word)
		{
			// tests all CONFigure commands
			case  'C':
				if (strcmp(word, _CONF) == 0 || strcmp(word, _CONFIGURE) == 0)
				{
					// get next token
					word = strtok(NULL, SEPARATOR);
					if (word == NULL)
					{
						return; // no match
					}

					parse_configure(word, outputBuffer);
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
					}
				}
				else if (strcmp(word, _PAUSE) == 0)
				{
					emit sendBlockingCommand(inputStr + "\r\n");
				}
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
	double temp1;

	if (strcmp(word, _CURR) == 0 || strcmp(word, _CURRENT) == 0)
	{
		word = strtok(NULL, DELIMITER);		// get next token

		if (word == NULL)
		{
			return; // no match
		}

		// CURRent:LIMit?
		else if (strcmp(word, _LIM) == 0 || strcmp(word, _LIMIT) == 0)
		{
			sprintf(outputBuffer, "%0.10e\n", model430->currentLimit());
			std::cout.write(outputBuffer, strlen(outputBuffer));
		}

		// CURRent:SUPPly?
		else if (strcmp(word, _SUPP) == 0 || strcmp(word, _SUPPLY) == 0)
		{
			sprintf(outputBuffer, "%0.10e\n", model430->supplyCurrent);
			std::cout.write(outputBuffer, strlen(outputBuffer));
		}

		// CURRent:MAGnet?
		else if (strcmp(word, _MAG) == 0 || strcmp(word, _MAGNET) == 0)
		{
			sprintf(outputBuffer, "%0.10e\n", model430->magnetCurrent);
			std::cout.write(outputBuffer, strlen(outputBuffer));
		}

		// CURRent:TARGet?
		else if (strcmp(word, _TARG) == 0 || strcmp(word, _TARGET) == 0)
		{
			
		}
	}
	else if (strcmp(word, _COIL) == 0 || strcmp(word, _COILCONST) == 0)
	{
		sprintf(outputBuffer, "%0.10e\n", model430->coilConstant());
		std::cout.write(outputBuffer, strlen(outputBuffer));
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
			return; // no match
		}
		else if (strcmp(word, _MAG) == 0 || strcmp(word, _MAGNET) == 0)
		{
			sprintf(outputBuffer, "%0.10e\n", model430->magnetField);
			std::cout.write(outputBuffer, strlen(outputBuffer));
		}
		else if (strcmp(word, _TARG) == 0 || strcmp(word, _TARGET) == 0)
		{
			
		}
		else if (strcmp(word, _UNITS) == 0)
		{
			sprintf(outputBuffer, "%d\n", model430->fieldUnits());
			std::cout.write(outputBuffer, strlen(outputBuffer));
		}
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
			return; // no match
		}
		else if (strcmp(word, _CURR) == 0 || strcmp(word, _CURRENT) == 0)
		{
			sprintf(outputBuffer, "%0.10e\n", model430->quenchCurrent);
			std::cout.write(outputBuffer, strlen(outputBuffer));
		}
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
		return;
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
				//parse_configure_Q(word);
				break;

			// CONFigure:S* commands
			case  'S':
				parse_configure_S(word, outputBuffer);
				break;

			default:
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
			return;
		}

		// CONFigure:CURRent:LIMit...
		else if (strcmp(word, _LIM) == 0 || strcmp(word, _LIMIT) == 0)
		{
			word = strtok(NULL, SEPARATOR);	// get next token

			if (word == NULL)
			{
				return;
			}

			// CONFigure:CURRent:LIMit
			else if (isValue(word))
			{
				double temp = strtod(word, NULL);
				emit sendBlockingCommand(inputStr + "\r\n");
				model430->currentLimit = temp;
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
	}	// end if COIL
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
			return;
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
			}
		}
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
			return;	// no argument
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
		}
			
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
			return;
		}

		// CONFigure:RAMP:RATE group
		else if (strcmp(word, _RATE) == 0)
		{
			word = strtok(NULL, SEPARATOR);	// get next token

			if (word == NULL)
			{
				return;
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
							}
						}
					}
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
							}
						}
					}
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
				}
			}
		}
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
			return;
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
			return;
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
		}
	}
}

//---------------------------------------------------------------------------

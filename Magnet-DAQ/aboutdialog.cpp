#include "stdafx.h"
#include "aboutdialog.h"

//---------------------------------------------------------------------------
aboutdialog::aboutdialog(QDialog *parent)
	: QDialog(parent)
{
	ui.setupUi(this);
	connect(ui.aboutQtButton, SIGNAL(clicked(bool)), this, SLOT(showAboutQt(bool)));
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	ui.license->setTextInteractionFlags(Qt::TextBrowserInteraction);
	ui.license->setOpenExternalLinks(true);
	ui.appname->setTextInteractionFlags(Qt::TextBrowserInteraction);
	ui.appname->setOpenExternalLinks(true);

#if defined(Q_OS_MACOS)
    // these font sizes seem to be somehow "frozen" in the ui file
    // replace them here in code so we don't have to have a Mac-specific ui file
    ui.license->setFont(QFont(".SF NS Text", 13));
#endif
}

//---------------------------------------------------------------------------
aboutdialog::~aboutdialog()
{
}

//---------------------------------------------------------------------------
void aboutdialog::showAboutQt(bool)
{
	qApp->aboutQt();
}

//---------------------------------------------------------------------------

//---------------------------------------------------------------------------

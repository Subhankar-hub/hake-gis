/***************************************************************************
                          qgsabout.cpp  -  description
                             -------------------
    begin                : Sat Aug 10 2002
    copyright            : (C) 2002 by Gary E.Sherman
    email                : sherman at mrcc.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsabout.h"

#include "qgsapplication.h"
#include "qgslogger.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QFile>
#include <QRegularExpression>
#include <QString>
#include <QUrl>

#include "moc_qgsabout.cpp"

using namespace Qt::StringLiterals;

#ifdef Q_OS_MACOS
// Modeless dialog with close button only
constexpr Qt::WindowFlags kAboutWindowFlags = Qt::WindowSystemMenuHint;
#else
// Normal dialog in non Mac-OS
constexpr Qt::WindowFlags kAboutWindowFlags = Qt::WindowFlags();
#endif

QgsAbout::QgsAbout( QWidget *parent )
  : QgsOptionsDialogBase( u"about"_s, parent, kAboutWindowFlags )
{
  setupUi( this );
  connect( btnQgisUser, &QPushButton::clicked, this, &QgsAbout::btnQgisUser_clicked );
  connect( btnQgisHome, &QPushButton::clicked, this, &QgsAbout::btnQgisHome_clicked );
  connect( btnCopyToClipboard, &QPushButton::clicked, this, &QgsAbout::btnCopyToClipboard_clicked );
  if constexpr ( QSysInfo::WordSize != 64 )
  {
    // 64 bit is the current standard. Only specify word size if it is not 64.
    initOptionsBase( true, tr( "%1 - %2 Bit" ).arg( windowTitle() ).arg( QSysInfo::WordSize ) );
  }
  else
  {
    initOptionsBase( true );
  }
  init();
}

void QgsAbout::init()
{
  setWhatsNew();
  setLicence();
}

void QgsAbout::setLicence()
{
  QFile licenceFile( QgsApplication::licenceFilePath() );
  QgsDebugMsgLevel( u"Reading licence file %1"_s.arg( licenceFile.fileName() ), 2 );
  if ( licenceFile.open( QIODevice::ReadOnly ) )
  {
    txtLicense->setText( licenceFile.readAll() );
  }
}

void QgsAbout::setVersion( const QString &v )
{
  txtVersion->setBackgroundRole( QPalette::NoRole );
  txtVersion->setAutoFillBackground( true );
  txtVersion->setHtml( v );
  mVersionString = v;
}

void QgsAbout::setWhatsNew()
{
  txtWhatsNew->clear();
  txtWhatsNew->document()->setDefaultStyleSheet( QgsApplication::reportStyleSheet() );
  if ( !QFile::exists( QgsApplication::pkgDataPath() + "/doc/NEWS.html" ) )
    return;

  txtWhatsNew->setSource( QString( "file:///" + QgsApplication::pkgDataPath() + "/doc/NEWS.html" ) );
}

void QgsAbout::btnCopyToClipboard_clicked()
{
  QGuiApplication::clipboard()->setText( mVersionString );
}

void QgsAbout::btnQgisUser_clicked()
{
  openUrl( u"https://haketech.com"_s );
}

void QgsAbout::btnQgisHome_clicked()
{
  openUrl( u"https://haketech.com"_s );
}

void QgsAbout::openUrl( const QUrl &url )
{
  //use the users default browser
  QDesktopServices::openUrl( url );
}

/*
 * The function below makes a name safe for using in most file system
 * Step 1: Code QString as UTF-8
 * Step 2: Replace all bytes of the UTF-8 above 0x7f with the hexcode in lower case.
 * Step 2: Replace all non [a-z][a-Z][0-9] with underscore (backward compatibility)
 */
QString QgsAbout::fileSystemSafe( const QString &fileName )
{
  QString result;
  QByteArray utf8 = fileName.toUtf8();

  for ( int i = 0; i < utf8.size(); i++ )
  {
    const uchar c = utf8[i];

    if ( c > 0x7f )
    {
      result = result + u"%1"_s.arg( c, 2, 16, QChar( '0' ) );
    }
    else
    {
      result = result + QChar( c );
    }
  }

  const thread_local QRegularExpression sNonAlphaNumericRx( u"[^a-zA-Z0-9]"_s );
  result.replace( sNonAlphaNumericRx, u"_"_s );
  QgsDebugMsgLevel( result, 3 );

  return result;
}

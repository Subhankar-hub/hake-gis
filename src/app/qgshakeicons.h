/***************************************************************************
  qgshakeicons.h
  -------------------
  begin                : September 2026
  copyright            : (C) 2026 by Hake Technologies
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef QGSHAKEICONS_H
#define QGSHAKEICONS_H

#include <QIcon>
#include <QString>

class QObject;

/**
 * Hake GeoDesk icon family for the application's own (Hake-owned) actions.
 *
 * Icons are SVGs compiled into the :/hake/icons resource. They are only used
 * under the "Hake Light" UI theme and only for the fixed set of core actions
 * listed in qgshakeicons.cpp; plugin-provided actions are never touched.
 */
class QgsHakeIcons
{
  public:
    //! Returns TRUE if \a themeName is the UI theme the Hake icon family is designed for.
    static bool isHakeTheme( const QString &themeName );

    //! Returns the Hake icon for \a resource (relative to :/hake/icons), or a null icon if it is missing.
    static QIcon icon( const QString &resource );

    /**
     * Applies the Hake icons to the core actions found under \a root when \a themeName
     * is the Hake theme, otherwise restores the icons those actions had before.
     */
    static void applyToActions( QObject *root, const QString &themeName );

    /**
     * Returns the Hake icon for \a resource under the Hake theme, otherwise the
     * stock theme icon \a stockThemeIcon. For actions whose icon changes at runtime.
     */
    static QIcon actionIcon( const QString &stockThemeIcon, const QString &resource );
};

#endif // QGSHAKEICONS_H

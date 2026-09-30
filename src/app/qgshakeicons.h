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
#include <QList>
#include <QString>

class QObject;
class QWidget;

/**
 * Hake GeoDesk icon family for the application's own (Hake-owned) actions.
 *
 * Icons are SVGs compiled into the :/hake/icons resource. They are only used
 * under the "Hake Light" UI theme and only for the fixed set of targets listed
 * in qgshakeicons.cpp: core actions, ribbon panel dock toggles, Data Source
 * Manager pages, and the bundled plugin commands named in the Hake icon catalog.
 * Any other plugin or extension action keeps its own icon.
 */
class QgsHakeIcons
{
  public:
    //! Returns TRUE if \a themeName is the UI theme the Hake icon family is designed for.
    static bool isHakeTheme( const QString &themeName );

    //! Returns the Hake icon for \a resource (relative to :/hake/icons), or a null icon if it is missing.
    static QIcon icon( const QString &resource );

    /**
     * Applies the Hake icons to the mapped actions and dock toggles found under \a root when
     * \a themeName is the Hake theme, otherwise restores the icons those actions had before.
     */
    static void applyToActions( QObject *root, const QString &themeName );

    /**
     * Applies the Hake icons to the source pages of the Data Source Manager \a dialog when
     * \a themeName is the Hake theme, otherwise restores the provider icons.
     */
    static void applyToDataSourceManager( QWidget *dialog, const QString &themeName );

    /**
     * Re-applies the Hake icons to actions under \a root whenever an action is added to one of
     * \a menus (or their submenus), so plugins loaded or reloaded later are covered.
     */
    static void watchMenus( QObject *root, const QList<QWidget *> &menus );

    /**
     * Returns the Hake icon for \a resource under the Hake theme, otherwise the
     * stock theme icon \a stockThemeIcon. For actions whose icon changes at runtime.
     */
    static QIcon actionIcon( const QString &stockThemeIcon, const QString &resource );
};

#endif // QGSHAKEICONS_H

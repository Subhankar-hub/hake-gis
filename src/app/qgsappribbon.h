/***************************************************************************
  qgsappribbon.h
  -------------------
  begin                : August 2026
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
#ifndef QGSAPPRIBBON_H
#define QGSAPPRIBBON_H

#include <QTabWidget>

class QAction;
class QHBoxLayout;
class QToolBar;
class QWidget;
class QgisApp;

/**
 * Tabbed ribbon that reuses existing QgisApp QActions via QToolButton::setDefaultAction.
 * Does not take ownership of those actions.
 */
class QgsAppRibbon : public QTabWidget
{
    Q_OBJECT

  public:
    explicit QgsAppRibbon( QWidget *parent, QgisApp *app );

    //! Adds Processing Toolbox toggle if the dock exists (may appear after plugins load).
    void refreshOptionalActions();

  private:
    QWidget *addPage( const QString &title );
    QHBoxLayout *addGroup( QWidget *page, const QString &title );
    void addActionButton( QHBoxLayout *groupLayout, QAction *action );
    void addNamedAction( QHBoxLayout *groupLayout, const QString &objectName );
    void addToolbarActions( QHBoxLayout *groupLayout, QToolBar *toolbar );

    QgisApp *mApp = nullptr;
    QHBoxLayout *mProcessingGroupLayout = nullptr;
    bool mProcessingActionAdded = false;
};

#endif // QGSAPPRIBBON_H

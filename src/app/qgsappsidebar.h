/***************************************************************************
  qgsappsidebar.h
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
#ifndef QGSAPPSIDEBAR_H
#define QGSAPPSIDEBAR_H

#include <QHash>
#include <QIcon>
#include <QToolBar>

class QAction;

/**
 * Permanent left navigation rail for Hake Geospatial.
 * Emits itemActivated with ids: browser, layers, styles, processing, plugins, favorites.
 */
class QgsAppSidebar : public QToolBar
{
    Q_OBJECT

  public:
    explicit QgsAppSidebar( QWidget *parent = nullptr );

    void setItemChecked( const QString &id, bool checked );
    bool isItemChecked( const QString &id ) const;

  signals:
    void itemActivated( const QString &id );

  private:
    QAction *addNavAction( const QString &id, const QString &text, const QIcon &icon, bool checkable );

    QHash<QString, QAction *> mActions;
};

#endif // QGSAPPSIDEBAR_H

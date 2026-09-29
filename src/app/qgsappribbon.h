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

#include <QHash>
#include <QList>
#include <QPointer>
#include <QStringList>
#include <QTabWidget>

class QLabel;
class QToolBar;
class QWidget;
class QgisApp;
class QgsAppRibbonGroup;
class QgsAppRibbonPage;

/**
 * Logical-pixel sizes for the ribbon, measured from the current font, style
 * and stylesheet rather than hard-coded, so they follow DPI and theme changes.
 */
struct QgsAppRibbonMetrics
{
    int smallIcon = 16;
    int largeIcon = 24;
    int rowHeight = 24;
    int tallHeight = 52;
    int captionHeight = 14;
    int horizontalMargin = 4;
    int verticalMargin = 2;
    int buttonAreaHeight = 52;
    int rows = 2;
    bool tallPrimary = true;
    bool captions = true;

    bool operator==( const QgsAppRibbonMetrics &other ) const = default;
};

/**
 * Tabbed ribbon that reuses existing QgisApp QActions via QToolButton::setDefaultAction.
 * Does not take ownership of those actions.
 */
class QgsAppRibbon : public QTabWidget
{
    Q_OBJECT

  public:
    explicit QgsAppRibbon( QWidget *parent, QgisApp *app );

    //! Resolves dock toggle actions (Browser, Layers, Processing Toolbox) whose docks are created after the ribbon.
    void refreshOptionalActions();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

  protected:
    bool event( QEvent *event ) override;
    void changeEvent( QEvent *event ) override;
    bool eventFilter( QObject *watched, QEvent *event ) override;
    void resizeEvent( QResizeEvent *event ) override;
    void showEvent( QShowEvent *event ) override;

  private:
    void syncChromeTabBarGeometry();
    void updateMetrics();
    QgsAppRibbonPage *addPage( const QString &title );
    QgsAppRibbonGroup *addGroup( QgsAppRibbonPage *page, const QString &title );
    void addNamedAction( QgsAppRibbonGroup *group, const QString &objectName, bool primary = false );
    void addDockToggle( QgsAppRibbonGroup *group, const QString &dockObjectName, bool primary = false );
    void mirrorToolbar( QgsAppRibbonGroup *group, QToolBar *toolbar );
    void syncMirroredGroup( QToolBar *toolbar );

    QgisApp *mApp = nullptr;
    QLabel *mBrand = nullptr;
    QList<QgsAppRibbonPage *> mPages;
    QHash<QToolBar *, QgsAppRibbonGroup *> mMirroredToolbars;
    QList<QPointer<QToolBar>> mPendingMirrorSyncs;
    QgsAppRibbonMetrics mMetrics;
    int mCommandHeight = 0;
    bool mUpdatingMetrics = false;
};

#endif // QGSAPPRIBBON_H

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

class QAction;
class QLabel;
class QMenu;
class QMenuBar;
class QToolBar;
class QToolButton;
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
    int buttonAreaHeight = 52;
    //! Spacing scale shared by every ribbon page, group and separator
    int spaceXs = 2;
    int spaceSm = 4;
    int spaceMd = 6;
    int spaceLg = 8;
    //! Widest label text before eliding, per button tier
    int largeLabelMax = 120;
    int compactLabelMax = 150;
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

    //! Resolves dock toggle actions (Browser, Layers, Processing Toolbox) and plugin menus (Processing) created after the ribbon.
    void refreshOptionalActions();

    //! Rebuilds the groups so button styles and collapsed-group icons follow action icons changed by a theme switch.
    void refreshIcons();

    /**
     * Shows \a action as a compact icon button beside the brand at the top-right of the tab strip.
     * Does not take ownership of the action.
     */
    void setThemeToggleAction( QAction *action );

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
    //! \a label is shown on the button only; the dock's toggle action text is left untouched.
    void addDockToggle( QgsAppRibbonGroup *group, const QString &dockObjectName, const QString &label, bool primary = false );
    //! Adds an existing menu as a drop-down button; hidden while the menu is empty.
    void addMenu( QgsAppRibbonGroup *group, QMenu *menu );
    //! Adds a menu created later (e.g. by a plugin), resolved by object name in refreshOptionalActions().
    void addDeferredMenu( QgsAppRibbonGroup *group, const QString &menuObjectName );
    void mirrorToolbar( QgsAppRibbonGroup *group, QToolBar *toolbar );
    void syncMirroredGroup( QToolBar *toolbar );

    /**
     * Watches the hidden classic menu bar: keeps top-level menu actions registered on the main
     * window (so their shortcuts work) and presents non-standard menus, such as those added by
     * installed extensions, in the Extensions tab.
     */
    void watchMenuBar( QMenuBar *menuBar );
    void syncMenuBar();
    bool isStandardMenu( const QMenu *menu ) const;

    QgisApp *mApp = nullptr;
    QLabel *mBrand = nullptr;
    QToolButton *mThemeToggle = nullptr;
    QList<QgsAppRibbonPage *> mPages;
    QHash<QToolBar *, QgsAppRibbonGroup *> mMirroredToolbars;
    QList<QPointer<QToolBar>> mPendingMirrorSyncs;
    QPointer<QMenuBar> mMenuBar;
    QgsAppRibbonGroup *mExtensionMenus = nullptr;
    QList<QPointer<QAction>> mAdoptedMenuBarActions;
    bool mMenuBarSyncPending = false;
    QgsAppRibbonMetrics mMetrics;
    int mCommandHeight = 0;
    bool mUpdatingMetrics = false;
};

#endif // QGSAPPRIBBON_H

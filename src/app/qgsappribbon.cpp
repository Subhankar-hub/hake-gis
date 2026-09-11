/***************************************************************************
  qgsappribbon.cpp
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

#include "qgsappribbon.h"

#include "qgisapp.h"
#include "qgsdockwidget.h"
#include "qgsguiutils.h"

#include <algorithm>

#include <QAction>
#include <QFrame>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSizePolicy>
#include <QTabBar>
#include <QToolBar>
#include <QToolButton>
#include <QWidget>
#include <QWidgetAction>

#include "moc_qgsappribbon.cpp"

using namespace Qt::StringLiterals;

QgsAppRibbon::QgsAppRibbon( QWidget *parent, QgisApp *app )
  : QTabWidget( parent )
  , mApp( app )
{
  setObjectName( u"HakeAppRibbon"_s );
  setDocumentMode( true );
  setMovable( false );
  setUsesScrollButtons( true );
  setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
  // Fusion leaves the empty region after the last tab unpainted unless QSS backgrounds are forced.
  // WA_StyledBackground is Qt-portable (Wayland-safe); do not use platform window APIs here.
  setAttribute( Qt::WA_StyledBackground, true );
  tabBar()->setAttribute( Qt::WA_StyledBackground, true );
  tabBar()->setAutoFillBackground( true );
  tabBar()->setExpanding( false );
  tabBar()->setDrawBase( false );
  tabBar()->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );

  // Fills any remaining header gap to the right of the last tab with chrome.
  auto *tabFiller = new QWidget( this );
  tabFiller->setObjectName( u"HakeAppRibbonTabFiller"_s );
  tabFiller->setAttribute( Qt::WA_StyledBackground, true );
  tabFiller->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
  setCornerWidget( tabFiller, Qt::TopRightCorner );

  if ( !mApp )
    return;

  // Home
  {
    QWidget *page = addPage( tr( "Home" ) );
    QHBoxLayout *project = addGroup( page, tr( "Project" ) );
    addNamedAction( project, u"mActionNewProject"_s );
    addNamedAction( project, u"mActionOpenProject"_s );
    addNamedAction( project, u"mActionSaveProject"_s );

    QHBoxLayout *edit = addGroup( page, tr( "Edit" ) );
    addNamedAction( edit, u"mActionUndo"_s );
    addNamedAction( edit, u"mActionRedo"_s );

    QHBoxLayout *map = addGroup( page, tr( "Map" ) );
    addNamedAction( map, u"mActionPan"_s );
    addNamedAction( map, u"mActionZoomIn"_s );
    addNamedAction( map, u"mActionZoomOut"_s );
    addNamedAction( map, u"mActionZoomFullExtent"_s );
    addNamedAction( map, u"mActionDraw"_s );

    QHBoxLayout *identify = addGroup( page, tr( "Identify" ) );
    addNamedAction( identify, u"mActionIdentify"_s );
    addNamedAction( identify, u"mActionOpenTable"_s );
  }

  // Data: layer sources, layouts, database and web services
  {
    QWidget *page = addPage( tr( "Data" ) );
    QHBoxLayout *layers = addGroup( page, tr( "Layers" ) );
    addNamedAction( layers, u"mActionDataSourceManager"_s );
    addNamedAction( layers, u"mActionAddOgrLayer"_s );
    addNamedAction( layers, u"mActionAddRasterLayer"_s );
    addNamedAction( layers, u"mActionAddMeshLayer"_s );
    addNamedAction( layers, u"mActionAddDelimitedText"_s );
    addNamedAction( layers, u"mActionAddSpatiaLiteLayer"_s );
    addNamedAction( layers, u"mActionAddVirtualLayer"_s );
    addNamedAction( layers, u"mActionAddWmsLayer"_s );
    addNamedAction( layers, u"mActionAddWfsLayer"_s );

    QHBoxLayout *layout = addGroup( page, tr( "Layout" ) );
    addNamedAction( layout, u"mActionNewPrintLayout"_s );
    addNamedAction( layout, u"mActionShowLayoutManager"_s );

    QHBoxLayout *database = addGroup( page, tr( "Database" ) );
    addToolbarActions( database, mApp->databaseToolBar() );

    QHBoxLayout *web = addGroup( page, tr( "Web" ) );
    addToolbarActions( web, mApp->webToolBar() );
  }

  // Analysis
  {
    QWidget *page = addPage( tr( "Analysis" ) );
    QHBoxLayout *measure = addGroup( page, tr( "Measure" ) );
    addNamedAction( measure, u"mActionMeasure"_s );
    addNamedAction( measure, u"mActionMeasureArea"_s );
    addNamedAction( measure, u"mActionMeasureBearing"_s );
    addNamedAction( measure, u"mActionMeasureAngle"_s );

    QHBoxLayout *stats = addGroup( page, tr( "Stats" ) );
    addNamedAction( stats, u"mActionStatisticalSummary"_s );
    addNamedAction( stats, u"mActionOpenFieldCalc"_s );

    mProcessingGroupLayout = addGroup( page, tr( "Processing" ) );
    addNamedAction( mProcessingGroupLayout, u"mActionShowPythonDialog"_s );
    refreshOptionalActions();
  }

  // View: navigation, bookmarks and 3D views
  {
    QWidget *page = addPage( tr( "View" ) );
    QHBoxLayout *navigate = addGroup( page, tr( "Navigate" ) );
    addNamedAction( navigate, u"mActionPanToSelected"_s );
    addNamedAction( navigate, u"mActionZoomToSelected"_s );
    addNamedAction( navigate, u"mActionZoomToLayers"_s );
    addNamedAction( navigate, u"mActionZoomActualSize"_s );
    addNamedAction( navigate, u"mActionZoomLast"_s );
    addNamedAction( navigate, u"mActionZoomNext"_s );
    addNamedAction( navigate, u"mActionNewMapCanvas"_s );
    addNamedAction( navigate, u"mActionNewBookmark"_s );
    addNamedAction( navigate, u"mActionShowBookmarks"_s );
    addNamedAction( navigate, u"mActionTemporalController"_s );

    QHBoxLayout *views3d = addGroup( page, tr( "3D" ) );
    addNamedAction( views3d, u"mActionNew3DMapCanvas"_s );
    addNamedAction( views3d, u"mActionNew3DMapCanvasGlobe"_s );
  }

  // Vector
  {
    QWidget *page = addPage( tr( "Vector" ) );
    QHBoxLayout *digitize = addGroup( page, tr( "Digitizing" ) );
    // Undo/Redo already live on Home > Edit; don't surface them twice
    const QStringList homeActions { u"mActionUndo"_s, u"mActionRedo"_s };
    addToolbarActions( digitize, mApp->digitizeToolBar(), homeActions );
    addToolbarActions( digitize, mApp->advancedDigitizeToolBar(), homeActions );

    QHBoxLayout *selection = addGroup( page, tr( "Selection" ) );
    addToolbarActions( selection, mApp->selectionToolBar() );

    QHBoxLayout *labels = addGroup( page, tr( "Labels" ) );
    addToolbarActions( labels, mApp->findChild<QToolBar *>( u"mLabelToolBar"_s ) );
  }

  // Raster
  {
    QWidget *page = addPage( tr( "Raster" ) );
    QHBoxLayout *stretch = addGroup( page, tr( "Stretch" ) );
    addToolbarActions( stretch, mApp->rasterToolBar() );
  }
}

void QgsAppRibbon::resizeEvent( QResizeEvent *event )
{
  QTabWidget::resizeEvent( event );
  syncChromeTabBarGeometry();
}

void QgsAppRibbon::showEvent( QShowEvent *event )
{
  QTabWidget::showEvent( event );
  syncChromeTabBarGeometry();
}

void QgsAppRibbon::syncChromeTabBarGeometry()
{
  QTabBar *bar = tabBar();
  if ( !bar )
    return;

  // Portable QWidget geometry only (valid on Wayland/X11/Windows/macOS). Document-mode
  // tab bars often keep sizeHint width (= tabs only); force the bar to span the full
  // ribbon so @chrome fills past the last tab without stretching tab labels.
  const int stripWidth = width();
  if ( stripWidth <= 0 )
    return;

  if ( bar->minimumWidth() != stripWidth )
    bar->setMinimumWidth( stripWidth );

  const int h = std::max( bar->height(), bar->sizeHint().height() );
  const QRect target( 0, bar->y(), stripWidth, h );
  if ( bar->geometry() != target )
    bar->setGeometry( target );

  if ( QWidget *filler = cornerWidget( Qt::TopRightCorner ) )
  {
    filler->setMinimumHeight( h );
    filler->setMaximumHeight( h );
  }
}

void QgsAppRibbon::refreshOptionalActions()
{
  if ( !mApp || !mProcessingGroupLayout || mProcessingActionAdded )
    return;

  const QList<QDockWidget *> docks = mApp->findChildren<QDockWidget *>();
  for ( QDockWidget *dock : docks )
  {
    if ( dock->objectName() == "ProcessingToolbox"_L1 )
    {
      if ( QAction *toggle = dock->toggleViewAction() )
      {
        addActionButton( mProcessingGroupLayout, toggle );
        mProcessingActionAdded = true;
      }
      break;
    }
  }
}

QWidget *QgsAppRibbon::addPage( const QString &title )
{
  QWidget *page = new QWidget( this );
  QHBoxLayout *layout = new QHBoxLayout( page );
  layout->setContentsMargins( 4, 2, 4, 2 );
  layout->setSpacing( 4 );
  layout->addStretch( 1 );
  addTab( page, title );
  return page;
}

QHBoxLayout *QgsAppRibbon::addGroup( QWidget *page, const QString &title )
{
  auto *pageLayout = qobject_cast<QHBoxLayout *>( page->layout() );

  // Single compact row of buttons; the group title stays available to
  // accessibility tools but is no longer rendered as a caption.
  QWidget *group = new QWidget( page );
  group->setAccessibleName( title );
  auto *buttonLayout = new QHBoxLayout( group );
  buttonLayout->setContentsMargins( 0, 0, 0, 0 );
  buttonLayout->setSpacing( 2 );
  buttonLayout->addStretch( 1 );

  // Insert before the trailing stretch
  const int stretchIndex = std::max( 0, pageLayout->count() - 1 );
  pageLayout->insertWidget( stretchIndex, group );

  QFrame *sep = new QFrame( page );
  sep->setFrameShape( QFrame::VLine );
  sep->setFrameShadow( QFrame::Plain );
  sep->setObjectName( u"HakeAppRibbonSeparator"_s );
  pageLayout->insertWidget( stretchIndex + 1, sep );

  return buttonLayout;
}

void QgsAppRibbon::addActionButton( QHBoxLayout *groupLayout, QAction *action )
{
  if ( !groupLayout || !action || action->isSeparator() )
    return;
  if ( qobject_cast<QWidgetAction *>( action ) )
    return;

  auto *button = new QToolButton( groupLayout->parentWidget() );
  button->setDefaultAction( action );
  button->setAutoRaise( true );
  button->setToolButtonStyle( Qt::ToolButtonIconOnly );
  button->setIconSize( QSize( QgsGuiUtils::scaleIconSize( 20 ), QgsGuiUtils::scaleIconSize( 20 ) ) );
  button->setFocusPolicy( Qt::NoFocus );

  const int stretchIndex = std::max( 0, groupLayout->count() - 1 );
  groupLayout->insertWidget( stretchIndex, button );
}

void QgsAppRibbon::addNamedAction( QHBoxLayout *groupLayout, const QString &objectName )
{
  if ( !mApp )
    return;
  addActionButton( groupLayout, mApp->findChild<QAction *>( objectName ) );
}

void QgsAppRibbon::addToolbarActions( QHBoxLayout *groupLayout, QToolBar *toolbar, const QStringList &excludedObjectNames )
{
  if ( !toolbar )
    return;
  const QList<QAction *> actions = toolbar->actions();
  for ( QAction *action : actions )
  {
    if ( action && excludedObjectNames.contains( action->objectName() ) )
      continue;
    addActionButton( groupLayout, action );
  }
}

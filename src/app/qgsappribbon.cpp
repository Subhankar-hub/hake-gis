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
#include <QLabel>
#include <QSizePolicy>
#include <QTabBar>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
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
  tabBar()->setExpanding( false );
  tabBar()->setDrawBase( false );

  if ( !mApp )
    return;

  // Home
  {
    QWidget *page = addPage( tr( "Home" ) );
    QHBoxLayout *project = addGroup( page, tr( "Project" ) );
    addNamedAction( project, u"mActionNewProject"_s );
    addNamedAction( project, u"mActionOpenProject"_s );
    addNamedAction( project, u"mActionSaveProject"_s );

    QHBoxLayout *clipboard = addGroup( page, tr( "Clipboard" ) );
    addNamedAction( clipboard, u"mActionUndo"_s );
    addNamedAction( clipboard, u"mActionRedo"_s );

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

  // Insert
  {
    QWidget *page = addPage( tr( "Insert" ) );
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

  // View
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

    QHBoxLayout *windows = addGroup( page, tr( "Windows" ) );
    addNamedAction( windows, u"mActionHelpContents"_s );
  }

  // Vector
  {
    QWidget *page = addPage( tr( "Vector" ) );
    QHBoxLayout *digitize = addGroup( page, tr( "Digitizing" ) );
    addToolbarActions( digitize, mApp->digitizeToolBar() );
    addToolbarActions( digitize, mApp->advancedDigitizeToolBar() );

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

  // Database
  {
    QWidget *page = addPage( tr( "Database" ) );
    QHBoxLayout *layers = addGroup( page, tr( "Layers" ) );
    addToolbarActions( layers, mApp->databaseToolBar() );
  }

  // Web
  {
    QWidget *page = addPage( tr( "Web" ) );
    QHBoxLayout *services = addGroup( page, tr( "Services" ) );
    addToolbarActions( services, mApp->webToolBar() );
  }

  // 3D
  {
    QWidget *page = addPage( tr( "3D" ) );
    QHBoxLayout *views = addGroup( page, tr( "Views" ) );
    addNamedAction( views, u"mActionNew3DMapCanvas"_s );
    addNamedAction( views, u"mActionNew3DMapCanvasGlobe"_s );
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
  layout->setContentsMargins( 6, 4, 6, 4 );
  layout->setSpacing( 8 );
  layout->addStretch( 1 );
  addTab( page, title );
  return page;
}

QHBoxLayout *QgsAppRibbon::addGroup( QWidget *page, const QString &title )
{
  auto *pageLayout = qobject_cast<QHBoxLayout *>( page->layout() );

  QWidget *group = new QWidget( page );
  QVBoxLayout *outer = new QVBoxLayout( group );
  outer->setContentsMargins( 4, 0, 4, 0 );
  outer->setSpacing( 2 );

  QWidget *buttons = new QWidget( group );
  auto *buttonLayout = new QHBoxLayout( buttons );
  buttonLayout->setContentsMargins( 0, 0, 0, 0 );
  buttonLayout->setSpacing( 2 );
  buttonLayout->addStretch( 1 );

  QLabel *caption = new QLabel( title, group );
  caption->setAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
  caption->setObjectName( u"HakeAppRibbonGroupLabel"_s );

  outer->addWidget( buttons, 1 );
  outer->addWidget( caption );

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
  button->setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
  button->setIconSize( QSize( QgsGuiUtils::scaleIconSize( 24 ), QgsGuiUtils::scaleIconSize( 24 ) ) );
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

void QgsAppRibbon::addToolbarActions( QHBoxLayout *groupLayout, QToolBar *toolbar )
{
  if ( !toolbar )
    return;
  const QList<QAction *> actions = toolbar->actions();
  for ( QAction *action : actions )
  {
    addActionButton( groupLayout, action );
  }
}

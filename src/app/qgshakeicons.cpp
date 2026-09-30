/***************************************************************************
  qgshakeicons.cpp
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

#include "qgshakeicons.h"

#include "qgsapplication.h"

#include <QAction>
#include <QFile>
#include <QHash>
#include <QIconEngine>
#include <QPainter>
#include <QPixmap>
#include <QString>
#include <QSvgRenderer>
#include <QVariant>

using namespace Qt::StringLiterals;

namespace
{
  struct HakeActionIcon
  {
    const char *objectName;
    const char *resource;
  };

  // Hake-owned core actions shown in the ribbon. Plugin actions must never be listed here.
  constexpr HakeActionIcon HAKE_ACTION_ICONS[] = {
    // Project
    { "mActionNewProject", "project/hake-project-new.svg" },
    { "mActionOpenProject", "project/hake-project-open.svg" },
    { "mActionSaveProject", "project/hake-project-save.svg" },
    { "mActionSaveProjectAs", "project/hake-project-save-as.svg" },
    { "mActionProjectProperties", "project/hake-project-properties.svg" },
    { "mActionExit", "project/hake-project-exit.svg" },
    // Editing
    { "mActionUndo", "editing/hake-editing-undo.svg" },
    { "mActionRedo", "editing/hake-editing-redo.svg" },
    // Navigation
    { "mActionPan", "navigation/hake-navigation-pan.svg" },
    { "mActionZoomIn", "navigation/hake-navigation-zoom-in.svg" },
    { "mActionZoomOut", "navigation/hake-navigation-zoom-out.svg" },
    { "mActionZoomFullExtent", "navigation/hake-navigation-full-extent.svg" },
    { "mActionDraw", "navigation/hake-navigation-refresh.svg" },
    { "mActionPanToSelected", "navigation/hake-navigation-pan-to-selection.svg" },
    { "mActionZoomToSelected", "navigation/hake-navigation-zoom-to-selection.svg" },
    { "mActionZoomToLayers", "navigation/hake-navigation-zoom-to-layers.svg" },
    { "mActionZoomActualSize", "navigation/hake-navigation-zoom-native.svg" },
    { "mActionZoomLast", "navigation/hake-navigation-zoom-last.svg" },
    { "mActionZoomNext", "navigation/hake-navigation-zoom-next.svg" },
    { "mActionIdentify", "navigation/hake-navigation-identify.svg" },
    // Layers and data sources
    { "mActionDataSourceManager", "layers/hake-layers-data-source-manager.svg" },
    { "mActionAddOgrLayer", "layers/hake-layers-add-vector.svg" },
    { "mActionAddRasterLayer", "layers/hake-layers-add-raster.svg" },
    { "mActionAddMeshLayer", "layers/hake-layers-add-mesh.svg" },
    { "mActionAddDelimitedText", "layers/hake-layers-add-delimited-text.svg" },
    { "mActionAddSpatiaLiteLayer", "layers/hake-layers-add-spatialite.svg" },
    { "mActionAddVirtualLayer", "layers/hake-layers-add-virtual.svg" },
    { "mActionAddWmsLayer", "layers/hake-layers-add-wms.svg" },
    { "mActionAddWfsLayer", "layers/hake-layers-add-wfs.svg" },
    { "mActionRemoveLayer", "layers/hake-layers-remove-layer.svg" },
    { "mActionOpenTable", "layers/hake-layers-attribute-table.svg" },
    // Map views, layouts and panels
    { "mActionNewPrintLayout", "map/hake-map-new-print-layout.svg" },
    { "mActionShowLayoutManager", "map/hake-map-layout-manager.svg" },
    { "mActionNewMapCanvas", "map/hake-map-new-map-view.svg" },
    { "mActionNew3DMapCanvas", "map/hake-map-new-3d-map-view.svg" },
    { "mActionNew3DMapCanvasGlobe", "map/hake-map-new-3d-globe-view.svg" },
    { "mActionNewBookmark", "map/hake-map-new-bookmark.svg" },
    { "mActionShowBookmarks", "map/hake-map-show-bookmarks.svg" },
    { "mActionTemporalController", "map/hake-map-temporal-controller.svg" },
    { "mActionToggleFullScreen", "map/hake-map-full-screen.svg" },
    // Measurement
    { "mActionMeasure", "measurement/hake-measurement-line.svg" },
    { "mActionMeasureArea", "measurement/hake-measurement-area.svg" },
    { "mActionMeasureBearing", "measurement/hake-measurement-bearing.svg" },
    { "mActionMeasureAngle", "measurement/hake-measurement-angle.svg" },
    // Analysis
    { "mActionStatisticalSummary", "analysis/hake-analysis-statistical-summary.svg" },
    { "mActionOpenFieldCalc", "analysis/hake-analysis-field-calculator.svg" },
    { "mActionShowPythonDialog", "analysis/hake-analysis-python-console.svg" },
    // Application settings
    { "mActionOptions", "settings/hake-settings-options.svg" },
    { "mActionStyleManager", "settings/hake-settings-style-manager.svg" },
    { "mActionCustomProjection", "settings/hake-settings-custom-projection.svg" },
    // Vector editing
    { "mActionToggleEditing", "vector/hake-vector-toggle-editing.svg" },
    { "mActionSaveLayerEdits", "vector/hake-vector-save-edits.svg" },
    { "mActionAddFeature", "vector/hake-vector-add-point.svg" },
    { "mActionVertexTool", "vector/hake-vector-vertex-tool.svg" },
    { "mActionMoveFeature", "vector/hake-vector-move-feature.svg" },
    { "mActionDeleteSelected", "vector/hake-vector-delete-selected.svg" },
    { "mActionCutFeatures", "vector/hake-vector-cut.svg" },
    { "mActionCopyFeatures", "vector/hake-vector-copy.svg" },
    { "mActionPasteFeatures", "vector/hake-vector-paste.svg" },
    // Selection
    { "mActionSelectFeatures", "selection/hake-selection-select-rectangle.svg" },
    { "mActionSelectPolygon", "selection/hake-selection-select-polygon.svg" },
    { "mActionSelectByExpression", "selection/hake-selection-select-expression.svg" },
    { "mActionDeselectAll", "selection/hake-selection-deselect-all.svg" },
    // Labels
    { "mActionLabeling", "labels/hake-labels-labeling.svg" },
    { "mActionMoveLabel", "labels/hake-labels-move-label.svg" },
    { "mActionRotateLabel", "labels/hake-labels-rotate-label.svg" },
    { "mActionShowPinnedLabels", "labels/hake-labels-pin-labels.svg" },
    { "mActionShowHideLabels", "labels/hake-labels-show-hide-labels.svg" },
    // Raster
    { "mActionLocalHistogramStretch", "raster/hake-raster-local-histogram-stretch.svg" },
    { "mActionFullHistogramStretch", "raster/hake-raster-full-histogram-stretch.svg" },
    { "mActionLocalCumulativeCutStretch", "raster/hake-raster-local-cumulative-cut.svg" },
    { "mActionFullCumulativeCutStretch", "raster/hake-raster-full-cumulative-cut.svg" },
    { "mActionIncreaseBrightness", "raster/hake-raster-brightness-increase.svg" },
    { "mActionDecreaseBrightness", "raster/hake-raster-brightness-decrease.svg" },
    { "mActionIncreaseContrast", "raster/hake-raster-contrast-increase.svg" },
    { "mActionDecreaseContrast", "raster/hake-raster-contrast-decrease.svg" },
    { "mActionIncreaseGamma", "raster/hake-raster-gamma-increase.svg" },
    { "mActionDecreaseGamma", "raster/hake-raster-gamma-decrease.svg" },
    { "mActionShowRasterCalculator", "raster/hake-raster-raster-calculator.svg" },
    // Extensions
    { "mActionManagePlugins", "extensions/hake-extensions-manage-plugins.svg" },
    // Mesh
    { "mActionNewMeshLayer", "mesh/hake-mesh-new-mesh-layer.svg" },
    { "mActionShowMeshCalculator", "mesh/hake-mesh-mesh-calculator.svg" },
    // Help
    { "mActionHelpContents", "help/hake-help-contents.svg" },
    { "mActionQgisHomePage", "help/hake-help-home-page.svg" },
    { "mActionAbout", "help/hake-help-about.svg" },
  };

  constexpr char STOCK_ICON_PROPERTY[] = "hakeStockIcon";
  constexpr char HAKE_ICON_KEY_PROPERTY[] = "hakeIconKey";

  // The SVGs are authored with these exact colors so state variants can be derived by substitution.
  constexpr char HAKE_STROKE[] = "#164A73";
  constexpr char HAKE_FILL[] = "#D6E4F4";
  constexpr char HAKE_ACTIVE_STROKE[] = "#0E3858";
  constexpr char HAKE_DISABLED_STROKE[] = "#A9BCCB";
  constexpr char HAKE_DISABLED_FILL[] = "#EEF3F7";

  /**
   * Renders a Hake SVG at the exact device pixel size requested, with muted colors
   * for QIcon::Disabled and the Hake active navy for QIcon::On (checked actions).
   */
  class QgsHakeIconEngine : public QIconEngine
  {
    public:
      explicit QgsHakeIconEngine( const QByteArray &svg )
        : mSvg( svg )
      {}

      void paint( QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state ) override
      {
        QSvgRenderer renderer( svgFor( mode, state ) );
        renderer.render( painter, QRectF( rect ) );
      }

      QPixmap pixmap( const QSize &size, QIcon::Mode mode, QIcon::State state ) override
      {
        return scaledPixmap( size, mode, state, 1.0 );
      }

      QPixmap scaledPixmap( const QSize &size, QIcon::Mode mode, QIcon::State state, qreal scale ) override
      {
        const QSize deviceSize = ( QSizeF( size ) * scale ).toSize();
        if ( deviceSize.isEmpty() )
          return QPixmap();

        const QString cacheKey = u"%1x%2:%3:%4"_s.arg( deviceSize.width() ).arg( deviceSize.height() ).arg( static_cast<int>( mode ) ).arg( static_cast<int>( state ) );
        auto it = mPixmaps.constFind( cacheKey );
        if ( it != mPixmaps.constEnd() )
          return *it;

        QPixmap pm( deviceSize );
        pm.fill( Qt::transparent );
        {
          QPainter painter( &pm );
          QSvgRenderer renderer( svgFor( mode, state ) );
          renderer.render( &painter, QRectF( QPointF( 0, 0 ), QSizeF( deviceSize ) ) );
        }
        pm.setDevicePixelRatio( scale );
        mPixmaps.insert( cacheKey, pm );
        return pm;
      }

      QSize actualSize( const QSize &size, QIcon::Mode, QIcon::State ) override
      {
        return size;
      }

      QIconEngine *clone() const override
      {
        return new QgsHakeIconEngine( *this );
      }

      QString key() const override
      {
        return u"QgsHakeIconEngine"_s;
      }

    private:
      QByteArray svgFor( QIcon::Mode mode, QIcon::State state ) const
      {
        QByteArray svg = mSvg;
        if ( mode == QIcon::Disabled )
          return svg.replace( HAKE_STROKE, HAKE_DISABLED_STROKE ).replace( HAKE_FILL, HAKE_DISABLED_FILL );
        if ( state == QIcon::On )
          return svg.replace( HAKE_STROKE, HAKE_ACTIVE_STROKE );
        return svg;
      }

      QByteArray mSvg;
      QHash<QString, QPixmap> mPixmaps;
  };
} // namespace

bool QgsHakeIcons::isHakeTheme( const QString &themeName )
{
  return themeName == "Hake Light"_L1;
}

QIcon QgsHakeIcons::icon( const QString &resource )
{
  static QHash<QString, QIcon> sIcons;
  auto it = sIcons.constFind( resource );
  if ( it != sIcons.constEnd() )
    return *it;

  static const bool sResourcesInitialized = [] {
    // Needed when qgis_app is built as a static library; harmless for shared builds.
    Q_INIT_RESOURCE( hake_icons );
    return true;
  }();
  Q_UNUSED( sResourcesInitialized )

  QIcon result;
  QFile file( u":/hake/icons/"_s + resource );
  if ( file.open( QIODevice::ReadOnly ) )
    result = QIcon( new QgsHakeIconEngine( file.readAll() ) );
  sIcons.insert( resource, result );
  return result;
}

void QgsHakeIcons::applyToActions( QObject *root, const QString &themeName )
{
  if ( !root )
    return;

  const bool hakeTheme = isHakeTheme( themeName );
  for ( const HakeActionIcon &entry : HAKE_ACTION_ICONS )
  {
    QAction *action = root->findChild<QAction *>( QLatin1String( entry.objectName ) );
    if ( !action )
      continue;

    const QVariant hakeKey = action->property( HAKE_ICON_KEY_PROPERTY );
    const bool showingHakeIcon = hakeKey.isValid() && hakeKey.toLongLong() == action->icon().cacheKey();

    if ( hakeTheme )
    {
      const QIcon hakeIcon = icon( QLatin1String( entry.resource ) );
      if ( hakeIcon.isNull() )
        continue;
      if ( !showingHakeIcon )
        action->setProperty( STOCK_ICON_PROPERTY, QVariant::fromValue( action->icon() ) );
      action->setIcon( hakeIcon );
      action->setProperty( HAKE_ICON_KEY_PROPERTY, hakeIcon.cacheKey() );
    }
    else if ( hakeKey.isValid() )
    {
      // setTheme() re-assigns most stock icons itself; only icons it does not own
      // (those defined solely in qgisapp.ui) are still showing the Hake icon here.
      if ( showingHakeIcon )
        action->setIcon( action->property( STOCK_ICON_PROPERTY ).value<QIcon>() );
      action->setProperty( STOCK_ICON_PROPERTY, QVariant() );
      action->setProperty( HAKE_ICON_KEY_PROPERTY, QVariant() );
    }
  }
}

QIcon QgsHakeIcons::actionIcon( const QString &stockThemeIcon, const QString &resource )
{
  if ( isHakeTheme( QgsApplication::themeName() ) )
  {
    const QIcon hakeIcon = icon( resource );
    if ( !hakeIcon.isNull() )
      return hakeIcon;
  }
  return QgsApplication::getThemeIcon( stockThemeIcon );
}

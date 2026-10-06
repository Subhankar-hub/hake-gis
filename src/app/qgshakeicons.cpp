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
#include "qgsgui.h"
#include "qgssourceselectprovider.h"
#include "qgssourceselectproviderregistry.h"

#include <QAction>
#include <QActionEvent>
#include <QDockWidget>
#include <QEvent>
#include <QFile>
#include <QHash>
#include <QIconEngine>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QSet>
#include <QStackedWidget>
#include <QString>
#include <QSvgRenderer>
#include <QTimer>
#include <QVariant>

using namespace Qt::StringLiterals;

namespace
{
  enum class HakeIconKind
  {
    Action,       //!< Core QAction, keyed by objectName
    Algorithm,    //!< Processing algorithm menu entry, keyed by algorithm id
    PluginAction, //!< Cataloged action of a bundled plugin, keyed by objectName
    DockPanel,    //!< Dock toggle presented as a ribbon panel, keyed by dock objectName
    DataSource,   //!< Data Source Manager page, keyed by source select provider name
    Menu,         //!< Core menu presented as a ribbon drop-down, keyed by menu objectName
  };

  struct HakeIcon
  {
      HakeIconKind kind;
      const char *key;
      const char *resource;
  };

  constexpr HakeIconKind A = HakeIconKind::Action;
  constexpr HakeIconKind ALG = HakeIconKind::Algorithm;
  constexpr HakeIconKind PLUGIN = HakeIconKind::PluginAction;
  constexpr HakeIconKind DOCK = HakeIconKind::DockPanel;
  constexpr HakeIconKind DSM = HakeIconKind::DataSource;
  constexpr HakeIconKind MENU = HakeIconKind::Menu;

  // The single authoritative Hake icon mapping (validated by scripts/hake_icon_coverage.py).
  // Only core actions and the plugin commands named in the Hake icon catalog may be listed;
  // other plugin and extension actions keep their own icons.
  constexpr HakeIcon HAKE_ICONS[] = {
    // Project
    { A, "mActionNewProject", "project/hake-project-new.svg" },
    { A, "mActionOpenProject", "project/hake-project-open.svg" },
    { A, "mActionSaveProject", "project/hake-project-save.svg" },
    { A, "mActionSaveProjectAs", "project/hake-project-save-as.svg" },
    { A, "mActionProjectProperties", "project/hake-project-properties.svg" },
    { A, "mActionExit", "project/hake-project-exit.svg" },
    // Editing
    { A, "mActionUndo", "editing/hake-editing-undo.svg" },
    { A, "mActionRedo", "editing/hake-editing-redo.svg" },
    // Navigation
    { A, "mActionPan", "navigation/hake-navigation-pan.svg" },
    { A, "mActionZoomIn", "navigation/hake-navigation-zoom-in.svg" },
    { A, "mActionZoomOut", "navigation/hake-navigation-zoom-out.svg" },
    { A, "mActionZoomFullExtent", "navigation/hake-navigation-full-extent.svg" },
    { A, "mActionDraw", "navigation/hake-navigation-refresh.svg" },
    { A, "mActionPanToSelected", "navigation/hake-navigation-pan-to-selection.svg" },
    { A, "mActionZoomToSelected", "navigation/hake-navigation-zoom-to-selection.svg" },
    { A, "mActionZoomToLayers", "navigation/hake-navigation-zoom-to-layers.svg" },
    { A, "mActionZoomActualSize", "navigation/hake-navigation-zoom-native.svg" },
    { A, "mActionZoomLast", "navigation/hake-navigation-zoom-last.svg" },
    { A, "mActionZoomNext", "navigation/hake-navigation-zoom-next.svg" },
    { A, "mActionIdentify", "navigation/hake-navigation-identify.svg" },
    // Layers and data sources
    { A, "mActionDataSourceManager", "layers/hake-layers-data-source-manager.svg" },
    { A, "mActionAddOgrLayer", "layers/hake-layers-add-vector.svg" },
    { A, "mActionAddRasterLayer", "layers/hake-layers-add-raster.svg" },
    { A, "mActionAddMeshLayer", "layers/hake-layers-add-mesh.svg" },
    { A, "mActionAddDelimitedText", "layers/hake-layers-add-delimited-text.svg" },
    { A, "mActionAddSpatiaLiteLayer", "layers/hake-layers-add-spatialite.svg" },
    { A, "mActionAddVirtualLayer", "layers/hake-layers-add-virtual.svg" },
    { A, "mActionAddWmsLayer", "layers/hake-layers-add-wms.svg" },
    { A, "mActionAddWfsLayer", "layers/hake-layers-add-wfs.svg" },
    { A, "mActionRemoveLayer", "layers/hake-layers-remove-layer.svg" },
    { A, "mActionOpenTable", "layers/hake-layers-attribute-table.svg" },
    // Map views, layouts and panels
    { A, "mActionNewPrintLayout", "map/hake-map-new-print-layout.svg" },
    { A, "mActionShowLayoutManager", "map/hake-map-layout-manager.svg" },
    { A, "mActionNewMapCanvas", "map/hake-map-new-map-view.svg" },
    { A, "mActionNew3DMapCanvas", "map/hake-map-new-3d-map-view.svg" },
    { A, "mActionNew3DMapCanvasGlobe", "map/hake-map-new-3d-globe-view.svg" },
    { A, "mActionNewBookmark", "map/hake-map-new-bookmark.svg" },
    { A, "mActionShowBookmarks", "map/hake-map-show-bookmarks.svg" },
    { A, "mActionTemporalController", "map/hake-map-temporal-controller.svg" },
    { A, "mActionToggleFullScreen", "map/hake-map-full-screen.svg" },
    // Measurement
    { A, "mActionMeasure", "measurement/hake-measurement-line.svg" },
    { A, "mActionMeasureArea", "measurement/hake-measurement-area.svg" },
    { A, "mActionMeasureBearing", "measurement/hake-measurement-bearing.svg" },
    { A, "mActionMeasureAngle", "measurement/hake-measurement-angle.svg" },
    // Analysis
    { A, "mActionStatisticalSummary", "analysis/hake-analysis-statistical-summary.svg" },
    { A, "mActionOpenFieldCalc", "analysis/hake-analysis-field-calculator.svg" },
    { A, "mActionShowPythonDialog", "analysis/hake-analysis-python-console.svg" },
    // Application settings
    { A, "mActionOptions", "settings/hake-settings-options.svg" },
    { A, "mActionStyleManager", "settings/hake-settings-style-manager.svg" },
    { A, "mActionCustomProjection", "settings/hake-settings-custom-projection.svg" },
    // Vector editing
    { A, "mActionToggleEditing", "vector/hake-vector-toggle-editing.svg" },
    { A, "mActionSaveLayerEdits", "vector/hake-vector-save-edits.svg" },
    { A, "mActionAddFeature", "vector/hake-vector-add-point.svg" },
    { A, "mActionVertexTool", "vector/hake-vector-vertex-tool.svg" },
    { A, "mActionMoveFeature", "vector/hake-vector-move-feature.svg" },
    { A, "mActionDeleteSelected", "vector/hake-vector-delete-selected.svg" },
    { A, "mActionCutFeatures", "vector/hake-vector-cut.svg" },
    { A, "mActionCopyFeatures", "vector/hake-vector-copy.svg" },
    { A, "mActionPasteFeatures", "vector/hake-vector-paste.svg" },
    // Selection
    { A, "mActionSelectFeatures", "selection/hake-selection-select-rectangle.svg" },
    { A, "mActionSelectPolygon", "selection/hake-selection-select-polygon.svg" },
    { A, "mActionSelectByExpression", "selection/hake-selection-select-expression.svg" },
    { A, "mActionDeselectAll", "selection/hake-selection-deselect-all.svg" },
    // Labels
    { A, "mActionLabeling", "labels/hake-labels-labeling.svg" },
    { A, "mActionMoveLabel", "labels/hake-labels-move-label.svg" },
    { A, "mActionRotateLabel", "labels/hake-labels-rotate-label.svg" },
    { A, "mActionShowPinnedLabels", "labels/hake-labels-pin-labels.svg" },
    { A, "mActionShowHideLabels", "labels/hake-labels-show-hide-labels.svg" },
    // Raster
    { A, "mActionLocalHistogramStretch", "raster/hake-raster-local-histogram-stretch.svg" },
    { A, "mActionFullHistogramStretch", "raster/hake-raster-full-histogram-stretch.svg" },
    { A, "mActionLocalCumulativeCutStretch", "raster/hake-raster-local-cumulative-cut.svg" },
    { A, "mActionFullCumulativeCutStretch", "raster/hake-raster-full-cumulative-cut.svg" },
    { A, "mActionIncreaseBrightness", "raster/hake-raster-brightness-increase.svg" },
    { A, "mActionDecreaseBrightness", "raster/hake-raster-brightness-decrease.svg" },
    { A, "mActionIncreaseContrast", "raster/hake-raster-contrast-increase.svg" },
    { A, "mActionDecreaseContrast", "raster/hake-raster-contrast-decrease.svg" },
    { A, "mActionIncreaseGamma", "raster/hake-raster-gamma-increase.svg" },
    { A, "mActionDecreaseGamma", "raster/hake-raster-gamma-decrease.svg" },
    { A, "mActionShowRasterCalculator", "raster/hake-raster-raster-calculator.svg" },
    // Extensions
    { A, "mActionManagePlugins", "extensions/hake-extensions-manage-plugins.svg" },
    // Mesh
    { A, "mActionNewMeshLayer", "mesh/hake-mesh-new-mesh-layer.svg" },
    { A, "mActionShowMeshCalculator", "mesh/hake-mesh-mesh-calculator.svg" },
    // Help
    { A, "mActionHelpContents", "help/hake-help-contents.svg" },
    { A, "mActionQgisHomePage", "help/hake-help-home-page.svg" },
    { A, "mActionAbout", "help/hake-help-about.svg" },
    { A, "mActionToolSearch", "help/hake-help-tool-search.svg" },

    // Catalog: Project
    { A, "mActionCloseProject", "project/hake-project-close.svg" },
    { A, "mActionRevertProject", "project/hake-project-revert.svg" },
    { A, "mActionSnappingOptions", "project/hake-project-snapping-options.svg" },
    { A, "mActionSaveMapAsImage", "project/hake-project-export-image.svg" },
    { A, "mActionSaveMapAsPdf", "project/hake-project-export-pdf.svg" },
    { A, "mActionDxfExport", "project/hake-project-export-dxf.svg" },
    { A, "mActionDwgImport", "project/hake-project-import-dwg.svg" },
    { A, "mActionNewReport", "project/hake-project-new-report.svg" },
    // Catalog: Edit
    { A, "mActionPasteAsNewVector", "vector/hake-vector-paste-as-new-vector.svg" },
    { A, "mActionPasteAsNewMemoryVector", "vector/hake-vector-paste-as-scratch.svg" },
    { A, "mActionSelectFreehand", "selection/hake-selection-select-freehand.svg" },
    { A, "mActionSelectRadius", "selection/hake-selection-select-radius.svg" },
    { A, "mActionSelectByForm", "selection/hake-selection-select-by-value.svg" },
    { A, "mActionDeselectActiveLayer", "selection/hake-selection-deselect-active-layer.svg" },
    { A, "mActionReselect", "selection/hake-selection-reselect.svg" },
    { A, "mActionSelectAll", "selection/hake-selection-select-all.svg" },
    { A, "mActionInvertSelection", "selection/hake-selection-invert.svg" },
    { A, "mActionFormAnnotation", "map/hake-map-form-annotation.svg" },
    { A, "mActionHtmlAnnotation", "map/hake-map-html-annotation.svg" },
    { A, "mActionMultiEditAttributes", "vector/hake-vector-multi-edit-attributes.svg" },
    { A, "mActionMergeFeatureAttributes", "vector/hake-vector-merge-attributes.svg" },
    { A, "mActionMoveFeatureCopy", "vector/hake-vector-copy-move-feature.svg" },
    { A, "mActionFeatureArray", "vector/hake-vector-feature-array.svg" },
    { A, "mActionRotateFeature", "vector/hake-vector-rotate-feature.svg" },
    { A, "mActionScaleFeature", "vector/hake-vector-scale-feature.svg" },
    { A, "mActionSimplifyFeature", "vector/hake-vector-simplify-feature.svg" },
    { A, "mActionAddRing", "vector/hake-vector-add-ring.svg" },
    { A, "mActionAddPart", "vector/hake-vector-add-part.svg" },
    { A, "mActionFillRing", "vector/hake-vector-fill-ring.svg" },
    { A, "mActionDeleteRing", "vector/hake-vector-delete-ring.svg" },
    { A, "mActionDeletePart", "vector/hake-vector-delete-part.svg" },
    { A, "mActionReshapeFeatures", "vector/hake-vector-reshape.svg" },
    { A, "mActionOffsetCurve", "vector/hake-vector-offset-curve.svg" },
    { A, "mActionChamferFillet", "vector/hake-vector-chamfer-fillet.svg" },
    { A, "mActionSplitFeatures", "vector/hake-vector-split-features.svg" },
    { A, "mActionSplitParts", "vector/hake-vector-split-parts.svg" },
    { A, "mActionMergeFeatures", "vector/hake-vector-merge-features.svg" },
    { A, "mActionReverseLine", "vector/hake-vector-reverse-line.svg" },
    { A, "mActionTrimExtendFeature", "vector/hake-vector-trim-extend.svg" },
    { A, "mActionRotatePointSymbols", "vector/hake-vector-rotate-point-symbols.svg" },
    { A, "mActionOffsetPointSymbol", "vector/hake-vector-offset-point-symbol.svg" },
    // Catalog: View
    { A, "mActionManage3DMapViews", "map/hake-map-manage-3d-views.svg" },
    { A, "mActionElevationController", "map/hake-map-elevation-controller.svg" },
    { A, "mActionDecorationGrid", "map/hake-map-decoration-grid.svg" },
    { A, "mActionDecorationScaleBar", "map/hake-map-decoration-scale-bar.svg" },
    { A, "mActionDecorationImage", "map/hake-map-decoration-image.svg" },
    { A, "mActionDecorationNorthArrow", "map/hake-map-decoration-north-arrow.svg" },
    { A, "mActionDecorationTitle", "map/hake-map-decoration-title.svg" },
    { A, "mActionDecorationCopyright", "map/hake-map-decoration-copyright.svg" },
    { A, "mActionDecorationLayoutExtent", "map/hake-map-decoration-layout-extent.svg" },
    { A, "mActionPreviewModeOff", "map/hake-map-preview-normal.svg" },
    { A, "mActionPreviewModeMono", "map/hake-map-preview-monochrome.svg" },
    { A, "mActionPreviewModeGrayscale", "map/hake-map-preview-grayscale.svg" },
    { A, "mActionPreviewProtanope", "map/hake-map-preview-protanopia.svg" },
    { A, "mActionPreviewDeuteranope", "map/hake-map-preview-deuteranopia.svg" },
    { A, "mActionPreviewTritanope", "map/hake-map-preview-tritanopia.svg" },
    { A, "mActionMapTips", "map/hake-map-map-tips.svg" },
    { A, "mActionShowBookmarkManager", "map/hake-map-bookmark-manager.svg" },
    { A, "mActionShowAllLayers", "layers/hake-layers-show-all.svg" },
    { A, "mActionHideAllLayers", "layers/hake-layers-hide-all.svg" },
    { A, "mActionShowSelectedLayers", "layers/hake-layers-show-selected.svg" },
    { A, "mActionHideSelectedLayers", "layers/hake-layers-hide-selected.svg" },
    { A, "mActionToggleSelectedLayers", "layers/hake-layers-toggle-selected.svg" },
    { A, "mActionToggleSelectedLayersIndependently", "layers/hake-layers-toggle-selected-independently.svg" },
    { A, "mActionHideDeselectedLayers", "layers/hake-layers-hide-deselected.svg" },
    // Catalog: Layer
    { A, "mActionNewGeoPackageLayer", "layers/hake-layers-new-geopackage.svg" },
    { A, "mActionNewVectorLayer", "layers/hake-layers-new-shapefile.svg" },
    { A, "mActionNewSpatiaLiteLayer", "layers/hake-layers-new-spatialite.svg" },
    { A, "mActionNewMemoryLayer", "layers/hake-layers-new-scratch.svg" },
    { A, "mActionNewGpxLayer", "layers/hake-layers-new-gpx.svg" },
    { A, "mActionNewVirtualLayer", "layers/hake-layers-new-virtual.svg" },
    { A, "mActionAddPgLayer", "layers/hake-layers-add-postgresql.svg" },
    { A, "mActionAddMssqlLayer", "layers/hake-layers-add-mssql.svg" },
    { A, "mActionAddOracleLayer", "layers/hake-layers-add-oracle.svg" },
    { A, "mActionAddHanaLayer", "layers/hake-layers-add-hana.svg" },
    { A, "mActionAddXyzLayer", "layers/hake-layers-add-xyz.svg" },
    { A, "mActionAddWcsLayer", "layers/hake-layers-add-wcs.svg" },
    { A, "mActionAddAfsLayer", "layers/hake-layers-add-arcgis-rest.svg" },
    { A, "mActionAddVectorTileLayer", "layers/hake-layers-add-vector-tile.svg" },
    { A, "mActionAddPointCloudLayer", "layers/hake-layers-add-point-cloud.svg" },
    { A, "mActionAddGpsLayer", "layers/hake-layers-add-gpx.svg" },
    { A, "mActionAddStacLayer", "layers/hake-layers-add-stac.svg" },
    { A, "mActionEmbedLayers", "layers/hake-layers-embed.svg" },
    { A, "mActionAddLayerDefinition", "layers/hake-layers-add-layer-definition.svg" },
    { A, "mActionShowGeoreferencer", "raster/hake-raster-georeferencer.svg" },
    { A, "mActionCopyStyle", "layers/hake-layers-copy-style.svg" },
    { A, "mActionPasteStyle", "layers/hake-layers-paste-style.svg" },
    { A, "mActionCopyLayer", "layers/hake-layers-copy-layer.svg" },
    { A, "mActionPasteLayer", "layers/hake-layers-paste-layer.svg" },
    { A, "mActionOpenTableSelected", "layers/hake-layers-attribute-table-selected.svg" },
    { A, "mActionOpenTableVisible", "layers/hake-layers-attribute-table-visible.svg" },
    { A, "mActionOpenTableEdited", "layers/hake-layers-attribute-table-edited.svg" },
    { A, "mActionAllEdits", "vector/hake-vector-current-edits.svg" },
    { A, "mActionLayerSaveAs", "layers/hake-layers-save-as.svg" },
    { A, "mActionSaveLayerDefinition", "layers/hake-layers-save-layer-definition.svg" },
    { A, "mActionDuplicateLayer", "layers/hake-layers-duplicate.svg" },
    { A, "mActionSetLayerScaleVisibility", "layers/hake-layers-scale-visibility.svg" },
    { A, "mActionSetLayerCRS", "layers/hake-layers-set-crs.svg" },
    { A, "mActionSetProjectCRSFromLayer", "layers/hake-layers-project-crs-from-layer.svg" },
    { A, "mActionLayerProperties", "layers/hake-layers-properties.svg" },
    { A, "mActionLayerSubsetString", "layers/hake-layers-filter.svg" },
    { A, "mActionAddToOverview", "layers/hake-layers-show-in-overview.svg" },
    { A, "mActionAddAllToOverview", "layers/hake-layers-show-all-in-overview.svg" },
    { A, "mActionRemoveAllFromOverview", "layers/hake-layers-hide-all-from-overview.svg" },
    // Catalog: Settings
    { A, "mActionConfigureShortcuts", "settings/hake-settings-keyboard-shortcuts.svg" },
    { A, "mActionCustomization", "settings/hake-settings-interface-customization.svg" },
    // Catalog: Database
    { A, "mActionQueryHistory", "database/hake-database-query-history.svg" },
    // Catalog: Help
    { A, "mActionHelpAPI", "help/hake-help-cpp-api.svg" },
    { A, "mActionHelpPyQgisAPI", "help/hake-help-python-api.svg" },
    { A, "mActionReportaBug", "help/hake-help-report-issue.svg" },
    { A, "mActionDonate", "help/hake-help-donate.svg" },
    { A, "mActionGetInvolved", "help/hake-help-get-involved.svg" },
    { A, "mActionNeedSupport", "help/hake-help-commercial-support.svg" },
    { A, "mActionSponsors", "help/hake-help-sustaining-members.svg" },
    { A, "mActionCheckQgisVersion", "help/hake-help-check-version.svg" },

    // Catalog: Ribbon panels (dock toggles)
    { DOCK, "ProcessingToolbox", "processing/hake-processing-toolbox.svg" },
    { DOCK, "Browser", "map/hake-map-browser-panel.svg" },
    { DOCK, "Layers", "map/hake-map-layers-panel.svg" },

    // Ribbon menu drop-downs (core menus only; extension menus keep their own icons)
    { MENU, "mProjectMenu", "menus/hake-menu-project.svg" },
    { MENU, "mEditMenu", "menus/hake-menu-edit.svg" },
    { MENU, "mViewMenu", "menus/hake-menu-view.svg" },
    { MENU, "mLayerMenu", "menus/hake-menu-layer.svg" },
    { MENU, "mSettingsMenu", "menus/hake-menu-settings.svg" },
    { MENU, "mPluginMenu", "menus/hake-menu-plugins.svg" },
    { MENU, "mVectorMenu", "menus/hake-menu-vector.svg" },
    { MENU, "mRasterMenu", "menus/hake-menu-raster.svg" },
    { MENU, "mDatabaseMenu", "menus/hake-menu-database.svg" },
    { MENU, "mWebMenu", "menus/hake-menu-web.svg" },
    { MENU, "mMeshMenu", "menus/hake-menu-mesh.svg" },
    { MENU, "mHelpMenu", "menus/hake-menu-help.svg" },
    { MENU, "processing", "menus/hake-menu-processing.svg" },

    // Catalog: Data Source Manager pages ("browser" is the built-in page without a provider)
    { DSM, "browser", "map/hake-map-browser-panel.svg" },
    { DSM, "ogr", "layers/hake-layers-add-vector.svg" },
    { DSM, "gdal", "layers/hake-layers-add-raster.svg" },
    { DSM, "mdal", "layers/hake-layers-add-mesh.svg" },
    { DSM, "pointcloud", "layers/hake-layers-add-point-cloud.svg" },
    { DSM, "delimitedtext", "layers/hake-layers-add-delimited-text.svg" },
    { DSM, "GeoPackage", "layers/hake-layers-source-geopackage.svg" },
    { DSM, "gpx", "layers/hake-layers-add-gpx.svg" },
    { DSM, "spatialite", "layers/hake-layers-add-spatialite.svg" },
    { DSM, "postgres", "layers/hake-layers-add-postgresql.svg" },
    { DSM, "mssql", "layers/hake-layers-add-mssql.svg" },
    { DSM, "oracle", "layers/hake-layers-add-oracle.svg" },
    { DSM, "hana", "layers/hake-layers-add-hana.svg" },
    { DSM, "virtual", "layers/hake-layers-add-virtual.svg" },
    { DSM, "wms", "layers/hake-layers-add-wms.svg" },
    { DSM, "WFS", "layers/hake-layers-add-wfs.svg" },
    { DSM, "wcs", "layers/hake-layers-add-wcs.svg" },
    { DSM, "xyz", "layers/hake-layers-add-xyz.svg" },
    { DSM, "vectortile", "layers/hake-layers-add-vector-tile.svg" },
    { DSM, "tiledscene", "layers/hake-layers-source-scene.svg" },
    { DSM, "arcgisfeatureserver", "layers/hake-layers-add-arcgis-rest.svg" },
    { DSM, "sensorthings", "layers/hake-layers-source-sensorthings.svg" },
    { DSM, "stac", "layers/hake-layers-add-stac.svg" },
    { DSM, "layermetadata", "web/hake-web-metasearch.svg" },

    // Catalog: Processing, Database and Web plugin commands
    { PLUGIN, "toolboxAction", "processing/hake-processing-toolbox.svg" },
    { PLUGIN, "modelerAction", "processing/hake-processing-model-designer.svg" },
    { PLUGIN, "historyAction", "processing/hake-processing-history.svg" },
    { PLUGIN, "resultsViewer", "processing/hake-processing-results-viewer.svg" },
    { PLUGIN, "editInPlaceFeatures", "processing/hake-processing-edit-in-place.svg" },
    { PLUGIN, "dbManager", "database/hake-database-db-manager.svg" },
    { PLUGIN, "action_run", "web/hake-web-metasearch.svg" },
    { PLUGIN, "action_help", "web/hake-web-metasearch-help.svg" },

    // Catalog: Vector > Analysis Tools
    { ALG, "native:distancematrix", "analysis/hake-analysis-distance-matrix.svg" },
    { ALG, "native:sumlinelengths", "analysis/hake-analysis-sum-line-lengths.svg" },
    { ALG, "native:countpointsinpolygon", "analysis/hake-analysis-count-points-in-polygon.svg" },
    { ALG, "native:listuniquevalues", "analysis/hake-analysis-list-unique-values.svg" },
    { ALG, "native:basicstatisticsforfields", "analysis/hake-analysis-basic-statistics.svg" },
    { ALG, "native:nearestneighbouranalysis", "analysis/hake-analysis-nearest-neighbour.svg" },
    { ALG, "native:meancoordinates", "analysis/hake-analysis-mean-coordinates.svg" },
    { ALG, "native:lineintersections", "analysis/hake-analysis-line-intersections.svg" },
    // Catalog: Vector > Research Tools
    { ALG, "native:creategrid", "analysis/hake-analysis-create-grid.svg" },
    { ALG, "native:randomselection", "analysis/hake-analysis-random-selection.svg" },
    { ALG, "native:randomselectionwithinsubsets", "analysis/hake-analysis-random-selection-subsets.svg" },
    { ALG, "native:randompointsinextent", "analysis/hake-analysis-random-points-extent.svg" },
    { ALG, "qgis:randompointsinlayerbounds", "analysis/hake-analysis-random-points-layer-bounds.svg" },
    { ALG, "native:randompointsinpolygons", "analysis/hake-analysis-random-points-in-polygons.svg" },
    { ALG, "qgis:randompointsinsidepolygons", "analysis/hake-analysis-random-points-inside-polygons.svg" },
    { ALG, "native:randompointsonlines", "analysis/hake-analysis-random-points-on-lines.svg" },
    { ALG, "qgis:regularpoints", "analysis/hake-analysis-regular-points.svg" },
    { ALG, "native:selectbylocation", "selection/hake-selection-select-by-location.svg" },
    { ALG, "native:selectwithindistance", "selection/hake-selection-select-within-distance.svg" },
    { ALG, "native:polygonfromlayerextent", "analysis/hake-analysis-polygon-from-extent.svg" },
    // Catalog: Vector > Geoprocessing Tools
    { ALG, "native:buffer", "vector/hake-vector-buffer.svg" },
    { ALG, "native:convexhull", "vector/hake-vector-convex-hull.svg" },
    { ALG, "native:intersection", "vector/hake-vector-intersection.svg" },
    { ALG, "native:union", "vector/hake-vector-union.svg" },
    { ALG, "native:symmetricaldifference", "vector/hake-vector-symmetrical-difference.svg" },
    { ALG, "native:clip", "vector/hake-vector-clip.svg" },
    { ALG, "native:difference", "vector/hake-vector-difference.svg" },
    { ALG, "native:dissolve", "vector/hake-vector-dissolve.svg" },
    { ALG, "qgis:eliminateselectedpolygons", "vector/hake-vector-eliminate-polygons.svg" },
    // Catalog: Vector > Geometry Tools
    { ALG, "native:checkvalidity", "vector/hake-vector-check-validity.svg" },
    { ALG, "native:exportaddgeometrycolumns", "vector/hake-vector-add-geometry-attributes.svg" },
    { ALG, "native:centroids", "vector/hake-vector-centroids.svg" },
    { ALG, "native:delaunaytriangulation", "vector/hake-vector-delaunay.svg" },
    { ALG, "native:voronoipolygons", "vector/hake-vector-voronoi.svg" },
    { ALG, "native:simplifygeometries", "vector/hake-vector-simplify.svg" },
    { ALG, "native:densifygeometries", "vector/hake-vector-densify.svg" },
    { ALG, "native:multiparttosingleparts", "vector/hake-vector-multipart-to-singleparts.svg" },
    { ALG, "native:collect", "vector/hake-vector-collect.svg" },
    { ALG, "native:polygonstolines", "vector/hake-vector-polygons-to-lines.svg" },
    { ALG, "qgis:linestopolygons", "vector/hake-vector-lines-to-polygons.svg" },
    { ALG, "native:extractvertices", "vector/hake-vector-extract-vertices.svg" },
    // Catalog: Vector > Data Management Tools
    { ALG, "native:reprojectlayer", "vector/hake-vector-reproject-layer.svg" },
    { ALG, "native:joinattributesbylocation", "vector/hake-vector-join-by-location.svg" },
    { ALG, "native:splitvectorlayer", "vector/hake-vector-split-layer.svg" },
    { ALG, "native:mergevectorlayers", "vector/hake-vector-merge-layers.svg" },
    { ALG, "native:createspatialindex", "vector/hake-vector-spatial-index.svg" },
    // Catalog: Raster
    { ALG, "native:alignrasters", "raster/hake-raster-align-rasters.svg" },
    { ALG, "gdal:warpreproject", "raster/hake-raster-warp-reproject.svg" },
    { ALG, "gdal:extractprojection", "raster/hake-raster-extract-projection.svg" },
    { ALG, "gdal:assignprojection", "raster/hake-raster-assign-projection.svg" },
    { ALG, "gdal:rasterize", "raster/hake-raster-rasterize.svg" },
    { ALG, "gdal:polygonize", "raster/hake-raster-polygonize.svg" },
    { ALG, "gdal:translate", "raster/hake-raster-translate.svg" },
    { ALG, "gdal:rgbtopct", "raster/hake-raster-rgb-to-palette.svg" },
    { ALG, "gdal:pcttorgb", "raster/hake-raster-palette-to-rgb.svg" },
    { ALG, "gdal:contour", "raster/hake-raster-contour.svg" },
    { ALG, "gdal:cliprasterbyextent", "raster/hake-raster-clip-by-extent.svg" },
    { ALG, "gdal:cliprasterbymasklayer", "raster/hake-raster-clip-by-mask.svg" },
    { ALG, "gdal:sieve", "raster/hake-raster-sieve.svg" },
    { ALG, "gdal:nearblack", "raster/hake-raster-near-black.svg" },
    { ALG, "gdal:fillnodata", "raster/hake-raster-fill-nodata.svg" },
    { ALG, "gdal:proximity", "raster/hake-raster-proximity.svg" },
    { ALG, "gdal:griddatametrics", "raster/hake-raster-grid-data-metrics.svg" },
    { ALG, "gdal:gridaverage", "raster/hake-raster-grid-average.svg" },
    { ALG, "gdal:gridinversedistance", "raster/hake-raster-grid-inverse-distance.svg" },
    { ALG, "gdal:gridnearestneighbor", "raster/hake-raster-grid-nearest-neighbour.svg" },
    { ALG, "gdal:aspect", "raster/hake-raster-aspect.svg" },
    { ALG, "gdal:hillshade", "raster/hake-raster-hillshade.svg" },
    { ALG, "gdal:roughness", "raster/hake-raster-roughness.svg" },
    { ALG, "gdal:slope", "raster/hake-raster-slope.svg" },
    { ALG, "gdal:tpitopographicpositionindex", "raster/hake-raster-tpi.svg" },
    { ALG, "gdal:triterrainruggednessindex", "raster/hake-raster-tri.svg" },
    { ALG, "gdal:buildvirtualraster", "raster/hake-raster-build-virtual-raster.svg" },
    { ALG, "gdal:merge", "raster/hake-raster-merge.svg" },
    { ALG, "gdal:gdalinfo", "raster/hake-raster-information.svg" },
    { ALG, "gdal:overviews", "raster/hake-raster-build-overviews.svg" },
    { ALG, "gdal:tileindex", "raster/hake-raster-tile-index.svg" },
  };

  struct HakePropertyPageIcon
  {
      const char *page;
      const char *resource;
  };

  // Layer properties sidebar pages, keyed by the stacked page objectName shared by all layer
  // properties dialogs. Pages not listed here (provider, plugin, 3D and layer-type specific
  // pages) keep their own icons.
  constexpr HakePropertyPageIcon HAKE_PROPERTY_PAGE_ICONS[] = {
    { "mOptsPage_Information", "properties/hake-properties-information.svg" },
    { "mOptsPage_Source", "properties/hake-properties-source.svg" },
    { "mOptsPage_Style", "properties/hake-properties-symbology.svg" },
    { "mOptsPage_Labels", "labels/hake-labels-labeling.svg" },
    { "mOptsPage_Labeling", "labels/hake-labels-labeling.svg" },
    { "mOptsPage_Masks", "properties/hake-properties-masks.svg" },
    { "mOptsPage_Diagrams", "properties/hake-properties-diagrams.svg" },
    { "mOptsPage_SourceFields", "properties/hake-properties-fields.svg" },
    { "mOptsPage_AttributesForm", "properties/hake-properties-attributes-form.svg" },
    { "mOptsPage_Joins", "properties/hake-properties-joins.svg" },
    { "mOptsPage_AuxiliaryStorage", "properties/hake-properties-auxiliary-storage.svg" },
    { "mOptsPage_Actions", "properties/hake-properties-actions.svg" },
    { "mOptsPage_Display", "properties/hake-properties-display.svg" },
    { "mOptsPage_Rendering", "properties/hake-properties-rendering.svg" },
    { "mOptsPage_Temporal", "map/hake-map-temporal-controller.svg" },
    { "mOptsPage_Variables", "properties/hake-properties-variables.svg" },
    { "mOptsPage_Elevation", "map/hake-map-elevation-controller.svg" },
    { "mOptsPage_Metadata", "properties/hake-properties-metadata.svg" },
    { "mOptsPage_DataDependencies", "properties/hake-properties-dependencies.svg" },
    { "mOptsPage_Legend", "properties/hake-properties-legend.svg" },
    { "mOptsPage_Server", "properties/hake-properties-server.svg" },
    { "mOptsPage_Digitizing", "properties/hake-properties-digitizing.svg" },
  };

  // MetaSearch uses generic action names, so they are only recognised inside its own Web submenu.
  constexpr char METASEARCH_MENU[] = "MetaSearch";
  // Processing menu entries, and the Selection toolbar buttons for the same algorithms.
  constexpr char PROCESSING_MENU_PREFIX[] = "mProcessingUserMenu_";
  constexpr char PROCESSING_TOOLBAR_PREFIX[] = "mProcessingAlg_";

  constexpr char STOCK_ICON_PROPERTY[] = "hakeStockIcon";
  constexpr char HAKE_ICON_KEY_PROPERTY[] = "hakeIconKey";

  // The SVGs are authored with these exact colors so state variants can be derived by substitution.
  constexpr char HAKE_STROKE[] = "#164A73";
  constexpr char HAKE_FILL[] = "#D6E4F4";
  constexpr char HAKE_PAPER[] = "#FFFFFF";

  //! Colors substituted for the authored colors, per theme variant.
  struct HakeIconPalette
  {
      const char *stroke;
      const char *fill;
      const char *paper;
      const char *activeStroke;
      const char *disabledStroke;
      const char *disabledFill;
  };

  // Hake Light: the authored colors themselves.
  constexpr HakeIconPalette LIGHT_ICON_PALETTE { HAKE_STROKE, HAKE_FILL, HAKE_PAPER, "#0E3858", "#A9BCCB", "#EEF3F7" };
  // Hake Night: pale glyphs on dark surfaces.
  constexpr HakeIconPalette NIGHT_ICON_PALETTE { "#CFE0F0", "#2B4A6B", "#1A2533", "#8EC5F5", "#5C6F84", "#1F2C3C" };

  /**
   * Replaces the authored stroke, fill and paper colors in one pass, so a replacement
   * color can never be matched again by a later substitution.
   */
  QByteArray substituteColors( const QByteArray &svg, const char *stroke, const char *fill, const char *paper )
  {
    constexpr qsizetype colorLength = 7;
    QByteArray out;
    out.reserve( svg.size() );
    qsizetype i = 0;
    while ( i < svg.size() )
    {
      if ( svg.at( i ) == '#' && i + colorLength <= svg.size() )
      {
        const QByteArray token = svg.mid( i, colorLength );
        const char *replacement = nullptr;
        if ( token == HAKE_STROKE )
          replacement = stroke;
        else if ( token == HAKE_FILL )
          replacement = fill;
        else if ( token == HAKE_PAPER )
          replacement = paper;
        if ( replacement )
        {
          out.append( replacement );
          i += colorLength;
          continue;
        }
      }
      out.append( svg.at( i ) );
      ++i;
    }
    return out;
  }

  /**
   * Renders a Hake SVG at the exact device pixel size requested, with muted colors
   * for QIcon::Disabled and the active stroke for QIcon::On (checked actions), in the
   * colors of the Hake Light or Hake Night variant.
   */
  class QgsHakeIconEngine : public QIconEngine
  {
    public:
      QgsHakeIconEngine( const QByteArray &svg, bool night )
        : mSvg( svg )
        , mNight( night )
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
        const HakeIconPalette &palette = mNight ? NIGHT_ICON_PALETTE : LIGHT_ICON_PALETTE;
        if ( mode == QIcon::Disabled )
          return substituteColors( mSvg, palette.disabledStroke, palette.disabledFill, palette.paper );
        if ( state == QIcon::On )
          return substituteColors( mSvg, palette.activeStroke, palette.fill, palette.paper );
        if ( !mNight )
          return mSvg;
        return substituteColors( mSvg, palette.stroke, palette.fill, palette.paper );
      }

      QByteArray mSvg;
      bool mNight = false;
      QHash<QString, QPixmap> mPixmaps;
  };
} // namespace

bool QgsHakeIcons::isHakeTheme( const QString &themeName )
{
  return QgsHakeTheme::variantForTheme( themeName ) != QgsHakeTheme::Variant::None;
}

QIcon QgsHakeIcons::icon( const QString &resource )
{
  return icon( resource, QgsHakeTheme::variantForTheme( QgsApplication::themeName() ) );
}

QIcon QgsHakeIcons::icon( const QString &resource, QgsHakeTheme::Variant variant )
{
  // At most one entry per resource and variant, so the cache stays bounded across theme switches.
  static QHash<QString, QIcon> sIcons;
  const bool night = variant == QgsHakeTheme::Variant::Night;
  const QString cacheKey = ( night ? u"night:"_s : u"light:"_s ) + resource;
  auto it = sIcons.constFind( cacheKey );
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
    result = QIcon( new QgsHakeIconEngine( file.readAll(), night ) );
  sIcons.insert( cacheKey, result );
  return result;
}

namespace
{
  // Marks Data Source Manager list items currently showing a Hake icon.
  constexpr int DSM_HAKE_ICON_ROLE = Qt::UserRole + 0x4841;
  // Holds the stock icon of a layer properties sidebar item currently showing a Hake icon.
  constexpr int PROPERTIES_STOCK_ICON_ROLE = Qt::UserRole + 0x4842;
  // Holds the stock icon of a Layer Properties sidebar item currently showing a Hake icon.
  constexpr int PROPERTY_PAGE_STOCK_ICON_ROLE = Qt::UserRole + 0x4842;

  void applyToAction( QAction *action, const char *resource, QgsHakeTheme::Variant variant )
  {
    if ( !action )
      return;
    const QVariant hakeKey = action->property( HAKE_ICON_KEY_PROPERTY );
    const bool showingHakeIcon = hakeKey.isValid() && hakeKey.toLongLong() == action->icon().cacheKey();

    if ( variant != QgsHakeTheme::Variant::None )
    {
      const QIcon hakeIcon = QgsHakeIcons::icon( QLatin1String( resource ), variant );
      if ( hakeIcon.isNull() )
        return;
      // When the action still shows a Hake icon (Light <-> Night switch) the stored stock icon is kept.
      if ( !showingHakeIcon )
        action->setProperty( STOCK_ICON_PROPERTY, QVariant::fromValue( action->icon() ) );
      if ( showingHakeIcon && hakeKey.toLongLong() == hakeIcon.cacheKey() )
        return;
      action->setIcon( hakeIcon );
      action->setProperty( HAKE_ICON_KEY_PROPERTY, hakeIcon.cacheKey() );
    }
    else if ( hakeKey.isValid() )
    {
      // setTheme() re-assigns most stock icons itself; only icons it does not own
      // (those defined solely in qgisapp.ui, plugin actions, dock toggles) are still
      // showing the Hake icon here.
      if ( showingHakeIcon )
        action->setIcon( action->property( STOCK_ICON_PROPERTY ).value<QIcon>() );
      action->setProperty( STOCK_ICON_PROPERTY, QVariant() );
      action->setProperty( HAKE_ICON_KEY_PROPERTY, QVariant() );
    }
  }

  bool isInMenu( const QAction *action, const QString &menuObjectName )
  {
    const QList<QObject *> objects = action->associatedObjects();
    for ( const QObject *object : objects )
    {
      if ( qobject_cast<const QMenu *>( object ) && object->objectName() == menuObjectName )
        return true;
    }
    return false;
  }

  /**
   * Re-applies the Hake icons when bundled plugins add their menu entries after startup
   * (plugin enabled or reloaded, Processing menus rebuilt).
   */
  class QgsHakeMenuWatcher : public QObject
  {
    public:
      explicit QgsHakeMenuWatcher( QObject *root )
        : QObject( root )
        , mRoot( root )
      {}

      void watch( QWidget *widget )
      {
        if ( !widget || mWatched.contains( widget ) )
          return;
        mWatched.insert( widget );
        connect( widget, &QObject::destroyed, this, [this]( QObject *object ) { mWatched.remove( object ); } );
        widget->installEventFilter( this );
        const QList<QAction *> actions = widget->actions();
        for ( QAction *action : actions )
        {
          if ( QMenu *menu = action->menu() )
            watch( menu );
        }
      }

    protected:
      bool eventFilter( QObject *watched, QEvent *event ) override
      {
        if ( event->type() == QEvent::ActionAdded )
        {
          if ( QAction *action = static_cast<QActionEvent *>( event )->action() )
          {
            if ( QMenu *menu = action->menu() )
              watch( menu );
          }
          scheduleApply();
        }
        return QObject::eventFilter( watched, event );
      }

    private:
      void scheduleApply()
      {
        if ( mApplyPending )
          return;
        mApplyPending = true;
        QTimer::singleShot( 0, this, [this] {
          mApplyPending = false;
          QgsHakeIcons::applyToActions( mRoot, QgsApplication::themeName() );
        } );
      }

      QObject *mRoot = nullptr;
      QSet<QObject *> mWatched;
      bool mApplyPending = false;
  };
} // namespace

void QgsHakeIcons::applyToActions( QObject *root, const QString &themeName )
{
  if ( !root )
    return;

  const QgsHakeTheme::Variant hakeTheme = QgsHakeTheme::variantForTheme( themeName );

  // Plugin and Processing actions can share an objectName (e.g. stale copies after a plugin reload), so look them all up.
  QMultiHash<QString, QAction *> namedActions;
  const QList<QAction *> allActions = root->findChildren<QAction *>();
  for ( QAction *action : allActions )
  {
    if ( !action->objectName().isEmpty() )
      namedActions.insert( action->objectName(), action );
  }

  for ( const HakeIcon &entry : HAKE_ICONS )
  {
    const QString key = QLatin1String( entry.key );
    switch ( entry.kind )
    {
      case HakeIconKind::Action:
      {
        if ( QAction *action = root->findChild<QAction *>( key ) )
          applyToAction( action, entry.resource, hakeTheme );
        break;
      }

      case HakeIconKind::Algorithm:
      {
        QList<QAction *> actions = namedActions.values( QLatin1String( PROCESSING_MENU_PREFIX ) + key );
        actions += namedActions.values( QLatin1String( PROCESSING_TOOLBAR_PREFIX ) + key );
        for ( QAction *action : std::as_const( actions ) )
          applyToAction( action, entry.resource, hakeTheme );
        break;
      }

      case HakeIconKind::PluginAction:
      {
        const bool metaSearchAction = key.startsWith( "action_"_L1 );
        const QList<QAction *> actions = namedActions.values( key );
        for ( QAction *action : actions )
        {
          if ( metaSearchAction && !isInMenu( action, QLatin1String( METASEARCH_MENU ) ) )
            continue;
          applyToAction( action, entry.resource, hakeTheme );
        }
        break;
      }

      case HakeIconKind::DockPanel:
      {
        if ( QDockWidget *dock = root->findChild<QDockWidget *>( key ) )
          applyToAction( dock->toggleViewAction(), entry.resource, hakeTheme );
        break;
      }

      case HakeIconKind::Menu:
      {
        if ( QMenu *menu = root->findChild<QMenu *>( key ) )
          applyToAction( menu->menuAction(), entry.resource, hakeTheme );
        break;
      }

      case HakeIconKind::DataSource:
        break;
    }
  }
}

void QgsHakeIcons::applyToDataSourceManager( QWidget *dialog, const QString &themeName )
{
  if ( !dialog )
    return;
  QListWidget *list = dialog->findChild<QListWidget *>( u"mOptionsListWidget"_s );
  if ( !list )
    return;

  QHash<QString, const char *> resources;
  for ( const HakeIcon &entry : HAKE_ICONS )
  {
    if ( entry.kind == HakeIconKind::DataSource )
      resources.insert( QLatin1String( entry.key ), entry.resource );
  }

  const QgsHakeTheme::Variant variant = QgsHakeTheme::variantForTheme( themeName );
  for ( int row = 0; row < list->count(); ++row )
  {
    QListWidgetItem *item = list->item( row );
    // The built-in Browser page is the only item without a provider name.
    QString key = item->data( Qt::UserRole ).toString();
    if ( key.isEmpty() )
      key = u"browser"_s;

    const auto it = resources.constFind( key );
    if ( it == resources.constEnd() )
      continue;

    if ( variant != QgsHakeTheme::Variant::None )
    {
      const QIcon hakeIcon = icon( QLatin1String( *it ), variant );
      if ( hakeIcon.isNull() )
        continue;
      item->setIcon( hakeIcon );
      item->setData( DSM_HAKE_ICON_ROLE, true );
    }
    else if ( item->data( DSM_HAKE_ICON_ROLE ).isValid() )
    {
      if ( key == "browser"_L1 )
        item->setIcon( QIcon( u":/images/themes/default/mActionFileOpen.svg"_s ) );
      else if ( QgsSourceSelectProvider *provider = QgsGui::sourceSelectProviderRegistry()->providerByName( key ) )
        item->setIcon( provider->icon() );
      item->setData( DSM_HAKE_ICON_ROLE, QVariant() );
    }
  }
}

void QgsHakeIcons::applyToLayerProperties( QWidget *dialog, const QString &themeName )
{
  if ( !dialog )
    return;
  QListWidget *list = dialog->findChild<QListWidget *>( u"mOptionsListWidget"_s );
  QStackedWidget *stack = dialog->findChild<QStackedWidget *>( u"mOptionsStackedWidget"_s );
  if ( !list || !stack )
    return;

  QHash<QString, const char *> resources;
  for ( const HakePropertyPageIcon &entry : HAKE_PROPERTY_PAGE_ICONS )
    resources.insert( QLatin1String( entry.page ), entry.resource );

  const QgsHakeTheme::Variant variant = QgsHakeTheme::variantForTheme( themeName );
  // Sidebar rows and stacked pages are kept in the same order by QgsOptionsDialogBase.
  const int rows = std::min( list->count(), stack->count() );
  for ( int row = 0; row < rows; ++row )
  {
    QListWidgetItem *item = list->item( row );
    const QWidget *page = stack->widget( row );
    const auto it = resources.constFind( page ? page->objectName() : QString() );
    if ( it == resources.constEnd() )
      continue;

    const QVariant stockIcon = item->data( PROPERTIES_STOCK_ICON_ROLE );
    if ( variant != QgsHakeTheme::Variant::None )
    {
      const QIcon hakeIcon = icon( QLatin1String( *it ), variant );
      if ( hakeIcon.isNull() )
        continue;
      if ( !stockIcon.isValid() )
        item->setData( PROPERTIES_STOCK_ICON_ROLE, QVariant::fromValue( item->icon() ) );
      item->setIcon( hakeIcon );
    }
    else if ( stockIcon.isValid() )
    {
      item->setIcon( stockIcon.value<QIcon>() );
      item->setData( PROPERTIES_STOCK_ICON_ROLE, QVariant() );
    }
  }
}

void QgsHakeIcons::watchMenus( QObject *root, const QList<QWidget *> &menus )
{
  if ( !root )
    return;
  static QPointer<QgsHakeMenuWatcher> sWatcher;
  if ( !sWatcher )
    sWatcher = new QgsHakeMenuWatcher( root );
  for ( QWidget *menu : menus )
    sWatcher->watch( menu );
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


file(GLOB QML_FILES *.qml)

PLUGIN_INSTALL(db_manager_community db_plugins/postgis/plugins/qgis_topoview/templates ${QML_FILES})

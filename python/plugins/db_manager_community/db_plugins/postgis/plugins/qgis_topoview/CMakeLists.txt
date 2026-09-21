file(GLOB PY_FILES *.py)

add_subdirectory(templates)

PLUGIN_INSTALL(db_manager_community db_plugins/postgis/plugins/qgis_topoview ${PY_FILES})

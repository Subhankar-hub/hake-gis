add_subdirectory(qgis_topoview)
add_subdirectory(versioning)

PLUGIN_INSTALL(db_manager_community db_plugins/postgis/plugins __init__.py)

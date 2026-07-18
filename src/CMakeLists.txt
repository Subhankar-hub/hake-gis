option(HAKE_M4A_RENAME_DLL_OUTPUTS "Rename qgis_* shared library output names to hake_* for M4A validation" OFF)

function(hake_set_output_name target legacy_name m4a_name)
  if (HAKE_M4A_RENAME_DLL_OUTPUTS)
    set(_output_name "${m4a_name}${QGISPOSTFIX}")
  else()
    set(_output_name "${legacy_name}${QGISPOSTFIX}")
  endif()

  set_target_properties(${target} PROPERTIES
    RUNTIME_OUTPUT_NAME ${_output_name}
    LIBRARY_OUTPUT_NAME ${_output_name}
  )
endfunction()

add_subdirectory(core)

if (WITH_AUTH)
  add_subdirectory(auth)
endif()

if (WITH_ANALYSIS)
  add_subdirectory(analysis)
endif()

if (WITH_GUI)
  add_subdirectory(ui)
  add_subdirectory(gui)
  add_subdirectory(native)
endif()

add_subdirectory(providers)

add_subdirectory(crssync)

if (WITH_CRASH_HANDLER)
  add_subdirectory(crashhandler)
endif()

add_subdirectory(test)

if (WITH_DESKTOP)
  add_subdirectory(app)
  add_subdirectory(plugins)
endif()

if (WITH_3D)
  add_subdirectory(3d)
endif()

if (WITH_BINDINGS)
  add_subdirectory(python)
endif()

if (WITH_SERVER)
  add_subdirectory(server)
endif()

if (WITH_CUSTOM_WIDGETS)
  add_subdirectory(customwidgets)
endif()

if (WITH_QUICK)
  add_subdirectory(quickgui)
endif()

if (WITH_HAKE_GIS_PROCESS)
  add_subdirectory(process)
endif()

project(modcw)

set(modcw_SOURCES
    cwmod.cpp
    cwmodbaseband.cpp
    cwmodsource.cpp
    cwmodplugin.cpp
    cwmodsettings.cpp
    cwmodwebapiadapter.cpp
)

set(modcw_HEADERS
    cwmod.h
    cwmodbaseband.h
    cwmodsource.h
    cwmodplugin.h
    cwmodsettings.h
    cwmodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(modcw_SOURCES
        ${modcw_SOURCES}
        cwmodgui.cpp
        cwmodgui.ui
    )
    set(modcw_HEADERS
        ${modcw_HEADERS}
        cwmodgui.h
    )
    set(TARGET_NAME ${PLUGINS_PREFIX}modcw)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}modcwsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${modcw_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME CWModPlugin ${modcw_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    ${TARGET_LIB}
    sdrbase
    ${TARGET_LIB_GUI}
    swagger
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols on Windows
if (WIN32)
    install(FILES $<TARGET_PROPERTY:${TARGET_NAME},RUNTIME_OUTPUT_DIRECTORY>/${TARGET_NAME}stripped.pdb CONFIGURATIONS Release DESTINATION ${INSTALL_FOLDER} RENAME ${TARGET_NAME}.pdb )
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()

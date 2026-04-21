project(modmorse)

set(modmorse_SOURCES
    morsemod.cpp
    morsemodbaseband.cpp
    morsemodsource.cpp
    morsemodplugin.cpp
    morsemodsettings.cpp
    morsemodwebapiadapter.cpp
)

set(modmorse_HEADERS
    morsemod.h
    morsemodbaseband.h
    morsemodsource.h
    morsemodplugin.h
    morsemodsettings.h
    morsemodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(modmorse_SOURCES
        ${modmorse_SOURCES}
        morsemodgui.cpp
        morsemodgui.ui
    )
    set(modmorse_HEADERS
        ${modmorse_HEADERS}
        morsemodgui.h
    )
    set(TARGET_NAME ${PLUGINS_PREFIX}modmorse)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}modmorsesrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${modmorse_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME MorseModPlugin ${modmorse_SOURCES})
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

if(WIN32)
    install(FILES $<TARGET_PROPERTY:${TARGET_NAME},RUNTIME_OUTPUT_DIRECTORY>/${TARGET_NAME}stripped.pdb CONFIGURATIONS Release DESTINATION ${INSTALL_FOLDER} RENAME ${TARGET_NAME}.pdb)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER})
endif()

add_subdirectory(tools/kiconfinder)
if (APPLE)
    add_subdirectory(tools/ksvg2icns)
endif()

set(kiconthemes_SRCS
  kiconbutton.cpp
  kicondialog.cpp
  kiconeffect.cpp
  kiconengine.cpp
  kiconloader.cpp
  kicontheme.cpp
)

add_library(KF5IconThemes ${kiconthemes_SRCS})
generate_export_header(KF5IconThemes BASE_NAME KIconThemes)
add_library(KF5::IconThemes ALIAS KF5IconThemes)

target_include_directories(KF5IconThemes INTERFACE "$<INSTALL_INTERFACE:${KF5_INCLUDE_INSTALL_DIR}/KIconThemes>")

target_link_libraries(KF5IconThemes
  PUBLIC
    Qt5::Widgets
  PRIVATE
    Qt5::DBus
    Qt5::Svg
    KF5::I18n          # for i18n in KIconDialog
    KF5::WidgetsAddons # for KPixmapSequence family
    KF5::ItemViews      # for KListWidgetSearchLine
    KF5::ConfigWidgets # for KColorScheme
)

set_target_properties(KF5IconThemes PROPERTIES VERSION   ${KICONTHEMES_VERSION_STRING}
                                               SOVERSION ${KICONTHEMES_SOVERSION}
                                               EXPORT_NAME IconThemes
)

ecm_generate_headers(KIconThemes_HEADERS
  HEADER_NAMES
  KIconButton
  KIconDialog
  KIconEffect
  KIconLoader
  KIconTheme
  KIconEngine

  REQUIRED_HEADERS KIconThemes_HEADERS
)

install(TARGETS KF5IconThemes EXPORT KF5IconThemesTargets ${KF5_INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/kiconthemes_export.h
  ${KIconThemes_HEADERS}
  DESTINATION ${KF5_INCLUDE_INSTALL_DIR}/KIconThemes COMPONENT Devel
)

include(ECMGeneratePriFile)
ecm_generate_pri_file(BASE_NAME KIconThemes LIB_NAME KF5IconThemes DEPS "widgets" FILENAME_VAR PRI_FILENAME)
install(FILES ${PRI_FILENAME} DESTINATION ${ECM_MKSPECS_INSTALL_DIR})

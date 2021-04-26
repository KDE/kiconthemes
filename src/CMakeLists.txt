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
qt5_add_resources(kiconthemes_SRCS
  hicolor.qrc
)
ecm_qt_declare_logging_category(kiconthemes_SRCS
    HEADER debug.h
    IDENTIFIER KICONTHEMES
    CATEGORY_NAME kf.iconthemes
    OLD_CATEGORY_NAMES kf5.kiconthemes
    DESCRIPTION "KIconThemes"
    EXPORT KICONTHEMES
)

add_library(KF5IconThemes ${kiconthemes_SRCS})
add_library(KF5::IconThemes ALIAS KF5IconThemes)
ecm_generate_export_header(KF5IconThemes
    BASE_NAME KIconThemes
    GROUP_BASE_NAME KF
    VERSION ${KF_VERSION}
    DEPRECATED_BASE_VERSION 0
    DEPRECATION_VERSIONS 4.8 5.0 5.63 5.64 5.65 5.66 5.82
    EXCLUDE_DEPRECATED_BEFORE_AND_AT ${EXCLUDE_DEPRECATED_BEFORE_AND_AT}
)

target_include_directories(KF5IconThemes INTERFACE "$<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR_KF5}/KIconThemes>")

target_link_libraries(KF5IconThemes
  PUBLIC
    Qt5::Widgets
  PRIVATE
    Qt5::Svg
    KF5::Archive       # for KCompressionDevice
    KF5::I18n          # for i18n in KIconDialog
    KF5::WidgetsAddons # for KPixmapSequence family
    KF5::ItemViews      # for KListWidgetSearchLine
    KF5::ConfigWidgets # for KColorScheme
    KF5::CoreAddons    # for kshareddatacache.h
)
if (TARGET Qt5::DBus)
  target_link_libraries(KF5IconThemes PRIVATE Qt5::DBus)
endif()

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
  DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF5}/KIconThemes COMPONENT Devel
)

if(BUILD_DESIGNERPLUGIN)
    add_subdirectory(designer)
endif()

if(BUILD_QCH)
    ecm_add_qch(
        KF5IconThemes_QCH
        NAME KIconThemes
        BASE_NAME KF5IconThemes
        VERSION ${KF_VERSION}
        ORG_DOMAIN org.kde
        SOURCES # using only public headers, to cover only public API
            ${KIconThemes_HEADERS}
        MD_MAINPAGE "${CMAKE_SOURCE_DIR}/README.md"
        IMAGE_DIRS "${CMAKE_SOURCE_DIR}/docs/pics"
        LINK_QCHS
            Qt5Widgets_QCH
        INCLUDE_DIRS
            ${CMAKE_CURRENT_BINARY_DIR}
        BLANK_MACROS
            KICONTHEMES_EXPORT
            KICONTHEMES_DEPRECATED
            KICONTHEMES_DEPRECATED_EXPORT
            "KICONTHEMES_DEPRECATED_VERSION(x, y, t)"
            "KICONTHEMES_DEPRECATED_VERSION_BELATED(x, y, xt, yt, t)"
            "KICONTHEMES_ENUMERATOR_DEPRECATED_VERSION(x, y, t)"
            "KICONTHEMES_ENUMERATOR_DEPRECATED_VERSION_BELATED(x, y, xt, yt, t)"
        TAGFILE_INSTALL_DESTINATION ${KDE_INSTALL_QTQCHDIR}
        QCH_INSTALL_DESTINATION ${KDE_INSTALL_QTQCHDIR}
        COMPONENT Devel
    )
endif()

include(ECMGeneratePriFile)
ecm_generate_pri_file(BASE_NAME KIconThemes LIB_NAME KF5IconThemes DEPS "widgets" FILENAME_VAR PRI_FILENAME INCLUDE_INSTALL_DIR ${KDE_INSTALL_INCLUDEDIR_KF5}/KIconThemes)
install(FILES ${PRI_FILENAME} DESTINATION ${ECM_MKSPECS_INSTALL_DIR})

add_library(KIconEnginePlugin MODULE kiconengineplugin.cpp)

target_link_libraries(KIconEnginePlugin
    PRIVATE
        Qt5::Gui
        KF5::IconThemes
)

install(TARGETS KIconEnginePlugin DESTINATION ${KDE_INSTALL_QTPLUGINDIR}/iconengines)

ecm_qt_install_logging_categories(
    EXPORT KICONTHEMES
    FILE kiconthemes.categories
    DESTINATION ${KDE_INSTALL_LOGGINGCATEGORIESDIR}
)

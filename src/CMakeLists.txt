add_subdirectory(tools/kiconfinder)
if (APPLE)
    add_subdirectory(tools/ksvg2icns)
endif()

add_library(KF5IconThemes)
add_library(KF5::IconThemes ALIAS KF5IconThemes)

target_sources(KF5IconThemes PRIVATE
    kiconbutton.cpp
    kiconbutton.h
    kiconcolors.cpp
    kiconcolors.h
    kicondialog.cpp
    kicondialog.h
    kicondialogmodel_p.h
    kicondialog_p.h
    kiconeffect.cpp
    kiconeffect.h
    kiconengine.cpp
    kiconengine.h
    kiconengineplugin.cpp
    kiconloader.cpp
    kiconloader.h
    kicontheme.cpp
    kicontheme.h
    kquickiconprovider.h

    hicolor.qrc
    )

if (WIN32)
   target_sources(KF5IconThemes PRIVATE
       DarkModeSetup.cpp)
endif()

# TODO what's that PRIVATE about above?
ki18n_wrap_ui(KF5IconThemes kicondialog.ui)

ecm_qt_declare_logging_category(KF5IconThemes
    HEADER debug.h
    IDENTIFIER KICONTHEMES
    CATEGORY_NAME kf.iconthemes
    OLD_CATEGORY_NAMES kf5.kiconthemes
    DESCRIPTION "KIconThemes"
    EXPORT KICONTHEMES
)

ecm_generate_export_header(KF5IconThemes
    BASE_NAME KIconThemes
    GROUP_BASE_NAME KF
    VERSION ${KF_VERSION}
    DEPRECATED_BASE_VERSION 0
    DEPRECATION_VERSIONS 4.8 5.0 5.63 5.64 5.65 5.66 5.82 5.104
    EXCLUDE_DEPRECATED_BEFORE_AND_AT ${EXCLUDE_DEPRECATED_BEFORE_AND_AT}
)

target_include_directories(KF5IconThemes INTERFACE "$<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR_KF}/KIconThemes>")

target_link_libraries(KF5IconThemes
  PUBLIC
    Qt${QT_MAJOR_VERSION}::Widgets
  PRIVATE
    Qt${QT_MAJOR_VERSION}::GuiPrivate
    Qt${QT_MAJOR_VERSION}::Svg
    KF5::Archive       # for KCompressionDevice
    KF5::I18n          # for i18n in KIconDialog
    KF5::WidgetsAddons # for KPixmapSequence family
    KF5::ConfigWidgets # for KColorScheme
    KF5::CoreAddons    # for kshareddatacache.h
)
if (TARGET Qt${QT_MAJOR_VERSION}::DBus)
  target_link_libraries(KF5IconThemes PRIVATE Qt${QT_MAJOR_VERSION}::DBus)
endif()

set_target_properties(KF5IconThemes PROPERTIES VERSION   ${KICONTHEMES_VERSION}
                                               SOVERSION ${KICONTHEMES_SOVERSION}
                                               EXPORT_NAME IconThemes
)

ecm_generate_headers(KIconThemes_HEADERS
  HEADER_NAMES
  KIconButton
  KIconColors
  KIconDialog
  KIconEffect
  KIconLoader
  KIconTheme
  KIconEngine
  KQuickIconProvider

  REQUIRED_HEADERS KIconThemes_HEADERS
)

install(TARGETS KF5IconThemes EXPORT KF5IconThemesTargets ${KF_INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/kiconthemes_export.h
  ${KIconThemes_HEADERS}
  DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF}/KIconThemes COMPONENT Devel
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
ecm_generate_pri_file(BASE_NAME KIconThemes LIB_NAME KF5IconThemes DEPS "widgets" FILENAME_VAR PRI_FILENAME INCLUDE_INSTALL_DIR ${KDE_INSTALL_INCLUDEDIR_KF}/KIconThemes)
install(FILES ${PRI_FILENAME} DESTINATION ${ECM_MKSPECS_INSTALL_DIR})

add_library(KIconEnginePlugin MODULE kiconengineplugin.cpp)

target_link_libraries(KIconEnginePlugin
    PRIVATE
        Qt${QT_MAJOR_VERSION}::Gui
        KF5::IconThemes
)

install(TARGETS KIconEnginePlugin DESTINATION ${KDE_INSTALL_QTPLUGINDIR}/iconengines)

ecm_qt_install_logging_categories(
    EXPORT KICONTHEMES
    FILE kiconthemes.categories
    DESTINATION ${KDE_INSTALL_LOGGINGCATEGORIESDIR}
)

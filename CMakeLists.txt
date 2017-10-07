cmake_minimum_required(VERSION 3.0)

set(KF5_VERSION "5.39.0") # handled by release scripts
set(KF5_DEP_VERSION "5.39.0") # handled by release scripts
project(KIconThemes VERSION ${KF5_VERSION})

# ECM setup
include(FeatureSummary)
find_package(ECM 5.39.0  NO_MODULE)
set_package_properties(ECM PROPERTIES TYPE REQUIRED DESCRIPTION "Extra CMake Modules." URL "https://projects.kde.org/projects/kdesupport/extra-cmake-modules")
feature_summary(WHAT REQUIRED_PACKAGES_NOT_FOUND FATAL_ON_MISSING_REQUIRED_PACKAGES)

set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})

include(KDEInstallDirs)
include(KDEFrameworkCompilerSettings NO_POLICY_SCOPE)
include(KDECMakeSettings)

include(GenerateExportHeader)
include(ECMSetupVersion)
include(ECMGenerateHeaders)
include(ECMMarkNonGuiExecutable)
include(ECMQtDeclareLoggingCategory)
include(ECMAddQch)

option(BUILD_QCH "Build API documentation in QCH format (for e.g. Qt Assistant, Qt Creator & KDevelop)" OFF)
add_feature_info(QCH ${BUILD_QCH} "API documentation in QCH format (for e.g. Qt Assistant, Qt Creator & KDevelop)")

ecm_setup_version(PROJECT
                  VARIABLE_PREFIX KICONTHEMES
                  VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/kiconthemes_version.h"
                  PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/KF5IconThemesConfigVersion.cmake"
                  SOVERSION 5)

# Dependencies
set(REQUIRED_QT_VERSION 5.7.0)
find_package(Qt5Widgets ${REQUIRED_QT_VERSION} REQUIRED NO_MODULE)
find_package(Qt5Svg ${REQUIRED_QT_VERSION} REQUIRED NO_MODULE)
find_package(Qt5DBus ${REQUIRED_QT_VERSION} REQUIRED NO_MODULE)

find_package(KF5Archive ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5I18n ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5CoreAddons ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5ConfigWidgets ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5WidgetsAddons ${KF5_DEP_VERSION} REQUIRED)
find_package(KF5ItemViews ${KF5_DEP_VERSION} REQUIRED)

remove_definitions(-DQT_NO_CAST_FROM_ASCII)
remove_definitions(-DQT_NO_CAST_FROM_BYTEARRAY)

add_definitions(-DTRANSLATION_DOMAIN=\"kiconthemes5\")
if (IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/po")
    ki18n_install(po)
endif()
add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(autotests)


# create a Config.cmake and a ConfigVersion.cmake file and install them
set(CMAKECONFIG_INSTALL_DIR "${KDE_INSTALL_CMAKEPACKAGEDIR}/KF5IconThemes")

if (BUILD_QCH)
    ecm_install_qch_export(
        TARGETS KF5IconThemes_QCH
        FILE KF5IconThemesQchTargets.cmake
        DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
        COMPONENT Devel
    )
    set(PACKAGE_INCLUDE_QCHTARGETS "include(\"\${CMAKE_CURRENT_LIST_DIR}/KF5IconThemesQchTargets.cmake\")")
endif()

include(CMakePackageConfigHelpers)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/KF5IconThemesConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/KF5IconThemesConfig.cmake"
  INSTALL_DESTINATION  ${CMAKECONFIG_INSTALL_DIR}
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/KF5IconThemesConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/KF5IconThemesConfigVersion.cmake"
  DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
  COMPONENT Devel
)

install(EXPORT KF5IconThemesTargets DESTINATION "${CMAKECONFIG_INSTALL_DIR}" FILE KF5IconThemesTargets.cmake NAMESPACE KF5:: )

install(FILES
   ${CMAKE_CURRENT_BINARY_DIR}/kiconthemes_version.h
  DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF5} COMPONENT Devel
)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

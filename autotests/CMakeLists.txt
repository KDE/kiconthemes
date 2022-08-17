
find_package(Qt${QT_MAJOR_VERSION} ${REQUIRED_QT_VERSION} CONFIG REQUIRED Test)

include(ECMAddTests)

macro(KICONTHEMES_TESTS)
    foreach(_testname ${ARGN})
        set(_srcs ${_testname}.cpp)
        qt_add_resources(_srcs resources.qrc)
        ecm_add_test(${_srcs} TEST_NAME ${_testname} LINK_LIBRARIES Qt${QT_MAJOR_VERSION}::Test KF5::IconThemes KF5::WidgetsAddons KF5::ConfigCore)
    endforeach(_testname)
endmacro()

kiconthemes_tests(
  kiconloader_unittest
  kiconloader_resourcethemetest
  kicontheme_unittest
  kiconengine_unittest
  kiconengine_scaled_unittest
  kicondialog_unittest
)

find_package(Qt${QT_MAJOR_VERSION} COMPONENTS Quick)
if (TARGET Qt${QT_MAJOR_VERSION}::Quick)
  kiconthemes_tests(kquickiconprovidertest)
  target_link_libraries(kquickiconprovidertest Qt${QT_MAJOR_VERSION}::Quick)
endif()

### Test for RCC loading

get_target_property(QT_RCC_EXECUTABLE Qt${QT_MAJOR_VERSION}::rcc LOCATION)
add_custom_command(OUTPUT icontheme.rcc
   COMMAND ${QT_RCC_EXECUTABLE} --binary -o icontheme.rcc ${CMAKE_CURRENT_SOURCE_DIR}/icontheme.qrc
       DEPENDS icontheme.qrc
)
ecm_add_test(kiconloader_rcctest.cpp LINK_LIBRARIES Qt${QT_MAJOR_VERSION}::Test KF5::IconThemes KF5::WidgetsAddons KF5::ConfigCore)
add_custom_target(rcctest-rcc ALL DEPENDS icontheme.rcc) # a custom target that is always built

set_tests_properties(kiconloader_unittest PROPERTIES RUN_SERIAL TRUE)

# Benchmark, compiled, but not run automatically with ctest
add_executable(kiconloader_benchmark kiconloader_benchmark.cpp)
target_link_libraries(kiconloader_benchmark Qt${QT_MAJOR_VERSION}::Test KF5::IconThemes KF5::WidgetsAddons KF5::ConfigCore)


find_package(Qt5 ${REQUIRED_QT_VERSION} CONFIG REQUIRED Test)

include(ECMAddTests)

macro(KICONTHEMES_TESTS)
    foreach(_testname ${ARGN})
        set(_srcs ${_testname}.cpp)
        qt5_add_resources(_srcs resources.qrc)
        ecm_add_test(${_srcs} TEST_NAME ${_testname} LINK_LIBRARIES Qt5::Test KF5::IconThemes KF5::WidgetsAddons KF5::ConfigCore)
    endforeach(_testname)
endmacro()

kiconthemes_tests(
  kiconloader_unittest
  kiconloader_resourcethemetest
  kiconloader_benchmark
  kicontheme_unittest
  kiconengine_unittest
)

### Test for RCC loading

get_target_property(QT_RCC_EXECUTABLE Qt5::rcc LOCATION)
add_custom_command(OUTPUT icontheme.rcc
   COMMAND ${QT_RCC_EXECUTABLE} --binary -o icontheme.rcc ${CMAKE_CURRENT_SOURCE_DIR}/icontheme.qrc
       DEPENDS icontheme.qrc
)
ecm_add_test(kiconloader_rcctest.cpp LINK_LIBRARIES Qt5::Test KF5::IconThemes KF5::WidgetsAddons KF5::ConfigCore)
add_custom_target(rcctest-rcc ALL DEPENDS icontheme.rcc) # a custom target that is always built

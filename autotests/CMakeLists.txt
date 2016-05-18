
find_package(Qt5 ${REQUIRED_QT_VERSION} CONFIG REQUIRED Test)

include(ECMAddTests)

macro(KICONTHEMES_TESTS)
    foreach(_testname ${ARGN})
        set(_srcs ${_testname}.cpp)
        qt5_add_resources(_srcs resources.qrc)
	ecm_add_test(${_srcs} TEST_NAME kiconthemes-${_testname} LINK_LIBRARIES Qt5::Test KF5::IconThemes KF5::WidgetsAddons KF5::ConfigCore)
    endforeach(_testname)
endmacro()

kiconthemes_tests(
  kiconloader_unittest
  kiconloader_resourcethemetest
  kiconloader_benchmark
  kicontheme_unittest
)

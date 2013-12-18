
find_package(Qt5 5.2.0 CONFIG REQUIRED Test)

include(ECMMarkAsTest)

macro(KICONTHEMES_TESTS)
    foreach(_testname ${ARGN})
        set(_srcs ${_testname}.cpp)
        qt5_add_resources(_srcs resources.qrc)
        add_executable(${_testname} ${_srcs})
        add_test(kiconthemes-${_testname} ${_testname})
        ecm_mark_as_test(${_testname})
        target_link_libraries(${_testname} Qt5::Test KF5::IconThemes KF5::WidgetsAddons)
    endforeach(_testname)
endmacro()

kiconthemes_tests(
  kiconloader_unittest
)

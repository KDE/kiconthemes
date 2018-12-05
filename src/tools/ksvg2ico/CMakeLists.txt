find_package(Png2Ico)
if (Png2Ico_FOUND)
    if (Png2Ico_HAS_RCFILE_ARGUMENT)
        # not used at the moment
        add_definitions(-DPNG2ICO_HAS_RCFILE_ARGUMENT)
    endif()
    # act as if there's a png2ico utility that supports both options
    if (Png2Ico_HAS_COLORS_ARGUMENT)
        # using --colors appears counter-productive
        # add_definitions(-DPNG2ICO_SUPPORTS_MULTIPLE_COLOR_DEPTHS)
    endif()
else()
    message(WARNING "Unable to find the png2ico utility - ksvg2ico will miss its runtime dependency")
endif()

add_executable(ksvg2ico ksvg2ico.cpp qicohandler.cpp)
ecm_mark_nongui_executable(ksvg2ico)
target_link_libraries(ksvg2ico Qt5::Gui Qt5::Svg)

install(TARGETS ksvg2ico ${KF5_INSTALL_TARGETS_DEFAULT_ARGS})

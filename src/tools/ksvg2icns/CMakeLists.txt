add_executable(ksvg2icns ksvg2icns.cpp)
ecm_mark_nongui_executable(ksvg2icns)
target_link_libraries(ksvg2icns Qt5::Gui Qt5::Svg)

install(TARGETS ksvg2icns ${KF5_INSTALL_TARGETS_DEFAULT_ARGS})

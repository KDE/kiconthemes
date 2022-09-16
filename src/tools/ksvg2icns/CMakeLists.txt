add_executable(ksvg2icns ksvg2icns.cpp)
ecm_mark_nongui_executable(ksvg2icns)
target_link_libraries(ksvg2icns Qt${QT_MAJOR_VERSION}::Gui Qt${QT_MAJOR_VERSION}::Svg)

install(TARGETS ksvg2icns ${KF_INSTALL_TARGETS_DEFAULT_ARGS})

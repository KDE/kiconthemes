add_executable(kiconfinder5 kiconfinder.cpp)
ecm_mark_nongui_executable(kiconfinder5)
target_link_libraries(kiconfinder5 Qt${QT_MAJOR_VERSION}::Gui KF5IconThemes)

install(TARGETS kiconfinder5 ${KF_INSTALL_TARGETS_DEFAULT_ARGS})

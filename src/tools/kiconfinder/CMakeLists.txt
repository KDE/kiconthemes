add_executable(kiconfinder kiconfinder.cpp)
ecm_mark_nongui_executable(kiconfinder)
target_link_libraries(kiconfinder Qt5::Gui KF5IconThemes)

install(TARGETS kiconfinder ${INSTALL_TARGETS_DEFAULT_ARGS})

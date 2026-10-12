# SPDX-FileCopyrightText: 2026 Chinmoy Pradhan <chinmoy.pradhan@machinesoul.in>
# SPDX-License-Identifier: BSD-3-Clause

find_program(GtkUpdateIconCache_EXE gtk-update-icon-cache)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GtkUpdateIconCache
    FOUND_VAR
        GtkUpdateIconCache_FOUND
    REQUIRED_VARS
        GtkUpdateIconCache_EXE
)

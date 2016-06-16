# KIconThemes

Icon GUI utilities

## Introduction

This library contains classes to improve the handling of icons
in applications using the KDE Frameworks. Provided are:

- KIconDialog: Dialog class to let the user select an icon
    from the list of installed icons.
- KIconButton: Custom button class that displays an icon.
    When clicking it, the user can change it using the icon dialog.
- KIconEffect: Applies various colorization effects to icons,
    which can be used to create selected/disabled icon images.

Other classes in this repository are used internally by the icon
theme configuration dialogs, and should not be used by applications.

## Icon Theme Deployment

On Linux/BSD, it is expected that the main icon themes (hicolor, oxygen, breeze)
are installed by the distribution. The platform theme plugin reads the icon
theme name from KConfig, and redirects QIcon::fromTheme calls to KIconEngine/KIconLoader,
which brings some benefits over Qt's internal icon loading, such as a cache shared
between processes.

On other platforms such as Windows and Mac OS, icon themes are not part of the system.
The deployment strategy for applications on those operations systems is the following:
- breeze-icons and other icon themes install .rcc files (binary resources, loadable by Qt)
- the installation process should copy one of these under the name "icontheme.rcc", in
    a directory found by [QStandardPaths::AppDataLocation](http://doc.qt.io/qt-5/qstandardpaths.html#StandardLocation-enum).
    For instance on Windows, icontheme.rcc is usually installed in APPROOT/data/,
    while on Mac OS it is installed in the Resources directory inside the application bundle.
- as long as the application links to KIconThemes (even if it doesn't use any of its API),
    the icontheme.rcc file will be found on startup, loaded, and set as the default icon theme.


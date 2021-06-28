#include <QApplication>
#include <kiconbutton.h>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_UseHighDpiPixmaps);

    //    KIconDialog::getIcon();

    KIconButton button;
    button.show();

    return app.exec();
}

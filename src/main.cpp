#include <QApplication>
#include "MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("MVS 相机查看器");
    app.setOrganizationName("MVSViewer");

    MainWindow window;
    window.show();

    return app.exec();
}

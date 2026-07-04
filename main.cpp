#include <QApplication>
#include <QFont>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    
    QNetworkProxyFactory::setUseSystemConfiguration(false);
    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);

    QFont font = app.font();
    font.setPointSize(10);
    app.setFont(font);

    MainWindow window;
    window.resize(1280, 820);
    window.show();

    return app.exec();
}

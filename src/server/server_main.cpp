#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include "server/GameServer.h"

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    QCoreApplication::setApplicationName("ChinatownServer");
    QCoreApplication::setApplicationVersion("1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("Chinatown board game TCP server");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption portOption(
        QStringList() << "p" << "port",
        "Server port. Default: 45454",
        "port",
        "45454"
    );
    parser.addOption(portOption);
    parser.process(app);

    bool ok = false;
    quint16 port = parser.value(portOption).toUShort(&ok);
    if (!ok) port = 45454;

    GameServer server;
    if (!server.listen(port)) {
        return 1;
    }

    return app.exec();
}

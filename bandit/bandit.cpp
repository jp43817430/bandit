#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QIcon>
#include "proxyserver.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    QQuickStyle::setStyle("material");
    QIcon::setThemeName("bandit");
    qmlRegisterType<ProxyServer>("BANDIT.ProxyServer", 1, 0, "ProxyServer");

    engine.load(QUrl(QStringLiteral("qrc:/Bandit.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}

#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "src/AppController.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("OpenShell"));
    app.setApplicationName(QStringLiteral("OpenShell"));
    app.setQuitOnLastWindowClosed(false);
    app.setWindowIcon(QIcon(QStringLiteral(":/qt/qml/OpenShell/assets/icons/openshell.svg")));

    AppController controller;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &controller);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    engine.loadFromModule("OpenShell", "Main");

    return app.exec();
}

#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQml>

#include "src/AppController.h"
#include "src/terminal/TerminalScreenItem.h"
#include "src/terminal/VtScreen.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("OpenShell"));
    app.setApplicationName(QStringLiteral("OpenShell"));
    app.setQuitOnLastWindowClosed(false);
    app.setWindowIcon(QIcon(QStringLiteral(":/qt/qml/OpenShell/assets/icons/openshell.svg")));

    qmlRegisterType<TerminalScreenItem>("OpenShell", 1, 0, "TerminalScreen");
    qmlRegisterUncreatableType<VtScreen>(
        "OpenShell", 1, 0, "VtScreen",
        QStringLiteral("VtScreen is created by SessionController; obtain via appController.sessionScreen(id)."));

    AppController controller;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &controller);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    QObject::connect(&controller, &AppController::languageChanged,
                     &engine, [&engine]() { engine.retranslate(); });

    engine.loadFromModule("OpenShell", "Main");

    return app.exec();
}

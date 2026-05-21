#if defined(OPENSHELL_MOBILE)
#include <QGuiApplication>
#else
#include <QApplication>
#endif
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QtQml>

#include "src/AppController.h"
#include "src/terminal/TerminalScreenItem.h"
#include "src/terminal/VtScreen.h"

int main(int argc, char *argv[])
{
#if defined(OPENSHELL_MOBILE)
    // 移动端强制使用 Material style，iOS 默认会挑系统 iOS style，导致
    // Android（Material）和 iOS（蓝色填充 UIKit 风）按钮配色完全不同。
    // 统一到 Material 后两端按钮一致，靠 MobileWindow.qml 里的 Material.* 属性主题化。
    QQuickStyle::setStyle(QStringLiteral("Material"));
    QGuiApplication app(argc, argv);
#else
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QApplication app(argc, argv);
#endif
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

#if defined(OPENSHELL_MOBILE)
    engine.loadFromModule("OpenShell", "MobileMain");
#else
    engine.loadFromModule("OpenShell", "Main");
#endif

    return app.exec();
}

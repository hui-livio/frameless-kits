#include "DemoOptions.h"

#include <FramelessKit/Initialization.h>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QTimer>
#include <QtQml/QQmlExtensionPlugin>
#include <cstdlib>

#ifdef FRAMELESSKIT_STATIC
Q_IMPORT_QML_PLUGIN(FramelessKitPlugin)
#endif

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    FramelessKit::initialize(true);
    QQmlApplicationEngine engine;
    engine.addImportPath(QString::fromUtf8(FRAMELESSKIT_DEMO_IMPORT_PATH));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(EXIT_FAILURE); },
        Qt::QueuedConnection);
    engine.setInitialProperties({ { QStringLiteral("initialMaterial"), int(demoInitialMaterial(app.arguments())) } });
    engine.loadFromModule("FramelessDemo", "Main");
    if (engine.rootObjects().isEmpty())
        return EXIT_FAILURE;
    auto* agent = engine.rootObjects().first()->findChild<QObject*>(QStringLiteral("frame"));
    if (!agent)
        return EXIT_FAILURE;
    runMaterialDemo(agent, app.arguments());
    return app.exec();
}

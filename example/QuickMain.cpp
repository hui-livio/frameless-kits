#include "DemoOptions.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QTimer>
#include <WindowHeler/Initialization.h>
#include <cstdlib>


int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    WindowHeler::initialize(true);
    QQmlApplicationEngine engine;
    engine.addImportPath(QString::fromUtf8(WINDOWHELER_DEMO_IMPORT_PATH));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(EXIT_FAILURE); },
        Qt::QueuedConnection);
    engine.setInitialProperties({ { QStringLiteral("initialMaterial"), int(demoInitialMaterial(app.arguments())) } });
    engine.loadFromModule("FramelessDemo", "Main");
    if (engine.rootObjects().isEmpty())
        return EXIT_FAILURE;
    auto* agent = engine.rootObjects().first()->findChild<QObject*>(QStringLiteral("material"));
    if (!agent)
        return EXIT_FAILURE;
    runMaterialDemo(agent, app.arguments());
    return app.exec();
}

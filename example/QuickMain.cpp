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
    engine.loadFromModule("FramelessDemo", "Main");
    if (app.arguments().contains(QStringLiteral("--smoke-test")))
        QTimer::singleShot(0, &app,
                           [&app, &engine] { app.exit(engine.rootObjects().isEmpty() ? EXIT_FAILURE : EXIT_SUCCESS); });
    return app.exec();
}

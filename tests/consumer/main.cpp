#include <WindowHeler/Initialization.h>
#ifdef CONSUMER_WIDGETS
#include <WindowHeler/FramelessWidgetAgent.h>
#include <QApplication>
#include <QWidget>
#else
#include <QGuiApplication>
#endif
#ifdef CONSUMER_QUICK
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#endif
#include <QDebug>
#include <memory>

int main(int argc, char** argv)
{
#ifdef CONSUMER_WIDGETS
    QApplication app(argc, argv);
#else
    QGuiApplication app(argc, argv);
#endif
#ifdef CONSUMER_QUICK
    WindowHeler::initialize(true);
    if (!QQuickWindow::hasDefaultAlphaBuffer())
        return 1;
#else
    WindowHeler::initialize();
#endif
#ifdef CONSUMER_WIDGETS
    QWidget window;
    WindowHeler::FramelessWidgetAgent agent(&window);
    QWidget titleBar(&window);
    agent.setTitleBar(&titleBar);
    if (agent.titleBar() != &titleBar || !window.windowFlags().testFlag(Qt::FramelessWindowHint))
        return 2;
    // A failed initialization check leaves the controller unbound. Exercise an
    // operation that requires a bound controller, including in the mixed DLL case.
    bool widgetError = false;
    QObject::connect(&agent, &WindowHeler::FramelessWidgetAgent::errorOccurred, &app,
                     [&](const QString&) { widgetError = true; });
    agent.setMaterial(WindowHeler::Material::Transparent);
    QCoreApplication::processEvents();
    if (widgetError)
        return 5;
#endif
#ifdef CONSUMER_QUICK
    QQmlEngine engine;
    engine.addImportPath(QString::fromUtf8(CONSUMER_QML_IMPORT_PATH));
    QQmlComponent component(&engine);
    component.setData(R"(
        import QtQuick
        import WindowHeler
        Window {
            id: root
            width: 320; height: 240
            FramelessAgent {
                objectName: "frame"
                window: root
                titleBar: bar
                material: FramelessAgent.Mica
                interactiveItems: [buttonArea]
            }
            Item { id: bar; width: 320; height: 40 }
            Item { id: buttonArea; width: 80; height: 40 }
        }
    )",
                      QUrl());
    std::unique_ptr<QObject> root(component.create());
    if (!root)
    {
        qCritical() << component.errors();
        return 3;
    }
    auto* quickWindow = qobject_cast<QQuickWindow*>(root.get());
    if (!quickWindow || !quickWindow->flags().testFlag(Qt::FramelessWindowHint))
        return 5;
    auto* frame = root->findChild<QObject*>(QStringLiteral("frame"));
    if (!frame || frame->property("material").toInt() != 4 || frame->property("effectiveMaterial").toInt() != 0)
        return 4;
#endif
    return 0;
}

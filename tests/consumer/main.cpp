#include <FramelessKit/Initialization.h>
#ifdef CONSUMER_WIDGETS
#include <FramelessKit/WidgetAgent.h>
#include <QApplication>
#include <QWidget>
#else
#include <QGuiApplication>
#endif
#ifdef CONSUMER_QUICK
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QtQml/QQmlExtensionPlugin>
#ifdef FRAMELESSKIT_STATIC
Q_IMPORT_QML_PLUGIN(FramelessKitPlugin)
#endif
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
    FramelessKit::initialize(true);
    if (!QQuickWindow::hasDefaultAlphaBuffer())
        return 1;
#else
    FramelessKit::initialize();
#endif
#ifdef CONSUMER_WIDGETS
    QWidget window;
    FramelessKit::WidgetAgent agent(&window);
    QWidget titleBar(&window);
    agent.setTitleBar(&titleBar);
    if (agent.titleBar() != &titleBar)
        return 2;
#endif
#ifdef CONSUMER_QUICK
    QQmlEngine engine;
    engine.addImportPath(QString::fromUtf8(CONSUMER_QML_IMPORT_PATH));
    QQmlComponent component(&engine);
    component.setData(R"(
        import QtQuick
        import FramelessKit
        Window {
            id: root
            width: 320; height: 240
            WindowAgent {
                objectName: "frame"
                window: root
                titleBar: bar
                material: WindowAgent.Mica
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
    auto* frame = root->findChild<QObject*>(QStringLiteral("frame"));
    if (!frame || frame->property("material").toInt() != 4 || frame->property("effectiveMaterial").toInt() != 0)
        return 4;
#endif
    return 0;
}

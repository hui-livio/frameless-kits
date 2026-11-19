#include "WindowsTestSupport.h"

#include <WindowHeler/Initialization.h>
#include <WindowHeler/FramelessAgent.h>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QSignalSpy>
#include <QtTest>


class QuickNativeTests : public QObject
{
    Q_OBJECT
private slots:
    void frameAndButton()
    {
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import QtQuick.Controls
            Window {
                width: 640; height: 420
                Item { objectName: "bar"; x: 8; y: 8; width: 624; height: 44
                    Button { objectName: "maximize"; x: 500; width: 100; height: 40; hoverEnabled: true
                        onClicked: { if (Window.window.visibility === Window.Maximized) Window.window.showNormal(); else Window.window.showMaximized(); }
                    }
                    TextField { objectName: "interactive"; x: 200; width: 100; height: 40 }
                }
            }
        )",
                          QUrl());
        std::unique_ptr<QObject> root(component.create());
        QVERIFY2(root, qPrintable(component.errorString()));
        auto* window = qobject_cast<QQuickWindow*>(root.get());
        QVERIFY(window);
        auto* bar             = window->findChild<QQuickItem*>("bar");
        auto* button          = window->findChild<QQuickItem*>("maximize");
        auto* interactive     = window->findChild<QQuickItem*>("interactive");
        const bool hadSurface = window->handle() != nullptr;
        WindowHeler::FramelessAgent agent;
        agent.setWindow(window);
        agent.setTitleBar(bar);
        agent.setMaximizeButton(button);
        auto list = agent.interactiveItems();
        list.append(&list, interactive);
        QCOMPARE(window->handle() != nullptr, hadSurface);
        QSignalSpy clicked(button, SIGNAL(clicked()));
        QVERIFY(clicked.isValid());
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QCoreApplication::processEvents();
        QTRY_COMPARE(nativeHit(window, { 30, 20 }), LRESULT(HTCAPTION));
        QCOMPARE(nativeHit(window, { 230, 20 }), LRESULT(HTCLIENT));
        QCOMPARE(nativeHit(window, { 550, 20 }), LRESULT(HTMAXBUTTON));
        QCOMPARE(nativeHit(window, { 0, 0 }), LRESULT(HTTOPLEFT));
        nativePress(window, { 550, 20 });
        QVERIFY(button->property("pressed").toBool());
        nativeRelease(window, { 550, 20 });
        QTRY_COMPARE(clicked.count(), 1);
        QTRY_COMPARE(window->visibility(), QWindow::Maximized);
        window->showNormal();
        QCoreApplication::processEvents();
        nativePress(window, { 550, 20 });
        SendMessageW(nativeHandle(window), WM_CANCELMODE, 0, 0);
        QVERIFY(!button->property("pressed").toBool());
        nativeRelease(window, { 550, 20 });
        QCOMPARE(clicked.count(), 1);
        nativePress(window, { 550, 20 });
        SendMessageW(nativeHandle(window), WM_MOUSEMOVE, MK_LBUTTON,
                     nativePoint(nativeHandle(window), { 100, 100 }, window->devicePixelRatio(), false));
        nativeRelease(window, { 100, 100 });
        QCOMPARE(clicked.count(), 1);
        QVERIFY(!button->property("pressed").toBool());
        nativePress(window, { 550, 20 });
        agent.setMaximizeButton(nullptr);
        QVERIFY(!button->property("pressed").toBool());
        nativeRelease(window, { 550, 20 });
        QCOMPARE(clicked.count(), 1);
        agent.setMaximizeButton(button);
        QQuickItem unsupported(window->contentItem());
        QSignalSpy errors(&agent, &WindowHeler::FramelessAgent::errorOccurred);
        agent.setMaximizeButton(&unsupported);
        QCOMPARE(errors.count(), 1);
        QCOMPARE(agent.maximizeButton(), button);
        window->showFullScreen();
        QTRY_COMPARE(window->visibility(), QWindow::FullScreen);
        QCOMPARE(nativeHit(window, { 550, 20 }), LRESULT(HTCLIENT));
        window->showNormal();
        QCoreApplication::processEvents();
        window->hide();
        window->destroy();
        window->show();
        QCoreApplication::processEvents();
        QTRY_COMPARE(nativeHit(window, { 30, 20 }), LRESULT(HTCAPTION));
        agent.setWindow(nullptr);
        QCoreApplication::processEvents();
        QVERIFY(!window->flags().testFlag(Qt::FramelessWindowHint));
    }
};
int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    WindowHeler::initialize(true);
    QuickNativeTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "QuickNativeTests.moc"

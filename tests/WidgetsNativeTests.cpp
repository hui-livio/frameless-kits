#include "WindowsTestSupport.h"
#include <FramelessKit/Initialization.h>
#include <FramelessKit/WidgetAgent.h>
#include <QApplication>
#include <QDialog>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest>

class WidgetsNativeTests : public QObject
{
    Q_OBJECT
private slots:
    void frameAndGeometry()
    {
        QWidget window;
        window.setGeometry(120, 120, 640, 420);
        window.setMinimumSize(320, 240);
        QWidget bar(&window); bar.setGeometry(8, 8, 624, 44);
        QPushButton button(&bar); button.setGeometry(500, 0, 100, 40);
        QWidget interactive(&bar); interactive.setGeometry(200, 0, 100, 40);
        auto agent = std::make_unique<FramelessKit::WidgetAgent>(&window);
        agent->setTitleBar(&bar); agent->setMaximizeButton(&button); agent->setInteractive(&interactive);
        QVERIFY(!window.testAttribute(Qt::WA_WState_Created));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTRY_COMPARE(nativeHit(window.windowHandle(), {30, 20}), LRESULT(HTCAPTION));
        QCOMPARE(nativeHit(window.windowHandle(), {230, 20}), LRESULT(HTCLIENT));
        QCOMPARE(nativeHit(window.windowHandle(), {550, 20}), LRESULT(HTMAXBUTTON));
        const QList<QPointF> edges{{0, 0}, {320, 0}, {639, 0}, {0, 210}, {639, 210},
                                  {0, 419}, {320, 419}, {639, 419}};
        const QList<int> hits{HTTOPLEFT, HTTOP, HTTOPRIGHT, HTLEFT, HTRIGHT, HTBOTTOMLEFT, HTBOTTOM, HTBOTTOMRIGHT};
        for (int i=0; i<edges.size(); ++i) QCOMPARE(nativeHit(window.windowHandle(), edges[i]), LRESULT(hits[i]));
        RECT frame{}, client{}; GetWindowRect(nativeHandle(window.windowHandle()), &frame);
        GetClientRect(nativeHandle(window.windowHandle()), &client);
        QCOMPARE(client.right, frame.right - frame.left);
        QCOMPARE(client.bottom, frame.bottom - frame.top);
        QCOMPARE(window.size(), QSize(640,420));
        button.setEnabled(false);
        QCOMPARE(nativeHit(window.windowHandle(), {550,20}), LRESULT(HTCLIENT));
        button.setEnabled(true);
        window.setFixedWidth(640);
        QCoreApplication::processEvents();
        QCOMPARE(nativeHit(window.windowHandle(), {0,210}), LRESULT(HTCLIENT));
        QCOMPARE(nativeHit(window.windowHandle(), {0,0}), LRESULT(HTTOP));
        QCOMPARE(nativeHit(window.windowHandle(), {550,20}), LRESULT(HTCLIENT));
        window.setMinimumWidth(320); window.setMaximumWidth(16777215);
        window.showMaximized();
        QTRY_VERIFY(window.isMaximized()); QCoreApplication::processEvents();
        QCOMPARE(nativeHit(window.windowHandle(), {0,100}), LRESULT(HTCLIENT));
        MONITORINFO monitor{sizeof(monitor)};
        GetMonitorInfoW(MonitorFromWindow(nativeHandle(window.windowHandle()), MONITOR_DEFAULTTONEAREST), &monitor);
        GetWindowRect(nativeHandle(window.windowHandle()), &frame);
        POINT origin{}; ClientToScreen(nativeHandle(window.windowHandle()), &origin);
        QVERIFY2(qAbs(origin.x - monitor.rcWork.left) <= 2, qPrintable(QString("Client left %1; work left %2").arg(origin.x).arg(monitor.rcWork.left)));
        QVERIFY2(qAbs(origin.y - monitor.rcWork.top) <= 2, qPrintable(QString("Client top %1; work top %2").arg(origin.y).arg(monitor.rcWork.top)));
        GetClientRect(nativeHandle(window.windowHandle()), &client);
        QVERIFY(qAbs(origin.x + client.right - monitor.rcWork.right) <= 2);
        QVERIFY(qAbs(origin.y + client.bottom - monitor.rcWork.bottom) <= 2);
        window.showFullScreen(); QTRY_VERIFY(window.isFullScreen()); QCoreApplication::processEvents();
        QCOMPARE(nativeHit(window.windowHandle(), {30,20}), LRESULT(HTCLIENT));
        window.showNormal(); QTRY_VERIFY(!window.isFullScreen()); QCoreApplication::processEvents();
        QCOMPARE(nativeHit(window.windowHandle(), {30,20}), LRESULT(HTCAPTION));
        window.hide(); window.show(); QCoreApplication::processEvents();
        QCOMPARE(nativeHit(window.windowHandle(), {30,20}), LRESULT(HTCAPTION));
        const auto originalSize = window.size();
        agent.reset(); QCoreApplication::processEvents();
        QVERIFY(!window.windowFlags().testFlag(Qt::FramelessWindowHint));
        QCOMPARE(window.size(), originalSize);
    }
    void commandsAndButtons()
    {
        QWidget window; window.resize(640,420);
        QWidget bar(&window); bar.setGeometry(8,8,624,44);
        QPushButton button(&bar); button.setGeometry(500,0,100,40);
        FramelessKit::WidgetAgent agent(&window); agent.setTitleBar(&bar); agent.setMaximizeButton(&button);
        connect(&button, &QPushButton::clicked, &window, [&] {
            window.isMaximized() ? window.showNormal() : window.showMaximized();
        });
        QSignalSpy clicked(&button, &QPushButton::clicked);
        window.show(); QVERIFY(QTest::qWaitForWindowExposed(&window)); QCoreApplication::processEvents();
        nativePress(window.windowHandle(), {550,20}); QVERIFY(button.isDown());
        nativeRelease(window.windowHandle(), {550,20});
        QTRY_COMPARE(clicked.count(),1); QTRY_VERIFY(window.isMaximized());
        window.showNormal(); QCoreApplication::processEvents();
        nativePress(window.windowHandle(), {550,20});
        SendMessageW(nativeHandle(window.windowHandle()), WM_MOUSEMOVE, MK_LBUTTON,
                     nativePoint(nativeHandle(window.windowHandle()), {100,100}, window.devicePixelRatioF(), false));
        nativeRelease(window.windowHandle(), {100,100});
        QCOMPARE(clicked.count(),1); QVERIFY(!button.isDown());
        nativePress(window.windowHandle(), {550,20});
        SendMessageW(nativeHandle(window.windowHandle()), WM_CANCELMODE,0,0);
        nativeRelease(window.windowHandle(), {550,20});
        QCOMPARE(clicked.count(),1); QVERIFY(!button.isDown());
        nativePress(window.windowHandle(), {550,20});
        agent.setMaximizeButton(nullptr);
        nativeRelease(window.windowHandle(), {550,20});
        QCOMPARE(clicked.count(),1); QVERIFY(!button.isDown());
        agent.setMaximizeButton(&button);
        button.setFocus(); QTest::keyClick(&button, Qt::Key_Space);
        QTRY_COMPARE(clicked.count(),2); QTRY_VERIFY(window.isMaximized());
        SendMessageW(nativeHandle(window.windowHandle()), WM_SYSCOMMAND, SC_RESTORE,0);
        QTRY_VERIFY(!window.isMaximized());
        SendMessageW(nativeHandle(window.windowHandle()), WM_SYSCOMMAND, SC_MAXIMIZE,0);
        QTRY_VERIFY(window.isMaximized());
        SendMessageW(nativeHandle(window.windowHandle()), WM_SYSCOMMAND, SC_RESTORE,0);
        QTRY_VERIFY(!window.isMaximized());
        QCOMPARE(clicked.count(),2);
        QDialog modal(&window); modal.setWindowModality(Qt::WindowModal); modal.show();
        QVERIFY(QTest::qWaitForWindowExposed(&modal));
        QCOMPARE(nativeHit(window.windowHandle(),{30,20}),LRESULT(HTCLIENT));
        modal.close(); QCoreApplication::processEvents();
        QCOMPARE(nativeHit(window.windowHandle(),{30,20}),LRESULT(HTCAPTION));
    }
    void recreateAndNegativeCoordinates()
    {
        QWidget window; window.resize(640,420);
        QWidget bar(&window); bar.setGeometry(8,8,624,44);
        FramelessKit::WidgetAgent agent(&window); agent.setTitleBar(&bar);
        window.show(); QVERIFY(QTest::qWaitForWindowExposed(&window)); QCoreApplication::processEvents();
        SetWindowPos(nativeHandle(window.windowHandle()), nullptr, -800, -300,0,0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        QCOMPARE(nativeHit(window.windowHandle(),{30,20}),LRESULT(HTCAPTION));
        window.hide();
        window.windowHandle()->destroy();
        window.show(); QCoreApplication::processEvents();
        QVERIFY(window.windowHandle()->handle());
        QTRY_COMPARE(nativeHit(window.windowHandle(),{30,20}),LRESULT(HTCAPTION));
    }
};
int main(int argc,char** argv)
{
    QApplication app(argc,argv); FramelessKit::initialize();
    WidgetsNativeTests tests; return QTest::qExec(&tests,argc,argv);
}
#include "WidgetsNativeTests.moc"

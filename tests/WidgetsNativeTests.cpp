#include "WindowsTestSupport.h"
#include <WindowHeler/Initialization.h>
#include <WindowHeler/FramelessWidgetAgent.h>
#ifdef TEST_WINDOW_MATERIAL
#include <WindowHeler/MaterialWidgetAgent.h>
#endif
#include <QApplication>
#include <QDialog>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest>

class WidgetsNativeTests : public QObject
{
    Q_OBJECT
private slots:
#ifdef TEST_WINDOW_MATERIAL
    void frameAndMaterial_data()
    {
        QTest::addColumn<bool>("frameFirst");
        QTest::newRow("frame first") << true;
        QTest::newRow("material first") << false;
    }
    void frameAndMaterial()
    {
        using namespace WindowHeler;
        QFETCH(bool, frameFirst);
        QWidget window;
        window.resize(640, 420);
        QWidget bar(&window);
        bar.setGeometry(8, 8, 624, 44);
        std::unique_ptr<FramelessWidgetAgent> frame;
        std::unique_ptr<MaterialWidgetAgent> material;
        if (frameFirst)
            frame = std::make_unique<FramelessWidgetAgent>(&window);
        material = std::make_unique<MaterialWidgetAgent>(&window);
        QSignalSpy materialErrors(material.get(), &MaterialWidgetAgent::errorOccurred);
        material->setMaterial(Material::Transparent);
        QCOMPARE(material->material(), Material::Transparent);
        if (!frameFirst)
            frame = std::make_unique<FramelessWidgetAgent>(&window);
        QSignalSpy frameErrors(frame.get(), &FramelessWidgetAgent::errorOccurred);
        frame->setTitleBar(&bar);
        QCOMPARE(frame->titleBar(), &bar);
        QVERIFY(!window.testAttribute(Qt::WA_WState_Created));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTRY_COMPARE(material->effectiveMaterial(), Material::Transparent);
        QCOMPARE(nativeHit(window.windowHandle(), {30, 20}), LRESULT(HTCAPTION));
        auto duplicate = std::make_unique<MaterialWidgetAgent>(&window);
        QSignalSpy duplicateErrors(duplicate.get(), &MaterialWidgetAgent::errorOccurred);
        QTRY_COMPARE(duplicateErrors.count(), 1);
        QCOMPARE(materialErrors.count(), 0);
        QCOMPARE(frameErrors.count(), 0);
        if (frameFirst)
        {
            material.reset();
            QTRY_VERIFY(!window.testAttribute(Qt::WA_NoSystemBackground));
            QCOMPARE(nativeHit(window.windowHandle(), {30, 20}), LRESULT(HTCAPTION));
        }
        else
        {
            frame.reset();
            QTRY_COMPARE(nativeHit(window.windowHandle(), {30, 20}), LRESULT(HTCLIENT));
            QCOMPARE(material->effectiveMaterial(), Material::Transparent);
            material->setMaterial(Material::Solid);
            QTRY_COMPARE(material->effectiveMaterial(), Material::Solid);
        }
    }
#endif
    void frameAndGeometry()
    {
        QWidget window;
        window.setGeometry(120, 120, 640, 420);
        window.setMinimumSize(320, 240);
        QWidget bar(&window); bar.setGeometry(8, 8, 624, 44);
        QPushButton button(&bar); button.setGeometry(500, 0, 100, 40);
        QWidget interactive(&bar); interactive.setGeometry(200, 0, 100, 40);
        auto agent = std::make_unique<WindowHeler::FramelessWidgetAgent>(&window);
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
        const HWND hwnd = nativeHandle(window.windowHandle());
        RECT frame{}, client{};
        QVERIFY(GetWindowRect(hwnd, &frame));
        QVERIFY(GetClientRect(hwnd, &client));
        POINT clientOrigin{};
        QVERIFY(ClientToScreen(hwnd, &clientOrigin));
        QCOMPARE(window.size(), QSize(640, 420));
        const auto scale = window.windowHandle()->devicePixelRatio();
        QCOMPARE(client.right, qRound(640 * scale));
        QCOMPARE(client.bottom, qRound(420 * scale));
        // The normal window retains the invisible sizing frame and visible DWM border,
        // but no native title bar. Compare their thickness at the current HWND DPI.
        const UINT dpi = GetDpiForWindow(hwnd);
        const int padding = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
        const int horizontalBorder = GetSystemMetricsForDpi(SM_CXSIZEFRAME, dpi) + padding + 2;
        const int verticalBorder = GetSystemMetricsForDpi(SM_CYSIZEFRAME, dpi) + padding + 2;
        const auto isBorder = [](int inset, int limit) { return inset >= 0 && inset <= limit; };
        QVERIFY(isBorder(clientOrigin.x - frame.left, horizontalBorder));
        QVERIFY(isBorder(frame.right - clientOrigin.x - client.right, horizontalBorder));
        QVERIFY(isBorder(clientOrigin.y - frame.top, verticalBorder));
        QVERIFY(isBorder(frame.bottom - clientOrigin.y - client.bottom, verticalBorder));
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
        WindowHeler::FramelessWidgetAgent agent(&window); agent.setTitleBar(&bar); agent.setMaximizeButton(&button);
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
        WindowHeler::FramelessWidgetAgent agent(&window); agent.setTitleBar(&bar);
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
    QApplication app(argc,argv); WindowHeler::initialize();
    WidgetsNativeTests tests; return QTest::qExec(&tests,argc,argv);
}
#include "WidgetsNativeTests.moc"

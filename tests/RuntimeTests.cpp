#include "Runtime_p.h"

#include <FramelessKit/Initialization.h>
#include <QGuiApplication>
#include <QThread>
#include <QWindow>
#include <QtTest>

class RuntimeTests final : public QObject
{
    Q_OBJECT
private slots:
    void validationAndIdempotence()
    {
        using namespace FramelessKit;
        QVERIFY(!Internal::isInitialized());
        auto* window = new QWindow;
        initialize();
        QVERIFY(!Internal::isInitialized());
        delete window;
        auto* worker = QThread::create([] { initialize(); });
        worker->start();
        QVERIFY(worker->wait(10000));
        delete worker;
        QVERIFY(!Internal::isInitialized());
        initialize();
        QVERIFY(Internal::isInitialized());
        QWindow existing;
        initialize();
        QVERIFY(Internal::isInitialized());
        initialize(true); // This target deliberately links Core only.
        QVERIFY(!Internal::isInitialized(true));
        QVERIFY(Internal::isInitialized());
    }
};
int main(int argc, char** argv)
{
    FramelessKit::initialize(); // No application: diagnostic, not a crash or implicit success.
    QGuiApplication app(argc, argv);
    RuntimeTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "RuntimeTests.moc"

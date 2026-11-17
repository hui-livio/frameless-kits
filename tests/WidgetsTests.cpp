#include <FramelessKit/Initialization.h>
#include <FramelessKit/WidgetAgent.h>
#include <QApplication>
#include <QPushButton>
#include <QWidget>
#include <QtTest>

class WidgetsTests final : public QObject
{
    Q_OBJECT
private slots:
    void configurationDoesNotCreateSurface()
    {
        using namespace FramelessKit;
        QWidget window;
        WidgetAgent agent(&window);
        QWidget bar(&window);
        QPushButton button(&bar);
        agent.setTitleBar(&bar);
        agent.setInteractive(&button);
        agent.setInteractive(&button, false);
        agent.setMaximizeButton(&button);
        QCOMPARE(agent.titleBar(), &bar);
        QCOMPARE(agent.maximizeButton(), &button);
        QVERIFY(!window.testAttribute(Qt::WA_WState_Created));
        QSignalSpy errors(&agent, &WidgetAgent::errorOccurred);
        QWidget other;
        agent.setTitleBar(&other);
        QCOMPARE(agent.titleBar(), &bar);
        QCOMPARE(errors.count(), 1);
        QSignalSpy changes(&agent, &WidgetAgent::materialChanged);
        agent.setMaterial(Material::Mica);
        QCOMPARE(agent.material(), Material::Mica);
        QCOMPARE(agent.effectiveMaterial(), Material::Solid);
        agent.setMaterial(Material::Mica);
        QCOMPARE(changes.count(), 1);
        agent.setTitleBar(nullptr);
        agent.setMaximizeButton(nullptr);
        QVERIFY(!agent.titleBar());
        QVERIFY(!agent.maximizeButton());
    }
    void duplicateAndOwnership()
    {
        using namespace FramelessKit;
        auto* window = new QWidget;
        auto* first  = new WidgetAgent(window);
        QPointer<WidgetAgent> guard(first);
        auto* second = new WidgetAgent(window);
        QSignalSpy errors(second, &WidgetAgent::errorOccurred);
        QTRY_COMPARE(errors.count(), 1);
        delete second;
        auto* title = new QWidget(window);
        first->setTitleBar(title);
        delete title;
        QVERIFY(!first->titleBar());
        delete window;
        QVERIFY(!guard);
    }
};
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    FramelessKit::initialize();
    WidgetsTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "WidgetsTests.moc"

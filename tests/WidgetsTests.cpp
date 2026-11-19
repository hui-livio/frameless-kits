#include <WindowHeler/Initialization.h>
#include <WindowHeler/FramelessWidgetAgent.h>
#ifdef TEST_WINDOW_MATERIAL
#include <WindowHeler/MaterialWidgetAgent.h>
#endif
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
        using namespace WindowHeler;
        QWidget window;
        FramelessWidgetAgent agent(&window);
        QWidget bar(&window);
        QPushButton button(&bar);
        agent.setTitleBar(&bar);
        agent.setInteractive(&button);
        agent.setInteractive(&button, false);
        agent.setMaximizeButton(&button);
        QCOMPARE(agent.titleBar(), &bar);
        QCOMPARE(agent.maximizeButton(), &button);
        QVERIFY(!window.testAttribute(Qt::WA_WState_Created));
        QSignalSpy errors(&agent, &FramelessWidgetAgent::errorOccurred);
        QWidget other;
        agent.setTitleBar(&other);
        QCOMPARE(agent.titleBar(), &bar);
        QCOMPARE(errors.count(), 1);
        agent.setTitleBar(nullptr);
        agent.setMaximizeButton(nullptr);
        QVERIFY(!agent.titleBar());
        QVERIFY(!agent.maximizeButton());
    }
    void duplicateAndOwnership()
    {
        using namespace WindowHeler;
        auto* window = new QWidget;
        auto* first  = new FramelessWidgetAgent(window);
        QPointer<FramelessWidgetAgent> guard(first);
        auto* second = new FramelessWidgetAgent(window);
        QSignalSpy errors(second, &FramelessWidgetAgent::errorOccurred);
        QTRY_COMPARE(errors.count(), 1);
        delete second;
        auto* title = new QWidget(window);
        first->setTitleBar(title);
        delete title;
        QVERIFY(!first->titleBar());
        delete window;
        QVERIFY(!guard);
    }
#ifdef TEST_WINDOW_MATERIAL
    void preservesApplicationBackgroundChanges()
    {
        using namespace WindowHeler;
        QWidget window;
        window.resize(240, 120);
        MaterialWidgetAgent material(&window);
        material.setMaterial(Material::Transparent);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTRY_COMPARE(material.effectiveMaterial(), Material::Transparent);

        window.setAttribute(Qt::WA_OpaquePaintEvent, true);
        window.setAutoFillBackground(true);
        material.setMaterial(Material::Solid);
        QTRY_COMPARE(material.effectiveMaterial(), Material::Solid);
        QVERIFY(window.testAttribute(Qt::WA_OpaquePaintEvent));
        QVERIFY(window.autoFillBackground());
    }
#endif
};
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    WindowHeler::initialize();
    WidgetsTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "WidgetsTests.moc"

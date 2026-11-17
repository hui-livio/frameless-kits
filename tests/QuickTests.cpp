#include "Runtime_p.h"

#include <FramelessKit/Initialization.h>
#include <FramelessKit/WindowAgent.h>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QtTest>

class QuickTests final : public QObject
{
    Q_OBJECT
private slots:
    void initializationTransitions()
    {
        using namespace FramelessKit;
        QVERIFY(!Internal::isInitialized());
        initialize(false);
        QVERIFY(Internal::isInitialized());
        auto* tooEarlyWindow = new QQuickWindow;
        initialize(true);
        QVERIFY(!Internal::isInitialized(true));
        delete tooEarlyWindow;
        QWindow ordinaryWindow;
        initialize(true);
        QVERIFY(Internal::isInitialized(true));
        QVERIFY(QQuickWindow::hasDefaultAlphaBuffer());
        QQuickWindow window;
        initialize(true);
        initialize(false);
        QVERIFY(Internal::isInitialized(true));
    }
    void propertyOrderAndLists()
    {
        using Agent = FramelessKit::WindowAgent;
        QQuickWindow window;
        QQuickItem bar(window.contentItem()), button(&bar);
        Agent agent;
        agent.classBegin();
        agent.setMaterial(Agent::Material::Mica);
        agent.setTitleBar(&bar);
        agent.setWindow(&window);
        auto list = agent.interactiveItems();
        list.append(&list, &button);
        agent.componentComplete();
        QCOMPARE(agent.material(), Agent::Material::Mica);
        QCOMPARE(agent.effectiveMaterial(), Agent::Material::Solid);
        QCOMPARE(list.count(&list), 1);
        QCOMPARE(list.at(&list, 0), &button);
        list.removeLast(&list);
        QCOMPARE(list.count(&list), 0);
        list.append(&list, &button);
        list.clear(&list);
        QCOMPARE(list.count(&list), 0);
        QQuickWindow other;
        QQuickItem crossWindow(other.contentItem());
        agent.setTitleBar(&crossWindow);
        QCOMPARE(agent.titleBar(), &bar);
        agent.setWindow(nullptr);
        QVERIFY(!agent.window());
        agent.setWindow(&window);
        QCOMPARE(agent.material(), Agent::Material::Mica);
    }
    void destroyedItemsAndHost()
    {
        using Agent  = FramelessKit::WindowAgent;
        auto* window = new QQuickWindow;
        Agent agent;
        agent.setWindow(window);
        auto* item = new QQuickItem(window->contentItem());
        agent.setTitleBar(item);
        auto list = agent.interactiveItems();
        list.append(&list, item);
        delete item;
        QVERIFY(!agent.titleBar());
        QCOMPARE(list.count(&list), 0);
        delete window;
        QVERIFY(!agent.window());
        QCOMPARE(agent.effectiveMaterial(), Agent::Material::Solid);
    }
};
int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    QuickTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "QuickTests.moc"

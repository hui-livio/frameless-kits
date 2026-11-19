#include "Runtime_p.h"

#include <WindowHeler/Initialization.h>
#include <WindowHeler/FramelessAgent.h>
#ifdef TEST_WINDOW_MATERIAL
#include <WindowHeler/MaterialAgent.h>
#endif
#include <QGuiApplication>
#include <QQuickWindow>
#include <QtTest>

class QuickTests final : public QObject
{
    Q_OBJECT
private slots:
    void initializationTransitions()
    {
        using namespace WindowHeler;
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
        using Agent = WindowHeler::FramelessAgent;
        QQuickWindow window;
        QQuickItem bar(window.contentItem()), button(&bar);
        Agent agent;
        agent.classBegin();
        agent.setTitleBar(&bar);
        agent.setWindow(&window);
        auto list = agent.interactiveItems();
        list.append(&list, &button);
        agent.componentComplete();
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
    }
    void destroyedItemsAndHost()
    {
        using Agent  = WindowHeler::FramelessAgent;
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
    }
#ifdef TEST_WINDOW_MATERIAL
    void preservesApplicationColorChanges()
    {
        using namespace WindowHeler;
        QQuickWindow window;
        window.resize(240, 120);
        window.setColor(QColor("steelblue"));
        MaterialAgent material;
        material.setWindow(&window);
        material.setMaterial(Material::Transparent);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTRY_COMPARE(material.effectiveMaterial(), Material::Transparent);

        window.setColor(QColor("tomato"));
        window.setColor(Qt::transparent);
        material.setMaterial(Material::Solid);
        QTRY_COMPARE(material.effectiveMaterial(), Material::Solid);
        QCOMPARE(window.color(), QColor(Qt::transparent));
    }
#endif
};
int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    QuickTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "QuickTests.moc"

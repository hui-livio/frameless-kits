#include "WindowController.h"

#include <FramelessKit/Initialization.h>
#include <QGuiApplication>
#include <QtTest>

using namespace FramelessKit;
using namespace FramelessKit::Internal;
class TestHost final : public HostAdapter
{
public:
    QPointer<QObject> root;
    QHash<const QObject*, QRectF> items;
    bool fullscreen = false;
    explicit TestHost(QObject* object) : root(object) {}
    QObject* host() const override
    {
        return root;
    }
    QWindow* window() const override
    {
        return nullptr;
    }
    bool owns(const QObject* item) const override
    {
        return item && item->parent() == root;
    }
    bool contains(const QObject* item, QPointF point) const override
    {
        return items.value(item).contains(point);
    }
    bool fullScreen() const override
    {
        return fullscreen;
    }
};
class CoreTests final : public QObject
{
    Q_OBJECT
private slots:
    void hitPriorityAndWeakReferences()
    {
        QObject root, bar(&root), control(&root), button(&root);
        button.setProperty("enabled", true);
        TestHost host(&root);
        host.items = { { &bar, { 0, 0, 300, 40 } },
                       { &control, { 100, 0, 100, 40 } },
                       { &button, { 150, 0, 40, 40 } } };
        HitTestModel model;
        model.titleBar = &bar;
        model.interactiveItems.append(&control);
        model.maximizeButton = &button;
        QCOMPARE(model.hitTest(host, { 10, 10 }), HitRegion::Caption);
        QCOMPARE(model.hitTest(host, { 120, 10 }), HitRegion::Client);
        QObject otherWindow;
        control.setParent(&otherWindow);
        QCOMPARE(model.hitTest(host, { 120, 10 }), HitRegion::Caption);
        control.setParent(&root);
        QCOMPARE(model.hitTest(host, { 160, 10 }), HitRegion::MaximizeButton);
        button.setProperty("enabled", false);
        QCOMPARE(model.hitTest(host, { 160, 10 }), HitRegion::Client);
        host.fullscreen = true;
        QCOMPARE(model.hitTest(host, { 10, 10 }), HitRegion::Client);
        host.fullscreen = false;
        auto* temporary = new QObject(&root);
        host.items.insert(temporary, { 0, 0, 300, 40 });
        model.titleBar = temporary;
        delete temporary;
        QCOMPARE(model.hitTest(host, { 10, 10 }), HitRegion::Client);
    }
    void bindingAndMaterials()
    {
        QObject root, other;
        WindowController first(std::make_unique<TestHost>(&root), false);
        QVERIFY(first.isBound());
        QVERIFY(!first.accepts(&other));
        WindowController duplicate(std::make_unique<TestHost>(&root), false);
        QSignalSpy duplicateErrors(&duplicate, &WindowController::errorOccurred);
        QVERIFY(!duplicate.isBound());
        QTRY_COMPARE(duplicateErrors.count(), 1);
        QSignalSpy errors(&first, &WindowController::errorOccurred);
        QSignalSpy applied(&first, &WindowController::effectiveMaterialChanged);
        first.setMaterial(Material::Mica);
        QCOMPARE(first.material(), Material::Mica);
        QCOMPARE(first.effectiveMaterial(), Material::Solid);
        QCOMPARE(errors.count(), 1);
        QCOMPARE(applied.count(), 0);
        first.setMaterial(Material::Mica);
        QCOMPARE(errors.count(), 1);
        first.setMaterial(Material::Solid);
        QCOMPARE(first.material(), Material::Solid);
        first.setMaterial(static_cast<Material>(999));
        QCOMPARE(first.material(), Material::Solid);
    }
    void bindingReleasesOnDestruction()
    {
        QObject root;
        {
            WindowController first(std::make_unique<TestHost>(&root), false);
            QVERIFY(first.isBound());
        }
        WindowController second(std::make_unique<TestHost>(&root), false);
        QVERIFY(second.isBound());
        auto* temporary = new QObject;
        WindowController controller(std::make_unique<TestHost>(temporary), false);
        QVERIFY(controller.isBound());
        delete temporary;
        QVERIFY(!controller.isBound());
        QCOMPARE(controller.hitTest({ 0, 0 }), HitRegion::Client);
    }
};
int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    FramelessKit::initialize();
    CoreTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "CoreTests.moc"

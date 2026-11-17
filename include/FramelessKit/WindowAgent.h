#pragma once
#include <FramelessKit/Export.h>
#include <QObject>
#include <QQmlListProperty>
#include <QQmlParserStatus>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtQml/qqmlregistration.h>
#include <memory>

namespace FramelessKit
{
// Not final: Qt's generated QML registration derives QQmlElement from this
// type.
class FRAMELESSKIT_QUICK_EXPORT WindowAgent : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    QML_ELEMENT
    Q_PROPERTY(QQuickWindow* window READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY(QQuickItem* titleBar READ titleBar WRITE setTitleBar NOTIFY titleBarChanged)
    Q_PROPERTY(QQuickItem* maximizeButton READ maximizeButton WRITE setMaximizeButton NOTIFY maximizeButtonChanged)
    Q_PROPERTY(QQmlListProperty<QQuickItem> interactiveItems READ interactiveItems NOTIFY interactiveItemsChanged)
    Q_PROPERTY(Material material READ material WRITE setMaterial NOTIFY materialChanged)
    Q_PROPERTY(Material effectiveMaterial READ effectiveMaterial NOTIFY effectiveMaterialChanged)
public:
    enum class Material
    {
        Solid,
        Transparent,
        Blur,
        Acrylic,
        Mica,
        MicaAlt
    };
    Q_ENUM(Material)
    explicit WindowAgent(QObject* parent = nullptr);
    ~WindowAgent() override;
    QQuickWindow* window() const;
    void setWindow(QQuickWindow* window);
    QQuickItem* titleBar() const;
    void setTitleBar(QQuickItem* item);
    QQuickItem* maximizeButton() const;
    void setMaximizeButton(QQuickItem* item);
    QQmlListProperty<QQuickItem> interactiveItems();
    Material material() const;
    Material effectiveMaterial() const;
    void setMaterial(Material material);
    void classBegin() override;
    void componentComplete() override;
signals:
    void windowChanged();
    void titleBarChanged();
    void maximizeButtonChanged();
    void interactiveItemsChanged();
    void materialChanged();
    void effectiveMaterialChanged();
    void errorOccurred(const QString& message);

private:
    void bindWindow();
    static void appendInteractiveItem(QQmlListProperty<QQuickItem>* list, QQuickItem* item);
    static qsizetype interactiveCount(QQmlListProperty<QQuickItem>* list);
    static QQuickItem* interactiveAt(QQmlListProperty<QQuickItem>* list, qsizetype index);
    static void clearInteractiveItems(QQmlListProperty<QQuickItem>* list);
    static void replaceInteractiveItem(QQmlListProperty<QQuickItem>* list, qsizetype index, QQuickItem* item);
    static void removeLastInteractiveItem(QQmlListProperty<QQuickItem>* list);
    class Private;
    std::unique_ptr<Private> d;
};
} // namespace FramelessKit

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
// Not final: Qt's generated QML registration derives QQmlElement from this type.
// 不是 final 的：Qt 的生成的 QML 注册从此类型派生 QQmlElement。
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

    // 窗口对象
    QQuickWindow* window() const;
    void setWindow(QQuickWindow* window);

    // 标题栏对象
    QQuickItem* titleBar() const;
    void setTitleBar(QQuickItem* item);

    // 最大化按钮对象
    QQuickItem* maximizeButton() const;
    void setMaximizeButton(QQuickItem* item);

    // 交互控件对象列表
    QQmlListProperty<QQuickItem> interactiveItems();

    // 窗口材质类型
    Material material() const;
    Material effectiveMaterial() const;
    void setMaterial(Material material); // 设置窗口材质类型,实时切换

    // QQmlParserStatus interface
    void classBegin() override;
    void componentComplete() override;

signals:
    void windowChanged();                       // 窗口对象改变
    void titleBarChanged();                     // 标题栏对象改变
    void maximizeButtonChanged();               // 最大化按钮对象改变
    void interactiveItemsChanged();             // 交互控件对象列表改变
    void materialChanged();                     // 窗口材质类型改变
    void effectiveMaterialChanged();            // 生效的窗口材质类型改变
    void errorOccurred(const QString& message); // 错误信号, message 为错误信息

private:
    // 绑定窗口对象
    void bindWindow();
    // 添加交互控件对象
    static void appendInteractiveItem(QQmlListProperty<QQuickItem>* list, QQuickItem* item);
    // 获取交互控件对象数量
    static qsizetype interactiveCount(QQmlListProperty<QQuickItem>* list);
    // 获取指定索引的交互控件对象
    static QQuickItem* interactiveAt(QQmlListProperty<QQuickItem>* list, qsizetype index);
    // 清空交互控件对象列表
    static void clearInteractiveItems(QQmlListProperty<QQuickItem>* list);
    // 替换指定索引的交互控件对象
    static void replaceInteractiveItem(QQmlListProperty<QQuickItem>* list, qsizetype index, QQuickItem* item);
    // 移除最后一个交互控件对象
    static void removeLastInteractiveItem(QQmlListProperty<QQuickItem>* list);

    class Private;
    std::unique_ptr<Private> d;
};
} // namespace FramelessKit

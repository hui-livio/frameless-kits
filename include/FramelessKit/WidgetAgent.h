#pragma once
#include <FramelessKit/Export.h>
#include <FramelessKit/Material.h>
#include <QObject>
#include <memory>

class QWidget;
class QAbstractButton;

namespace FramelessKit
{
/**
 * @brief 窗口代理类
 * @note  此类用于在 QWidget 窗口中实现无边框窗口的功能
 */
class FRAMELESSKIT_WIDGETS_EXPORT WidgetAgent final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(FramelessKit::Material material READ material WRITE setMaterial NOTIFY materialChanged)
    Q_PROPERTY(FramelessKit::Material effectiveMaterial READ effectiveMaterial NOTIFY effectiveMaterialChanged)
public:
    explicit WidgetAgent(QWidget* window);
    ~WidgetAgent() override;

    // 窗口对象
    QWidget* window() const;
    // 标题栏对象
    QWidget* titleBar() const;
    // 设置标题栏对象
    void setTitleBar(QWidget* titleBar);
    // 设置交互控件对象
    void setInteractive(QWidget* widget, bool interactive = true);
    // 最大化按钮对象
    QAbstractButton* maximizeButton() const;
    // 设置最大化按钮对象
    void setMaximizeButton(QAbstractButton* button);
    // 窗口材质类型
    Material material() const;
    // 生效的窗口材质类型
    Material effectiveMaterial() const;

public slots:
    // 设置窗口材质类型,实时切换
    void setMaterial(Material material);

signals:
    void materialChanged();                     // 窗口材质类型改变
    void effectiveMaterialChanged();            // 生效的窗口材质类型改变
    void errorOccurred(const QString& message); // 错误信号, message 为错误信息

private:
    class Private;
    std::unique_ptr<Private> d;
};
} // namespace FramelessKit

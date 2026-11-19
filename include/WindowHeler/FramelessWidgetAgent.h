#pragma once
#include <QObject>
#include <WindowHeler/Export.h>
#include <memory>

class QWidget;
class QAbstractButton;

namespace WindowHeler
{
/**
 * @brief 窗口代理类
 * @note  此类用于在 QWidget 窗口中实现无边框窗口的功能
 */
class WINDOWHELER_FRAMELESS_WIDGETS_EXPORT FramelessWidgetAgent final : public QObject
{
    Q_OBJECT
public:
    explicit FramelessWidgetAgent(QWidget* window);
    ~FramelessWidgetAgent() override;

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
signals:
    void materialChanged();                     // 窗口材质类型改变
    void effectiveMaterialChanged();            // 生效的窗口材质类型改变
    void errorOccurred(const QString& message); // 错误信号, message 为错误信息

private:
    class Private;
    std::unique_ptr<Private> d;
};
} // namespace WindowHeler

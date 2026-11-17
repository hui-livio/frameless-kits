#include "../core/Runtime_p.h"
#include "../core/WindowController.h"

#include <FramelessKit/WidgetAgent.h>
#include <QAbstractButton>
#include <QPointer>
#include <QWidget>
#include <QWindow>

namespace FramelessKit
{
namespace
{
class WidgetHostAdapter final : public Internal::HostAdapter
{
public:
    explicit WidgetHostAdapter(QWidget* root) : m_root(root) {}
    QObject* host() const override
    {
        return m_root && m_root->isWindow() ? m_root.data() : nullptr;
    }
    QWindow* window() const override
    {
        return m_root ? m_root->windowHandle() : nullptr;
    }
    bool owns(const QObject* item) const override
    {
        auto* widget = qobject_cast<const QWidget*>(item);
        return m_root && widget && widget->window() == m_root;
    }
    bool contains(const QObject* item, QPointF point) const override
    {
        auto* widget = qobject_cast<const QWidget*>(item);
        if (!widget || !owns(item) || !m_root)
            return false;
        for (auto* ancestor = widget; ancestor; ancestor = ancestor->parentWidget())
        {
            if (!ancestor->isVisible() || !QRectF(ancestor->rect()).contains(ancestor->mapFrom(m_root, point)))
                return false;
            if (!ancestor->mask().isEmpty() && !ancestor->mask().contains(ancestor->mapFrom(m_root, point).toPoint()))
                return false;
            if (ancestor == m_root)
                break;
        }
        return true;
    }
    bool fullScreen() const override
    {
        return m_root && m_root->isFullScreen();
    }

private:
    QPointer<QWidget> m_root;
};
} // namespace
class WidgetAgent::Private
{
public:
    explicit Private(QWidget* host) : window(host), controller(std::make_unique<WidgetHostAdapter>(host), false) {}
    QPointer<QWidget> window;
    QPointer<QWidget> titleBar;
    QPointer<QAbstractButton> maximizeButton;
    Internal::WindowController controller;
};
WidgetAgent::WidgetAgent(QWidget* window) : QObject(window), d(new Private(window))
{
    connect(&d->controller, &Internal::WindowController::effectiveMaterialChanged, this,
            &WidgetAgent::effectiveMaterialChanged);
    connect(&d->controller, &Internal::WindowController::errorOccurred, this, &WidgetAgent::errorOccurred);
}
WidgetAgent::~WidgetAgent() = default;
QWidget* WidgetAgent::window() const
{
    return d->window;
}
QWidget* WidgetAgent::titleBar() const
{
    return d->titleBar;
}
void WidgetAgent::setTitleBar(QWidget* item)
{
    if (d->controller.setTitleBar(item))
        d->titleBar = item;
}
void WidgetAgent::setInteractive(QWidget* widget, bool enabled)
{
    d->controller.setInteractive(widget, enabled);
}
QAbstractButton* WidgetAgent::maximizeButton() const
{
    return d->maximizeButton;
}
void WidgetAgent::setMaximizeButton(QAbstractButton* button)
{
    if (d->controller.setMaximizeButton(button))
        d->maximizeButton = button;
}
Material WidgetAgent::material() const
{
    return d->controller.material();
}
Material WidgetAgent::effectiveMaterial() const
{
    return d->controller.effectiveMaterial();
}
void WidgetAgent::setMaterial(Material value)
{
    const auto previous = material();
    d->controller.setMaterial(value);
    if (previous != material())
        emit materialChanged();
}
} // namespace FramelessKit

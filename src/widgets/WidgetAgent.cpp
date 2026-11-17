#include "../core/Runtime_p.h"
#include "../core/WindowController.h"

#include <FramelessKit/WidgetAgent.h>
#include <QAbstractButton>
#include <QCoreApplication>
#include <QEnterEvent>
#include <QMouseEvent>
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
    Qt::WindowFlags flags() const override { return m_root ? m_root->windowFlags() : Qt::WindowFlags{}; }
    Qt::WindowStates states() const override { return m_root ? m_root->windowState() : Qt::WindowStates{}; }
    QSize minimumSize() const override { return m_root ? m_root->minimumSize() : QSize{}; }
    QSize maximumSize() const override { return m_root ? m_root->maximumSize() : QSize{}; }
    void setFrameless(bool enabled) override
    {
        if (!m_root) return;
        const bool visible = m_root->isVisible();
        const auto state = m_root->windowState();
        const QRect geometry = m_root->geometry();
        m_root->setWindowFlag(Qt::FramelessWindowHint, enabled);
        if (!state) m_root->setGeometry(geometry);
        if (visible) { m_root->setWindowState(state); m_root->show(); }
    }
    void setStates(Qt::WindowStates state) override { if (m_root) m_root->setWindowState(state); }
    void close() override { if (m_root) m_root->close(); }
    bool supportsMaximizeButton(const QObject* item) const override
    {
        return qobject_cast<const QAbstractButton*>(item);
    }
    void buttonEvent(QObject* object, QEvent::Type type, QPointF point, Qt::MouseButton button,
                     Qt::MouseButtons buttons) override
    {
        auto* widget = qobject_cast<QAbstractButton*>(object);
        if (!widget || !m_root || !owns(widget)) return;
        const auto local = widget->mapFrom(m_root, point);
        const auto global = m_root->mapToGlobal(point);
        if (type == QEvent::Enter)
        {
            QEnterEvent event(local, point, global);
            QCoreApplication::sendEvent(widget, &event);
        }
        else if (type == QEvent::Leave)
        {
            QEvent event(type);
            QCoreApplication::sendEvent(widget, &event);
        }
        else
        {
            QMouseEvent event(type, local, point, global, button, buttons, QGuiApplication::keyboardModifiers());
            QCoreApplication::sendEvent(widget, &event);
        }
    }
    void cancelButton(QObject* object) override
    {
        if (auto* button = qobject_cast<QAbstractButton*>(object))
        {
            button->setDown(false);
            if (QWidget::mouseGrabber() == button) button->releaseMouse();
            QEvent leave(QEvent::Leave);
            QCoreApplication::sendEvent(button, &leave);
        }
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

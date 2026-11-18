#include "../core/Runtime_p.h"
#include "../core/WindowController.h"

#include <FramelessKit/WidgetAgent.h>
#include <QAbstractButton>
#include <QCoreApplication>
#include <QEnterEvent>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPointer>
#include <QWidget>
#include <QWindow>

#ifdef Q_OS_MACOS
#include "CocoaWidgetSafeArea_p.h"
#endif

namespace FramelessKit
{
namespace
{
class WidgetHostAdapter final : public Internal::HostAdapter
{
public:
    explicit WidgetHostAdapter(QWidget* root) : m_root(root) {}
    ~WidgetHostAdapter() override
    {
        setMaterialBackground(false);
        if (m_root && m_reservedAlpha)
        {
            // Qt keeps an existing surface; do not implicitly destroy/recreate it.
            const bool noBackground = m_root->testAttribute(Qt::WA_NoSystemBackground);
            m_root->setAttribute(Qt::WA_TranslucentBackground, false);
            m_root->setAttribute(Qt::WA_NoSystemBackground, noBackground);
        }
    }
    void prepareMaterialSurface() override
    {
        if (!m_root || (window() && window()->handle()) || m_root->testAttribute(Qt::WA_TranslucentBackground))
            return;
        const bool noBackground = m_root->testAttribute(Qt::WA_NoSystemBackground);
        m_root->setAttribute(Qt::WA_TranslucentBackground);
        m_root->setAttribute(Qt::WA_NoSystemBackground, noBackground);
        m_reservedAlpha = true;
    }
    QColor materialColor() const override
    {
        return m_root ? m_root->palette().color(m_root->backgroundRole()) : QColor(Qt::white);
    }
    void setMaterialBackground(bool enabled) override
    {
        if (!m_root || enabled == m_materialBackground)
            return;
        if (enabled)
        {
            m_originalNoBackground = m_root->testAttribute(Qt::WA_NoSystemBackground);
            m_originalOpaque       = m_root->testAttribute(Qt::WA_OpaquePaintEvent);
            m_originalAutoFill     = m_root->autoFillBackground();
            m_root->setAttribute(Qt::WA_NoSystemBackground);
            m_root->setAttribute(Qt::WA_OpaquePaintEvent, false);
            m_root->setAutoFillBackground(false);
        }
        else
        {
            if (m_root->testAttribute(Qt::WA_NoSystemBackground))
                m_root->setAttribute(Qt::WA_NoSystemBackground, m_originalNoBackground);
            if (!m_root->testAttribute(Qt::WA_OpaquePaintEvent))
                m_root->setAttribute(Qt::WA_OpaquePaintEvent, m_originalOpaque);
            if (!m_root->autoFillBackground())
                m_root->setAutoFillBackground(m_originalAutoFill);
        }
        m_materialBackground = enabled;
        m_root->update();
    }

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

    Qt::WindowFlags flags() const override
    {
        return m_root ? m_root->windowFlags() : Qt::WindowFlags{};
    }
    bool windowCapability(Qt::WindowType hint) const override
    {
        return flags().testFlag(hint);
    }

    Qt::WindowStates states() const override
    {
        return m_root ? m_root->windowState() : Qt::WindowStates{};
    }

    QSize minimumSize() const override
    {
        return m_root ? m_root->minimumSize() : QSize{};
    }

    QSize maximumSize() const override
    {
        return m_root ? m_root->maximumSize() : QSize{};
    }

    void setFrameless(bool enabled) override
    {
        if (!m_root)
            return;
        const bool visible   = m_root->isVisible();
        const auto state     = m_root->windowState();
        const QRect geometry = m_root->geometry();
        m_root->setWindowFlag(Qt::FramelessWindowHint, enabled);
        if (!state)
            m_root->setGeometry(geometry);
        if (visible)
        {
            m_root->setWindowState(state);
            m_root->show();
        }
    }

    void setStates(Qt::WindowStates state) override
    {
        if (m_root)
            m_root->setWindowState(state);
    }

    void close() override
    {
        if (m_root)
            m_root->close();
    }

    bool supportsMaximizeButton(const QObject* item) const override
    {
        return qobject_cast<const QAbstractButton*>(item);
    }

    void buttonEvent(QObject* object, QEvent::Type type, QPointF point, Qt::MouseButton button,
                     Qt::MouseButtons buttons) override
    {
        auto* widget = qobject_cast<QAbstractButton*>(object);
        if (!widget || !m_root || !owns(widget))
            return;
        const auto local  = widget->mapFrom(m_root, point);
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
            const QPointer<QAbstractButton> guard(button);
            button->setDown(false);
            if (!guard)
                return;
            if (QWidget::mouseGrabber() == button)
                button->releaseMouse();
            if (!guard)
                return;
            QEvent leave(QEvent::Leave);
            QCoreApplication::sendEvent(button, &leave);
        }
    }

private:
    QPointer<QWidget> m_root;
    bool m_reservedAlpha        = false;
    bool m_materialBackground   = false;
    bool m_originalNoBackground = false;
    bool m_originalOpaque       = false;
    bool m_originalAutoFill     = false;
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
#ifdef Q_OS_MACOS
    std::unique_ptr<Internal::CocoaWidgetSafeArea> safeArea;
#endif
};

WidgetAgent::WidgetAgent(QWidget* window) : QObject(window), d(new Private(window))
{
    connect(&d->controller, &Internal::WindowController::effectiveMaterialChanged, this,
            &WidgetAgent::effectiveMaterialChanged);
    connect(&d->controller, &Internal::WindowController::errorOccurred, this, &WidgetAgent::errorOccurred);
#ifdef Q_OS_MACOS
    if (d->controller.isBound() && QGuiApplication::platformName() == QStringLiteral("cocoa"))
    {
        d->safeArea = std::make_unique<Internal::CocoaWidgetSafeArea>(window);
        d->safeArea->setTitleBar(nullptr);
    }
#endif
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
    if (!d->controller.setTitleBar(item))
        return;
    d->titleBar = item;
#ifdef Q_OS_MACOS
    if (d->safeArea)
        d->safeArea->setTitleBar(item);
#endif
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
    const QPointer<WidgetAgent> guard(this);
    if (d->controller.setMaximizeButton(button) && guard)
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
    const QPointer<WidgetAgent> guard(this);
    const auto previous = material();
    d->controller.setMaterial(value);
    if (guard && previous != material())
        emit materialChanged();
}
} // namespace FramelessKit

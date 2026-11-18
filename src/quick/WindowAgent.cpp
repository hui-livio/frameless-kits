#include "../core/Runtime_p.h"
#include "../core/WindowController.h"

#include <FramelessKit/WindowAgent.h>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QHash>
#include <QHoverEvent>
#include <QMouseEvent>
#include <QPalette>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <algorithm>

namespace FramelessKit
{
namespace
{
class QuickHostAdapter final : public Internal::HostAdapter
{
public:
    explicit QuickHostAdapter(QQuickWindow* window) : m_window(window) {}
    ~QuickHostAdapter() override
    {
        setMaterialBackground(false);
    }
    bool materialSurfaceAvailable(QString& error) const override
    {
        // QQuickWindow::setColor(opaque) may report alphaBufferSize == -1 after
        // returning to Solid. The surface was nevertheless reserved before creation.
        if (m_window && m_window->handle() && m_window->format().alphaBufferSize() > 0)
            m_confirmedAlpha = true;
        if (m_reservedAlpha && m_confirmedAlpha && m_window && m_window->handle())
            return true;
        return HostAdapter::materialSurfaceAvailable(error);
    }
    void materialSurfaceDestroyed() override
    {
        m_confirmedAlpha = false;
    }
    void prepareMaterialSurface() override
    {
        if (!m_window)
            return;
        m_reservedAlpha = m_window->format().alphaBufferSize() > 0;
        if (m_window->handle())
            return;
        auto format = m_window->requestedFormat();
        if (format.alphaBufferSize() < 8)
        {
            format.setAlphaBufferSize(8);
            m_window->setFormat(format);
        }
        m_reservedAlpha = true;
    }
    QColor materialColor() const override
    {
        const QColor color = m_background ? m_originalColor : (m_window ? m_window->color() : QColor{});
        return color.isValid() && color.alpha() > 0 ? color : QGuiApplication::palette().color(QPalette::Window);
    }
    void setMaterialBackground(bool enabled) override
    {
        if (!m_window || enabled == m_background)
            return;
        m_background = enabled;
        if (enabled)
        {
            m_originalColor = m_window->color();
            m_window->setColor(Qt::transparent);
        }
        else if (m_window->color() == QColor(Qt::transparent))
            m_window->setColor(m_originalColor);
    }
    QObject* host() const override
    {
        return m_window;
    }

    QWindow* window() const override
    {
        return m_window;
    }

    bool owns(const QObject* item) const override
    {
        auto* quickItem = qobject_cast<const QQuickItem*>(item);
        return quickItem && m_window && quickItem->window() == m_window;
    }

    bool contains(const QObject* item, QPointF point) const override
    {
        auto* quickItem = qobject_cast<const QQuickItem*>(item);
        if (!quickItem || !owns(item) || !quickItem->isVisible() ||
            !quickItem->contains(quickItem->mapFromScene(point)))
            return false;
        for (auto* ancestor = quickItem->parentItem(); ancestor; ancestor = ancestor->parentItem())
        {
            if (!ancestor->isVisible())
                return false;
            if (ancestor->clip() && !ancestor->contains(ancestor->mapFromScene(point)))
                return false;
        }
        return true;
    }

    bool fullScreen() const override
    {
        return m_window && m_window->visibility() == QWindow::FullScreen;
    }

    bool supportsMaximizeButton(const QObject* item) const override
    {
        return item && item->inherits("QQuickAbstractButton");
    }

    void buttonEvent(QObject* object, QEvent::Type type, QPointF point, Qt::MouseButton button,
                     Qt::MouseButtons buttons) override
    {
        auto* item = qobject_cast<QQuickItem*>(object);
        if (!item || !m_window || !owns(item))
            return;
        if (type == QEvent::Leave)
        {
            QHoverEvent leave(QEvent::HoverLeave, QPointF{}, QPointF{}, QPointF{},
                              QGuiApplication::keyboardModifiers());
            QCoreApplication::sendEvent(item, &leave);
            return;
        }
        if (type == QEvent::Enter)
            type = QEvent::MouseMove;
        QMouseEvent event(type, point, point, m_window->mapToGlobal(point), button, buttons,
                          QGuiApplication::keyboardModifiers());
        QCoreApplication::sendEvent(m_window, &event);
    }

    void cancelButton(QObject* object) override
    {
        if (auto* item = qobject_cast<QQuickItem*>(object))
        {
            const QPointer<QQuickItem> guard(item);
            item->ungrabMouse();
            if (!guard)
                return;
            QEvent cancel(QEvent::UngrabMouse);
            QCoreApplication::sendEvent(item, &cancel);
            if (!guard)
                return;
            QHoverEvent leave(QEvent::HoverLeave, QPointF{}, QPointF{}, QPointF{},
                              QGuiApplication::keyboardModifiers());
            QCoreApplication::sendEvent(item, &leave);
        }
    }

private:
    QPointer<QQuickWindow> m_window;
    QColor m_originalColor;
    bool m_background             = false;
    bool m_reservedAlpha          = false;
    mutable bool m_confirmedAlpha = false;
};

constexpr auto coreMaterial(WindowAgent::Material value)
{
    return static_cast<FramelessKit::Material>(value);
}

static_assert(int(WindowAgent::Material::MicaAlt) == int(FramelessKit::Material::MicaAlt));
} // namespace
class WindowAgent::Private
{
public:
    QPointer<QQuickWindow> window;
    QPointer<QQuickItem> titleBar;
    QPointer<QQuickItem> maximizeButton;
    QList<QPointer<QQuickItem>> interactive;
    QHash<QQuickItem*, QMetaObject::Connection> interactiveDestroyed;
    QMetaObject::Connection windowDestroyed;
    QMetaObject::Connection titleDestroyed;
    QMetaObject::Connection buttonDestroyed;
    std::unique_ptr<Internal::WindowController> controller;
    Material material = Material::Solid;
    bool complete     = true;
};
WindowAgent::WindowAgent(QObject* parent) : QObject(parent), d(std::make_unique<Private>()) {}
WindowAgent::~WindowAgent() = default;

void WindowAgent::classBegin()
{
    d->complete = false;
}

void WindowAgent::componentComplete()
{
    d->complete = true;
    bindWindow();
}

QQuickWindow* WindowAgent::window() const
{
    return d->window;
}

void WindowAgent::setWindow(QQuickWindow* value)
{
    if (!Internal::checkGuiThread("WindowAgent::setWindow") || d->window == value)
        return;
    disconnect(d->windowDestroyed);
    d->controller.reset();
    d->window = value;
    if (value)
    {
        d->windowDestroyed = connect(value, &QObject::destroyed, this,
                                     [this]
                                     {
                                         d->controller.reset();
                                         emit windowChanged();
                                     });
    }
    if (d->complete)
        bindWindow();
    emit windowChanged();
}

void WindowAgent::bindWindow()
{
    d->controller.reset();
    if (!d->window)
        return;
    d->controller = std::make_unique<Internal::WindowController>(std::make_unique<QuickHostAdapter>(d->window), true);
    connect(d->controller.get(), &Internal::WindowController::effectiveMaterialChanged, this,
            &WindowAgent::effectiveMaterialChanged);
    connect(d->controller.get(), &Internal::WindowController::errorOccurred, this, &WindowAgent::errorOccurred);
    if (!d->controller->isBound())
        return;
    if (!d->controller->setTitleBar(d->titleBar) && d->titleBar)
    {
        d->titleBar = nullptr;
        emit titleBarChanged();
    }
    if (!d->controller->setMaximizeButton(d->maximizeButton) && d->maximizeButton)
    {
        d->maximizeButton = nullptr;
        emit maximizeButtonChanged();
    }
    for (auto it = d->interactive.begin(); it != d->interactive.end();)
    {
        if (!*it || !d->controller->setInteractive(*it, true))
            it = d->interactive.erase(it);
        else
            ++it;
    }
    d->controller->setMaterial(coreMaterial(d->material));
}

QQuickItem* WindowAgent::titleBar() const
{
    return d->titleBar;
}

void WindowAgent::setTitleBar(QQuickItem* item)
{
    if (!Internal::checkGuiThread("WindowAgent::setTitleBar") || d->titleBar == item)
        return;
    if (d->complete && d->controller && !d->controller->setTitleBar(item))
        return;
    disconnect(d->titleDestroyed);
    d->titleBar = item;
    if (item)
        d->titleDestroyed = connect(item, &QObject::destroyed, this, [this] { emit titleBarChanged(); });
    emit titleBarChanged();
}

QQuickItem* WindowAgent::maximizeButton() const
{
    return d->maximizeButton;
}

void WindowAgent::setMaximizeButton(QQuickItem* item)
{
    const QPointer<WindowAgent> guard(this);
    if (!Internal::checkGuiThread("WindowAgent::setMaximizeButton") || d->maximizeButton == item)
        return;
    if (d->complete && d->controller && !d->controller->setMaximizeButton(item))
        return;
    if (!guard)
        return;
    disconnect(d->buttonDestroyed);
    d->maximizeButton = item;
    if (item)
        d->buttonDestroyed = connect(item, &QObject::destroyed, this, [this] { emit maximizeButtonChanged(); });
    emit maximizeButtonChanged();
}

QQmlListProperty<QQuickItem> WindowAgent::interactiveItems()
{
    return { this,
             nullptr,
             appendInteractiveItem,
             interactiveCount,
             interactiveAt,
             clearInteractiveItems,
             replaceInteractiveItem,
             removeLastInteractiveItem };
}

void WindowAgent::appendInteractiveItem(QQmlListProperty<QQuickItem>* list, QQuickItem* item)
{
    auto* self = static_cast<WindowAgent*>(list->object);
    if (!Internal::checkGuiThread("WindowAgent::interactiveItems") || !item)
        return;
    if (self->d->complete && self->d->controller && !self->d->controller->setInteractive(item, true))
        return;
    if (self->d->interactive.contains(item))
        return;
    self->d->interactive.append(item);
    self->d->interactiveDestroyed.insert(item, connect(item, &QObject::destroyed, self,
                                                       [self, item]
                                                       {
                                                           self->d->interactiveDestroyed.remove(item);
                                                           const auto removed = self->d->interactive.removeAll(nullptr);
                                                           if (removed)
                                                               emit self->interactiveItemsChanged();
                                                       }));
    emit self->interactiveItemsChanged();
}

qsizetype WindowAgent::interactiveCount(QQmlListProperty<QQuickItem>* list)
{
    return static_cast<WindowAgent*>(list->object)->d->interactive.size();
}

QQuickItem* WindowAgent::interactiveAt(QQmlListProperty<QQuickItem>* list, qsizetype index)
{
    return static_cast<WindowAgent*>(list->object)->d->interactive.value(index);
}

void WindowAgent::clearInteractiveItems(QQmlListProperty<QQuickItem>* list)
{
    auto* self = static_cast<WindowAgent*>(list->object);
    if (!Internal::checkGuiThread("WindowAgent::interactiveItems") || self->d->interactive.isEmpty())
        return;
    if (self->d->controller)
    {
        for (const auto& item : self->d->interactive)
            if (item)
                self->d->controller->setInteractive(item, false);
    }
    for (const auto& connection : self->d->interactiveDestroyed)
        disconnect(connection);
    self->d->interactiveDestroyed.clear();
    self->d->interactive.clear();
    emit self->interactiveItemsChanged();
}

void WindowAgent::replaceInteractiveItem(QQmlListProperty<QQuickItem>* list, qsizetype index, QQuickItem* item)
{
    auto* self = static_cast<WindowAgent*>(list->object);
    if (!Internal::checkGuiThread("WindowAgent::interactiveItems") || !item || index < 0 ||
        index >= self->d->interactive.size())
        return;
    if (self->d->controller && !self->d->controller->accepts(item))
        return;
    auto entries   = self->d->interactive;
    entries[index] = item;
    clearInteractiveItems(list);
    for (const auto& entry : entries)
        if (entry)
            appendInteractiveItem(list, entry);
}

void WindowAgent::removeLastInteractiveItem(QQmlListProperty<QQuickItem>* list)
{
    auto* self = static_cast<WindowAgent*>(list->object);
    if (!Internal::checkGuiThread("WindowAgent::interactiveItems") || self->d->interactive.isEmpty())
        return;
    const auto item = self->d->interactive.takeLast();
    if (item)
        disconnect(self->d->interactiveDestroyed.take(item));
    if (self->d->controller && item)
        self->d->controller->setInteractive(item, false);
    emit self->interactiveItemsChanged();
}

WindowAgent::Material WindowAgent::material() const
{
    return d->material;
}

WindowAgent::Material WindowAgent::effectiveMaterial() const
{
    return d->controller ? static_cast<Material>(d->controller->effectiveMaterial()) : Material::Solid;
}

void WindowAgent::setMaterial(Material value)
{
    if (!Internal::checkGuiThread("WindowAgent::setMaterial") || d->material == value)
        return;
    if (int(value) < int(Material::Solid) || int(value) > int(Material::MicaAlt))
    {
        emit errorOccurred(QStringLiteral("Invalid material value."));
        return;
    }
    const QPointer<WindowAgent> guard(this);
    d->material = value;
    if (d->controller)
        d->controller->setMaterial(coreMaterial(value));
    if (guard)
        emit materialChanged();
}
} // namespace FramelessKit

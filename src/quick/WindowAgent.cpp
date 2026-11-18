#include "../core/Runtime_p.h"
#include "../core/WindowController.h"

#include <FramelessKit/WindowAgent.h>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QHash>
#include <QHoverEvent>
#include <QMouseEvent>
#include <QPalette>
#include <QPlatformSurfaceEvent>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScopeGuard>
#include <algorithm>
#include <utility>

namespace FramelessKit
{
namespace
{
// QQuickWindow may stop reporting alpha after its clear color becomes opaque.
// Keep confirmed capability with the native-surface lifetime, not an Agent's
// shorter binding lifetime. The window owns this observer, even between Agents.
class QuickMaterialSurface final : public QObject
{
public:
    explicit QuickMaterialSurface(QQuickWindow* window) : QObject(window), m_window(window)
    {
        window->installEventFilter(this);
    }
    void prepare()
    {
        if (!m_window)
            return;
        if (m_window->format().alphaBufferSize() > 0)
            reserved = true;
        if (m_window->handle())
            return;
        auto format = m_window->requestedFormat();
        if (format.alphaBufferSize() < 8)
        {
            format.setAlphaBufferSize(8);
            m_window->setFormat(format);
        }
        reserved = true;
    }
    bool available()
    {
        if (!m_window || !m_window->handle())
            return false;
        if (m_window->format().alphaBufferSize() > 0)
            confirmed = true;
        return reserved && confirmed;
    }
    void invalidate()
    {
        confirmed = false;
    }

protected:
    bool eventFilter(QObject*, QEvent* event) override
    {
        if (event->type() == QEvent::PlatformSurface &&
            static_cast<QPlatformSurfaceEvent*>(event)->surfaceEventType() ==
                QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed)
            invalidate();
        return false;
    }

private:
    QPointer<QQuickWindow> m_window;
    bool reserved  = false;
    bool confirmed = false;
};

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
        if (m_surface && m_surface->available())
            return true;
        return HostAdapter::materialSurfaceAvailable(error);
    }
    void materialSurfaceDestroyed() override
    {
        if (m_surface)
            m_surface->invalidate();
    }
    void prepareMaterialSurface() override
    {
        if (!m_window)
            return;
        for (auto* child : m_window->children())
        {
            if (auto* surface = dynamic_cast<QuickMaterialSurface*>(child))
            {
                m_surface = surface;
                break;
            }
        }
        if (!m_surface)
            m_surface = new QuickMaterialSurface(m_window);
        m_surface->prepare();
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
        const QPointer<QQuickWindow> window = m_window;
        const auto* quickItem               = qobject_cast<const QQuickItem*>(item);
        if (!quickItem || !window || quickItem->window() != window)
            return false;
        QList<QPointer<const QQuickItem>> path;
        for (auto* ancestor = quickItem; ancestor; ancestor = ancestor->parentItem())
            path.append(ancestor);
        // contains() can invoke application masks. Everything used after that
        // callback is a local guarded snapshot, even if the adapter is destroyed.
        const auto valid = [&]
        {
            if (!window)
                return false;
            for (qsizetype i = 0; i < path.size(); ++i)
            {
                if (!path[i] || !path[i]->isVisible() || path[i]->window() != window ||
                    path[i]->parentItem() != (i + 1 < path.size() ? path[i + 1].data() : nullptr))
                    return false;
            }
            return true;
        };
        if (!valid())
            return false;
        for (qsizetype i = 0; i < path.size(); ++i)
        {
            if (i == 0 || path[i]->clip())
            {
                const bool inside = path[i]->contains(path[i]->mapFromScene(point));
                if (!valid() || !inside)
                    return false;
            }
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
    QPointer<QuickMaterialSurface> m_surface;
    QColor m_originalColor;
    bool m_background = false;
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
    Material material           = Material::Solid;
    Material notifiedEffective  = Material::Solid;
    quint64 bindingRevision     = 0;
    quint64 titleRevision       = 0;
    quint64 buttonRevision      = 0;
    quint64 interactiveRevision = 0;
    bool complete               = true;
    bool destroying             = false;
    bool bindingBusy            = false;
    bool bindingPending         = false;

    void finishBinding(WindowAgent* self)
    {
        bindingBusy = false;
        if (bindingPending && !destroying)
            self->setWindow(window);
    }

    void notifyEffective(WindowAgent* self)
    {
        const auto value = self->effectiveMaterial();
        if (value == notifiedEffective)
            return;
        notifiedEffective = value;
        emit self->effectiveMaterialChanged();
    }
    void updateInteractive(WindowAgent* self, QList<QPointer<QQuickItem>> candidates);
};

void WindowAgent::Private::updateInteractive(WindowAgent* self, QList<QPointer<QQuickItem>> candidates)
{
    if (destroying)
        return;
    const QPointer<WindowAgent> guard(self);
    const QPointer<Internal::WindowController> bound(controller.get());
    const quint64 binding  = bindingRevision;
    const quint64 revision = ++interactiveRevision;
    const auto valid       = [&]
    {
        return guard && !guard->d->destroying && guard->d->bindingRevision == binding &&
               guard->d->interactiveRevision == revision && guard->d->controller.get() == bound;
    };
    QList<QPointer<QQuickItem>> next;
    const auto previous = interactive;
    for (const auto& item : candidates)
    {
        if (!item || next.contains(item))
            continue;
        // Existing foreign registrations can always be removed; ownership is
        // checked only when adding an item, before changing either list.
        if (bound && !previous.contains(item))
        {
            const bool accepted = bound->accepts(item);
            if (!valid() || !accepted || !item)
                return;
        }
        next.append(item);
    }
    if (next == previous)
        return;
    if (bound)
    {
        for (const auto& item : previous)
            if (item && !next.contains(item))
            {
                bound->setInteractive(item, false);
                if (!valid())
                    return;
            }
        for (const auto& item : next)
            if (item && !previous.contains(item))
            {
                const bool accepted = bound->setInteractive(item, true);
                if (!valid() || !accepted)
                    return;
            }
    }
    for (const auto& item : previous)
        if (item && !next.contains(item))
            QObject::disconnect(interactiveDestroyed.take(item));
    interactive = std::move(next);
    for (const auto& item : std::as_const(interactive))
        if (item && !interactiveDestroyed.contains(item))
        {
            auto* key = item.data();
            interactiveDestroyed.insert(key, QObject::connect(key, &QObject::destroyed, self,
                                                              [self, key]
                                                              {
                                                                  self->d->interactiveDestroyed.remove(key);
                                                                  if (self->d->interactive.removeAll(nullptr))
                                                                  {
                                                                      ++self->d->interactiveRevision;
                                                                      if (!self->d->destroying)
                                                                          emit self->interactiveItemsChanged();
                                                                  }
                                                              }));
        }
    // One atomic list mutation, then one callback boundary. Nothing accesses
    // this Private or the list again after the notification.
    emit self->interactiveItemsChanged();
}

WindowAgent::WindowAgent(QObject* parent) : QObject(parent), d(std::make_unique<Private>()) {}
WindowAgent::~WindowAgent()
{
    d->destroying = true;
    ++d->bindingRevision;
    disconnect(d->windowDestroyed);
    disconnect(d->titleDestroyed);
    disconnect(d->buttonDestroyed);
    for (const auto& connection : std::as_const(d->interactiveDestroyed))
        disconnect(connection);
    auto retired = std::move(d->controller);
    if (retired)
        disconnect(retired.get(), nullptr, this, nullptr);
    retired.reset();
}

void WindowAgent::classBegin()
{
    if (d->destroying)
        return;
    d->complete = false;
}

void WindowAgent::componentComplete()
{
    if (d->destroying)
        return;
    d->complete = true;
    bindWindow();
}

QQuickWindow* WindowAgent::window() const
{
    return d->window;
}

void WindowAgent::setWindow(QQuickWindow* value)
{
    if (!Internal::checkGuiThread("WindowAgent::setWindow") || d->destroying ||
        (!d->bindingPending && d->window == value && (value || (!d->controller && !d->windowDestroyed))))
        return;
    if (d->bindingBusy)
    {
        d->window = value;
        ++d->bindingRevision;
        d->bindingPending = true;
        return;
    }
    const QPointer<WindowAgent> guard(this);
    d->bindingBusy    = true;
    d->bindingPending = false;
    const auto finish = qScopeGuard(
        [guard]
        {
            if (guard)
                guard->d->finishBinding(guard);
        });
    const QPointer<QQuickWindow> target(value);
    const quint64 revision = ++d->bindingRevision;
    disconnect(d->windowDestroyed);
    d->windowDestroyed = {};
    d->window          = target;
    // Detach ownership first: restoring the old host color can synchronously
    // delete this Agent or request a different window.
    auto retired       = std::move(d->controller);
    if (retired)
        disconnect(retired.get(), nullptr, this, nullptr);
    retired.reset();
    const auto valid = [&] { return guard && !guard->d->destroying && guard->d->bindingRevision == revision; };
    if (!valid())
        return;
    if (d->window)
    {
        d->windowDestroyed = connect(d->window, &QObject::destroyed, this,
                                     [this, revision]
                                     {
                                         if (!d->destroying && d->bindingRevision == revision)
                                             setWindow(nullptr);
                                     });
    }
    d->notifyEffective(this);
    if (!valid())
        return;
    if (d->complete)
        bindWindow();
    if (valid())
        emit windowChanged();
}

void WindowAgent::bindWindow()
{
    if (d->destroying || !d->window || d->controller)
        return;
    const QPointer<WindowAgent> guard(this);
    const bool ownsOperation = !d->bindingBusy;
    d->bindingBusy           = true;
    const auto finish        = qScopeGuard(
        [guard, ownsOperation]
        {
            if (guard && ownsOperation)
                guard->d->finishBinding(guard);
        });
    const QPointer<QQuickWindow> target(d->window);
    const quint64 revision = d->bindingRevision;
    auto created = std::make_unique<Internal::WindowController>(std::make_unique<QuickHostAdapter>(target), true);
    if (!guard || d->destroying || d->bindingRevision != revision || !target || d->window != target)
        return;
    d->controller = std::move(created);
    const QPointer<Internal::WindowController> controller(d->controller.get());
    const auto valid = [&]
    {
        return guard && !guard->d->destroying && guard->d->bindingRevision == revision && controller &&
               guard->d->controller.get() == controller && target && guard->d->window == target;
    };
    connect(d->controller.get(), &Internal::WindowController::effectiveMaterialChanged, this,
            [this, controller, revision]
            {
                if (!d->destroying && d->bindingRevision == revision && d->controller.get() == controller)
                    d->notifyEffective(this);
            });
    connect(d->controller.get(), &Internal::WindowController::errorOccurred, this, &WindowAgent::errorOccurred);
    if (!d->controller->isBound())
        return;
    const auto title         = d->titleBar;
    const bool titleAccepted = controller->setTitleBar(title);
    if (!valid())
        return;
    if (!titleAccepted && title && d->titleBar == title)
    {
        setTitleBar(nullptr);
        if (!valid())
            return;
    }
    const auto button         = d->maximizeButton;
    const bool buttonAccepted = controller->setMaximizeButton(button);
    if (!valid())
        return;
    if (!buttonAccepted && button && d->maximizeButton == button)
    {
        setMaximizeButton(nullptr);
        if (!valid())
            return;
    }
    const auto items = d->interactive;
    bool pruned      = false;
    for (const auto& item : items)
    {
        if (!item || !d->interactive.contains(item))
            continue;
        const bool accepted = controller->setInteractive(item, true);
        if (!valid())
            return;
        if (!accepted && d->interactive.removeAll(item))
        {
            disconnect(d->interactiveDestroyed.take(item));
            ++d->interactiveRevision;
            pruned = true;
        }
    }
    if (pruned)
    {
        emit interactiveItemsChanged();
        if (!valid())
            return;
    }
    controller->setMaterial(coreMaterial(d->material));
    if (valid())
        d->notifyEffective(this);
}

QQuickItem* WindowAgent::titleBar() const
{
    return d->titleBar;
}

void WindowAgent::setTitleBar(QQuickItem* item)
{
    if (!Internal::checkGuiThread("WindowAgent::setTitleBar") || d->destroying || d->titleBar == item)
        return;
    const QPointer<WindowAgent> guard(this);
    const QPointer<QQuickItem> candidate(item);
    const quint64 binding  = d->bindingRevision;
    const quint64 revision = ++d->titleRevision;
    const bool accepted    = !d->complete || !d->controller || d->controller->setTitleBar(item);
    if (!guard || d->destroying || d->bindingRevision != binding || d->titleRevision != revision || !accepted ||
        (item && !candidate))
        return;
    disconnect(d->titleDestroyed);
    d->titleBar = item;
    if (item)
        d->titleDestroyed = connect(item, &QObject::destroyed, this,
                                    [this]
                                    {
                                        ++d->titleRevision;
                                        if (!d->destroying)
                                            emit titleBarChanged();
                                    });
    emit titleBarChanged();
}

QQuickItem* WindowAgent::maximizeButton() const
{
    return d->maximizeButton;
}

void WindowAgent::setMaximizeButton(QQuickItem* item)
{
    const QPointer<WindowAgent> guard(this);
    if (!Internal::checkGuiThread("WindowAgent::setMaximizeButton") || d->destroying || d->maximizeButton == item)
        return;
    const QPointer<QQuickItem> candidate(item);
    const quint64 binding  = d->bindingRevision;
    const quint64 revision = ++d->buttonRevision;
    const bool accepted    = !d->complete || !d->controller || d->controller->setMaximizeButton(item);
    if (!guard || d->destroying || d->bindingRevision != binding || d->buttonRevision != revision || !accepted ||
        (item && !candidate))
        return;
    disconnect(d->buttonDestroyed);
    d->maximizeButton = item;
    if (item)
        d->buttonDestroyed = connect(item, &QObject::destroyed, this,
                                     [this]
                                     {
                                         ++d->buttonRevision;
                                         if (!d->destroying)
                                             emit maximizeButtonChanged();
                                     });
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
    if (!Internal::checkGuiThread("WindowAgent::interactiveItems") || self->d->destroying || !item)
        return;
    if (self->d->interactive.contains(item))
        return;
    auto entries = self->d->interactive;
    entries.append(item);
    self->d->updateInteractive(self, std::move(entries));
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
    if (!Internal::checkGuiThread("WindowAgent::interactiveItems") || self->d->destroying)
        return;
    self->d->updateInteractive(self, {});
}

void WindowAgent::replaceInteractiveItem(QQmlListProperty<QQuickItem>* list, qsizetype index, QQuickItem* item)
{
    auto* self = static_cast<WindowAgent*>(list->object);
    if (!Internal::checkGuiThread("WindowAgent::interactiveItems") || self->d->destroying || !item || index < 0 ||
        index >= self->d->interactive.size())
        return;
    auto entries   = self->d->interactive;
    entries[index] = item;
    self->d->updateInteractive(self, std::move(entries));
}

void WindowAgent::removeLastInteractiveItem(QQmlListProperty<QQuickItem>* list)
{
    auto* self = static_cast<WindowAgent*>(list->object);
    if (!Internal::checkGuiThread("WindowAgent::interactiveItems") || self->d->destroying ||
        self->d->interactive.isEmpty())
        return;
    auto entries = self->d->interactive;
    entries.removeLast();
    self->d->updateInteractive(self, std::move(entries));
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
    if (!Internal::checkGuiThread("WindowAgent::setMaterial") || d->destroying || d->material == value)
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

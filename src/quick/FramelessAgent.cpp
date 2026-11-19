#include "../core/Runtime_p.h"
#include "../core/WindowController.h"

#include <WindowHeler/FramelessAgent.h>
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

#ifdef Q_OS_WIN
#include <QOperatingSystemVersion>
#include <QSettings>
#include <QSGSimpleRectNode>
#include <QStyleHints>
#include <limits>
#include <qt_windows.h>
#endif

namespace WindowHeler
{
namespace
{
#ifdef Q_OS_WIN
class QuickTopBorder final : public QQuickItem
{
public:
    static bool needed()
    {
        return QGuiApplication::platformName() == QStringLiteral("windows") &&
               QOperatingSystemVersion::current().microVersion() < 22000;
    }

    explicit QuickTopBorder(QQuickWindow* host) : QQuickItem(host->contentItem())
    {
        setObjectName(QStringLiteral("_windowheler_quickTopBorder"));
        setFlag(ItemHasContents);
        setAcceptedMouseButtons(Qt::NoButton);
        setZ(std::numeric_limits<qreal>::max());
        connect(parentItem(), &QQuickItem::widthChanged, this, &QuickTopBorder::updateGeometry);
        connect(host, &QWindow::windowStateChanged, this, &QuickTopBorder::updateGeometry);
        connect(host, &QWindow::activeChanged, this, &QuickTopBorder::updateColor);
        connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, &QuickTopBorder::updateColor);
        qApp->installEventFilter(this);
        updateGeometry();
        updateColor();
    }

protected:
    QSGNode* updatePaintNode(QSGNode* old, UpdatePaintNodeData*) override
    {
        // Quick's swapchain replaces the top client pixel after a GDI draw. Put
        // the edge in the same scene graph as the rest of the window instead.
        auto* node = old ? static_cast<QSGSimpleRectNode*>(old) : new QSGSimpleRectNode;
        node->setRect(boundingRect());
        node->setColor(m_color);
        return node;
    }

    void itemChange(ItemChange change, const ItemChangeData& data) override
    {
        QQuickItem::itemChange(change, data);
        if (change == ItemDevicePixelRatioHasChanged)
            setHeight(1.0 / data.realValue);
    }

    bool eventFilter(QObject* object, QEvent* event) override
    {
        if ((object == qApp || object == window()) &&
            (event->type() == QEvent::ThemeChange || event->type() == QEvent::ApplicationPaletteChange ||
             event->type() == QEvent::PaletteChange))
            updateColor();
        return false;
    }

private:
    void updateGeometry()
    {
        if (auto* host = window())
        {
            setVisible(!(host->windowStates() & (Qt::WindowMaximized | Qt::WindowFullScreen | Qt::WindowMinimized)));
            setWidth(host->contentItem()->width());
            setHeight(1.0 / host->devicePixelRatio());
        }
    }

    void updateColor()
    {
        if (!window())
            return;
        const bool active = window()->isActive();
        const bool dark = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
        // Win10's neutral frame tones for light/dark and focused/unfocused windows.
        QColor color = dark ? (active ? QColor(51, 51, 51) : QColor(61, 61, 62))
                            : (active ? QColor(110, 110, 110) : QColor(167, 167, 167));
        HIGHCONTRASTW contrast{sizeof(contrast), 0, nullptr};
        if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) &&
            (contrast.dwFlags & HCF_HIGHCONTRASTON))
        {
            const COLORREF border = GetSysColor(active ? COLOR_ACTIVEBORDER : COLOR_INACTIVEBORDER);
            color = QColor(GetRValue(border), GetGValue(border), GetBValue(border));
        }
        else if (active)
        {
            const QSettings dwm(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\DWM"),
                                QSettings::NativeFormat);
            bool ok = false;
            const uint accent = dwm.value(QStringLiteral("AccentColor")).toUInt(&ok);
            if (ok && dwm.value(QStringLiteral("ColorPrevalence")).toBool())
                color = QColor(GetRValue(accent), GetGValue(accent), GetBValue(accent));
        }
        if (m_color != color)
        {
            m_color = color;
            update();
        }
    }

    QColor m_color;
};
#endif

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

} // namespace
class FramelessAgent::Private
{
public:
    QPointer<QQuickWindow> window;
#ifdef Q_OS_WIN
    QPointer<QuickTopBorder> topBorder;
#endif
    QPointer<QQuickItem> titleBar;
    QPointer<QQuickItem> maximizeButton;
    QList<QPointer<QQuickItem>> interactive;
    QHash<QQuickItem*, QMetaObject::Connection> interactiveDestroyed;
    QMetaObject::Connection windowDestroyed;
    QMetaObject::Connection titleDestroyed;
    QMetaObject::Connection buttonDestroyed;
    std::shared_ptr<Internal::WindowController> controller;
    quint64 bindingRevision     = 0;
    quint64 titleRevision       = 0;
    quint64 buttonRevision      = 0;
    quint64 interactiveRevision = 0;
    bool complete               = true;
    bool destroying             = false;
    bool bindingBusy            = false;
    bool bindingPending         = false;

    void finishBinding(FramelessAgent* self)
    {
        bindingBusy = false;
        if (bindingPending && !destroying)
            self->setWindow(window);
    }

    void updateInteractive(FramelessAgent* self, QList<QPointer<QQuickItem>> candidates);
};

void FramelessAgent::Private::updateInteractive(FramelessAgent* self, QList<QPointer<QQuickItem>> candidates)
{
    if (destroying)
        return;
    const QPointer<FramelessAgent> guard(self);
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

FramelessAgent::FramelessAgent(QObject* parent) : QObject(parent), d(std::make_unique<Private>()) {}
FramelessAgent::~FramelessAgent()
{
    d->destroying = true;
    ++d->bindingRevision;
    disconnect(d->windowDestroyed);
    disconnect(d->titleDestroyed);
    disconnect(d->buttonDestroyed);
    for (const auto& connection : std::as_const(d->interactiveDestroyed))
        disconnect(connection);
#ifdef Q_OS_WIN
    delete d->topBorder.data();
#endif
    auto retired = std::move(d->controller);
    if (retired)
        disconnect(retired.get(), nullptr, this, nullptr);
    retired.reset();
}

void FramelessAgent::classBegin()
{
    if (d->destroying)
        return;
    d->complete = false;
}

void FramelessAgent::componentComplete()
{
    if (d->destroying)
        return;
    d->complete = true;
    bindWindow();
}

QQuickWindow* FramelessAgent::window() const
{
    return d->window;
}

void FramelessAgent::setWindow(QQuickWindow* value)
{
    if (!Internal::checkGuiThread("FramelessAgent::setWindow") || d->destroying ||
        (!d->bindingPending && d->window == value && (value || (!d->controller && !d->windowDestroyed))))
        return;
    if (d->bindingBusy)
    {
        d->window = value;
        ++d->bindingRevision;
        d->bindingPending = true;
        return;
    }
    const QPointer<FramelessAgent> guard(this);
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
#ifdef Q_OS_WIN
    auto oldBorder = std::exchange(d->topBorder, {});
    delete oldBorder.data();
#endif
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
    if (!valid())
        return;
    if (d->complete)
        bindWindow();
    if (valid())
        emit windowChanged();
}

void FramelessAgent::bindWindow()
{
    if (d->destroying || !d->window || d->controller)
        return;
    const QPointer<FramelessAgent> guard(this);
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
    auto created = Internal::WindowController::acquire(std::make_unique<QuickHostAdapter>(target), true,
                                                       Internal::Capability::Frameless);
    if (!guard || d->destroying || d->bindingRevision != revision || !target || d->window != target)
        return;
    d->controller = std::move(created);
    const QPointer<Internal::WindowController> controller(d->controller.get());
    const auto valid = [&]
    {
        return guard && !guard->d->destroying && guard->d->bindingRevision == revision && controller &&
               guard->d->controller.get() == controller && target && guard->d->window == target;
    };
    connect(d->controller.get(), &Internal::WindowController::errorOccurred, this, &FramelessAgent::errorOccurred);
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
#ifdef Q_OS_WIN
    if (QuickTopBorder::needed())
    {
        const QPointer<QuickTopBorder> border = new QuickTopBorder(target);
        if (valid())
            d->topBorder = border;
        else
            delete border.data();
    }
#endif
}

QQuickItem* FramelessAgent::titleBar() const
{
    return d->titleBar;
}

void FramelessAgent::setTitleBar(QQuickItem* item)
{
    if (!Internal::checkGuiThread("FramelessAgent::setTitleBar") || d->destroying || d->titleBar == item)
        return;
    const QPointer<FramelessAgent> guard(this);
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

QQuickItem* FramelessAgent::maximizeButton() const
{
    return d->maximizeButton;
}

void FramelessAgent::setMaximizeButton(QQuickItem* item)
{
    const QPointer<FramelessAgent> guard(this);
    if (!Internal::checkGuiThread("FramelessAgent::setMaximizeButton") || d->destroying || d->maximizeButton == item)
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

QQmlListProperty<QQuickItem> FramelessAgent::interactiveItems()
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

void FramelessAgent::appendInteractiveItem(QQmlListProperty<QQuickItem>* list, QQuickItem* item)
{
    auto* self = static_cast<FramelessAgent*>(list->object);
    if (!Internal::checkGuiThread("FramelessAgent::interactiveItems") || self->d->destroying || !item)
        return;
    if (self->d->interactive.contains(item))
        return;
    auto entries = self->d->interactive;
    entries.append(item);
    self->d->updateInteractive(self, std::move(entries));
}

qsizetype FramelessAgent::interactiveCount(QQmlListProperty<QQuickItem>* list)
{
    return static_cast<FramelessAgent*>(list->object)->d->interactive.size();
}

QQuickItem* FramelessAgent::interactiveAt(QQmlListProperty<QQuickItem>* list, qsizetype index)
{
    return static_cast<FramelessAgent*>(list->object)->d->interactive.value(index);
}

void FramelessAgent::clearInteractiveItems(QQmlListProperty<QQuickItem>* list)
{
    auto* self = static_cast<FramelessAgent*>(list->object);
    if (!Internal::checkGuiThread("FramelessAgent::interactiveItems") || self->d->destroying)
        return;
    self->d->updateInteractive(self, {});
}

void FramelessAgent::replaceInteractiveItem(QQmlListProperty<QQuickItem>* list, qsizetype index, QQuickItem* item)
{
    auto* self = static_cast<FramelessAgent*>(list->object);
    if (!Internal::checkGuiThread("FramelessAgent::interactiveItems") || self->d->destroying || !item || index < 0 ||
        index >= self->d->interactive.size())
        return;
    auto entries   = self->d->interactive;
    entries[index] = item;
    self->d->updateInteractive(self, std::move(entries));
}

void FramelessAgent::removeLastInteractiveItem(QQmlListProperty<QQuickItem>* list)
{
    auto* self = static_cast<FramelessAgent*>(list->object);
    if (!Internal::checkGuiThread("FramelessAgent::interactiveItems") || self->d->destroying ||
        self->d->interactive.isEmpty())
        return;
    auto entries = self->d->interactive;
    entries.removeLast();
    self->d->updateInteractive(self, std::move(entries));
}

} // namespace WindowHeler

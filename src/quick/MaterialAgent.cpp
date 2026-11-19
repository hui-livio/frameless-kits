#include "../core/Runtime_p.h"
#include "../core/WindowController.h"

#include <WindowHeler/MaterialAgent.h>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QPlatformSurfaceEvent>
#include <QPointer>
#include <QPalette>
#include <QQuickItem>
#include <QQuickWindow>
#include <utility>

namespace WindowHeler
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

} // namespace
class MaterialAgent::Private
{
public:
    QPointer<QQuickWindow> window;
    std::shared_ptr<Internal::WindowController> controller;
    QMetaObject::Connection windowDestroyed;
    Material requested = Material::Solid;
    quint64 bindingRevision = 0;
    bool destroying = false;
};

MaterialAgent::MaterialAgent(QObject* parent) : QObject(parent), d(std::make_unique<Private>()) {}
MaterialAgent::~MaterialAgent()
{
    d->destroying = true;
    disconnect(d->windowDestroyed);
    if (d->controller)
        disconnect(d->controller.get(), nullptr, this, nullptr);
    d->controller.reset();
}
QQuickWindow* MaterialAgent::window() const
{
    return d->window;
}
void MaterialAgent::setWindow(QQuickWindow* value)
{
    if (!Internal::checkGuiThread("MaterialAgent::setWindow") || d->destroying ||
        (d->window == value && (value || !d->controller)))
        return;
    const QPointer<MaterialAgent> guard(this);
    const QPointer<QQuickWindow> target(value);
    const auto previous = effectiveMaterial();
    const auto revision = ++d->bindingRevision;
    const auto current = [&] { return guard && guard->d->bindingRevision == revision; };
    disconnect(d->windowDestroyed);
    d->window = target;
    auto retired = std::move(d->controller);
    if (retired)
        disconnect(retired.get(), nullptr, this, nullptr);
    retired.reset();
    if (!current())
        return;
    if (target)
    {
        auto controller = Internal::WindowController::acquire(std::make_unique<QuickHostAdapter>(target), true,
                                                               Internal::Capability::Material);
        if (!current() || !target)
            return;
        d->controller = std::move(controller);
        d->windowDestroyed = connect(target, &QObject::destroyed, this, [this] { setWindow(nullptr); });
        connect(d->controller.get(), &Internal::WindowController::errorOccurred, this, &MaterialAgent::errorOccurred);
        d->controller->setMaterial(d->requested);
        if (!current())
            return;
        connect(d->controller.get(), &Internal::WindowController::effectiveMaterialChanged, this,
                &MaterialAgent::effectiveMaterialChanged);
    }
    if (previous != effectiveMaterial())
        emit effectiveMaterialChanged();
    if (current())
        emit windowChanged();
}
MaterialAgent::Material MaterialAgent::material() const
{
    return d->requested;
}
MaterialAgent::Material MaterialAgent::effectiveMaterial() const
{
    return d->controller ? d->controller->effectiveMaterial() : Material::Solid;
}
void MaterialAgent::setMaterial(Material value)
{
    if (!Internal::checkGuiThread("MaterialAgent::setMaterial") || d->destroying)
        return;
    if (int(value) < int(Material::Solid) || int(value) > int(Material::MicaAlt))
    {
        emit errorOccurred(QStringLiteral("Invalid material value."));
        return;
    }
    const QPointer<MaterialAgent> guard(this);
    const auto previous = std::exchange(d->requested, value);
    if (d->controller)
        d->controller->setMaterial(value);
    if (guard && previous != value && d->requested == value)
        emit materialChanged();
}
} // namespace WindowHeler

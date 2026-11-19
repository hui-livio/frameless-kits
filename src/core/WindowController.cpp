#include "WindowController.h"

#include "Runtime_p.h"

#include <QHash>
#include <QLoggingCategory>
#include <QPointer>
#include <QTimer>
#include <utility>

Q_LOGGING_CATEGORY(fkWindow, "windowheler.window")
namespace WindowHeler::Internal
{
namespace
{
QHash<QObject*, WindowController*>& bindings()
{
    static QHash<QObject*, WindowController*> value;
    return value;
}
} // namespace
WindowController::WindowController(std::unique_ptr<HostAdapter> host, bool quick, QObject* parent)
    : QObject(parent), m_host(std::move(host))
{
    if (!checkGuiThread("WindowController"))
        return;
    if (!isInitialized(quick))
    {
        report(QStringLiteral("Call WindowHeler::initialize with the required "
                              "frontend before creating an Agent."),
               true);
        return;
    }
    QObject* object = m_host ? m_host->host() : nullptr;
    if (!object)
    {
        report(QStringLiteral("Agent requires a valid top-level window."), true);
        return;
    }
    if (bindings().contains(object))
    {
        report(QStringLiteral("This window already has an Agent; the original "
                              "binding is unchanged."),
               true);
        return;
    }
    bindings().insert(object, this);
    m_bindingKey              = object;
    m_hostDestroyedConnection = connect(object, &QObject::destroyed, this,
                                        [this]
                                        {
                                            removeBinding();
                                            m_backend.reset();
                                        });
    m_bound                   = true;
    m_host->prepareMaterialSurface();
    m_backend = createPlatformBackend(*m_host, m_hitTest, [this](const QString& error) { report(error, true); });
    m_backend->setMaterialRefresh([this] { refreshMaterial(); });
}
WindowController::~WindowController()
{
    // Keep the registry reservation until native/background cleanup finishes.
    // Otherwise a different Agent could bind here and have its state overwritten
    // by the remainder of this destructor.
    m_bound = false;
    m_hitTest.invalidateOwner();
    disconnect(m_hostDestroyedConnection);
    for (const auto& connection : std::as_const(m_registrationConnections))
        disconnect(connection);
    m_registrationConnections.clear();
    m_backend.reset();
    // Keep the model alive while host cleanup can still invoke application callbacks.
    m_host.reset();
    removeBinding();
}
void WindowController::removeBinding()
{
    const auto it = bindings().find(m_bindingKey);
    if (it != bindings().end() && it.value() == this)
        bindings().erase(it);
    m_bindingKey = nullptr;
    m_bound      = false;
    m_hitTest.invalidateOwner();
}
void WindowController::trackRegistration(QObject* item)
{
    if (item && !m_registrationConnections.contains(item))
        m_registrationConnections.insert(
            item, connect(item, &QObject::destroyed, this, [this, item] { removeDestroyedRegistration(item); }));
}
void WindowController::untrackRegistration(QObject* item)
{
    if (item && item != m_titleBarKey && item != m_maximizeButtonKey && !m_hitTest.interactiveItems.contains(item))
        disconnect(m_registrationConnections.take(item));
}
void WindowController::removeDestroyedRegistration(QObject* identity)
{
    disconnect(m_registrationConnections.take(identity));
    if (m_titleBarKey == identity)
    {
        m_titleBarKey = nullptr;
        m_hitTest.titleBar.clear();
    }
    if (m_maximizeButtonKey == identity)
    {
        m_maximizeButtonKey = nullptr;
        m_hitTest.maximizeButton.clear();
    }
    // QPointer is already null when destroyed is delivered; prune those entries too.
    m_hitTest.interactiveItems.removeIf([identity](const auto& item) { return !item || item.data() == identity; });
    ++m_hitTest.revision;
}
void WindowController::report(const QString& message, bool queued)
{
    if (m_lastError == message)
        return;
    m_lastError = message;
    qCWarning(fkWindow).noquote() << message;
    if (queued)
        QTimer::singleShot(0, this, [this, message] { emit errorOccurred(message); });
    else
        emit errorOccurred(message);
}
bool WindowController::accepts(QObject* item)
{
    if (!checkGuiThread("Agent configuration") || !m_bound)
        return false;
    if (item && !m_host->owns(item))
    {
        report(QStringLiteral("Registered objects must belong to the bound window."));
        return false;
    }
    return true;
}
bool WindowController::setTitleBar(QObject* item)
{
    if (!accepts(item))
        return false;
    if (m_titleBarKey == item)
        return true;
    QObject* previous  = m_titleBarKey;
    m_titleBarKey      = item;
    m_hitTest.titleBar = item;
    ++m_hitTest.revision;
    untrackRegistration(previous);
    trackRegistration(item);
    return true;
}
bool WindowController::setInteractive(QObject* item, bool enabled)
{
    // Removing a registration remains valid after it has moved to another window.
    if (!item || !accepts(enabled ? item : nullptr))
        return false;
    if (enabled && !m_hitTest.interactiveItems.contains(item))
    {
        m_hitTest.interactiveItems.append(item);
        ++m_hitTest.revision;
        trackRegistration(item);
    }
    else if (!enabled && m_hitTest.interactiveItems.removeAll(item))
    {
        ++m_hitTest.revision;
        untrackRegistration(item);
    }
    return true;
}
bool WindowController::setMaximizeButton(QObject* item)
{
    if (!accepts(item))
        return false;
    if (item && !m_host->supportsMaximizeButton(item))
    {
        report(QStringLiteral("Maximize button must be a QAbstractButton or a Qt Quick Controls AbstractButton."));
        return false;
    }
    const QPointer<WindowController> guard(this);
    const QPointer<QObject> candidate(item);
    const QPointer<QObject> hostObject(m_host->host());
    const QPointer<PlatformBackend> backend(m_backend.get());
    const QPointer<QObject> previous(m_hitTest.maximizeButton);
    if (!backend || !hostObject)
        return false;
    // Reserve this mutation before cancellation; a nested mutation must win, even if
    // it ultimately restores the same button identity.
    const quint64 revision = ++m_hitTest.revision;
    backend->resetInput();
    if (!guard || !hostObject || !backend || (item && !candidate))
        return false;
    if (!m_bound || m_backend.get() != backend || m_hitTest.revision != revision ||
        m_hitTest.maximizeButton != previous || m_host->host() != hostObject)
        return false;
    if (candidate && !m_host->owns(candidate))
        return false;
    QObject* previousKey     = m_maximizeButtonKey;
    m_maximizeButtonKey      = item;
    m_hitTest.maximizeButton = item;
    ++m_hitTest.revision;
    untrackRegistration(previousKey);
    trackRegistration(item);
    return true;
}
void WindowController::setMaterial(Material material)
{
    if (!checkGuiThread("Agent material configuration") || !m_bound)
        return;
    if (int(material) < int(Material::Solid) || int(material) > int(Material::MicaAlt))
    {
        report(QStringLiteral("Invalid material value."));
        return;
    }
    m_material.requested = material;
    refreshMaterial();
}
void WindowController::refreshMaterial()
{
    if (!m_bound || !m_backend || m_applyingMaterial)
        return;
    m_applyingMaterial = true;
    const QPointer<WindowController> guard(this);
    const auto previous  = m_material.applied;
    const auto requested = m_material.requested;
    QString error;
    const bool ok = m_material.apply(*m_backend, *m_host, error);
    if (!guard)
        return;
    m_applyingMaterial = false;
    if (!ok)
        report(error, true);
    else
        m_lastError.clear();
    // A synchronous host color notification can change the request while applying.
    // Replay it outside that notification rather than dropping it or recursing.
    if (requested != m_material.requested)
        QTimer::singleShot(0, this, [this] { refreshMaterial(); });
    if (previous != m_material.applied)
        emit effectiveMaterialChanged();
}
HitRegion WindowController::hitTest(QPointF point) const
{
    if (!checkGuiThread("Agent hit test") || !m_bound)
        return HitRegion::Client;
    return m_hitTest.hitTest(*m_host, point);
}
} // namespace WindowHeler::Internal

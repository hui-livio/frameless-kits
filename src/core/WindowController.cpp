#include "WindowController.h"

#include "Runtime_p.h"

#include <QHash>
#include <QLoggingCategory>
#include <QTimer>

Q_LOGGING_CATEGORY(fkWindow, "framelesskit.window")
namespace FramelessKit::Internal
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
        report(QStringLiteral("Call FramelessKit::initialize with the required "
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
    connect(object, &QObject::destroyed, this,
            [this, object]
            {
                bindings().remove(object);
                m_bound = false;
                m_backend.reset();
            });
    m_bound   = true;
    m_backend = createPlatformBackend(*m_host, m_hitTest, [this](const QString& error) { report(error, true); });
}
WindowController::~WindowController()
{
    m_backend.reset();
    if (m_bound && m_host && bindings().value(m_host->host()) == this)
        bindings().remove(m_host->host());
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
    m_hitTest.titleBar = item;
    return true;
}
bool WindowController::setInteractive(QObject* item, bool enabled)
{
    if (!accepts(item) || !item)
        return false;
    if (enabled && !m_hitTest.interactiveItems.contains(item))
        m_hitTest.interactiveItems.append(item);
    else if (!enabled)
        m_hitTest.interactiveItems.removeAll(item);
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
    m_backend->resetInput();
    m_hitTest.maximizeButton = item;
    return true;
}
void WindowController::setMaterial(Material material)
{
    if (!checkGuiThread("Agent material configuration") || !m_bound || m_material.requested == material)
        return;
    if (int(material) < int(Material::Solid) || int(material) > int(Material::MicaAlt))
    {
        report(QStringLiteral("Invalid material value."));
        return;
    }
    const auto previous = m_material.applied;
    QString error;
    if (!m_material.apply(*m_backend, material, error))
        report(error);
    else
        m_lastError.clear();
    if (previous != m_material.applied)
        emit effectiveMaterialChanged();
}
HitRegion WindowController::hitTest(QPointF point) const
{
    if (!checkGuiThread("Agent hit test") || !m_bound)
        return HitRegion::Client;
    return m_hitTest.hitTest(*m_host, point);
}
} // namespace FramelessKit::Internal

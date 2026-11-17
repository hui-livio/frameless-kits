#pragma once
#include "HitTestModel_p.h"
#include "MaterialController_p.h"

#include <FramelessKit/CoreExport.h>
#include <memory>

namespace FramelessKit::Internal
{
// Private cross-library ABI, never installed as a public application header.
class FRAMELESSKIT_CORE_EXPORT WindowController final : public QObject
{
    Q_OBJECT
public:
    explicit WindowController(std::unique_ptr<HostAdapter> host, bool quick, QObject* parent = nullptr);
    ~WindowController() override;
    bool isBound() const
    {
        return m_bound;
    }
    bool accepts(QObject* item);
    bool setTitleBar(QObject* item);
    bool setInteractive(QObject* item, bool interactive);
    bool setMaximizeButton(QObject* item);
    void setMaterial(Material material);
    Material material() const
    {
        return m_material.requested;
    }
    Material effectiveMaterial() const
    {
        return m_material.applied;
    }
    HitRegion hitTest(QPointF point) const;
signals:
    void effectiveMaterialChanged();
    void errorOccurred(const QString& message);

private:
    void report(const QString& message, bool queued = false);
    std::unique_ptr<HostAdapter> m_host;
    std::unique_ptr<PlatformBackend> m_backend;
    HitTestModel m_hitTest;
    MaterialController m_material;
    QString m_lastError;
    bool m_bound = false;
};
} // namespace FramelessKit::Internal

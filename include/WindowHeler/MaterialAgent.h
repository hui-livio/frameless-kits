#pragma once
#include <QObject>
#include <QQuickWindow>
#include <QtQml/qqmlregistration.h>
#include <WindowHeler/Export.h>
#include <WindowHeler/Material.h>
#include <memory>

namespace WindowHeler
{
namespace Internal
{
struct WindowContextHandle;
}
// Not final: Qt derives QQmlElement from this type when creating it in QML.
class WINDOWHELER_MATERIAL_QUICK_EXPORT MaterialAgent : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QQuickWindow* window READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY(Material material READ material WRITE setMaterial NOTIFY materialChanged)
    Q_PROPERTY(Material effectiveMaterial READ effectiveMaterial NOTIFY effectiveMaterialChanged)
public:
    enum class Material
    {
        Solid,
        Transparent,
        Blur,
        Acrylic,
        Mica,
        MicaAlt
    };
    Q_ENUM(Material)
    explicit MaterialAgent(QObject* parent = nullptr);
    ~MaterialAgent() override;
    QQuickWindow* window() const;
    void setWindow(QQuickWindow* window);
    Material material() const;
    Material effectiveMaterial() const;
    void setMaterial(Material material);
signals:
    void windowChanged();
    void materialChanged();
    void effectiveMaterialChanged();
    void errorOccurred(const QString& message);

private:
    class Private;
    std::unique_ptr<Private> d;
};
} // namespace WindowHeler

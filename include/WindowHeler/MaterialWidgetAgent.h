#pragma once
#include <WindowHeler/Export.h>
#include <WindowHeler/Material.h>
#include <QObject>
#include <memory>

class QWidget;
namespace WindowHeler
{
/** Window material capability for QWidget hosts. */
class WINDOWHELER_MATERIAL_WIDGETS_EXPORT MaterialWidgetAgent final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(WindowHeler::Material material READ material WRITE setMaterial NOTIFY materialChanged)
    Q_PROPERTY(WindowHeler::Material effectiveMaterial READ effectiveMaterial NOTIFY effectiveMaterialChanged)
public:
    explicit MaterialWidgetAgent(QWidget* window);
    ~MaterialWidgetAgent() override;

    QWidget* window() const;
    Material material() const;
    Material effectiveMaterial() const;

public slots:
    void setMaterial(Material material);

signals:
    void materialChanged();
    void effectiveMaterialChanged();
    void errorOccurred(const QString& message);

private:
    class Private;
    std::unique_ptr<Private> d;
};
} // namespace WindowHeler

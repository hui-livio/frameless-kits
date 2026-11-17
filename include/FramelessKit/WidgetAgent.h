#pragma once
#include <FramelessKit/Material.h>
#include <FramelessKit/WidgetsExport.h>
#include <QObject>
#include <memory>

class QWidget;
class QAbstractButton;

namespace FramelessKit
{
class FRAMELESSKIT_WIDGETS_EXPORT WidgetAgent final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(FramelessKit::Material material READ material WRITE setMaterial NOTIFY materialChanged)
    Q_PROPERTY(FramelessKit::Material effectiveMaterial READ effectiveMaterial NOTIFY effectiveMaterialChanged)
public:
    explicit WidgetAgent(QWidget* window);
    ~WidgetAgent() override;
    QWidget* window() const;
    QWidget* titleBar() const;
    void setTitleBar(QWidget* titleBar);
    void setInteractive(QWidget* widget, bool interactive = true);
    QAbstractButton* maximizeButton() const;
    void setMaximizeButton(QAbstractButton* button);
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
} // namespace FramelessKit

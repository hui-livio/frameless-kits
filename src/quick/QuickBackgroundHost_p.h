#pragma once
#include "../core/HostAdapter_p.h"

#include <QGuiApplication>
#include <QPalette>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGRectangleNode>
#include <limits>

namespace WindowHeler::Internal
{
// Fill the ordinary window background below the DWM strip, not the strip itself.
class InsetQuickBackground final : public QQuickItem
{
public:
    explicit InsetQuickBackground(QQuickWindow* host) : QQuickItem(host->contentItem())
    {
        setFlag(ItemHasContents);
        setZ(std::numeric_limits<qreal>::lowest());
        auto* root  = parentItem();
        auto resize = [this, root] { setSize(root->size()); };
        connect(root, &QQuickItem::widthChanged, this, resize);
        connect(root, &QQuickItem::heightChanged, this, resize);
        resize();
    }

    void setFill(const QColor& color)
    {
        if (m_color != color)
        {
            m_color = color;
            update();
        }
    }

private:
    void geometryChange(const QRectF& geometry, const QRectF& oldGeometry) override
    {
        QQuickItem::geometryChange(geometry, oldGeometry);
        if (geometry.size() != oldGeometry.size())
            update();
    }

    void itemChange(ItemChange change, const ItemChangeData& data) override
    {
        QQuickItem::itemChange(change, data);
        if (change == ItemDevicePixelRatioHasChanged)
            update();
    }

    QSGNode* updatePaintNode(QSGNode* old, UpdatePaintNodeData*) override
    {
        auto* node        = old ? static_cast<QSGRectangleNode*>(old) : window()->createRectangleNode();
        const qreal inset = 1.0 / window()->effectiveDevicePixelRatio();
        node->setRect(0, inset, width(), qMax(qreal(0), height() - inset));
        node->setColor(m_color);
        return node;
    }
    QColor m_color;
};

// Either Quick agent may create the shared window controller first.
class QuickBackgroundHost : public HostAdapter
{
public:
    explicit QuickBackgroundHost(QQuickWindow* window) : m_window(window) {}
    ~QuickBackgroundHost() override
    {
        m_inset = m_materialBackground = false;
        synchronize();
    }
    QColor materialColor() const override
    {
        const QColor color = m_ownsClear ? m_originalColor : (m_window ? m_window->color() : QColor{});
        return color.isValid() && color.alpha() > 0 ? color : QGuiApplication::palette().color(QPalette::Window);
    }
    void setMaterialBackground(bool enabled) override
    {
        if (m_materialBackground != enabled)
        {
            m_materialBackground = enabled;
            synchronize();
        }
    }
    void setNativeTopInset(bool enabled) override
    {
        if (m_inset != enabled)
        {
            m_inset = enabled;
            synchronize();
        }
    }

protected:
    QPointer<QQuickWindow> m_window;

private:
    void synchronize()
    {
        if (!m_window)
            return;
        const bool ownsClear = m_inset || m_materialBackground;
        if (ownsClear && (!m_ownsClear || m_window->color() != QColor(Qt::transparent)))
            m_originalColor = m_window->color();
        const bool restore = m_ownsClear && !ownsClear && m_window->color() == QColor(Qt::transparent);
        m_ownsClear        = ownsClear;
        if (m_inset && !m_materialBackground)
        {
            if (!m_fill)
            {
                m_fill = new InsetQuickBackground(m_window);
                QObject::connect(m_window, &QQuickWindow::colorChanged, m_fill,
                                 [this](const QColor& color)
                                 {
                                     if (color != QColor(Qt::transparent))
                                     {
                                         m_originalColor = color;
                                         m_fill->setFill(color);
                                         m_window->setColor(Qt::transparent);
                                     }
                                 });
            }
            m_fill->setFill(m_originalColor);
        }
        else
            delete m_fill.data();
        if (ownsClear && m_window->color() != QColor(Qt::transparent))
            m_window->setColor(Qt::transparent);
        else if (restore)
            m_window->setColor(m_originalColor);
    }
    QPointer<InsetQuickBackground> m_fill;
    QColor m_originalColor;
    bool m_materialBackground = false;
    bool m_inset              = false;
    bool m_ownsClear          = false;
};
} // namespace WindowHeler::Internal

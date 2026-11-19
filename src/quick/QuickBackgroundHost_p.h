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
    explicit QuickBackgroundHost(QQuickWindow* window) : m_window(window)
    {
        if (m_window)
        {
            m_colorConnection = QObject::connect(m_window, &QQuickWindow::colorChanged, m_window,
                                                 [this](const QColor& color) { handleColorChanged(color); });
        }
    }
    ~QuickBackgroundHost() override
    {
        QObject::disconnect(m_colorConnection);
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
    void setWindowColor(const QColor& color)
    {
        if (!m_window)
            return;
        m_colorChangeGuard = true;
        m_window->setColor(color);
        m_colorChangeGuard = false;
        m_lastOwnedColor   = color;
    }
    void handleColorChanged(const QColor& color)
    {
        if (m_colorChangeGuard || !m_ownsClear || !m_window)
            return;
        m_applicationChangedColor = true;
        if (color == QColor(Qt::transparent))
            return;
        m_originalColor             = color;
        if (m_fill)
            m_fill->setFill(color);
        setWindowColor(Qt::transparent);
    }
    void synchronize()
    {
        if (!m_window)
            return;
        const bool ownsClear = m_inset || m_materialBackground;
        if (ownsClear && (!m_ownsClear || m_window->color() != QColor(Qt::transparent)))
            m_originalColor = m_window->color();
        if (ownsClear && !m_ownsClear)
            m_applicationChangedColor = false;
        const bool restore = m_ownsClear && !ownsClear && !m_applicationChangedColor &&
                             m_window->color() == m_lastOwnedColor;
        m_ownsClear        = ownsClear;
        if (m_inset && !m_materialBackground)
        {
            if (!m_fill)
            {
                m_fill = new InsetQuickBackground(m_window);
            }
            m_fill->setFill(m_originalColor);
        }
        else
            delete m_fill.data();
        if (ownsClear && m_window->color() != QColor(Qt::transparent))
            setWindowColor(Qt::transparent);
        else if (restore)
            setWindowColor(m_originalColor);
    }
    QPointer<InsetQuickBackground> m_fill;
    QMetaObject::Connection m_colorConnection;
    QColor m_originalColor;
    QColor m_lastOwnedColor;
    bool m_materialBackground = false;
    bool m_inset              = false;
    bool m_ownsClear          = false;
    bool m_colorChangeGuard   = false;
    bool m_applicationChangedColor = false;
};
} // namespace WindowHeler::Internal

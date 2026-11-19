#pragma once

#include <QEvent>
#include <QLayout>
#include <QList>
#include <QPointer>
#include <QSet>
#include <QWidget>
#include <utility>

namespace WindowHeler::Internal
{
// Included by WidgetAgent only on macOS; all operations use public Qt APIs.
class CocoaWidgetSafeArea final : public QObject
{
public:
    explicit CocoaWidgetSafeArea(QWidget* host) : m_host(host) {}
    ~CocoaWidgetSafeArea() override
    {
        m_synchronizing = true; // Suppress queued updates during reentrant teardown.
        clearObservers();
        const auto states = std::move(m_states);
        for (const auto& state : states)
            restoreAttribute(state);
    }

    void setTitleBar(QWidget* titleBar)
    {
        m_titleBar = titleBar;
        synchronize();
    }

protected:
    bool eventFilter(QObject* object, QEvent* event) override
    {
        switch (event->type())
        {
        case QEvent::Destroy:
            // QWidget sends Destroy before QObject clears its QPointers.
            // Never restore an attribute into a widget being torn down.
            m_states.removeIf([object](const WidgetState& state) { return state.widget == object; });
            if (m_host == object)
                m_host.clear();
            if (m_titleBar == object)
                m_titleBar.clear();
            [[fallthrough]];
        case QEvent::ChildAdded:
        case QEvent::ChildRemoved:
        case QEvent::ChildPolished:
        case QEvent::ParentChange:
        case QEvent::Show:
        case QEvent::Hide:
            scheduleSynchronize();
            break;
        default:
            break;
        }
        return false;
    }

private:
    struct WidgetState
    {
        QPointer<QWidget> widget;
        bool respectsSafeArea;
    };

    static void applyAttribute(QPointer<QWidget> widget, bool enabled)
    {
        if (!widget || widget->testAttribute(Qt::WA_ContentsMarginsRespectsSafeArea) == enabled)
            return;
        widget->setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea, enabled);
        if (!widget)
            return;
        if (auto* layout = widget->layout())
            layout->invalidate();
        if (!widget)
            return;
        widget->updateGeometry();
        if (widget)
            widget->update();
        // Layout activation stays queued; synchronous activation can reenter
        // application resize handlers while the tracked hierarchy is changing.
    }

    static void restoreAttribute(const WidgetState& state)
    {
        // Restore only our last applied value; a later application write wins.
        if (state.widget && !state.widget->testAttribute(Qt::WA_ContentsMarginsRespectsSafeArea))
            applyAttribute(state.widget, state.respectsSafeArea);
    }

    static void collectTitleBar(QWidget* widget, QSet<QWidget*>& managed, QSet<QWidget*>& observed)
    {
        managed.insert(widget);
        for (auto* child : widget->children())
        {
            auto* childWidget = qobject_cast<QWidget*>(child);
            if (!childWidget)
                continue;
            if (childWidget->isWindow())
                observed.insert(childWidget);
            else
                collectTitleBar(childWidget, managed, observed);
        }
    }

    void clearObservers()
    {
        for (const auto& widget : m_observed)
            if (widget)
                widget->removeEventFilter(this);
        m_observed.clear();
    }

    void scheduleSynchronize()
    {
        if (m_synchronizing)
        {
            m_resynchronize = true;
            return;
        }
        if (m_pending)
            return;
        m_pending = true;
        QMetaObject::invokeMethod(
            this,
            [this]
            {
                m_pending = false;
                synchronize();
            },
            Qt::QueuedConnection);
    }

    void synchronize()
    {
        if (m_synchronizing)
        {
            m_resynchronize = true;
            return;
        }
        m_synchronizing = true;
        m_resynchronize = false;
        const QPointer<CocoaWidgetSafeArea> guard(this);
        QSet<QWidget*> managed;
        QSet<QWidget*> observed;
        if (m_host)
        {
            observed.insert(m_host);
            if (m_host->isWindow())
            {
                managed.insert(m_host);
                if (m_titleBar && m_titleBar->window() == m_host)
                {
                    collectTitleBar(m_titleBar, managed, observed);
                    for (auto* parent = m_titleBar->parentWidget(); parent && parent != m_host;
                         parent       = parent->parentWidget())
                        managed.insert(parent);
                }
            }
        }
        // Keep observing an externally reparented title bar and its ancestors
        // so moving a whole container back into the host is detected too.
        for (auto* widget = m_titleBar.data(); widget; widget = widget->parentWidget())
        {
            observed.insert(widget);
            if (widget->isWindow())
                break;
        }
        observed.unite(managed);
        clearObservers();
        for (auto* widget : observed)
        {
            m_observed.append(widget);
            widget->installEventFilter(this);
        }

        QList<WidgetState> next;
        QList<WidgetState> changes;
        QList<WidgetState> removed;
        for (const auto& state : m_states)
        {
            if (!state.widget)
                continue;
            if (managed.remove(state.widget))
            {
                next.append(state);
                changes.append({ state.widget, false });
            }
            else
                removed.append(state);
        }
        for (auto* widget : managed)
        {
            const WidgetState state{ widget, widget->testAttribute(Qt::WA_ContentsMarginsRespectsSafeArea) };
            next.append(state);
            m_states.append(state);
            changes.append({ widget, false });
        }
        // Retain old and new snapshots until all layout callbacks finish, so
        // detaching the handler mid-update still restores every affected widget.
        for (const auto& state : removed)
        {
            restoreAttribute(state);
            if (!guard)
                return;
        }
        for (const auto& change : changes)
        {
            applyAttribute(change.widget, change.respectsSafeArea);
            if (!guard)
                return;
        }
        m_states        = std::move(next);
        m_synchronizing = false;
        if (m_resynchronize)
            scheduleSynchronize();
    }

    QPointer<QWidget> m_host;
    QPointer<QWidget> m_titleBar;
    QList<WidgetState> m_states;
    QList<QPointer<QWidget>> m_observed;
    bool m_pending       = false;
    bool m_synchronizing = false;
    bool m_resynchronize = false;
};
} // namespace WindowHeler::Internal

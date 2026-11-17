#include <FramelessKit/Export.h>
#include <QGuiApplication>
#include <QQuickWindow>

namespace FramelessKit::Internal
{
FRAMELESSKIT_QUICK_EXPORT bool prepareQuick(QString& error)
{
    for (QWindow* window : QGuiApplication::allWindows())
    {
        if (qobject_cast<QQuickWindow*>(window))
        {
            error = QStringLiteral("initialize(true) must precede the first QQuickWindow.");
            return false;
        }
    }
    QQuickWindow::setDefaultAlphaBuffer(true);
    return true;
}
} // namespace FramelessKit::Internal

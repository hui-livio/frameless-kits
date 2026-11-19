#include <FramelessKit/Export.h>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QtCore/qtsymbolmacros.h>

QT_DECLARE_EXTERN_SYMBOL_VOID(qml_register_types_FramelessKit)

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
    // Keep Qt's generated registrar even when Quick is a static archive.
    // Its QQmlModuleRegistration handles registration; no separate plugin is needed.
    QT_KEEP_SYMBOL(qml_register_types_FramelessKit)
    QQuickWindow::setDefaultAlphaBuffer(true);
    return true;
}
} // namespace FramelessKit::Internal

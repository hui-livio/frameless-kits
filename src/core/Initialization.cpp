#include "Runtime_p.h"

#include <FramelessKit/Initialization.h>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QThread>

Q_LOGGING_CATEGORY(fkRuntime, "framelesskit.runtime")
namespace FramelessKit
{
namespace
{
// Constant initialization avoids cross-DLL/static-bootstrap initialization
// ordering.
bool commonInitialized                      = false;
bool quickInitialized                       = false;
Internal::QuickInitializer quickInitializer = nullptr;
} // namespace
namespace Internal
{
void registerQuickInitializer(QuickInitializer initializer)
{
    quickInitializer = initializer;
}
bool checkGuiThread(const char* operation)
{
    auto* app = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
    if (!app || QThread::currentThread() != app->thread())
    {
        qCCritical(fkRuntime) << operation << "requires a QGuiApplication and its GUI thread";
        return false;
    }
    return true;
}
bool isInitialized(bool quick)
{
    return commonInitialized && (!quick || quickInitialized);
}
} // namespace Internal
void initialize(bool useQML)
{
    if (!Internal::checkGuiThread("FramelessKit::initialize"))
        return;
    if (!commonInitialized)
    {
        if (!QGuiApplication::allWindows().isEmpty())
        {
            qCCritical(fkRuntime) << "initialize must be called before creating windows";
            return;
        }
        commonInitialized = true;
    }
    if (!useQML || quickInitialized)
        return;
    if (!quickInitializer)
    {
        qCCritical(fkRuntime) << "initialize(true) requires linking FramelessKit::Quick";
        return;
    }
    QString error;
    if (!quickInitializer(error))
    {
        qCCritical(fkRuntime).noquote() << error;
        return;
    }
    quickInitialized = true;
}
} // namespace FramelessKit

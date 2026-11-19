#include "Runtime_p.h"

#include <FramelessKit/Initialization.h>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QThread>
#include <QVariant>

Q_LOGGING_CATEGORY(fkRuntime, "framelesskit.runtime")
namespace FramelessKit
{
namespace
{
// Each frontend embeds Core. Keep the initialization flags on the application
// so both DLLs observe the same state when Widgets and Quick are linked together.
constexpr auto commonInitializedProperty    = "_framelesskit_common_initialized";
constexpr auto quickInitializedProperty     = "_framelesskit_quick_initialized";
// Constant initialization allows the consumer bootstrap to run before main().
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
    auto* app = QCoreApplication::instance();
    return app && app->property(commonInitializedProperty).toBool() &&
           (!quick || app->property(quickInitializedProperty).toBool());
}
} // namespace Internal
void initialize(bool useQML)
{
    if (!Internal::checkGuiThread("FramelessKit::initialize"))
        return;
    if (!Internal::isInitialized())
    {
        if (!QGuiApplication::allWindows().isEmpty())
        {
            qCCritical(fkRuntime) << "initialize must be called before creating windows";
            return;
        }
        QCoreApplication::instance()->setProperty(commonInitializedProperty, true);
    }
    if (!useQML || Internal::isInitialized(true))
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
    QCoreApplication::instance()->setProperty(quickInitializedProperty, true);
}
} // namespace FramelessKit

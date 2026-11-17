// Compiled into Quick consumers so static archive elimination cannot discard
// initialize(true)'s adapter. Core remains completely independent of QtQuick.
#include <FramelessKit/CoreExport.h>
#include <FramelessKit/QuickExport.h>
#include <QString>

namespace FramelessKit::Internal
{
using QuickInitializer = bool (*)(QString& error);
FRAMELESSKIT_CORE_EXPORT void registerQuickInitializer(QuickInitializer);
FRAMELESSKIT_QUICK_EXPORT bool prepareQuick(QString& error);
namespace
{
struct QuickBootstrap
{
    QuickBootstrap()
    {
        registerQuickInitializer(prepareQuick);
    }
};
const QuickBootstrap bootstrap;
} // namespace
} // namespace FramelessKit::Internal

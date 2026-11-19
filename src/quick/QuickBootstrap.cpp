// Linked directly into Quick consumers so static archive elimination cannot discard
// initialize(true)'s adapter. Core remains completely independent of QtQuick.
#include <WindowHeler/Export.h>
#include <QString>

namespace WindowHeler::Internal
{
using QuickInitializer = bool (*)(QString& error);
WINDOWHELER_CORE_EXPORT void registerQuickInitializer(QuickInitializer);
WINDOWHELER_QUICK_EXPORT bool prepareQuick(QString& error);
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
} // namespace WindowHeler::Internal

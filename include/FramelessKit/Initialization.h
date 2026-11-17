#pragma once
#include <FramelessKit/CoreExport.h>

namespace FramelessKit
{
// Call on the GUI thread after application construction, before any windows.
// Quick consumers must link FramelessKit::Quick before requesting useQML.
FRAMELESSKIT_CORE_EXPORT void initialize(bool useQML = false);
} // namespace FramelessKit

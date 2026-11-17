#pragma once
#include <QtCore/qglobal.h>

// Define FRAMELESSKIT_STATIC when building/using static libraries or compiling
// the sources directly into an application. Without it, shared-library use is
// the default. Each DLL defines only its own FRAMELESSKIT_BUILD_* macro.
#if defined(FRAMELESSKIT_STATIC)
    #define FRAMELESSKIT_CORE_EXPORT
    #define FRAMELESSKIT_WIDGETS_EXPORT
    #define FRAMELESSKIT_QUICK_EXPORT
#else
    #if defined(FRAMELESSKIT_BUILD_CORE)
        #define FRAMELESSKIT_CORE_EXPORT Q_DECL_EXPORT
    #else
        #define FRAMELESSKIT_CORE_EXPORT Q_DECL_IMPORT
    #endif

    #if defined(FRAMELESSKIT_BUILD_WIDGETS)
        #define FRAMELESSKIT_WIDGETS_EXPORT Q_DECL_EXPORT
    #else
        #define FRAMELESSKIT_WIDGETS_EXPORT Q_DECL_IMPORT
    #endif

    #if defined(FRAMELESSKIT_BUILD_QUICK)
        #define FRAMELESSKIT_QUICK_EXPORT Q_DECL_EXPORT
    #else
        #define FRAMELESSKIT_QUICK_EXPORT Q_DECL_IMPORT
    #endif
#endif

#pragma once
#include <QtCore/qglobal.h>

/**
 * @brief The FRAMELESSKIT_CORE_EXPORT macro is used for exporting symbols from the FramelessKit core library.
 * @brief The FRAMELESSKIT_WIDGETS_EXPORT macro is used for exporting symbols from the FramelessKit widgets library.
 * @brief The FRAMELESSKIT_QUICK_EXPORT macro is used for exporting symbols from the FramelessKit quick library.
 */
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

#include <FramelessKit/Export.h>
#include <string_view>

#define FK_STRINGIFY_IMPL(value) #value
#define FK_STRINGIFY(value) FK_STRINGIFY_IMPL(value fk_export_sentinel)

namespace
{
constexpr std::string_view coreExport    = FK_STRINGIFY(FRAMELESSKIT_CORE_EXPORT);
constexpr std::string_view widgetsExport = FK_STRINGIFY(FRAMELESSKIT_WIDGETS_EXPORT);
constexpr std::string_view quickExport   = FK_STRINGIFY(FRAMELESSKIT_QUICK_EXPORT);

#if defined(FRAMELESSKIT_STATIC)
// MSVC's traditional preprocessor can retain whitespace before an empty macro.
constexpr std::string_view trim(std::string_view value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    return first == std::string_view::npos ? std::string_view{} : value.substr(first);
}
static_assert(trim(coreExport) == "fk_export_sentinel");
static_assert(trim(widgetsExport) == "fk_export_sentinel");
static_assert(trim(quickExport) == "fk_export_sentinel");
#else
#if defined(FRAMELESSKIT_BUILD_CORE)
static_assert(coreExport == FK_STRINGIFY(Q_DECL_EXPORT));
#else
static_assert(coreExport == FK_STRINGIFY(Q_DECL_IMPORT));
#endif
#if defined(FRAMELESSKIT_BUILD_WIDGETS)
static_assert(widgetsExport == FK_STRINGIFY(Q_DECL_EXPORT));
#else
static_assert(widgetsExport == FK_STRINGIFY(Q_DECL_IMPORT));
#endif
#if defined(FRAMELESSKIT_BUILD_QUICK)
static_assert(quickExport == FK_STRINGIFY(Q_DECL_EXPORT));
#else
static_assert(quickExport == FK_STRINGIFY(Q_DECL_IMPORT));
#endif
#endif
} // namespace

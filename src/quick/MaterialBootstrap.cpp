#include <QtCore/qtsymbolmacros.h>

QT_DECLARE_EXTERN_SYMBOL_VOID(qml_register_types_WindowHeler_Material)

namespace
{
struct MaterialBootstrap
{
    MaterialBootstrap()
    {
        // Retain the generated QQmlModuleRegistration without registering twice.
        QT_KEEP_SYMBOL(qml_register_types_WindowHeler_Material)
    }
};
const MaterialBootstrap bootstrap;
} // namespace

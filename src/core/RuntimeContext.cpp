#include "RuntimeContext_p.h"
#include <QHash>
#include <QMutex>
#include <QPointer>
#include <QObject>
#include <memory>

namespace WindowHeler::Internal
{
struct WindowContextHandle
{
    QObject* key = nullptr;
    QPointer<QObject> host;
    quint32 capabilities = 0;
    qsizetype references = 0;
};
namespace
{
QMutex mutex;
QHash<QObject*, WindowContextHandle*> contexts;
}

WindowContextHandle* windowheler_acquire_context(QObject* host)
{
    if (!host)
        return nullptr;
    QMutexLocker lock(&mutex);
    auto it = contexts.find(host);
    if (it == contexts.end())
    {
        auto* context = new WindowContextHandle;
        context->key = host;
        context->host = host;
        QObject::connect(host, &QObject::destroyed, [host]
        {
            QMutexLocker lock(&mutex);
            auto it = contexts.find(host);
            if (it == contexts.end())
                return;
            auto* context = it.value();
            contexts.erase(it);
            context->host = nullptr;
            if (context->references == 0 && context->capabilities == 0)
                delete context;
        });
        it = contexts.insert(host, context);
    }
    ++it.value()->references;
    return it.value();
}

void windowheler_retain_capability(WindowContextHandle* context, Capability capability)
{
    if (!context)
        return;
    QMutexLocker lock(&mutex);
    context->capabilities |= static_cast<quint32>(capability);
}

void windowheler_release_capability(WindowContextHandle* context, Capability capability)
{
    if (!context)
        return;
    QMutexLocker lock(&mutex);
    context->capabilities &= ~static_cast<quint32>(capability);
}

void windowheler_release_context(WindowContextHandle* context)
{
    if (!context)
        return;
    QMutexLocker lock(&mutex);
    if (context->references > 0)
        --context->references;
    if (context->references == 0 && context->capabilities == 0)
    {
        if (context->host)
        {
            auto it = contexts.find(context->key);
            if (it != contexts.end())
                contexts.erase(it);
        }
        delete context;
    }
}
} // namespace WindowHeler::Internal

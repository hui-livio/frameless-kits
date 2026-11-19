#pragma once
#include <WindowHeler/Material.h>
#include <QCoreApplication>
#include <QDebug>
#include <QMetaEnum>
#include <QStringList>
#include <QTimer>
#include <QVariant>
#include <array>

inline WindowHeler::Material demoInitialMaterial(const QStringList& arguments)
{
    const auto values = QMetaEnum::fromType<WindowHeler::Material>();
    for (const auto& argument : arguments)
    {
        if (!argument.startsWith(QStringLiteral("--material=")))
            continue;
        const QString name = argument.mid(11);
        for (int i = 0; i < values.keyCount(); ++i)
            if (name.compare(QString::fromLatin1(values.key(i)), Qt::CaseInsensitive) == 0)
                return static_cast<WindowHeler::Material>(values.value(i));
        qFatal("Unknown material: %s", qPrintable(name));
    }
    return WindowHeler::Material::Solid;
}
inline void printDemoMaterial(QObject* agent)
{
    const auto values = QMetaEnum::fromType<WindowHeler::Material>();
    qInfo().nospace() << "material=" << values.valueToKey(agent->property("material").toInt())
                      << "; effectiveMaterial=" << values.valueToKey(agent->property("effectiveMaterial").toInt());
}
// Optional example playback, not a test target or a pixel-effect assertion.
inline void runMaterialDemo(QObject* agent, const QStringList& arguments)
{
    const bool smoke = arguments.contains(QStringLiteral("--smoke-test"));
    if (arguments.contains(QStringLiteral("--cycle-materials")))
    {
        auto* timer = new QTimer(agent);
        QObject::connect(timer, &QTimer::timeout, agent,
                         [agent, timer, smoke, step = 0]() mutable
                         {
                             constexpr std::array<int, 9> sequence{ 0, 1, 2, 3, 4, 5, 2, 1, 0 };
                             printDemoMaterial(agent);
                             if (step < int(sequence.size()))
                                 agent->setProperty("material", sequence[step++]);
                             else
                             {
                                 timer->stop();
                                 timer->deleteLater();
                                 if (smoke)
                                     QCoreApplication::quit();
                             }
                         });
        timer->start(200);
    }
    else if (smoke)
        QTimer::singleShot(350, agent,
                           [agent]
                           {
                               printDemoMaterial(agent);
                               QCoreApplication::quit();
                           });
}

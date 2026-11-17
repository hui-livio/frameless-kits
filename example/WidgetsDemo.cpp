// Frameless window demo with native material selection and diagnostics.
#include "DemoOptions.h"

#include <FramelessKit/Initialization.h>
#include <FramelessKit/WidgetAgent.h>
#include <QApplication>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    FramelessKit::initialize();
    using FramelessKit::Material;

    QWidget window;
    window.setWindowTitle(QObject::tr("无边框模块接口演示"));
    window.resize(960, 600);
    window.setMinimumSize(760, 420);

    auto* layout = new QVBoxLayout(&window);
    layout->setContentsMargins(8, 8, 8, 8);

    auto* titleBar = new QWidget(&window);
    titleBar->setFixedHeight(44);
    auto* titleLayout = new QHBoxLayout(titleBar);
#ifdef Q_OS_MACOS
    titleLayout->setContentsMargins(80, 0, 8, 0);
#else
    titleLayout->setContentsMargins(8, 0, 8, 0);
#endif
    titleLayout->addWidget(new QLabel(QObject::tr("我的应用"), titleBar));
    titleLayout->addStretch();

    auto* titleControls  = new QWidget(titleBar);
    auto* controlsLayout = new QHBoxLayout(titleControls);
    controlsLayout->setContentsMargins(0, 0, 0, 0);

    auto* search = new QLineEdit(titleControls);
    search->setPlaceholderText(QObject::tr("搜索"));
    search->setFixedWidth(180);
    auto* minimize = new QToolButton(titleControls);
    minimize->setText(QObject::tr("最小化"));
    auto* maximize = new QToolButton(titleControls);
    maximize->setText(QObject::tr("最大化/还原"));
    auto* close = new QToolButton(titleControls);
    close->setText(QObject::tr("关闭"));

    controlsLayout->addWidget(search);
    controlsLayout->addWidget(minimize);
    controlsLayout->addWidget(maximize);
    controlsLayout->addWidget(close);
#ifdef Q_OS_MACOS
    // Use Cocoa's standard traffic-light controls on the left.
    minimize->hide();
    maximize->hide();
    close->hide();
#endif
    titleLayout->addWidget(titleControls);
    layout->addWidget(titleBar);

#ifdef Q_OS_MACOS
    const QString descriptionText = QObject::tr("拖动标题栏移动窗口，双击遵循系统设置。\n"
                                                "使用左上角系统按钮控制窗口，边缘可缩放。\n"
                                                "材质由系统合成；不可用时说明原因并回退纯色。");
#else
    const QString descriptionText = QObject::tr("拖动标题栏移动窗口，双击最大化/还原。\n"
                                                "边缘可缩放，右击标题栏可打开系统菜单。\n"
                                                "材质由系统合成；不可用时说明原因并回退纯色。");
#endif
    auto* description = new QLabel(descriptionText, &window);
    description->setAlignment(Qt::AlignCenter);
    description->setWordWrap(true);
    layout->addWidget(description, 1);

    auto* materialLayout = new QHBoxLayout;
    materialLayout->addWidget(new QLabel(QObject::tr("背景材质"), &window));
    auto* materials = new QComboBox(&window);
    materials->addItem(QObject::tr("纯色"), static_cast<int>(Material::Solid));
    materials->addItem(QObject::tr("透明"), static_cast<int>(Material::Transparent));
    materials->addItem(QObject::tr("背景模糊"), static_cast<int>(Material::Blur));
    materials->addItem(QObject::tr("亚克力"), static_cast<int>(Material::Acrylic));
    materials->addItem(QObject::tr("云母"), static_cast<int>(Material::Mica));
    materials->addItem(QObject::tr("云母 Alt"), static_cast<int>(Material::MicaAlt));
    const Material initialMaterial = demoInitialMaterial(app.arguments());
    materials->setCurrentIndex(materials->findData(static_cast<int>(initialMaterial)));
    materialLayout->addWidget(materials);
    materialLayout->addStretch();
    layout->addLayout(materialLayout);

    auto* materialState = new QLabel(&window);
    layout->addWidget(materialState);
    auto* feedback = new QLabel(QObject::tr("请选择材质；保留系统边框和阴影，不改变内容控件的不透明度。"), &window);
    feedback->setWordWrap(true);
    layout->addWidget(feedback);

    // Existing Qt window commands remain owned by the application.
    QObject::connect(minimize, &QToolButton::clicked, &window, &QWidget::showMinimized);
    QObject::connect(maximize, &QToolButton::clicked, &window,
                     [&window]
                     {
                         if (window.isMaximized())
                             window.showNormal();
                         else
                             window.showMaximized();
                     });
    QObject::connect(close, &QToolButton::clicked, &window, &QWidget::close);

    // FramelessKit integration: one agent, four configuration concepts.
    auto* frame = new FramelessKit::WidgetAgent(&window);

    // Optional diagnostics; ordinary applications do not have to subscribe.
    QObject::connect(frame, &FramelessKit::WidgetAgent::errorOccurred, feedback, &QLabel::setText);

    frame->setTitleBar(titleBar);
    frame->setInteractive(titleControls);
    frame->setMaximizeButton(maximize);
    const auto updateMaterialState = [frame, materialState, materials]
    {
        materialState->setText(QObject::tr("请求：%1；实际配置：%2")
                                   .arg(materials->itemText(int(frame->material())),
                                        materials->itemText(int(frame->effectiveMaterial()))));
    };
    QObject::connect(frame, &FramelessKit::WidgetAgent::materialChanged, &window, updateMaterialState);
    QObject::connect(frame, &FramelessKit::WidgetAgent::effectiveMaterialChanged, &window, updateMaterialState);
    frame->setMaterial(initialMaterial);
    updateMaterialState();

    QObject::connect(materials, &QComboBox::activated, &window,
                     [frame, materials, feedback](int index)
                     {
                         feedback->setText(QObject::tr("已请求：%1").arg(materials->itemText(index)));
                         frame->setMaterial(static_cast<Material>(materials->itemData(index).toInt()));
                     });

    window.show();
    runMaterialDemo(frame, app.arguments());
    return app.exec();
}

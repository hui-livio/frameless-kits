// Frameless window demo. Window materials are intentionally not implemented.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FramelessKit

Window {
    id: root

    width: 960
    height: 600
    minimumWidth: 760
    minimumHeight: 420
    visible: true
    title: qsTr("无边框模块接口演示")

    WindowAgent {
        id: frame

        window: root
        titleBar: titleBarItem
        interactiveItems: [titleControls]
        maximizeButton: maximizeButtonItem
        material: WindowAgent.Solid

        onErrorOccurred: function (message) {
            feedback.text = message;
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 12

        Item {
            id: titleBarItem

            Layout.fillWidth: true
            Layout.preferredHeight: 44

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Qt.platform.os === "osx" ? 80 : 8
                anchors.rightMargin: 8

                Label {
                    Layout.fillWidth: true
                    text: qsTr("我的应用")
                    elide: Text.ElideRight
                }

                RowLayout {
                    id: titleControls

                    spacing: 4

                    TextField {
                        Layout.preferredWidth: 180
                        placeholderText: qsTr("搜索")
                    }

                    Button {
                        visible: Qt.platform.os !== "osx"
                        text: qsTr("最小化")
                        onClicked: root.showMinimized()
                    }

                    Button {
                        id: maximizeButtonItem
                        visible: Qt.platform.os !== "osx"

                        text: root.visibility === Window.Maximized ? qsTr("还原") : qsTr("最大化")
                        onClicked: {
                            if (root.visibility === Window.Maximized)
                                root.showNormal();
                            else
                                root.showMaximized();
                        }
                    }

                    Button {
                        visible: Qt.platform.os !== "osx"
                        text: qsTr("关闭")
                        onClicked: root.close()
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.fillHeight: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.Wrap
            text: Qt.platform.os === "osx"
                ? qsTr("拖动标题栏移动窗口，双击遵循系统设置。\n使用左上角系统按钮控制窗口，边缘可缩放。\n非纯色请求会报告原因并保持纯色。")
                : qsTr("拖动标题栏移动窗口，双击最大化/还原。\n边缘可缩放，右击标题栏可打开系统菜单。\n非纯色请求会报告原因并保持纯色。")
        }

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: qsTr("背景材质")
            }

            ComboBox {
                id: materials

                Layout.preferredWidth: 180
                textRole: "text"
                valueRole: "value"
                currentIndex: 0
                model: [
                    {
                        text: qsTr("纯色"),
                        value: WindowAgent.Solid
                    },
                    {
                        text: qsTr("透明"),
                        value: WindowAgent.Transparent
                    },
                    {
                        text: qsTr("背景模糊"),
                        value: WindowAgent.Blur
                    },
                    {
                        text: qsTr("亚克力"),
                        value: WindowAgent.Acrylic
                    },
                    {
                        text: qsTr("云母"),
                        value: WindowAgent.Mica
                    },
                    {
                        text: qsTr("云母 Alt"),
                        value: WindowAgent.MicaAlt
                    }
                ]
                onActivated: {
                    feedback.text = qsTr("已请求：%1").arg(materials.currentText);
                    frame.material = materials.currentValue;
                }
            }

            Item {
                Layout.fillWidth: true
            }
        }

        Label {
            id: feedback

            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: qsTr("无边框窗口已启用；窗口材质暂不实现。")
        }
    }
}

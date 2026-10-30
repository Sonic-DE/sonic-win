/*
    SPDX-FileCopyrightText: 2019 Valerio Pilo <vpilo@coldshock.net>
    SPDX-FileCopyrightText: 2023 Joshua Goins <josh@redstrate.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2

import org.kde.kcmutils as KCM
import org.kde.kirigami 2.20 as Kirigami

KCM.SimpleKCM {
    title: i18n("Titlebar")

    // A [ToolsArea] opacity, 0–100 %: a percentage box between "Transparent" and
    // "Opaque" over a slider, both editing the same value.
    component OpacitySlider: ColumnLayout {
        id: opacity
        required property string settingName
        Layout.fillWidth: true
        Layout.minimumWidth: Kirigami.Units.gridUnit * 20

        RowLayout {
            QQC2.Label {
                text: i18nc("@label:slider end", "Transparent")
            }
            Item {
                Layout.fillWidth: true
            }
            QQC2.SpinBox {
                from: 0
                to: 100
                value: kcm.toolsArea[opacity.settingName]
                textFromValue: (value, locale) => i18nc("opacity percentage", "%1 %", value)
                valueFromText: (text, locale) => parseInt(text)
                editable: true
                onValueModified: kcm.toolsArea[opacity.settingName] = value
            }
            Item {
                Layout.fillWidth: true
            }
            QQC2.Label {
                text: i18nc("@label:slider end", "Opaque")
            }
        }
        QQC2.Slider {
            Layout.fillWidth: true
            from: 0
            to: 100
            stepSize: 1
            value: kcm.toolsArea[opacity.settingName]
            onMoved: kcm.toolsArea[opacity.settingName] = value

            KCM.SettingStateBinding {
                configObject: kcm.toolsArea
                settingName: opacity.settingName
            }
        }
    }

    // An exception's opacity: overridden (checked, with its value) or not (the setting
    // above applies).
    component OpacityOverride: RowLayout {
        id: opacityOverride
        required property var exceptionModel
        required property string role
        required property string label
        readonly property bool overridden: exceptionModel[role] !== undefined && exceptionModel[role] !== null
        QQC2.CheckBox {
            text: opacityOverride.label
            checked: opacityOverride.overridden
            onToggled: opacityOverride.exceptionModel[opacityOverride.role] = checked ? kcm.toolsArea[opacityOverride.role] : null
        }
        QQC2.SpinBox {
            from: 0
            to: 100
            enabled: opacityOverride.overridden
            value: opacityOverride.overridden ? opacityOverride.exceptionModel[opacityOverride.role] : kcm.toolsArea[opacityOverride.role]
            textFromValue: (value, locale) => i18nc("opacity percentage", "%1 %", value)
            valueFromText: (text, locale) => parseInt(text)
            editable: true
            onValueModified: opacityOverride.exceptionModel[opacityOverride.role] = value
        }
    }

    // An exception's on/off setting: overridden or not, and its value.
    component SwitchOverride: RowLayout {
        id: switchOverride
        required property var exceptionModel
        required property string role
        required property string label
        readonly property bool overridden: exceptionModel[role] !== undefined && exceptionModel[role] !== null
        QQC2.CheckBox {
            text: i18nc("@option:check override a setting", "Override:")
            checked: switchOverride.overridden
            onToggled: switchOverride.exceptionModel[switchOverride.role] = checked ? kcm.toolsArea[switchOverride.role] : null
        }
        QQC2.CheckBox {
            text: switchOverride.label
            enabled: switchOverride.overridden
            checked: switchOverride.overridden ? switchOverride.exceptionModel[switchOverride.role] : kcm.toolsArea[switchOverride.role]
            onToggled: switchOverride.exceptionModel[switchOverride.role] = checked
        }
    }

    // One exception: the applications it matches, and each opacity setting it overrides
    // (unchecked: the setting above applies).
    component ExceptionEditor: ColumnLayout {
        id: exception
        required property int index
        required property var model
        // A built-in default exception: it can only be switched off.
        readonly property bool isDefault: model.isDefault === true
        Layout.fillWidth: true
        Layout.maximumWidth: Kirigami.Units.gridUnit * 30

        RowLayout {
            QQC2.CheckBox {
                checked: exception.model.exceptionEnabled
                onToggled: exception.model.exceptionEnabled = checked
                QQC2.ToolTip.text: i18nc("@info:tooltip", "Use this exception")
                QQC2.ToolTip.visible: hovered
            }
            QQC2.TextField {
                Layout.fillWidth: true
                readOnly: exception.isDefault
                placeholderText: i18nc("@info:placeholder", "Application name or window class (regular expression)")
                text: exception.model.pattern
                onTextEdited: exception.model.pattern = text
            }
            QQC2.Label {
                visible: exception.isDefault
                text: i18nc("@label a built-in exception", "Default")
            }
            QQC2.Button {
                visible: !exception.isDefault
                icon.name: "list-remove"
                display: QQC2.AbstractButton.IconOnly
                text: i18nc("@action:button", "Remove Exception")
                onClicked: kcm.toolsAreaExceptions.remove(exception.index)
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered
            }
        }

        OpacityOverride {
            enabled: !exception.isDefault
            exceptionModel: exception.model
            role: "activeOpacity"
            label: i18nc("@option:check", "Active window opacity:")
        }
        OpacityOverride {
            enabled: !exception.isDefault
            exceptionModel: exception.model
            role: "inactiveOpacity"
            label: i18nc("@option:check", "Inactive window opacity:")
        }
        SwitchOverride {
            enabled: !exception.isDefault
            exceptionModel: exception.model
            role: "opaqueWhenMaximized"
            label: i18nc("checkbox label", "Always make maximized titlebars opaque")
        }
        SwitchOverride {
            enabled: !exception.isDefault
            exceptionModel: exception.model
            role: "blurBehind"
            label: i18nc("checkbox label", "Blur titlebars that have transparency")
        }
        SwitchOverride {
            enabled: !exception.isDefault
            exceptionModel: exception.model
            role: "applyOpacityToHeader"
            label: i18nc("checkbox label", "Apply these opacity settings to the entire header area")
        }
        SwitchOverride {
            enabled: !exception.isDefault
            exceptionModel: exception.model
            role: "matchApplicationColor"
            label: i18nc("checkbox label", "Match the titlebar to the application colour")
        }

        Kirigami.Separator {
            Layout.fillWidth: true
        }
    }

    component ToolsAreaCheckBox: QQC2.CheckBox {
        id: box
        required property string settingName
        checked: kcm.toolsArea[settingName]
        onToggled: kcm.toolsArea[settingName] = checked

        KCM.SettingStateBinding {
            configObject: kcm.toolsArea
            settingName: box.settingName
        }
    }

    // One scrolling column: the opacity settings and exceptions don't fit beside the button editor.
    ColumnLayout {
        Buttons {
            Layout.fillWidth: true
        }

        QQC2.CheckBox {
            id: closeOnDoubleClickOnMenuCheckBox
            text: i18nc("checkbox label", "Close windows by double clicking the menu button")
            checked: kcm.settings.closeOnDoubleClickOnMenu
            onToggled: {
                kcm.settings.closeOnDoubleClickOnMenu = checked;
                infoLabel.visible = checked;
            }

            KCM.SettingStateBinding {
                configObject: kcm.settings
                settingName: "closeOnDoubleClickOnMenu"
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            id: infoLabel
            type: Kirigami.MessageType.Information
            text: i18nc("popup tip", "Click and hold on the menu button to show the menu.")
            showCloseButton: true
            visible: false
        }

        QQC2.CheckBox {
            id: showToolTipsCheckBox
            text: i18nc("checkbox label", "Show titlebar button tooltips")
            checked: kcm.settings.showToolTips
            onToggled: kcm.settings.showToolTips = checked

            KCM.SettingStateBinding {
                configObject: kcm.settings
                settingName: "showToolTips"
            }
        }

        // The titlebar's transparency, shared by every theme and by the toolbars and menu
        // bars under the titlebar ([ToolsArea] in kdeglobals).
        Kirigami.Separator {
            Layout.fillWidth: true
        }

        Kirigami.FormLayout {
            Layout.fillWidth: true

            Kirigami.Separator {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18nc("@title:group", "Titlebar opacity")
            }

            OpacitySlider {
                Kirigami.FormData.label: i18nc("@label:slider", "Active window:")
                settingName: "activeOpacity"
            }
            OpacitySlider {
                Kirigami.FormData.label: i18nc("@label:slider", "Inactive window:")
                settingName: "inactiveOpacity"
            }

            ToolsAreaCheckBox {
                text: i18nc("checkbox label", "Always make maximized titlebars opaque")
                settingName: "opaqueWhenMaximized"
            }
            ToolsAreaCheckBox {
                text: i18nc("checkbox label", "Blur titlebars that have transparency")
                settingName: "blurBehind"
            }
            ToolsAreaCheckBox {
                text: i18nc("checkbox label", "Apply these opacity settings to the entire header area (toolbars and menu bars)")
                settingName: "applyOpacityToHeader"
            }
            ToolsAreaCheckBox {
                text: i18nc("checkbox label", "Match the titlebar to the application colour")
                settingName: "matchApplicationColor"
            }

            Kirigami.Separator {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18nc("@title:group", "Exceptions")
            }

            QQC2.Label {
                Layout.fillWidth: true
                Layout.maximumWidth: Kirigami.Units.gridUnit * 30
                wrapMode: Text.WordWrap
                text: i18nc("@info", "An exception changes only the titlebar opacity settings above, for the applications it matches.")
            }

            Repeater {
                model: kcm.toolsAreaExceptions
                delegate: ExceptionEditor {}
            }

            QQC2.Button {
                icon.name: "list-add"
                text: i18nc("@action:button", "Add Exception")
                onClicked: kcm.toolsAreaExceptions.add()
            }
        }
    }
}

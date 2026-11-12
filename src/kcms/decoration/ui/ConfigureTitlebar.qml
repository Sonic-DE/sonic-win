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

    ColumnLayout {
        id: titlebarColumn

        readonly property bool applicationMenuButtonInLayout: kcm.settings.buttonsOnLeft.indexOf("N") >= 0
                                                              || kcm.settings.buttonsOnRight.indexOf("N") >= 0
        readonly property bool applicationMenuOptionsAvailable: applicationMenuButtonInLayout && kcm.themeSupportsTitleBarMenu

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

        RowLayout {
            QQC2.Label {
                text: i18nc("@label:listbox", "Application menu button style:")
                enabled: applicationMenuStyleComboBox.enabled
            }
            QQC2.ComboBox {
                id: applicationMenuStyleComboBox
                // In the order of KDecoration3::ApplicationMenuStyle.
                model: [
                    i18nc("@item:inlistbox application menu button style", "Single menu button"),
                    i18nc("@item:inlistbox application menu button style", "Always shown"),
                    i18nc("@item:inlistbox application menu button style", "Shown on hover"),
                ]
                currentIndex: kcm.settings.applicationMenuStyle
                onActivated: kcm.settings.applicationMenuStyle = currentIndex

                KCM.SettingStateBinding {
                    configObject: kcm.settings
                    settingName: "applicationMenuStyle"
                    extraEnabledConditions: titlebarColumn.applicationMenuOptionsAvailable
                }
            }
        }

        QQC2.CheckBox {
            text: i18nc("checkbox label", "Show a search button with the application menu")
            checked: kcm.settings.applicationMenuSearch
            onToggled: kcm.settings.applicationMenuSearch = checked

            KCM.SettingStateBinding {
                configObject: kcm.settings
                settingName: "applicationMenuSearch"
                extraEnabledConditions: titlebarColumn.applicationMenuOptionsAvailable
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Information
            visible: !titlebarColumn.applicationMenuOptionsAvailable
            text: titlebarColumn.applicationMenuButtonInLayout
                ? i18nc("@info", "The selected window decoration style shows the application menu as a single button.")
                : i18nc("@info", "Add the Application menu button to the titlebar to use these options.")
        }
    }
}

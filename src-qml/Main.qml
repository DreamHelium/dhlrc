import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: realRoot
    title: i18nc("@title:window", "Homepage")
    width: 400
    height: 300

    globalDrawer: Kirigami.GlobalDrawer {
        actions: []
    }

    pageStack.initialPage: Kirigami.ScrollablePage {
        id: root

        Component {
            id: nbtReaderComponent
            NbtReader {}
        }

        Kirigami.PagePool {
            id: pagePool
        }

        actions: [
            Kirigami.Action {
                id: homeAction
                text: i18n("Home")
                icon.name: "go-home"
                onTriggered: realRoot.pageStack.pop(root)
            }
        ]
        Controls.SwipeView {
            id: swipeView
            anchors.fill: parent
            clip: true
            onCurrentIndexChanged: ft.currentIndex = currentIndex

            Kirigami.Page {
                id: regionPage
                ColumnLayout {
                    width: regionPage.width
                    spacing: Kirigami.Units.smallSpacing

                    Controls.Button {
                        id: nbtReaderBtn
                        text: i18n("&NBT File Reader")
                        Layout.alignment: Qt.AlignHCenter
                        action: Kirigami.PagePoolAction {
                            pagePool: pagePool
                            basePage : root
                            page: Qt.resolvedUrl("NbtReader.qml")
                        }
                    }

                    Controls.Button {
                        id: manageBtn
                        text: i18n("&Manage Region")
                        Layout.alignment: Qt.AlignHCenter
                        action : Kirigami.PagePoolAction{
                            pagePool : pagePool
                            basePage : root
                            page : Qt.resolvedUrl("Manage.qml")
                        }
                    }

                    Controls.Button {
                        id: modifyBtn
                        text: i18n("M&odify Region")
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Controls.Button {
                        id: blockReaderBtn
                        text: i18n("&Block Reader")
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }
            Kirigami.Page {
                id: utilityPage
                title: "Timers"
                ColumnLayout {
                    width: utilityPage.width
                    spacing: Kirigami.Units.smallSpacing

                    DhRealConfigDialog{
                        id : configDialog
                    }

                    Controls.Button {
                        id: configBtn
                        text: i18n("&Configure")
                        Layout.alignment: Qt.AlignHCenter
                        onClicked : {
                            configDialog.showWin();
                        }
                    }
                }
            }
        }

        footer: Kirigami.NavigationTabBar {
            id: ft
            actions: [
                Kirigami.Action {
                    icon.name: ":/cn/dh/dhlrc/region.svg"
                    text: i18n("Region")
                    checked: true
                    onTriggered: swipeView.currentIndex = ft.currentIndex
                },
                Kirigami.Action {
                    // icon.name: "player-time"
                    text: i18n("Utility")
                    onTriggered: swipeView.currentIndex = ft.currentIndex
                }
            ]
        }
    }
}

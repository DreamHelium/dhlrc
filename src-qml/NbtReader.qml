pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import cn.dh.dhlrc

Kirigami.Page {
    id: nbtReader

    title: "NBT File Reader"

    DhNbt {
        id: dhNbt

        onLoadError: function (msg) {
            errorLoading.text = msg;
            errorLoading.visible = true;
        }
        onProgressChange: function (value) {
            progressBar.value = value;
            progressValue.text = value.toString() + "%";
        }
        onLabelChange: function (text) {
            statusLabel.text = text;
        }
        onSuccess: {
            errorLoading.visible = false;
            multipleFileDropped.visible = false;
            treeView.model = dhNbt.model;
        }
    }

    Component {
        id: listViewDelegate
        Rectangle {
            id: rectangle
            required property string key
            required property string type
            required property string value
            width: 100
            height: 10
            visible: true
            color: "red"
            Text {
                text: parent.key
            }
        }
    }

    DropArea {
        anchors.fill: parent
        onDropped: function (drop) {
            if (drop.urls.length > 1) {
                multipleFileDropped.visible = true;
                return;
            } else
                multipleFileDropped.visible = false;
            if (drop.urls.length == 1) {
                dhNbt.loadFilename(drop.urls[0]);
                dhNbt.loadFile();
            }
        }
    }

    ColumnLayout {
        id: coloumLayout
        anchors.fill: parent
        spacing: Kirigami.Units.largeSpacing
        Kirigami.InlineMessage {
            id: multipleFileDropped
            Layout.fillWidth: true
            type: Kirigami.MessageType.Warning
            text: i18n("Multiple files dropped!")
            visible: false
            showCloseButton: true
        }

        Kirigami.InlineMessage {
            id: errorLoading
            Layout.fillWidth: true
            type: Kirigami.MessageType.Error

            visible: false
            showCloseButton: true
        }
        width: parent.width
        Controls.Label {
            text: i18n("Drag file to read NBT.")
            Layout.alignment: Qt.AlignHCenter
        }
        Controls.Label {
            id: statusLabel
        }
        RowLayout {
            Controls.ProgressBar {
                id: progressBar
                Layout.fillWidth: true
            }
            Controls.Label {
                id: progressValue
                text: "0%"
            }
        }

        Kirigami.PromptDialog {
            id: promptDialog
        }

        ColumnLayout {
            id: layout
            Kirigami.SearchField {
                id: field
                onTextChanged: {
                    dhNbt.model.setFilterRegularExpression(field.text);
                    console.log(field.text);
                }
            }

            Controls.ScrollView {
                topPadding: 10
                Layout.fillWidth: true
                Layout.fillHeight: true
                TreeView {
                    id: treeView
                    // Layout.fillWidth: true
                    Layout.preferredHeight: parent.height - 100

                    model: dhNbt.model
                    selectionModel: selectionModel
                    delegate: Controls.TreeViewDelegate {
                        id: treeViewDelegate

                        TapHandler {
                            onSingleTapped: {
                                infoLabel.text = "Type: " + i18n(treeViewDelegate.model.type) + "\n" + "Value: " + treeViewDelegate.model.value;
                            }
                            onDoubleTapped: {
                                promptDialog.title = "Information of the item " + treeViewDelegate.model.key;
                                promptDialog.subtitle = "Type: " + i18n(treeViewDelegate.model.type) + "\n" + "Value: " + treeViewDelegate.model.value;
                                promptDialog.open();
                            }
                        }
                        contentItem: Text {
                            text: treeViewDelegate.model.key
                        }
                    }
                }
            }

            Rectangle {
                Layout.preferredHeight: 1
                Layout.fillWidth: true
                color: "black"
            }
            Controls.ScrollView {
                topPadding: 10
                Layout.fillWidth: true
                Layout.fillHeight: true
                Controls.Label {
                    id: infoLabel
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    width: parent.width
                    wrapMode: Text.WrapAnywhere
                }
            }
        }
    }
}

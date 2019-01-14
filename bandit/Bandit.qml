import QtQuick 2.11
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.3
import QtQuick.Controls.Material 2.1
//import QtQuick.Dialogs 1.3
import Qt.labs.platform 1.0
import BANDIT.ProxyServer 1.0

ApplicationWindow {
    id: window
    width: 640
    height: 480
    visible: true
    title: "Bandit"

    Material.theme: Material.Dark;
    Material.background: "#2c3e50"
    Material.primary: "#34495e"
    Material.foreground: "#eeeeee"
    Material.accent: "#27ae60"
    font.family: "Nimbus Mono L"//"Manjari Thin"

    property bool projectChoosen: false
    property string currentProject: ""

    /// Proxy >
    signal dataReceived()

    property string     proxyData: ""
    property bool       proxyRunning: false
    property bool       proxyIsBusy: false
    property bool       proxyIsRequest: false
    property bool       proxyIntercept: false
    property int        proxyThreadID: 0
    // config
    property bool       proxyUpdateReq: true
    property bool       proxyUpdateRes: true
    property int        proxyInterType: 1
    property string     proxyHostText: "localhost"
    property string     proxyPortText: "8080"


    ProxyServer {
        id: prxServer
        function proxyGetNextData(){
            var tmp = getNextData();
            proxyIsBusy = true;
            tmp = String(tmp).split('\x03\x02\x01');
            proxyIsRequest = tmp[0] === "1" ? true : false;
            proxyThreadID = tmp[1];
            proxyData = tmp.slice(2).join('\x03\x02\x01');
            if(proxyIsRequest) {
                //prxServer.sendDataBack(proxyThreadID, false, "HTTP/1.1 102 Processing\n\n", false);
            }
        }
        function proxyClearInterceptor(){
            proxyData = ""
            proxyIsBusy = false;
            proxyThreadID = 0;
            proxyIsRequest = false;
        }

        onIncomingData: {
            if(!proxyIsBusy){
                proxyGetNextData();
                window.dataReceived();
            }
        }
    }
    /// < Proxy

    header: ToolBar {
        Material.foreground: "white"

        RowLayout {
            spacing: 20
            anchors.fill: parent

            ToolButton {
                icon.name: stackView.depth > 1 ? "back" : "menu"
                visible: projectChoosen
                onClicked: {
                    if (stackView.depth > 1) {
                        stackView.pop()
                        toolsListView.currentIndex = -1
                    } else {
                        drawer.open()
                    }
                }
            }

            Label {
                id: titleLabel
                text: toolsListView.currentItem ? toolsListView.currentItem.text : "BANDIT"
                font.pixelSize: 20
                elide: Label.ElideRight
                horizontalAlignment: Qt.AlignHCenter
                verticalAlignment: Qt.AlignVCenter
                Layout.fillWidth: true
            }

            ToolButton {
                icon.name: "settings"
                visible: projectChoosen
                onClicked: {
                    settingsStack.push(toolsListView.currentIndex == -1 ? "qrc:/pages/Settings.qml" : toolsListModel.get(toolsListView.currentIndex).settings);
                    settingsDrawer.open();
                }
            }
        }
    }

    Drawer {
        id: settingsDrawer
        width: window.width
        height: window.height
        interactive: false
        Label {
            y: 15
            text: toolsListView.currentIndex == -1 ? "General Settings" : toolsListModel.get(toolsListView.currentIndex).title+" Settings"
            anchors.horizontalCenter: parent.horizontalCenter
        }
        ToolButton {
            anchors.right: parent.right
            icon.name: "close"
            icon.color: "#27ae60"
            onClicked: settingsDrawer.close()
        }
        StackView {
            id: settingsStack
            anchors.fill: parent
            anchors.topMargin: 42
        }
        onClosed: {
            settingsStack.pop();
        }
    }

    Drawer {
        id: drawer
        width: Math.min(window.width, window.height) / 3 * 2
        height: window.height
        interactive: stackView.depth === 1

        ListView {
            id: toolsListView

            focus: true
            currentIndex: -1
            anchors.fill: parent

            delegate: ItemDelegate {
                width: parent.width
                icon.name: model.icon
                text: model.title
                highlighted: ListView.isCurrentItem
                onClicked: {
                    toolsListView.currentIndex = index
                    stackView.push(model.source)
                    drawer.close()
                }
            }

            model: ListModel {
                id: toolsListModel
                ListElement { title: "Proxy"; source: "qrc:/pages/Proxy.qml"; icon: "proxy"; settings: "qrc:/pages/ProxySettings.qml" }
            }

            ScrollIndicator.vertical: ScrollIndicator { }
        }
    }

    StackView {
        id: stackView
        anchors.fill: parent

        initialItem: Pane {
            id: pane

            Image {
                id: logo
                width: 180
                height: 180
                anchors.centerIn: parent
                anchors.verticalCenterOffset: -85
                fillMode: Image.PreserveAspectFit
                source: "qrc:/images/logo.png"
            }

            Label {
                text: currentProject
                visible: projectChoosen
                anchors.verticalCenterOffset: -20
                anchors.verticalCenter: parent.verticalCenter
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Button {
                text: projectChoosen ? "SAVE" : "TMP"
                anchors.verticalCenterOffset: 20
                anchors.verticalCenter: parent.verticalCenter
                anchors.horizontalCenter: parent.horizontalCenter
                highlighted: true
                width: 80
                onClicked: {
                    if(projectChoosen){
                        saveProjectDialog.open();
                    }else{
                        projectChoosen = true
                        currentProject = ".tmpbandit"
                    }
                }

                FileDialog {
                    id: saveProjectDialog
                    title: "Choose project file to save"
                    folder: shortcuts.home
                    defaultSuffix: ".bndt"
                    onAccepted: {
                        currentProject = saveProjectDialog.fileUrls[0].replace("file://","");
                    }
                    visible: false
                }
            }
            Button {
                text: "OPEN"
                anchors.verticalCenterOffset: 60
                anchors.verticalCenter: parent.verticalCenter
                anchors.horizontalCenter: parent.horizontalCenter
                highlighted: true
                width: 80
                onClicked: {
                    projectChoosen = true
                }
            }
        }
    }
}

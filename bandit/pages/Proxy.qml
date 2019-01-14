import QtQuick 2.6
import QtQuick.Controls 2.1

ScrollablePage {
    Column {
        spacing: 10
        width: parent.width

        Row {
            spacing: 10
            Button {
                icon.name: "forw"
                width: height
                highlighted: true
                onClicked: {
                    if(proxyIsBusy){
                        prxServer.sendDataBack(proxyThreadID, proxyIsRequest, proxyData, (proxyIsRequest && proxyUpdateReq) || (!proxyIsRequest && proxyUpdateRes));
                        proxyTextArea.text = "";
                        if(prxServer.queueNotEmpty()){
                            prxServer.proxyGetNextData();
                            proxyTextArea.text = proxyData
                        }else{
                            prxServer.proxyClearInterceptor();
                        }
                    }
                }
            }
            Button {
                icon.name: "drop"
                width: height
                highlighted: true
                onClicked: {
                    if(proxyIsBusy){
                        if(proxyThreadID != 0) prxServer.closeThread(proxyThreadID);
                        proxyTextArea.text = "";
                        if(prxServer.queueNotEmpty()){
                            prxServer.proxyGetNextData();
                            proxyTextArea.text = proxyData
                        }else{
                            prxServer.proxyClearInterceptor();
                        }
                    }
                }
            }
            Button {
                icon.name: "inter"
                width: height
                checkable: true
                checked: proxyIntercept
                onClicked: {
                    if(checked){
                        prxServer.setInterception(proxyInterType);
                    } else {
                        prxServer.setInterception(0);
                        if(proxyIsBusy){
                            prxServer.sendDataBack(proxyThreadID, proxyIsRequest, proxyData, (proxyIsRequest && proxyUpdateReq) || (!proxyIsRequest && proxyUpdateRes));
                            prxServer.proxyClearInterceptor();
                            proxyTextArea.text = "";
                        }
                    }
                    proxyIntercept = !proxyIntercept
                }
            }
        }

        TextArea {
            id: proxyTextArea
            selectByMouse: true
            width: parent.width - 20
            font.pixelSize: 10
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            onTextChanged: proxyData = text
            Component.onCompleted: if(proxyData != "") text = proxyData
            Connections {
                target: window
                onDataReceived: {
                    proxyTextArea.text = proxyData
                }
            }
        }
    }
}


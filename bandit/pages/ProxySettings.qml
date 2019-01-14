import QtQuick 2.6
import QtQuick.Controls 2.1

ScrollablePage {
    Column {
        width: parent.width
        TextField {
            id: proxyHost
            placeholderText: "Proxy host"
            text: proxyHostText
            width: parent.width
            enabled: !proxyOn.checked
        }
        TextField {
            id: proxyPort
            placeholderText: "Proxy port"
            text: proxyPortText
            validator: IntValidator { bottom:1; top: 65535}
            width: parent.width
            enabled: !proxyOn.checked
        }
        CheckDelegate {
            id: proxyOn
            width: parent.width
            text: "Proxy on"
            checked: proxyRunning
            onClicked: {
                if(proxyHost.text.trim() != "" && proxyPort.text.trim() != ""){
                    var ret = prxServer.startStopServer(proxyHost.text, proxyPort.text);
                    if(!ret) checked = false;
                }else checked = false;
                if(checked) proxyRunning = true;
                else        proxyRunning = false;
                proxyData = "";
            }
        }
        CheckDelegate {
            id: interReq
            width: parent.width
            text: "Intercept requests"
            checked: proxyInterType & 1
            onClicked: {
                if(checked){
                    proxyInterType |= 1;
                }else{
                    proxyInterType &= 2;
                }
                if(proxyIntercept) prxServer.setInterception(proxyInterType);
            }
        }
        CheckDelegate {
            id: updateReqLength
            width: parent.width
            text: "Fix request"
            enabled: interReq.checked
            checked: proxyUpdateReq
            onClicked: {
                proxyUpdateReq = !proxyUpdateReq
            }
        }
        CheckDelegate {
            id: interRes
            width: parent.width
            text: "Intercept responses"
            checked: proxyInterType & 2
            onClicked: {
                if(checked){
                    proxyInterType |= 2;
                }else{
                    proxyInterType &= 1;
                }
                if(proxyIntercept) prxServer.setInterception(proxyInterType);
            }
        }
        CheckDelegate {
            id: updateResLength
            width: parent.width
            text: "Fix Response"
            enabled: interRes.checked
            checked: proxyUpdateRes
            onClicked: {
                proxyUpdateRes = !proxyUpdateRes
            }
        }
    }
}

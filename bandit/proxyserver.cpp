#include "proxyserver.h"

ProxyServer::ProxyServer()
{
    m_threadCount = 0;
    m_threadId = 1;
    m_interceptType = 0;
}

bool ProxyServer::startStopServer(QString host, quint16 port)
{
    if(!this->isListening()){
        if(!this->listen(host == "localhost" || host == "127.0.0.1" ? QHostAddress::LocalHost : QHostAddress(host), port)){
            return false;
        }
        return true;
    }else{
        this->close();
        emit closeThreads();
        m_threadCount = 0;
        m_threadId = 1;
        m_interceptType = 0;
        m_threads.clear();
        return true;
    }
}

void ProxyServer::setInterception(char type = 0)
{
    m_interceptType = type;
    switch(type){
    case 0:
        while(m_reqResps.length() > 0){
            QPair<unsigned int, ReqResp> el = m_reqResps.dequeue();
            if(el.second.request) emit processRequestToThread(el.second.data, el.first);
            else emit processResponseToThread(el.second.data, el.first);
        }
        break;
    case 1:
        for(int i = 0;i<m_reqResps.length();i++){
            QPair<unsigned int, ReqResp> el = m_reqResps.at(i);
            if(!el.second.request){
                emit processResponseToThread(el.second.data, el.first);
                m_reqResps.removeAt(i);
            }
        }
        break;
    case 2:
        for(int i = 0;i<m_reqResps.length();i++){
            QPair<unsigned int, ReqResp> el = m_reqResps.at(i);
            if(el.second.request){
                emit processRequestToThread(el.second.data, el.first);
                m_reqResps.removeAt(i);
            }
        }
        break;
    }
}

QString ProxyServer::getNextData()
{
    QPair<unsigned int, ReqResp> pr;
    pr = m_reqResps.dequeue();
    return QString(pr.second.request ? "1" : "0")+"\x03\x02\x01"+QString::number(pr.first)+"\x03\x02\x01"+pr.second.data.replace('\x00',"呵");
}

void ProxyServer::sendDataBack(unsigned int id, bool request, QByteArray data, bool updateContentLen)
{
    QByteArray ba;
    ba.append('\x00');
    data = data.replace("呵",ba);

    if(updateContentLen){
        int crlfId = data.indexOf("\r\n\r\n");
        QStringList headers = QString(data).split(crlfId == -1 ? "\n\n" : "\r\n\r\n")[0].split(crlfId == -1 ? "\n" : "\r\n");
        int contentLenInd = headers.indexOf(QRegExp("content-length:.*", Qt::CaseInsensitive));
        if(contentLenInd != -1){
            int prevLen = headers[contentLenInd].replace("content-length:", "", Qt::CaseInsensitive).trimmed().toInt();
            int len = data.mid(data.indexOf(crlfId == -1 ? "\n\n" : "\r\n\r\n")+(crlfId == -1 ? 2 : 4)).length();
            if(len != prevLen && prevLen != 0){
                headers.removeAt(contentLenInd);
                headers.append("Content-Length: "+QString::number(len));
                data = headers.join(crlfId == -1 ? "\n" : "\r\n").toStdString().c_str()+data.mid(data.indexOf(crlfId == -1 ? "\n\n" : "\r\n\r\n"));
            }
        }
    }
    if(request) emit processRequestToThread(data, id);
    else emit processResponseToThread(data, id);
}

void ProxyServer::closeThread(unsigned int id)
{
    emit closeThr(id);
}

bool ProxyServer::queueNotEmpty()
{
    if(m_reqResps.length() > 0) return true;
    return false;
}

void ProxyServer::threadFinished()
{
    m_threadCount--;
    if(m_threads.length() > 0){
        m_threads.dequeue()->start();
        m_threadCount++;
    }
}

void ProxyServer::requestReadyFromThread(QByteArray data, unsigned int id)
{
    if((m_interceptType & 1) == 1){
        m_reqResps.enqueue(QPair<unsigned int,ReqResp>(id, ReqResp(true, data)));
        emit incomingData();
    }else{
        sendDataBack(id, true, data, false);
    }
}

void ProxyServer::responseReadyFromThread(QByteArray data, unsigned int id)
{
    if((m_interceptType & 2) == 2){
        m_reqResps.enqueue(QPair<unsigned int,ReqResp>(id, ReqResp(false, data)));
        emit incomingData();
    }else{
        sendDataBack(id, false, data, false);
    }
}

void ProxyServer::incomingConnection(qintptr desc){
    if(m_threadId == 0) m_threadId++;
    m_threadId++;

    ProxyServerThread* thr = new ProxyServerThread(desc, m_threadId);
    connect(thr, SIGNAL(finished()), this, SLOT(threadFinished()));
    connect(thr, SIGNAL(finished()), thr, SLOT(deleteLater()));

    connect(thr, SIGNAL(requestReady(QByteArray, unsigned int)), this, SLOT(requestReadyFromThread(QByteArray, unsigned int)));
    connect(thr, SIGNAL(responseReady(QByteArray, unsigned int)), this, SLOT(responseReadyFromThread(QByteArray, unsigned int)));

    connect(this, SIGNAL(processRequestToThread(QByteArray, unsigned int)), thr, SLOT(processRequest(QByteArray, unsigned int)));
    connect(this, SIGNAL(processResponseToThread(QByteArray, unsigned int)), thr, SLOT(processResponse(QByteArray, unsigned int)));

    connect(this, SIGNAL(closeThreads()), thr, SLOT(closeAll()), Qt::DirectConnection);
    connect(this, SIGNAL(closeThr(unsigned int)), thr, SLOT(closeMe(unsigned int)), Qt::DirectConnection);

    if(m_threadCount < MAXTHREADS){
        thr->start();
        m_threadCount++;
    }else{
        m_threads.enqueue(thr);
    }
}


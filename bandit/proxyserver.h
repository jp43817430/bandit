#ifndef PROXYSERVER_H
#define PROXYSERVER_H
#include <QTcpServer>
#include <QThread>
#include <QQueue>

#include "proxyserverthread.h"

#define MAXTHREADS 150

struct ReqResp {
    ReqResp(bool req = false, QByteArray dt = ""){ request = req; data = dt; }
    bool request;
    QByteArray data;
};

class ProxyServer : public QTcpServer
{
    Q_OBJECT
public:
    ProxyServer();
    Q_INVOKABLE bool startStopServer(QString host, quint16 port);
    Q_INVOKABLE void setInterception(char type);
    Q_INVOKABLE void sendDataBack(unsigned int id, bool request, QByteArray data, bool updateContentLen);
    Q_INVOKABLE void closeThread(unsigned int id);
    Q_INVOKABLE QString getNextData();
    Q_INVOKABLE bool queueNotEmpty();

signals:
    void processRequestToThread(QByteArray data, unsigned int id);
    void processResponseToThread(QByteArray data, unsigned int id);
    void closeThreads();
    void incomingData();

    void closeThr(unsigned int id);

public slots:
    void threadFinished();
    void requestReadyFromThread(QByteArray data, unsigned int id);
    void responseReadyFromThread(QByteArray data, unsigned int id);

protected:
    void incomingConnection(qintptr handle);

private:
    QQueue<ProxyServerThread*> m_threads;
    QQueue<QPair<unsigned int,ReqResp>> m_reqResps;
    int m_threadCount;
    unsigned int m_threadId;
    char m_interceptType;
};

#endif // PROXYSERVER_H

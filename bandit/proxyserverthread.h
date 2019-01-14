#ifndef PROXYSERVERTHREAD_H
#define PROXYSERVERTHREAD_H

#include <QObject>
#include <QThread>
#include <QRunnable>
#include <QSsl>
#include <QSslKey>
#include <QSslSocket>
#include <QTimer>

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>

#include <QDir>
#include <QQueue>

#include "proxyhttpscheme.h"

class ProxyServerThread : public QThread
{
    Q_OBJECT
public:
    explicit ProxyServerThread(qintptr desc, unsigned int id, QObject *parent = nullptr);
    void run();
    void createCertIfNotEx(QString dns);

signals:
    void requestReady(QByteArray req, unsigned int id);
    void responseReady(QByteArray res, unsigned int id);

public slots:
    void processRequest(QByteArray data, unsigned int id);
    void processResponse(QByteArray data, unsigned int id);

    void fromReadyRead();
    void toReadyRead();
    void closeConnections();

    void closeMe(unsigned int id);
    void closeAll();

private:
    qintptr m_desc;
    qintptr m_descTo;
    unsigned int m_id;

    QTimer* m_checker;

    bool m_closeMe;

    QSslSocket* m_socketFrom;
    QSslSocket* m_socketTo;

    ProxyHTTPScheme* m_to;
    ProxyHTTPScheme* m_from;

    bool m_connected;

    static const char m_rootCert[];
    static const char m_rootKey[];
    static const char m_servKey[];
    static const char m_respClose[];
};

#endif // PROXYSERVERTHREAD_H

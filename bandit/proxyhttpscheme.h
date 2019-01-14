#ifndef HTTPSCHEME_H
#define HTTPSCHEME_H

#include <QObject>
#include <QtDebug>

enum HTTP{
    Method,
    URL,
    Host,
    Port,
    Connection,
    ContentLength,
    TransferEncoding,
    RetCode,
    ContentEncoding
};

class ProxyHTTPScheme
{
public:
    ProxyHTTPScheme(bool request);
    ~ProxyHTTPScheme();
    void update(QByteArray data);
    void clear();
    void unchunk();

    QString getElement(HTTP name);
    QByteArray getData();

    bool transmissionEnds();

private:
    QByteArray m_data;
    QString m_http[9];
    bool m_request;
    bool m_chunked;
};

#endif // HTTPSCHEME_H

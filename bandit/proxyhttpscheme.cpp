#include "proxyhttpscheme.h"

ProxyHTTPScheme::ProxyHTTPScheme(bool request){ m_data = ""; m_request = request; }

ProxyHTTPScheme::~ProxyHTTPScheme()
{
}

void ProxyHTTPScheme::update(QByteArray data)
{
    m_data += data;

    int crlfId = m_data.indexOf("\r\n\r\n");

    QStringList headers = QString(m_data).split(crlfId == -1 ? "\n\n" : "\r\n\r\n")[0].split(crlfId == -1 ? "\n" : "\r\n");
    if(m_request){
        m_http[HTTP::Method] = headers[0].split(' ')[0].toLower();

        int hostInd = headers.indexOf(QRegExp("host:.*", Qt::CaseInsensitive));
        m_http[HTTP::Host] = hostInd == -1 ? "" : headers[hostInd].replace(QRegExp("host:", Qt::CaseInsensitive), "").trimmed();
        m_http[HTTP::Host] = m_http[HTTP::Host].split(':')[0];
        m_http[HTTP::Port] = m_http[HTTP::Host].indexOf(':') == -1 ? "80" : m_http[HTTP::Host].split(':')[1];

        if(headers[0].split(' ').length() > 1){
            m_http[HTTP::URL] = headers[0].split(' ')[1];
        }

        if(m_http[HTTP::URL].contains(QRegExp("^[htpsf]{3,5}://"))){
            QString strToReplace = m_http[HTTP::URL].split("/").mid(3,m_http[HTTP::URL].split("/").length()).join("/");
            m_data = m_data.replace(m_http[HTTP::URL].toStdString().c_str(), (strToReplace.isEmpty() ? "/" : (strToReplace[0] == '/' ? strToReplace : "/"+strToReplace)).toStdString().c_str());
        }

        int prxCnInd = headers.indexOf(QRegExp("proxy-connection:.*", Qt::CaseInsensitive));
        if(prxCnInd != -1){
            QString strToReplace = headers[prxCnInd];
            m_data = m_data.replace(headers[prxCnInd].toStdString().c_str(), strToReplace.replace("proxy-", "", Qt::CaseInsensitive).toStdString().c_str());
        }

        // Драл я нафиг это сжатие, с их долбанными алгоритмами и кучей сторонних
        // библиотек - я ничего не знаю кроме плэин текста!
        int accptEncInd = headers.indexOf(QRegExp("accept-encoding:.*", Qt::CaseInsensitive));
        if(accptEncInd != -1){
            QByteArray strToReplace = "Accept-Encoding: identity";
            m_data.replace(headers[accptEncInd].toStdString().c_str(), strToReplace);
        }
    }

    int contentLenInd = headers.indexOf(QRegExp("content-length:.*", Qt::CaseInsensitive));
    m_http[HTTP::ContentLength] = contentLenInd == -1 ? "" : headers[contentLenInd].replace(QRegExp("content-length:", Qt::CaseInsensitive), "").trimmed();
    m_http[HTTP::ContentLength] = m_http[HTTP::ContentLength] == "0" ? "" : m_http[HTTP::ContentLength];

    int transferEncInd = headers.indexOf(QRegExp("transfer-encoding:.*", Qt::CaseInsensitive));
    m_http[HTTP::TransferEncoding] = transferEncInd == -1 ? "" : headers[transferEncInd].replace(QRegExp("transfer-encoding:", Qt::CaseInsensitive), "").trimmed().toLower();

    int connectionInd = headers.indexOf(QRegExp("connection:.*", Qt::CaseInsensitive));
    m_http[HTTP::Connection] = connectionInd == -1 ? (m_http[HTTP::TransferEncoding] == "chunked" ? "keep-alive" : "") : headers[connectionInd].replace(QRegExp("connection:", Qt::CaseInsensitive), "").trimmed().toLower();

    int encodingInd = headers.indexOf(QRegExp("content-encoding:.*", Qt::CaseInsensitive));
    m_http[HTTP::ContentEncoding] = encodingInd == -1 ? "" : headers[encodingInd].replace(QRegExp("content-encoding:", Qt::CaseInsensitive), "").trimmed().toLower();
}

void ProxyHTTPScheme::clear()
{
    m_data = "";
    for(int i = 0;i<m_http->length();i++) m_http[i] = "";
}

void ProxyHTTPScheme::unchunk()
{
    if(m_http[HTTP::TransferEncoding].contains("chunked", Qt::CaseInsensitive)){
        QStringList headers = QString(m_data.mid(0, m_data.indexOf("\r\n\r\n"))).split("\r\n");
        int trInd = headers.indexOf(QRegExp("transfer-encoding:.*", Qt::CaseInsensitive));

        QByteArray body = m_data.mid(m_data.indexOf("\r\n\r\n")+4);
        QByteArray result;
        int chunkInd = 0;

        while((chunkInd = body.indexOf("\r\n")) != -1){
            int len = body.mid(0, chunkInd).toInt(nullptr,16);
            result += body.mid(chunkInd+2, len);
            body = body.mid(chunkInd+2+len);
        }

        if(trInd != -1){
            headers.removeAt(trInd);
            headers.append(QString("Content-Length: %1").arg(result.length()));
        }
        body = headers.join("\r\n").append("\r\n\r\n").toStdString().c_str();
        body.append(result);
        m_data = body;
    }
}

bool ProxyHTTPScheme::transmissionEnds()
{
    int m_contentLen = m_http[HTTP::ContentLength] == "" ? 0 : m_http[HTTP::ContentLength].toInt();

    if( (m_contentLen == 0 && m_data.mid(m_data.length()-4,4) != "\r\n\r\n" && !m_http[HTTP::TransferEncoding].contains("chunked", Qt::CaseInsensitive))
    ||  (m_contentLen != 0 && m_data.mid(m_data.indexOf("\r\n\r\n")+4, m_data.length()).length() != m_contentLen)
    ||  (m_contentLen == 0 && m_data.mid(m_data.length()-5,5) != "0\r\n\r\n" && m_http[HTTP::TransferEncoding].contains("chunked", Qt::CaseInsensitive))){
        return false;
    }
    return true;
}

QString ProxyHTTPScheme::getElement(HTTP name)
{
    return m_http[name];
}

QByteArray ProxyHTTPScheme::getData()
{
    return m_data;
}

#include "proxyserverthread.h"

ProxyServerThread::ProxyServerThread(qintptr desc, unsigned int id, QObject *parent)
        : QThread(parent), m_desc(desc), m_id(id) {
    m_descTo = -1;
    m_connected = false;
    m_closeMe = false;
    m_to = new ProxyHTTPScheme(true);
    m_from = new ProxyHTTPScheme(false);
}

void ProxyServerThread::run()
{
    m_socketFrom = new QSslSocket();
    m_socketTo = new QSslSocket();

    m_socketFrom->setSocketDescriptor(m_desc);
    connect(m_socketFrom, SIGNAL(readyRead()), this, SLOT(fromReadyRead()), Qt::DirectConnection);
    connect(m_socketFrom, SIGNAL(disconnected()), this, SLOT(closeConnections()), Qt::DirectConnection);
    connect(m_socketTo, SIGNAL(disconnected()), this, SLOT(closeConnections()), Qt::DirectConnection);

    connect(m_socketTo, SIGNAL(readyRead()), this, SLOT(toReadyRead()), Qt::DirectConnection);

    m_checker = new QTimer();
    connect(m_checker, &QTimer::timeout, this, [this](){
        if(this->m_closeMe) this->closeConnections();
    }, Qt::DirectConnection);
    m_checker->start(100);



    exec();
}

// Запрос
void ProxyServerThread::processRequest(QByteArray data = "", unsigned int id = 0)
{
    if(!id || id == m_id){
        if(data != ""){
            m_to->clear();
            m_to->update(data);
        }
        if(!m_closeMe)
            m_socketTo->write(m_to->getData());
        m_to->clear();
    }
}

// Ответ
void ProxyServerThread::processResponse(QByteArray data = "", unsigned int id = 0)
{
    if(!id || id == m_id){
        if(data != ""){
            m_from->clear();
            m_from->update(data);
        }
        if(!m_closeMe)
            m_socketFrom->write(m_from->getData());
        m_from->clear();
    }
}

// Данные пришедшие от клиента
void ProxyServerThread::fromReadyRead()
{

    if(!m_closeMe)
        m_to->update(m_socketFrom->readAll());

    while(!m_to->transmissionEnds()) return;

    if(m_to->getElement(HTTP::TransferEncoding) == "chunked")
        m_to->unchunk();

    if(m_to->getElement(HTTP::Method) == "connect"){
        QString dns = m_to->getElement(HTTP::URL).split(':')[0];
        if(!dns.isEmpty()){
            createCertIfNotEx(dns);
            m_socketFrom->setPrivateKey(QSslKey(m_servKey, QSsl::Rsa, QSsl::Pem));
            m_socketFrom->setLocalCertificate("certs/"+dns+".crt");
            m_socketFrom->write("HTTP/1.0 200 Connection established\r\n\r\n");
            m_socketFrom->startServerEncryption();
        }
        m_to->clear();
    }else{
        QString url = m_to->getElement(HTTP::URL);
        if(url.contains("bandit.im")){
            m_socketFrom->write((QString("HTTP/1.1 200 Ok\r\nContent-Disposition: attachment; filename=bandit.crt\r\n\r\n")+QString(m_rootCert)).toStdString().c_str());
            m_socketFrom->close();
        }else{
            if(!m_connected){
                if(m_socketFrom->isEncrypted()) m_socketTo->connectToHostEncrypted(m_to->getElement(HTTP::Host), m_to->getElement(HTTP::Port) == "80" ? 443 : static_cast<quint16>(m_to->getElement(HTTP::Port).toInt()));
                else m_socketTo->connectToHost(m_to->getElement(HTTP::Host), m_to->getElement(HTTP::Port) == "80" ? 80 : static_cast<quint16>(m_to->getElement(HTTP::Port).toInt()));
                m_connected = true;
                m_descTo = m_socketTo->socketDescriptor();
            }
            emit requestReady(m_to->getData(), m_id);
        }
    }
}

// Данные от сервера
void ProxyServerThread::toReadyRead()
{
    if(!m_closeMe)
        m_from->update(m_socketTo->readAll());

    while(!m_from->transmissionEnds()) return;

    if(m_from->getElement(HTTP::TransferEncoding) == "chunked")
        m_from->unchunk();
    emit responseReady(m_from->getData(), m_id);
}

void ProxyServerThread::closeConnections()
{
    if(m_closeMe)
        m_socketFrom->write(m_respClose);
    m_socketTo->flush();
    m_socketFrom->flush();
    m_socketFrom->close();
    m_socketTo->close();
    m_socketFrom->disconnectFromHost();
    m_socketTo->disconnectFromHost();
    m_checker->stop();
    m_checker->deleteLater();
    quit();
}

void ProxyServerThread::closeMe(unsigned int id)
{
    if(id == m_id){
        closeAll();
    }
}

void ProxyServerThread::closeAll()
{
    m_closeMe = true;
}

void ProxyServerThread::createCertIfNotEx(QString dns){
    QDir* dr = new QDir("certs");
    if(dr->exists()){
        QFile* fl = new QFile("certs/"+dns+".crt");
        fl->deleteLater();
        if(fl->exists()) return;
    }else{
        QDir().mkdir("certs");
    }
    delete dr;

    EVP_PKEY    *privkey, *pubkey;
    X509        *newcert, *cacert;
    X509_NAME   *name;
    X509_REQ    *certreq = nullptr;

    BIO         *cerbio = nullptr;
    BIO         *keybio = nullptr;
    BIO         *outbio = nullptr;
    // Запрос на создание сертификата
    keybio = BIO_new_mem_buf(m_servKey, -1);

    privkey = PEM_read_bio_PrivateKey(keybio, nullptr, nullptr, nullptr);

    certreq = X509_REQ_new();
    X509_REQ_set_version(certreq, 2);

    name = X509_REQ_get_subject_name(certreq);
    X509_NAME_add_entry_by_txt(name,"C", MBSTRING_ASC, (const unsigned char*)"FF", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name,"ST", MBSTRING_ASC, (const unsigned char*)"BB", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name,"L", MBSTRING_ASC, (const unsigned char*)"CC", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name,"O", MBSTRING_ASC, (const unsigned char*)"BANDIT", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name,"CN", MBSTRING_ASC, (const unsigned char*)dns.toStdString().c_str(), -1, -1, 0);
    X509_REQ_set_pubkey(certreq, privkey);
    X509_REQ_sign(certreq, privkey, EVP_sha256());

    EVP_PKEY_free(privkey);
    BIO_free_all(keybio);
    // Создание буфферов для сертификатов и ключа
    outbio = BIO_new_file(("certs/"+dns+".crt").toStdString().c_str(),"w");

    cerbio = BIO_new_mem_buf(m_rootCert, -1);
    cacert = PEM_read_bio_X509(cerbio, nullptr, nullptr, nullptr);

    keybio = BIO_new_mem_buf(m_rootKey, -1);
    privkey = PEM_read_bio_PrivateKey(keybio, nullptr, nullptr, nullptr);
    // Создаем сертификат с данными запроса
    newcert=X509_new();
    X509_set_version(newcert, 2);

    long hash = 0;
    for(auto i : dns) hash = static_cast<long>(i.toLatin1()) + (hash << 6) + (hash << 16) - hash;
    ASN1_INTEGER_set(X509_get_serialNumber(newcert), hash+time(nullptr));

    name = X509_REQ_get_subject_name(certreq);
    X509_set_subject_name(newcert, name);

    name = X509_get_subject_name(cacert);
    X509_set_issuer_name(newcert, name);

    pubkey=X509_REQ_get_pubkey(certreq);
    X509_set_pubkey(newcert, pubkey);

    X509_gmtime_adj(X509_get_notBefore(newcert),0);
    X509_gmtime_adj(X509_get_notAfter(newcert), 3153600000);
    X509_EXTENSION *ext;
    QString st;
    ext = X509V3_EXT_conf_nid(nullptr, nullptr, NID_subject_alt_name, (char*)(QString("DNS:%1").arg(dns).toStdString().c_str()));
    X509_add_ext(newcert, ext, -1);
    X509_EXTENSION_free(ext);
    X509_sign(newcert, privkey, EVP_sha256());

    PEM_write_bio_X509(outbio, newcert);

    EVP_PKEY_free(pubkey);
    EVP_PKEY_free(privkey);
    X509_REQ_free(certreq);
    X509_free(newcert);
    BIO_free_all(cerbio);
    BIO_free_all(keybio);
    BIO_free_all(outbio);
}

const char ProxyServerThread::m_servKey[] =
"-----BEGIN RSA PRIVATE KEY-----\n\
MIIEpQIBAAKCAQEAv0D+/IsNp5t75LvxRTttzLgfRcXJrx382sd/75LMrROKO9Q+\n\
Qawz4Ig0+ybrvEXDuYfkFVd3cvheovOgxLv3q0WqmgqsZRzPobvw/nWplRkC5R1v\n\
bov8JDm5/9lpIfaSBDHgvlGiGuWdq7Nb+azMPE/R0zmmwebMnP61BKadWZn+Wzyy\n\
qVAYgzXk5uNU5rfdZ2gNEqm4Zc2rin3ZvuB2kmfwSazuvVrO0KXCuBTx37NLN773\n\
gPg2o8l1CTi0mlXRcGLjACdrapFkGq1T9BRSEbxiOFlNXZ4OsjwKJ2KtDRw6igte\n\
zDbXk8IaZ2HgrgIOdD8HVRKWb/rdBDTa6lUUNwIDAQABAoIBADLn9fb6fDP0qtGD\n\
RQEMhHlNOOW1c61s6fTBzUfTZy9aWBz8rWWFlHlbR97hyTVfzG0Bwq/7jAvKGEZ/\n\
WneDILUx4esGlESFmq6N++N/JhhNg/vADfz6va5Wvy9eiyFDJDKBpDSEPgWywHkH\n\
UJ/wYpxbEpqKadyKtkPDeCM4xgEnCc4FhvaaKe/srdTs3ct1aTJ3OkZqaPWPkpVL\n\
ETosZ2LvhKPBdLUSoSDqKgn5B1Ga2K3unXPrKrClGV9mPPhum4F2v8hmXomGMSsU\n\
nCUtcHfi0WyogjVgQc76Sa95L85wurkgjHtMHq+D9XU3l7jyXVT3i0TfIopiSc6A\n\
SlAVe4ECgYEA8eUMtGN3wv7BL4yvRAnwYlNf1UCxmVroxXuilJ4+5dUo/0/Mxt6F\n\
QdoM725Yzhjz5mJ1uzZqic2nN2KTjbkrlMJQR9Na39VFe6AbCuJvz9m9ZJnd8AvJ\n\
Ji0n8ZErBEWa4+2VhQ3xIjBgUx7aKBVe1CBkasdHwiuucmgrb1d85wcCgYEAymf9\n\
ydmU76N4jpXdJZRaOfq5qem7yN5F0FNWcuWcjYUzjzBhDw+gGXG0ql7hoadecXFT\n\
ooNeNJ4EgggYrBgdywgWsm77weJcmUVcd4jm73Gm6xOugr50AMZpnYVx9glre+sh\n\
w/eOh17/ylGODBAQ2Zij1sfBNezC7umcOOh+bVECgYEAw78hGtDrV2akmn/0TNDm\n\
MAtcH8wsa/c/KqA0HqQ4LfDjLkg7ZCsqFvIHSbI4Nv/GT4cZ0pfkewNq78zbrvJL\n\
rCPap2UHwt5pTfNwLsyywrZnJwPEr7451qTwD/Hzp49UGmJmfrebjJktOdZbn9g7\n\
VtmnqSj+jsiecIKPs5hOwP0CgYEAh+AVm9pnxBNuS/HI+oBDraZl2u2gdTDZlwdL\n\
Aminls1zlk046t5ncI1HZhO5zkZH/UnBhlg/9nyZtBzxSx4JahGtAu3ELhziYrYQ\n\
Y+JQfS9DiY212ek2gIqxveAmZ69dK1vmV+KpGLEqb8wd2nUyipCQdA+24ZdmKPJE\n\
QGO683ECgYEAlhFf+AJKTxNgOZlI72G2NL6dfHR05nDEjFXye7O3fh2UwmWz+zu7\n\
/PuM64sFUlxBaUWcSk7LzJ/Y/HEwUlaidSdn1kbhP37eBxCv+YRp3HE3Bqz6ppv8\n\
L4KLv51CuB/o6C8840hj3TXi53nlknoyXPH+8aKYrygL0JslzxeQC2I=\n\
-----END RSA PRIVATE KEY-----";

const char ProxyServerThread::m_rootKey[] =
"-----BEGIN RSA PRIVATE KEY-----\n\
MIIEogIBAAKCAQEAxajxHUWrIrmvHRSjPXAfGNP3UBK9H8TIJsYOmuX4P06mV3x9\n\
4P9QY87L03FryEFRwskCPo+uoeDvmLF0Gou+h/WUpSxyCLVTCgC//k7h2hQJ2Ta+\n\
qFU/51lxW/Gd4bFW4dS3wsl53LmfXFZIkmE+f4BTdR0dyMoTsdIMgzlbKwzGdnkX\n\
Ip9zWJ9eJrFYhw7jk0CTgF1N6IO/fe8KpGx2tujHbAeAztOUTVZYW3sjkcvnsMXK\n\
Bn6fMrFPKIYLzaG6aFTrectjDvQVZrGK12VzzECsBP81EAncBORlx+kf6uvrpbvv\n\
ok+CB4C/OM9yfSxUhXreL374mBkU8fV0BssK1wIDAQABAoIBAHBlwC5IFqfZdPWR\n\
bb4bCuPgbOrwrPDqwnIh/94fVYoSXE61cRNHu9v1lTw4a/TlB+w+x3Lx23kb4sMu\n\
bXpG6uJ/SLagTnwbOAwhHwwqqQ2MhYkqM0Kfr4YL+4UwIPpdEK28e35deVmFiFRG\n\
kGAEwjhYrZyUIQKzvn1Il2rra6dPTZrT3u74PtFrYXT/8OwHmZsDjvbiavwBNhSl\n\
NsaA9m7AErfqNtbNdOgyeNJAElQ/qXLhiymPvgsljCUBF1eCo6q0GUBh/FayQFSm\n\
hPf7vMRNpJRNe0AczdArVSbez+76g1QGyMtBo5336biCCR6ZWywLeW9v9+4e7p4t\n\
5+k7dXkCgYEA4ViUEO/5B72M4uJ4AbHPMhnLGaeu1WYs8M2JZFTY8Of58KvaVojL\n\
RO2uQSKYZdKH7lvB8x4/RzlJ3AbHWRLichH4AiHMWImkyJadkbiI83JyUFBN2QEs\n\
unc4zHcJFgj/2SVY4cSkW0U5xK4o9Ex59tZO6oCfE+742CUyd7OaAIUCgYEA4Iw6\n\
EYGCS799+xT0CO2MiUqaGLs8o8FySoKR0tN/POmr/zRRJD/AeO+DC1ALAyAWy9wy\n\
a5RtpVEmKxbT8VNiXdYICL1/AVau9mBgZ3PbmKlBEDuQr2gaZjg28QBy9il75MoG\n\
i78VT6lkkS041w+9wAJlce24L5O/3MI79guhiqsCgYBLyXearqppQQWV9KZ5o2xU\n\
pDobObriCEgLAIU5mhOQCeSUXafDvKPoXatiOplYfVK8Bl4XPs/3Szwc5Ka8vU4t\n\
IP/w758DE9+4mncJ4C8m+RGbEzbrSaraV2hh05LZt1Mcm6Pl4jWIgKKqFAywBNxr\n\
+K13zqQxlwhX4UK04VFk5QKBgBd8s2o3WLChpATKSUhRyJxnsDycARjD2DeQ9r3N\n\
n4Z4jqQQDdizzmcX7mYVhkFabuf97UwxZ1KebVaeeabJaQWVqJt5brpuHbjplcvc\n\
Y4DRW1veTyD5y6EbiiulN2EmL65br1mYsBr4BevlhgeAvwFBfujuy5A2bSAjNPBk\n\
gQ4nAoGAQq6KBniVLhVKi0ELwNyoX0TjY51qHnTrIJFIkvUhesLjwErhswf7Wtr5\n\
txUBltjHXGzHA5Rry8XO4CooJcV7ML5ksdFHzJSHVygnSk4NUTU6p2Y43f9QQzc6\n\
ZjNEl1U1q8lKO7FzNC/6VM4TKzr2+jhxWAUb4O+RuTgl5S5Jy+I=\n\
-----END RSA PRIVATE KEY-----";

const char ProxyServerThread::m_rootCert[] =
"-----BEGIN CERTIFICATE-----\n\
MIIDjDCCAnSgAwIBAgIJAOy12GVm/6fiMA0GCSqGSIb3DQEBCwUAMFoxCzAJBgNV\n\
BAYTAkZGMQswCQYDVQQIDAJCQjELMAkGA1UEBwwCQ0MxDzANBgNVBAoMBkJBTkRJ\n\
VDEPMA0GA1UECwwGQkFORElUMQ8wDQYDVQQDDAZCQU5ESVQwIBcNMTgxMjE4MTQ1\n\
ODUwWhgPMjExODExMjQxNDU4NTBaMFoxCzAJBgNVBAYTAkZGMQswCQYDVQQIDAJC\n\
QjELMAkGA1UEBwwCQ0MxDzANBgNVBAoMBkJBTkRJVDEPMA0GA1UECwwGQkFORElU\n\
MQ8wDQYDVQQDDAZCQU5ESVQwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIB\n\
AQDFqPEdRasiua8dFKM9cB8Y0/dQEr0fxMgmxg6a5fg/TqZXfH3g/1BjzsvTcWvI\n\
QVHCyQI+j66h4O+YsXQai76H9ZSlLHIItVMKAL/+TuHaFAnZNr6oVT/nWXFb8Z3h\n\
sVbh1LfCyXncuZ9cVkiSYT5/gFN1HR3IyhOx0gyDOVsrDMZ2eRcin3NYn14msViH\n\
DuOTQJOAXU3og7997wqkbHa26MdsB4DO05RNVlhbeyORy+ewxcoGfp8ysU8ohgvN\n\
obpoVOt5y2MO9BVmsYrXZXPMQKwE/zUQCdwE5GXH6R/q6+ulu++iT4IHgL84z3J9\n\
LFSFet4vfviYGRTx9XQGywrXAgMBAAGjUzBRMB0GA1UdDgQWBBT7hRO1vtkMORYa\n\
Rvqd4ylE9BnjxTAfBgNVHSMEGDAWgBT7hRO1vtkMORYaRvqd4ylE9BnjxTAPBgNV\n\
HRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBCwUAA4IBAQAmoS6cEdhJFJZhlufjORxy\n\
qx1E+IJh5USmwoL+vFNWOkYvOAWr0DKHaDQ+qYnbWcOlT4ByXHSXqNciUpZi1FSv\n\
r3fB8w2sonUL1KWcGtIL5EOCyRMkRTQJN7vFC5TeTRXt61innJSwei4BA52Iu45N\n\
GsCQswIHQ9ga6djxqRpqPJ8fDyC1zdnwradU7y/miHYeB1sdJYURePPA2fJzWreo\n\
r1jzqnOKslZGQUx5+6+ZmlVM1AdZGRcRDnISUb6xbGtbZj6zdAOAKzH3iZhJEYXe\n\
oJpujKZ7A4rq44V4t+iYhpNIZ43beS7MYd2In++yvHl+ckh63b9fhQG4ixqf+IeR\n\
-----END CERTIFICATE-----";

const char ProxyServerThread::m_respClose[] =
"HTTP/1.1 200 OK\r\n"
"Connection: close\r\n\r\n"
"<html>\
    <head>\
        <title>\
            Closed\
        </title>\
    </head>\
    <body style=\"background-color: #2c3e50\">\
        <div>\
            <svg xmlns=\"http://www.w3.org/2000/svg\" x=\"0px\" y=\"0px\"\
            width=\"200\" height=\"200\"\
            viewBox=\"0 0 223 223\"\
            style=\" fill:#000000;position: fixed;left: 50%;top:50%;margin-top: -100px;margin-left: -100px;\"><g fill=\"none\" fill-rule=\"nonzero\" stroke=\"none\" stroke-width=\"1\" stroke-linecap=\"butt\" stroke-linejoin=\"miter\" stroke-miterlimit=\"10\" stroke-dasharray=\"\" stroke-dashoffset=\"0\" font-family=\"none\" font-weight=\"none\" font-size=\"none\" text-anchor=\"none\" style=\"mix-blend-mode: normal\"><path d=\"M0,223.99307v-223.99307h223.99307v223.99307z\" fill=\"none\"></path><g fill=\"#27ae60\"><path d=\"M111.5,22.3c-49.17575,0 -89.2,40.02425 -89.2,89.2c0,49.17575 40.02425,89.2 89.2,89.2c49.17575,0 89.2,-40.02425 89.2,-89.2c0,-49.17575 -40.02425,-89.2 -89.2,-89.2zM111.5,37.16667c41.14121,0 74.33333,33.19212 74.33333,74.33333c0,41.14121 -33.19212,74.33333 -74.33333,74.33333c-41.14121,0 -74.33333,-33.19212 -74.33333,-74.33333c0,-41.14121 33.19212,-74.33333 74.33333,-74.33333zM78.05,89.2c-6.15797,0 -11.15,4.99203 -11.15,11.15c0,6.15797 4.99203,11.15 11.15,11.15c6.15797,0 11.15,-4.99203 11.15,-11.15c0,-6.15797 -4.99203,-11.15 -11.15,-11.15zM144.95,89.2c-6.15797,0 -11.15,4.99203 -11.15,11.15c0,6.15797 4.99203,11.15 11.15,11.15c6.15797,0 11.15,-4.99203 11.15,-11.15c0,-6.15797 -4.99203,-11.15 -11.15,-11.15zM111.5,126.36667c-12.31703,0 -22.3,17.4163 -22.3,29.73333c0,12.31703 9.98297,14.86667 22.3,14.86667c12.31703,0 22.3,-2.54963 22.3,-14.86667c0,-12.31703 -9.98297,-29.73333 -22.3,-29.73333z\"></path></g></g></svg>\
        </div>\
        <h1 style=\"position: fixed;left: 50%;top:70%;margin-left: -160px;font-size: 24px; font-family: sans-serif;color:#9fa6ad;\">Connection closed by proxy</h1>\
    </body>\
</html>";

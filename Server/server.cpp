#include "qmutex.h"
#include <QDebug>
#include <QStringBuilder>
#include <QTimer>

#include "server.h"
#include "sending.h"
#include "enums.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QCoreApplication>
#include <QDir>

QList<QTcpSocket *> server::Sockets;
QMutex server::mutex;



server::server() {

    Read_Config(); // reading config flie

    if (this->listen(addressEnum, server_port)) {
        qDebug() << "Server started on port 2323";
    } else {
        qDebug() << "Error starting server";
    }
}

void server::setSending(Sending &sending) {
    sendingPtr = &sending;
    connect(this, &server::newClientConnected, sendingPtr, &Sending::Get_New_Client);
    connect(this, &server::disconnectedClient, sendingPtr, &Sending::Get_Disconnected_Client);
    connect(this, &server::sendingMesage, sendingPtr, &Sending::sendToSocket);
}

void server::incomingConnection(qintptr socketDescriptor) {
    QTcpSocket *socket = new QTcpSocket(this);
    if (socket->setSocketDescriptor(socketDescriptor)) {
        QString clientIP = socket->peerAddress().toString();
        qDebug() << "Client connected from IP:" << clientIP
                 << "with deck:" << socketDescriptor;

        Sockets.push_back(socket);
        qDebug() << "Socket :" << socket->peerPort();

        QString IP = socket->peerAddress().toString();

        connect(socket, &QTcpSocket::readyRead, this, &server::slotsReadyRead);
        connect(socket, &QTcpSocket::disconnected, this,
                [this, socketDescriptor, IP]() {
            qDebug() << "disconnected dest form server" << socketDescriptor;
            emit disconnectedClient(socketDescriptor, IP);
        });
        qDebug() << "Client connected, socket descriptor:" << socketDescriptor;

        emit newClientConnected(socket, Sockets);
    }
}

void server::slotsReadyRead() {
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (socket) {
        QDataStream in(socket);
        in.setVersion(QDataStream::Qt_6_0);

        if (in.status() == QDataStream::Ok) {
            qDebug() << "Reading data...";
            QString str;
            in >> str;

            QString Identifier = str.left(1);
            QStringList parts = str.split(",");
            int messageType = parts[0].toInt();

            if (messageType == MESAGE) {
                QTcpSocket *RESIVER = nullptr; // Указатель на сокет получателя
                QTcpSocket *SENDER = nullptr; // Указатель на сокет отправителя
                QString TEXT = parts[3];

                for (int i = 0; i < Sockets.size(); ++i) {
                    if (Sockets[i]->socketDescriptor() == parts[1].toLongLong())
                        RESIVER = Sockets[i];
                    if (Sockets[i]->socketDescriptor() == parts[2].toLongLong())
                        SENDER = Sockets[i];
                }

                QByteArray data;
                QDataStream out(&data, QIODevice::WriteOnly);
                out.setVersion(QDataStream::Qt_6_0);
                QString message = QString("%1,%2,%3,%4")
                                      .arg(MESAGE)
                                      .arg(QString::number(RESIVER->socketDescriptor()))
                                      .arg(QString::number(SENDER->socketDescriptor()))
                                      .arg(TEXT);
                out << message;
                emit sendingMesage(RESIVER, message);
            }
        }
    }
}

void server::Read_Config() {

    QString appDir = QCoreApplication::applicationDirPath();
    QDir baseDir(appDir);
    baseDir.cdUp(); // Поднимаемся к папке build
    baseDir.cdUp(); // Поднимаемся к папке Client
    baseDir.cdUp();

    QString filePath = baseDir.filePath("config.json");
    qDebug() << filePath;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Error open config";
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    // Парсим JSON
    QJsonDocument config_json = QJsonDocument::fromJson(data);

    if (config_json.isNull()) {
        qDebug() << "Error work with config";
        return;
    }

    QJsonObject config_obj = config_json.object();

    QString server_channel = config_obj.value("Settings").toObject().value("host-adres").toString();
    server_port = config_obj.value("Settings").toObject().value("server-port").toInt();

    if(server_channel == "Any") addressEnum = QHostAddress::Any;

    qDebug() << server_channel;
    qDebug() << server_port;
}

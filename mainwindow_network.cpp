#include "mainwindow.h"
#include "constants.h"

#include <QAbstractSocket>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkInterface>
#include <QPushButton>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTextEdit>

// ==================== 网络工具栏 ====================

QWidget *MainWindow::createNetworkBar()
{
    auto *w = new QWidget;
    w->setFixedHeight(38);
    w->setStyleSheet(QString("background: %1; border-bottom: 1px solid #e4e4e4;").arg(kToolbarBg));

    auto *lay = new QHBoxLayout(w);
    lay->setContentsMargins(20, 4, 20, 3);
    lay->setSpacing(6);

    // 昵称
    auto *nameLabel = new QLabel("昵称:");
    nameLabel->setStyleSheet("font-size: 12px; color: #888; border: none;");
    m_localNameEdit = new QLineEdit;
    m_localNameEdit->setText("用户");
    m_localNameEdit->setFixedWidth(80);
    m_localNameEdit->setFixedHeight(26);
    m_localNameEdit->setStyleSheet(QString(
        "QLineEdit { border: 1px solid #d0d0d0; border-radius: 3px;"
        " padding: 2px 6px; background: white; font-size: 12px; }"
        "QLineEdit:focus { border-color: %1; }"
    ).arg(kBtnBlue));

    auto *sep0 = new QFrame;
    sep0->setFrameShape(QFrame::VLine);
    sep0->setStyleSheet("color: #d0d0d0;");

    // 端口
    auto *portLabel = new QLabel("端口:");
    portLabel->setStyleSheet("font-size: 12px; color: #888; border: none;");

    m_portEdit = new QLineEdit;
    m_portEdit->setText("12345");
    m_portEdit->setFixedWidth(58);
    m_portEdit->setFixedHeight(26);
    m_portEdit->setStyleSheet(QString(
        "QLineEdit { border: 1px solid #d0d0d0; border-radius: 3px;"
        " padding: 2px 6px; background: white; font-size: 12px; }"
        "QLineEdit:focus { border-color: %1; }"
    ).arg(kBtnBlue));

    m_btnListen = new QPushButton("启动监听");
    m_btnListen->setFixedSize(72, 26);
    m_btnListen->setCursor(Qt::PointingHandCursor);
    m_btnListen->setStyleSheet(QString(
        "QPushButton { background: #27ae60; color: white; border: none;"
        " border-radius: 3px; font-size: 12px; }"
        "QPushButton:hover { background: #219a52; }"
    ));

    m_btnStopListen = new QPushButton("停止");
    m_btnStopListen->setFixedSize(48, 26);
    m_btnStopListen->setEnabled(false);
    m_btnStopListen->setCursor(Qt::PointingHandCursor);
    m_btnStopListen->setStyleSheet(QString(
        "QPushButton { background: #e74c3c; color: white; border: none;"
        " border-radius: 3px; font-size: 12px; }"
        "QPushButton:hover { background: #c0392b; }"
        "QPushButton:disabled { background: #ccc; }"
    ));

    auto *sep1 = new QFrame;
    sep1->setFrameShape(QFrame::VLine);
    sep1->setStyleSheet("color: #d0d0d0;");

    // 连接对方
    auto *addrLabel = new QLabel("连接:");
    addrLabel->setStyleSheet("font-size: 12px; color: #888; border: none;");

    m_peerAddrEdit = new QLineEdit;
    m_peerAddrEdit->setPlaceholderText("IP:端口 (如 127.0.0.1:12345)");
    m_peerAddrEdit->setFixedHeight(26);
    m_peerAddrEdit->setStyleSheet(QString(
        "QLineEdit { border: 1px solid #d0d0d0; border-radius: 3px;"
        " padding: 2px 8px; background: white; font-size: 12px; }"
        "QLineEdit:focus { border-color: %1; }"
    ).arg(kBtnBlue));

    m_btnConnectPeer = new QPushButton("连接");
    m_btnConnectPeer->setFixedSize(48, 26);
    m_btnConnectPeer->setCursor(Qt::PointingHandCursor);
    m_btnConnectPeer->setStyleSheet(QString(
        "QPushButton { background: %1; color: white; border: none;"
        " border-radius: 3px; font-size: 12px; }"
        "QPushButton:hover { background: %2; }"
    ).arg(kBtnBlue, kBtnBlueHover));

    auto *sep2 = new QFrame;
    sep2->setFrameShape(QFrame::VLine);
    sep2->setStyleSheet("color: #d0d0d0;");

    m_netStatusLabel = new QLabel("网络: 未启动");
    m_netStatusLabel->setStyleSheet("font-size: 12px; color: #999; border: none;");

    lay->addWidget(nameLabel);
    lay->addWidget(m_localNameEdit);
    lay->addWidget(sep0);
    lay->addWidget(portLabel);
    lay->addWidget(m_portEdit);
    lay->addWidget(m_btnListen);
    lay->addWidget(m_btnStopListen);
    lay->addWidget(sep1);
    lay->addWidget(addrLabel);
    lay->addWidget(m_peerAddrEdit, 1);
    lay->addWidget(m_btnConnectPeer);
    lay->addWidget(sep2);
    lay->addWidget(m_netStatusLabel);
    lay->addStretch();

    return w;
}

// ==================== 网络槽实现 ====================

QStringList MainWindow::localIPs() const
{
    QStringList ips;
    const QList<QHostAddress> addrs = QNetworkInterface::allAddresses();
    for (const auto &addr : addrs) {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol &&
            !addr.isLoopback() && !addr.isLinkLocal()) {
            ips.append(addr.toString());
        }
    }
    if (ips.isEmpty()) ips.append("127.0.0.1");
    return ips;
}

void MainWindow::onStartListen()
{
    bool ok;
    quint16 port = m_portEdit->text().toUShort(&ok);
    if (!ok || port == 0) {
        QMessageBox::warning(this, "错误", "请输入有效的端口号 (1-65535)");
        return;
    }
    m_listenPort = port;

    if (!m_tcpServer) {
        m_tcpServer = new QTcpServer(this);
        connect(m_tcpServer, &QTcpServer::newConnection, this, &MainWindow::onNewConnection);
    }

    if (m_tcpServer->isListening()) {
        m_tcpServer->close();
    }

    if (m_tcpServer->listen(QHostAddress::Any, m_listenPort)) {
        m_btnListen->setEnabled(false);
        m_btnStopListen->setEnabled(true);
        m_portEdit->setEnabled(false);
        updateNetworkStatus();

        QStringList ips = localIPs();
        m_statusBar->setText(QString::fromUtf8(
            "\xF0\x9F\x8C\x90 正在监听端口 %1 | 本机: %2 | 等待好友连接..."
        ).arg(m_listenPort).arg(ips.join(", ")));
    } else {
        QMessageBox::warning(this, "错误",
            QString("无法在端口 %1 上启动监听:\n%2")
            .arg(m_listenPort)
            .arg(m_tcpServer->errorString()));
    }
}

void MainWindow::onStopListen()
{
    for (auto it = m_peerSockets.begin(); it != m_peerSockets.end(); ++it) {
        it.value()->close();
    }
    qDeleteAll(m_peerSockets);
    m_peerSockets.clear();

    if (m_tcpServer) {
        m_tcpServer->close();
    }

    m_btnListen->setEnabled(true);
    m_btnStopListen->setEnabled(false);
    m_portEdit->setEnabled(true);
    updateNetworkStatus();
    m_statusBar->setText(QString::fromUtf8("\xE2\x9A\xA0 网络监听已停止"));
}

void MainWindow::onConnectPeer()
{
    QString text = m_peerAddrEdit->text().trimmed();
    if (text.isEmpty()) return;

    QString host = "127.0.0.1";
    quint16 port = m_listenPort;

    if (text.contains(':')) {
        int idx = text.lastIndexOf(':');
        host = text.left(idx).trimmed();
        bool ok;
        quint16 p = text.mid(idx + 1).trimmed().toUShort(&ok);
        if (ok && p > 0) port = p;
    } else {
        host = text;
    }

    auto *socket = new QTcpSocket(this);
    connect(socket, &QTcpSocket::readyRead,    this, &MainWindow::onReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &MainWindow::onPeerDisconnected);
    connect(socket, &QTcpSocket::connected, this, [this, socket]() {
        QString localName = m_localNameEdit->text().trimmed();
        if (localName.isEmpty()) localName = "用户";
        QString nameMsg = "NAME:" + localName + "\n";
        socket->write(nameMsg.toUtf8());
        m_statusBar->setText(QString("已连接到 %1:%2，等待对方确认...")
            .arg(socket->peerAddress().toString())
            .arg(socket->peerPort()));
    });

    typedef void (QAbstractSocket::*QAbstractSocketError)(QAbstractSocket::SocketError);
    connect(socket, static_cast<QAbstractSocketError>(&QAbstractSocket::errorOccurred),
            this, [this, host, port, socket](QAbstractSocket::SocketError) {
        m_statusBar->setText(QString("连接失败: %1:%2 - %3")
            .arg(host).arg(port).arg(socket->errorString()));
    });

    socket->connectToHost(host, port);
    m_peerAddrEdit->clear();
    m_statusBar->setText(QString("正在连接 %1:%2 ...").arg(host).arg(port));
}

void MainWindow::onNewConnection()
{
    while (m_tcpServer && m_tcpServer->hasPendingConnections()) {
        QTcpSocket *socket = m_tcpServer->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead,    this, &MainWindow::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &MainWindow::onPeerDisconnected);

        typedef void (QAbstractSocket::*QAbstractSocketError)(QAbstractSocket::SocketError);
        connect(socket, static_cast<QAbstractSocketError>(&QAbstractSocket::errorOccurred),
                this, [this, socket](QAbstractSocket::SocketError) {
            m_statusBar->setText(QString("连接错误: %1").arg(socket->errorString()));
        });

        m_statusBar->setText(QString("新连接来自 %1:%2")
            .arg(socket->peerAddress().toString())
            .arg(socket->peerPort()));
    }
}

void MainWindow::onReadyRead()
{
    auto *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    while (socket->canReadLine()) {
        QString line = QString::fromUtf8(socket->readLine()).trimmed();
        if (line.isEmpty()) continue;

        if (line.startsWith("NAME:")) {
            QString peerName = line.mid(5).trimmed();
            if (peerName.isEmpty()) peerName = "未知";

            if (m_peerSockets.contains(peerName)) {
                QTcpSocket *old = m_peerSockets.take(peerName);
                old->close();
                old->deleteLater();
            }
            m_peerSockets[peerName] = socket;
            updateNetworkStatus();
            m_statusBar->setText(QString::fromUtf8(
                "\xF0\x9F\x94\x97 已与 %1 建立连接").arg(peerName));

            if (m_chatDisplay && m_currentChatPeer == peerName) {
                m_chatDisplay->append(QString(
                    "<div style='text-align:center; color:#27ae60; font-size:12px; margin:8px 0;'>"
                    "%1 已上线，消息将通过网络发送</div>"
                ).arg(peerName));
            }
        }
        else if (line.startsWith("MSG:")) {
            QString msgText = line.mid(4);
            QString peerName;
            for (auto it = m_peerSockets.begin(); it != m_peerSockets.end(); ++it) {
                if (it.value() == socket) {
                    peerName = it.key();
                    break;
                }
            }
            displayReceivedMessage(peerName.isEmpty() ? "未知" : peerName, msgText);
        }
    }
}

void MainWindow::onPeerDisconnected()
{
    auto *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QString peerName;
    for (auto it = m_peerSockets.begin(); it != m_peerSockets.end(); ++it) {
        if (it.value() == socket) {
            peerName = it.key();
            m_peerSockets.erase(it);
            break;
        }
    }

    socket->deleteLater();
    updateNetworkStatus();

    if (!peerName.isEmpty()) {
        m_statusBar->setText(QString("%1 已断开连接").arg(peerName));
        if (m_chatDisplay && m_currentChatPeer == peerName) {
            m_chatDisplay->append(QString(
                "<div style='text-align:center; color:#e74c3c; font-size:12px; margin:8px 0;'>"
                "%1 已断开连接，消息将切换为本地模拟</div>"
            ).arg(peerName));
        }
    }
}

// ==================== 网络消息发送与接收 ====================

void MainWindow::sendChatMessage(const QString &peerName, const QString &text)
{
    if (!m_peerSockets.contains(peerName)) return;

    QTcpSocket *socket = m_peerSockets[peerName];
    QString msg = "MSG:" + text + "\n";
    socket->write(msg.toUtf8());

    if (m_chatDisplay) {
        QString myName = m_localNameEdit->text().trimmed();
        if (myName.isEmpty()) myName = "我";
        m_chatDisplay->append(makeBubble(text, true, myName, kBtnBlue));
    }
}

void MainWindow::displayReceivedMessage(const QString &peerName, const QString &text)
{
    if (m_chatDisplay && m_currentChatPeer == peerName) {
        m_chatDisplay->append(makeBubble(text, false, peerName, m_currentPeerColor));
    } else {
        m_statusBar->setText(QString::fromUtf8(
            "\xF0\x9F\x93\xA9 %1 发来消息: %2"
        ).arg(peerName, text.left(30)));
    }
}

void MainWindow::updateNetworkStatus()
{
    bool listening = m_tcpServer && m_tcpServer->isListening();
    int peerCount = m_peerSockets.size();

    if (listening && peerCount > 0) {
        m_netStatusLabel->setText(QString::fromUtf8(
            "\xF0\x9F\x9F\xA2 监听:%1 | 已连接:%2人")
            .arg(m_listenPort).arg(peerCount));
        m_netStatusLabel->setStyleSheet("font-size: 12px; color: #27ae60; border: none; font-weight: bold;");
    } else if (listening) {
        m_netStatusLabel->setText(QString::fromUtf8(
            "\xF0\x9F\x9F\xA1 监听中:%1 | 等待连接...").arg(m_listenPort));
        m_netStatusLabel->setStyleSheet("font-size: 12px; color: #f39c12; border: none; font-weight: bold;");
    } else if (peerCount > 0) {
        m_netStatusLabel->setText(QString::fromUtf8(
            "\xF0\x9F\x94\xB5 已连接:%1人 (未监听)").arg(peerCount));
        m_netStatusLabel->setStyleSheet("font-size: 12px; color: #3498db; border: none; font-weight: bold;");
    } else {
        m_netStatusLabel->setText(QString::fromUtf8("\xE2\x9A\xAA 网络: 未启动"));
        m_netStatusLabel->setStyleSheet("font-size: 12px; color: #999; border: none;");
    }
}

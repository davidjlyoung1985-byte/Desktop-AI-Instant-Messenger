#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

// ============ 统一联系人数据模型 ============
// 集中管理：内置 AI 联系人、局域网 TCP 对等方、手动添加的好友
// 消除 mainwindow.cpp 中三处硬编码的联系人定义

struct Contact
{
    // 显示用名（DeepSeek, 李四, 王五... 或 TCP peer 昵称）
    QString displayName;

    // 唯一 ID，AI 联系人用 "ds"/"claude"，普通人用姓名，TCP peer 用 "tcp:ip:port"
    QString id;

    // 头像字母（如 "D", "李", "W"）
    QString avatarLetter;

    // 类型: "ai_claude", "ai_deepseek", "person", "tcp_peer"
    QString type;

    // 个性签名 / 最后一条消息摘要
    QString statusText;

    // 是否在线
    bool online = false;

    // TCP peer 专用字段
    QString peerAddress;
    int     peerPort = 0;
};

class ContactStore : public QObject
{
    Q_OBJECT

public:
    static ContactStore *instance();

    // ── 查询 ──
    const QVector<Contact> &allContacts()   const { return m_contacts; }
    int                      contactCount()  const { return m_contacts.size(); }
    const Contact           *byId(const QString &id) const;
    const Contact           *byDisplayName(const QString &name) const;
    int                      indexOf(const QString &id) const;

    // ── 修改 ──
    void addContact(const Contact &c);
    void updateContact(const QString &id, const Contact &c);
    void removeContact(const QString &id);

    // ── 快捷方法 ──
    void addTcpPeer(const QString &name, const QString &address, int port);
    void removeTcpPeer(const QString &name);
    void updateLastMessage(const QString &id, const QString &summary);

    // ── 持久化 ──
    void saveToFile();
    void loadFromFile();

signals:
    void contactsChanged();

private:
    explicit ContactStore(QObject *parent = nullptr);
    void loadDefaults();
    QString dataFilePath() const;

    static ContactStore *s_instance;
    QVector<Contact> m_contacts;
};

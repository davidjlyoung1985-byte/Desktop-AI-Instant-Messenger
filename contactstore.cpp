#include "contactstore.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

ContactStore *ContactStore::s_instance = nullptr;

ContactStore *ContactStore::instance()
{
    if (!s_instance) {
        s_instance = new ContactStore(qApp);
        s_instance->loadFromFile();
        if (s_instance->m_contacts.isEmpty())
            s_instance->loadDefaults();
    }
    return s_instance;
}

ContactStore::ContactStore(QObject *parent)
    : QObject(parent)
{
}

QString ContactStore::dataFilePath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/contacts.json";
}

void ContactStore::saveToFile()
{
    QJsonArray arr;
    for (const auto &c : m_contacts) {
        QJsonObject o;
        o["displayName"]   = c.displayName;
        o["id"]            = c.id;
        o["avatarLetter"]  = c.avatarLetter;
        o["type"]          = c.type;
        o["statusText"]    = c.statusText;
        o["online"]        = c.online;
        o["peerAddress"]   = c.peerAddress;
        o["peerPort"]      = c.peerPort;
        arr.append(o);
    }
    QFile f(dataFilePath());
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(arr).toJson());
}

void ContactStore::loadFromFile()
{
    QFile f(dataFilePath());
    if (!f.open(QIODevice::ReadOnly)) return;

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray()) return;

    m_contacts.clear();
    for (const auto &v : doc.array()) {
        QJsonObject o = v.toObject();
        Contact c;
        c.displayName  = o["displayName"].toString();
        c.id           = o["id"].toString();
        c.avatarLetter = o["avatarLetter"].toString();
        c.type         = o["type"].toString();
        c.statusText   = o["statusText"].toString();
        c.online       = o["online"].toBool(true);
        c.peerAddress  = o["peerAddress"].toString();
        c.peerPort     = o["peerPort"].toInt();
        m_contacts.append(c);
    }
}

void ContactStore::loadDefaults()
{
    // ── AI 联系人 ──

    m_contacts.append({
        QString::fromUtf8("DeepSeek"),
        "ds",
        "DS",
        "ai_deepseek",
        "DeepSeek AI — 耐心靠谱的技术朋友",
        true
    });

    m_contacts.append({
        QString::fromUtf8("\xe6\x9d\x8e\xe5\x9b\x9b"),  // 李四
        "claude",
        QString::fromUtf8("\xe6\x9d\x8e"),  // 李
        "ai_claude",
        QString::fromUtf8("Claude AI — 务实沉稳的好友"),
        true
    });

    // ── 普通联系人 ──
    m_contacts.append({
        QString::fromUtf8("\xe7\x8e\x8b\xe4\xba\x94"), "wangwu",  "王", "person", "今日天气不错 ☀",             true
    });
    m_contacts.append({
        QString::fromUtf8("\xe8\xb5\xb5\xe5\x85\xad"), "zhaoliu", "赵", "person", "最近在学 Qt6",                true
    });
    m_contacts.append({
        QString::fromUtf8("\xe5\x88\x98\xe5\xb7\xa5"), "liugong", "刘", "person", "全栈工程师 / 远程协作",        false
    });
    m_contacts.append({
        QString::fromUtf8("\xe9\x99\x88\xe5\xa7\x90"), "chenjie", "陈", "person", "UI/UX 设计师 @ 鹅厂",          true
    });
    m_contacts.append({
        QString::fromUtf8("\xe6\x9d\xa8\xe6\x80\xbb"), "yangzong", "杨", "person", "技术总监 — 代码不达标的别",   false
    });
    m_contacts.append({
        QString::fromUtf8("\xe5\x91\xa8\xe6\x98\x9f\xe6\x98\x9f"), "zhouxingxing","周", "person", "努力搬砖中…", false
    });
    m_contacts.append({
        QString::fromUtf8("\xe7\x8e\x8b\xe8\x80\x81\xe5\xb8\x88"),"wanglaoshi","师", "person", "数学老师 · 业余编程爱好者", true
    });
    m_contacts.append({
        QString::fromUtf8("\xe6\x9d\x8e\xe6\x9c\xba\xe5\x99\xa8\xe4\xba\xba"), "lirobot", "机", "person", "IoT 爱好者 · 树莓派玩家", false
    });
}

const Contact *ContactStore::byId(const QString &id) const
{
    for (const auto &c : m_contacts) {
        if (c.id == id)
            return &c;
    }
    return nullptr;
}

const Contact *ContactStore::byDisplayName(const QString &name) const
{
    for (const auto &c : m_contacts) {
        if (c.displayName == name)
            return &c;
    }
    return nullptr;
}

int ContactStore::indexOf(const QString &id) const
{
    for (int i = 0; i < m_contacts.size(); ++i) {
        if (m_contacts[i].id == id)
            return i;
    }
    return -1;
}

void ContactStore::addContact(const Contact &c)
{
    // 避免重复
    for (const auto &existing : m_contacts) {
        if (existing.id == c.id) {
            updateContact(c.id, c);
            return;
        }
    }
    m_contacts.append(c);
    saveToFile();
    emit contactsChanged();
}

void ContactStore::updateContact(const QString &id, const Contact &c)
{
    for (auto &existing : m_contacts) {
        if (existing.id == id) {
            existing = c;
            saveToFile();
            emit contactsChanged();
            return;
        }
    }
    addContact(c);
}

void ContactStore::removeContact(const QString &id)
{
    m_contacts.erase(
        std::remove_if(m_contacts.begin(), m_contacts.end(),
            [&](const Contact &c) { return c.id == id; }),
        m_contacts.end()
    );
    saveToFile();
    emit contactsChanged();
}

void ContactStore::addTcpPeer(const QString &name, const QString &address, int port)
{
    QString id = QString("tcp:%1:%2").arg(address).arg(port);
    Contact c;
    c.displayName = name;
    c.id          = id;
    c.avatarLetter = name.left(1);
    c.type         = "tcp_peer";
    c.statusText   = QString("局域网: %1:%2").arg(address).arg(port);
    c.online       = true;
    c.peerAddress  = address;
    c.peerPort     = port;
    updateContact(id, c);
}

void ContactStore::removeTcpPeer(const QString &name)
{
    for (auto &c : m_contacts) {
        if (c.type == "tcp_peer" && c.displayName == name) {
            m_contacts.erase(
                std::remove_if(m_contacts.begin(), m_contacts.end(),
                    [&](const Contact &x) { return x.id == c.id; }),
                m_contacts.end()
            );
            emit contactsChanged();
            return;
        }
    }
}

void ContactStore::updateLastMessage(const QString &id, const QString &summary)
{
    for (auto &c : m_contacts) {
        if (c.id == id) {
            c.statusText = summary;
            emit contactsChanged();
            return;
        }
    }
}

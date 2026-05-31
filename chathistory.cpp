#include "chathistory.h"

#include <QCoreApplication>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariant>

ChatHistory *ChatHistory::s_instance = nullptr;

ChatHistory *ChatHistory::instance()
{
    if (!s_instance) {
        s_instance = new ChatHistory(qApp);
        s_instance->initDatabase();
    }
    return s_instance;
}

ChatHistory::ChatHistory(QObject *parent)
    : QObject(parent)
{
}

void ChatHistory::initDatabase()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    m_dbPath = dir + "/chat_history.db";

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "chathistory");
    db.setDatabaseName(m_dbPath);

    if (!db.open()) {
        qWarning("ChatHistory: Failed to open database: %s",
                 qPrintable(db.lastError().text()));
        return;
    }

    QSqlQuery q(db);
    q.exec(
        "CREATE TABLE IF NOT EXISTS messages ("
        "  id              INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  conversation_id TEXT    NOT NULL,"
        "  sender_name     TEXT    NOT NULL,"
        "  content         TEXT    NOT NULL,"
        "  is_from_me      INTEGER NOT NULL DEFAULT 0,"
        "  created_at      TEXT    NOT NULL DEFAULT (datetime('now','localtime'))"
        ")"
    );
    q.exec("CREATE INDEX IF NOT EXISTS idx_conv ON messages(conversation_id, created_at)");
}

// ─── 保存 ─────────────────────────────────────────────────────────────

void ChatHistory::saveMessage(const ChatMessage &msg)
{
    QSqlDatabase db = QSqlDatabase::database("chathistory");
    if (!db.isOpen()) return;

    QSqlQuery q(db);
    q.prepare(
        "INSERT INTO messages (conversation_id, sender_name, content, is_from_me, created_at)"
        " VALUES (:cid, :sender, :content, :me, :ts)"
    );
    q.bindValue(":cid",     msg.conversationId);
    q.bindValue(":sender",  msg.senderName);
    q.bindValue(":content", msg.content);
    q.bindValue(":me",      msg.isFromMe ? 1 : 0);
    q.bindValue(":ts",      msg.timestamp.isValid()
                              ? msg.timestamp.toString("yyyy-MM-dd HH:mm:ss")
                              : QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    if (!q.exec()) {
        qWarning("ChatHistory: save failed: %s", qPrintable(q.lastError().text()));
    }
}

// ─── 加载 ─────────────────────────────────────────────────────────────

QVector<ChatMessage> ChatHistory::loadMessages(const QString &conversationId, int limit)
{
    QVector<ChatMessage> result;
    QSqlDatabase db = QSqlDatabase::database("chathistory");
    if (!db.isOpen()) return result;

    QSqlQuery q(db);
    q.prepare(
        "SELECT id, conversation_id, sender_name, content, is_from_me, created_at"
        " FROM messages WHERE conversation_id = :cid"
        " ORDER BY created_at ASC"
        " LIMIT :lim"
    );
    q.bindValue(":cid", conversationId);
    q.bindValue(":lim", limit);

    if (!q.exec()) return result;

    while (q.next()) {
        ChatMessage m;
        m.id             = q.value(0).toInt();
        m.conversationId = q.value(1).toString();
        m.senderName     = q.value(2).toString();
        m.content        = q.value(3).toString();
        m.isFromMe       = q.value(4).toBool();
        m.timestamp      = QDateTime::fromString(q.value(5).toString(), "yyyy-MM-dd HH:mm:ss");
        result.append(m);
    }
    return result;
}

// ─── 对话列表 ─────────────────────────────────────────────────────────

QStringList ChatHistory::allConversations() const
{
    QStringList convs;
    QSqlDatabase db = QSqlDatabase::database("chathistory");
    if (!db.isOpen()) return convs;

    QSqlQuery q(db);
    q.exec("SELECT DISTINCT conversation_id FROM messages ORDER BY conversation_id");
    while (q.next())
        convs.append(q.value(0).toString());
    return convs;
}

void ChatHistory::deleteConversation(const QString &conversationId)
{
    QSqlDatabase db = QSqlDatabase::database("chathistory");
    if (!db.isOpen()) return;

    QSqlQuery q(db);
    q.prepare("DELETE FROM messages WHERE conversation_id = :cid");
    q.bindValue(":cid", conversationId);
    q.exec();
}

// ─── 最近一条 ─────────────────────────────────────────────────────────

ChatMessage ChatHistory::lastMessage(const QString &conversationId) const
{
    ChatMessage m;
    QSqlDatabase db = QSqlDatabase::database("chathistory");
    if (!db.isOpen()) return m;

    QSqlQuery q(db);
    q.prepare(
        "SELECT id, sender_name, content, is_from_me, created_at"
        " FROM messages WHERE conversation_id = :cid"
        " ORDER BY created_at DESC LIMIT 1"
    );
    q.bindValue(":cid", conversationId);
    if (q.exec() && q.next()) {
        m.id             = q.value(0).toInt();
        m.conversationId = conversationId;
        m.senderName     = q.value(1).toString();
        m.content        = q.value(2).toString();
        m.isFromMe       = q.value(3).toBool();
        m.timestamp      = QDateTime::fromString(q.value(4).toString(), "yyyy-MM-dd HH:mm:ss");
    }
    return m;
}

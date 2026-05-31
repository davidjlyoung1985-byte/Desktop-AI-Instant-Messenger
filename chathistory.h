#pragma once
#include <QObject>
#include <QString>
#include <QDateTime>
#include <QVector>

// ============ 聊天历史持久化（SQLite）============

struct ChatMessage
{
    int      id = -1;
    QString  conversationId;  // 对话 ID（如 "ds", "claude", "tcp:192.168.1.5:12345"）
    QString  senderName;      // 发送者显示名
    QString  content;         // 消息内容（HTML）
    QDateTime timestamp;
    bool     isFromMe = false;
};

class ChatHistory : public QObject
{
    Q_OBJECT

public:
    static ChatHistory *instance();

    // ── 消息 CRUD ──
    void saveMessage(const ChatMessage &msg);
    QVector<ChatMessage> loadMessages(const QString &conversationId, int limit = 200);

    // ── 对话管理 ──
    QStringList allConversations() const;
    void        deleteConversation(const QString &conversationId);

    // ── 最近摘要 ──
    ChatMessage lastMessage(const QString &conversationId) const;

private:
    explicit ChatHistory(QObject *parent = nullptr);
    void initDatabase();

    static ChatHistory *s_instance;
    QString m_dbPath;
};

#pragma once
#include <QObject>
#include <QJsonArray>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

// ============ Claude AI 客户端 ============
// 预配置 cc-vibe.com 代理，兼容 OpenAI / Anthropic 两种 API 格式

class ClaudeClient : public QObject
{
    Q_OBJECT

public:
    explicit ClaudeClient(QObject *parent = nullptr);

    bool isConfigured() const { return m_configured; }
    QString apiKey()    const { return m_apiKey; }

    /// 弹出配置对话框，返回 true 表示配置成功
    bool showConfigDialog(QWidget *parent);

    /// 发送消息到 Claude API，自动维护对话历史
    void sendMessage(const QString &text);

    /// 发送文件内容（文本文件直接读取内容）
    void sendFileMessage(const QString &filePath, const QString &userText);

    /// 发送图片（base64 编码，走视觉模型）
    void sendImageMessage(const QString &imagePath, const QString &userText);

    /// 重设对话（清空历史，保留 system prompt）
    void resetConversation();

    /// 设置技能提示词后缀
    void setSkillPrompt(const QString &suffix);

signals:
    void thinking();
    void replyReceived(const QString &content);
    void errorOccurred(const QString &error);

private slots:
    void onReplyFinished();

private:
    QNetworkAccessManager *m_manager;
    QJsonArray  m_messages;
    QString     m_apiKey;
    QString     m_baseUrl;
    QString     m_model;
    QString     m_skillSuffix;
    bool        m_configured = false;
};

#pragma once
#include <QObject>
#include <QJsonArray>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

// ============ DeepSeek AI 客户端 ============
// 封装 API 配置、HTTP 请求、对话历史管理

class DeepSeekClient : public QObject
{
    Q_OBJECT

public:
    explicit DeepSeekClient(QObject *parent = nullptr);

    bool isConfigured() const { return m_configured; }
    QString apiKey()    const { return m_apiKey; }
    QString baseUrl()   const { return m_baseUrl; }
    QString model()     const { return m_model; }

    /// 弹出配置对话框，返回 true 表示配置成功
    bool showConfigDialog(QWidget *parent);

    /// 发送消息到 DeepSeek API，自动维护对话历史
    void sendMessage(const QString &text);

    /// 发送文件内容（文本文件直接读取内容；其他文件附带文件信息）
    void sendFileMessage(const QString &filePath, const QString &userText);

    /// 发送图片（base64 编码，走视觉模型）
    void sendImageMessage(const QString &imagePath, const QString &userText);

    /// 重设对话（清空历史，保留 system prompt）
    void resetConversation();

    /// 设置技能提示词后缀（追加到 system prompt 之后）
    void setSkillPrompt(const QString &suffix);

signals:
    void thinking();                          // 请求已发出，等待中
    void replyReceived(const QString &content); // 收到回复
    void errorOccurred(const QString &error);   // 出错

private slots:
    void onReplyFinished();

private:
    QNetworkAccessManager *m_manager;
    QJsonArray  m_messages;
    QString     m_apiKey;
    QString     m_baseUrl;
    QString     m_model;
    QString     m_skillSuffix;   // 技能提示词后缀
    bool        m_configured = false;
};

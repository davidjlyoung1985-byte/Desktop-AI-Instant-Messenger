#include "claudeclient.h"
#include "constants.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QVBoxLayout>

#include <QJsonDocument>
#include <QJsonObject>

#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QMimeDatabase>

// ===== 预配置的 cc-vibe.com 默认值 =====
static const char *kDefaultApiKey = "sk-9dd4870dee77c8f3643fab2e9bf691ef2d64ad86fc31feee8656d499dc638592";
static const char *kDefaultBaseUrl = "https://cc-vibe.com/v1";
static const char *kDefaultModel   = "claude-sonnet-4-20250514";

ClaudeClient::ClaudeClient(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
    , m_apiKey(kDefaultApiKey)
    , m_baseUrl(kDefaultBaseUrl)
    , m_model(kDefaultModel)
{
}

bool ClaudeClient::showConfigDialog(QWidget *parent)
{
    // 已配置过，直接通过
    if (m_configured && !m_apiKey.isEmpty())
        return true;

    auto *dlg = new QDialog(parent);
    dlg->setWindowTitle("Claude AI 连线 — cc-vibe.com");
    dlg->setFixedSize(480, 200);
    dlg->setStyleSheet(QString(
        "QDialog { background: %1; }"
        "QLabel { font-size: 13px; color: #555; }"
        "QLineEdit {"
        "  border: 1px solid #d0d0d0; border-radius: 4px;"
        "  padding: 6px 10px; font-size: 12px; background: #f5f5f5;"
        "}"
    ).arg(kWhite));

    auto *lay = new QVBoxLayout(dlg);
    lay->setContentsMargins(24, 20, 24, 20);
    lay->setSpacing(10);

    auto *title = new QLabel(QString::fromUtf8("\xF0\x9F\xA4\x96 李四 — Claude AI 连线配置"));
    title->setStyleSheet("font-size: 16px; font-weight: bold; color: #333;");
    lay->addWidget(title);

    // Key (只读显示，mask)
    auto *keyLayout = new QHBoxLayout;
    auto *keyLabel = new QLabel("Key:");
    keyLabel->setFixedWidth(40);
    auto *keyEdit = new QLineEdit;
    keyEdit->setText(m_apiKey);
    keyEdit->setEchoMode(QLineEdit::Password);
    keyEdit->setReadOnly(true);
    keyLayout->addWidget(keyLabel);
    keyLayout->addWidget(keyEdit, 1);
    lay->addLayout(keyLayout);

    // URL (只读显示)
    auto *urlLayout = new QHBoxLayout;
    auto *urlLabel = new QLabel("URL:");
    urlLabel->setFixedWidth(40);
    auto *urlEdit = new QLineEdit;
    urlEdit->setText(m_baseUrl);
    urlEdit->setReadOnly(true);
    urlLayout->addWidget(urlLabel);
    urlLayout->addWidget(urlEdit, 1);
    lay->addLayout(urlLayout);

    auto *hint = new QLabel("来源: cc-vibe.com — 点击\"确认连接\"开始对话");
    hint->setStyleSheet("font-size: 11px; color: #aaa;");
    lay->addWidget(hint);

    // 一个按钮
    auto *btnLayout = new QHBoxLayout;
    btnLayout->addStretch();
    auto *confirmBtn = new QPushButton("确认连接");
    confirmBtn->setFixedSize(140, 36);
    confirmBtn->setCursor(Qt::PointingHandCursor);
    confirmBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: white; border: none;"
        " border-radius: 6px; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { background: %2; }"
    ).arg(kBtnBlue, kBtnBlueHover));
    btnLayout->addWidget(confirmBtn);
    btnLayout->addStretch();
    lay->addLayout(btnLayout);

    connect(confirmBtn, &QPushButton::clicked, dlg, [dlg]{ dlg->accept(); });

    if (dlg->exec() == QDialog::Accepted) {
        m_configured = true;
        resetConversation();
        dlg->deleteLater();
        return true;
    }
    dlg->deleteLater();
    return false;
}

void ClaudeClient::sendMessage(const QString &text)
{
    if (m_apiKey.isEmpty()) return;

    // 添加用户消息 (OpenAI 兼容格式: content 为纯文本字符串)
    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = text;
    m_messages.append(userMsg);

    // 构造请求 body (OpenAI 兼容格式)
    QJsonObject body;
    body["model"] = m_model;
    body["messages"] = m_messages;
    body["stream"] = false;

    QJsonDocument doc(body);

    // 完整 URL: 如果 baseUrl 不含 /chat/completions 则补上
    QString fullUrl = m_baseUrl;
    if (!fullUrl.endsWith("/chat/completions")) {
        if (fullUrl.endsWith("/"))
            fullUrl.chop(1);
        fullUrl += "/chat/completions";
    }

    QUrl url(fullUrl);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());
    // 同时保留 x-api-key 兼容 Anthropic 原生 API
    req.setRawHeader("x-api-key", m_apiKey.toUtf8());

    QNetworkReply *reply = m_manager->post(req, doc.toJson());
    connect(reply, &QNetworkReply::finished, this, &ClaudeClient::onReplyFinished);

    emit thinking();
}

void ClaudeClient::sendFileMessage(const QString &filePath, const QString &userText)
{
    if (m_apiKey.isEmpty()) return;

    QFileInfo fi(filePath);
    QString fileName = fi.fileName();
    qint64 fileSize = fi.size();

    QString content;
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        content = QString::fromUtf8(file.readAll());
        file.close();
    }

    QString prompt = QString::fromUtf8(
        "【用户发送了文件】\n文件名: %1\n大小: %2 字节\n\n"
        "文件内容:\n```\n%3\n```\n\n"
        "用户说: %4\n\n请分析文件内容并回答用户。"
    ).arg(fileName).arg(fileSize).arg(content, userText);

    sendMessage(prompt);
}

void ClaudeClient::sendImageMessage(const QString &imagePath, const QString &userText)
{
    if (m_apiKey.isEmpty()) return;

    QFileInfo fi(imagePath);
    QString fileName = fi.fileName();
    qint64 fileSize = fi.size();

    QImage img(imagePath);
    int w = img.width(), h = img.height();
    QString sizeDesc = fileSize < 1024
        ? QString("%1 B").arg(fileSize)
        : QString("%1 KB").arg(fileSize / 1024.0, 0, 'f', 1);

    // 读取图片 base64
    QFile file(imagePath);
    QByteArray imgData;
    QString mediaType = "image/png";
    if (file.open(QIODevice::ReadOnly)) {
        imgData = file.readAll();
        file.close();
    }
    QMimeDatabase mimeDb;
    mediaType = mimeDb.mimeTypeForFile(imagePath).name();

    // 构造带图片的消息 (OpenAI 多模态格式)
    QJsonObject userMsg;
    userMsg["role"] = "user";

    QJsonArray contentArr;
    QJsonObject textPart;
    textPart["type"] = "text";
    textPart["text"] = userText.isEmpty()
        ? QString::fromUtf8("请描述这张图片的内容。")
        : userText;
    contentArr.append(textPart);

    QJsonObject imagePart;
    imagePart["type"] = "image_url";
    QJsonObject imgUrl;
    imgUrl["url"] = QString("data:%1;base64,%2").arg(mediaType, QString::fromUtf8(imgData.toBase64()));
    imagePart["image_url"] = imgUrl;
    contentArr.append(imagePart);

    userMsg["content"] = contentArr;
    m_messages.append(userMsg);

    // 构造请求 body
    QJsonObject body;
    body["model"] = m_model;
    body["messages"] = m_messages;
    body["stream"] = false;

    QJsonDocument doc(body);

    QString fullUrl = m_baseUrl;
    if (!fullUrl.endsWith("/chat/completions")) {
        if (fullUrl.endsWith("/"))
            fullUrl.chop(1);
        fullUrl += "/chat/completions";
    }

    QUrl url(fullUrl);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());
    req.setRawHeader("x-api-key", m_apiKey.toUtf8());

    QNetworkReply *reply = m_manager->post(req, doc.toJson());
    connect(reply, &QNetworkReply::finished, this, &ClaudeClient::onReplyFinished);

    emit thinking();
}

void ClaudeClient::setSkillPrompt(const QString &suffix)
{
    m_skillSuffix = suffix;
}

void ClaudeClient::resetConversation()
{
    m_messages = QJsonArray();
    // system prompt (OpenAI 兼容格式: role=system, content=字符串)
    QJsonObject sysMsg;
    sysMsg["role"] = "system";
    QString prompt = QString::fromUtf8(
        "你叫李四，一个务实、沉稳的好友。风格："
        "用中文，说话简洁接地气；"
        "回答先给结论再展开细节；"
        "有代码问题给出示例和原理；"
        "像资深同事一样讲清楚为什么，但不啰嗦。");
    if (!m_skillSuffix.isEmpty())
        prompt += "\n\n" + m_skillSuffix;
    sysMsg["content"] = prompt;
    m_messages.append(sysMsg);
}

void ClaudeClient::onReplyFinished()
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray data = reply->readAll();

    // 网络层错误 (DNS失败、连接超时等)
    if (reply->error() != QNetworkReply::NoError) {
        QString errText = reply->errorString();

        QJsonDocument errDoc = QJsonDocument::fromJson(data);
        if (errDoc.isObject()) {
            QJsonObject errObj = errDoc.object();
            if (errObj.contains("error")) {
                QJsonObject errDetail = errObj["error"].toObject();
                QString msg = errDetail["message"].toString();
                QString type = errDetail["type"].toString();
                if (!msg.isEmpty()) {
                    errText = QString("[HTTP %1] %2%3")
                        .arg(httpStatus > 0 ? QString::number(httpStatus) : QString("N/A"))
                        .arg(type.isEmpty() ? "" : type + ": ")
                        .arg(msg);
                }
            }
        }
        if (httpStatus > 0 && !errText.startsWith("[HTTP")) {
            errText = QString("[HTTP %1] %2").arg(httpStatus).arg(errText);
        }

        reply->deleteLater();
        if (!m_messages.isEmpty()) m_messages.removeLast();
        emit errorOccurred(errText);
        return;
    }

    // 检查 HTTP 状态码
    if (httpStatus >= 400) {
        QString errMsg = QString("[HTTP %1] 请求失败").arg(httpStatus);
        QJsonDocument errDoc = QJsonDocument::fromJson(data);
        if (errDoc.isObject()) {
            QJsonObject errObj = errDoc.object();
            if (errObj.contains("error")) {
                QJsonObject detail = errObj["error"].toObject();
                errMsg = QString("[HTTP %1] %2: %3")
                    .arg(httpStatus)
                    .arg(detail["type"].toString("unknown"))
                    .arg(detail["message"].toString("无详细信息"));
            }
        }
        reply->deleteLater();
        if (!m_messages.isEmpty()) m_messages.removeLast();
        emit errorOccurred(errMsg);
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject obj = doc.object();
    QString content;

    // ---- 解析响应：兼容 OpenAI 和 Anthropic 两种格式 ----

    // 1. 先检查顶层 error
    if (obj.contains("error")) {
        QJsonObject errObj = obj["error"].toObject();
        reply->deleteLater();
        if (!m_messages.isEmpty()) m_messages.removeLast();
        emit errorOccurred(QString("[%1] %2")
            .arg(errObj["type"].toString("unknown"),
                 errObj["message"].toString("未知错误")));
        return;
    }

    // 2. 尝试 OpenAI 格式: choices[0].message.content
    QJsonArray choices = obj["choices"].toArray();
    if (!choices.isEmpty()) {
        QJsonObject choice = choices[0].toObject();
        QJsonObject message = choice["message"].toObject();
        content = message["content"].toString();
        // 处理 content 可能是数组的情况
        if (content.isEmpty() && message["content"].isArray()) {
            QJsonArray ca = message["content"].toArray();
            for (const auto &v : ca) {
                QJsonObject block = v.toObject();
                if (block["type"].toString() == "text")
                    content += block["text"].toString();
            }
        }
    }

    // 3. OpenAI 格式没有内容 → 尝试 Anthropic 格式: content[0].text
    if (content.isEmpty()) {
        QJsonArray contentArr = obj["content"].toArray();
        if (!contentArr.isEmpty()) {
            for (const auto &val : contentArr) {
                QJsonObject block = val.toObject();
                if (block["type"].toString() == "text") {
                    content += block["text"].toString();
                }
            }
        }
    }

    if (!content.isEmpty()) {
        // 添加 AI 回复到对话历史
        QJsonObject aiMsg;
        aiMsg["role"] = "assistant";
        aiMsg["content"] = content;
        m_messages.append(aiMsg);

        reply->deleteLater();
        emit replyReceived(content);
    } else {
        // 解析失败: 输出原始响应用于诊断
        QString rawPreview = QString::fromUtf8(data.left(300));
        reply->deleteLater();
        if (!m_messages.isEmpty()) m_messages.removeLast();
        emit errorOccurred(QString("响应解析失败 [HTTP %1]\n模型: %2\n原始响应: %3")
            .arg(httpStatus)
            .arg(m_model, rawPreview));
    }
}

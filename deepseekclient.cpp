#include "deepseekclient.h"
#include "configmanager.h"
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

DeepSeekClient::DeepSeekClient(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{
    auto *cfg = ConfigManager::instance();
    m_apiKey  = cfg->deepseekApiKey();
    m_baseUrl = cfg->deepseekBaseUrl();
    m_model   = cfg->deepseekModel();
}

bool DeepSeekClient::showConfigDialog(QWidget *parent)
{
    auto *cfg = ConfigManager::instance();

    auto *dlg = new QDialog(parent);
    dlg->setWindowTitle("配置 DeepSeek API");
    dlg->setFixedSize(480, 310);
    dlg->setStyleSheet(QString(
        "QDialog { background: %1; }"
        "QLabel { font-size: 13px; color: #555; }"
        "QLineEdit {"
        "  border: 1px solid #d0d0d0; border-radius: 4px;"
        "  padding: 6px 10px; font-size: 13px;"
        "}"
        "QLineEdit:focus { border-color: %2; }"
    ).arg(kWhite, kBtnBlue));

    auto *lay = new QVBoxLayout(dlg);
    lay->setContentsMargins(24, 20, 24, 20);
    lay->setSpacing(12);

    auto *title = new QLabel("请输入 DeepSeek API 信息:");
    title->setStyleSheet("font-size: 15px; font-weight: bold; color: #333;");
    lay->addWidget(title);

    // API Key
    auto *keyLayout = new QHBoxLayout;
    auto *keyLabel = new QLabel("API Key:");
    keyLabel->setFixedWidth(70);
    auto *keyEdit = new QLineEdit;
    keyEdit->setPlaceholderText("sk-xxxxxxxxxxxxxxxxxxxxxxxx");
    keyEdit->setEchoMode(QLineEdit::Password);
    keyEdit->setText(cfg->deepseekApiKey());
    keyLayout->addWidget(keyLabel);
    keyLayout->addWidget(keyEdit, 1);
    lay->addLayout(keyLayout);

    // Base URL
    auto *urlLayout = new QHBoxLayout;
    auto *urlLabel = new QLabel("API 地址:");
    urlLabel->setFixedWidth(70);
    auto *urlEdit = new QLineEdit;
    urlEdit->setText(cfg->deepseekBaseUrl());
    urlLayout->addWidget(urlLabel);
    urlLayout->addWidget(urlEdit, 1);
    lay->addLayout(urlLayout);

    // Model
    auto *modelLayout = new QHBoxLayout;
    auto *modelLabel = new QLabel("Model:");
    modelLabel->setFixedWidth(70);
    auto *modelEdit = new QLineEdit;
    modelEdit->setText(cfg->deepseekModel());
    modelEdit->setPlaceholderText("deepseek-chat / deepseek-reasoner");
    modelLayout->addWidget(modelLabel);
    modelLayout->addWidget(modelEdit, 1);
    lay->addLayout(modelLayout);

    auto *hint = new QLabel(QString::fromUtf8(
        "获取 Key: https://platform.deepseek.com/api_keys\n"
        "官方地址: https://api.deepseek.com/v1/chat/completions"));
    hint->setStyleSheet("font-size: 11px; color: #aaa; margin-top: 4px;");
    hint->setWordWrap(true);
    lay->addWidget(hint);

    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    btnBox->button(QDialogButtonBox::Ok)->setText("确认配置");
    btnBox->button(QDialogButtonBox::Ok)->setStyleSheet(QString(
        "QPushButton { background: %1; color: white; border: none; border-radius: 4px;"
        " padding: 8px 20px; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { background: %2; }"
    ).arg(kBtnBlue, kBtnBlueHover));
    btnBox->button(QDialogButtonBox::Cancel)->setText("取消");
    lay->addWidget(btnBox);

    connect(btnBox, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

    if (dlg->exec() == QDialog::Accepted) {
        QString newKey   = keyEdit->text().trimmed();
        QString newUrl   = urlEdit->text().trimmed();
        QString newModel = modelEdit->text().trimmed();

        if (newKey.isEmpty()) {
            QMessageBox::warning(parent, "缺少 Key", "请输入 DeepSeek API Key");
            dlg->deleteLater();
            return false;
        }
        if (newModel.isEmpty())
            newModel = "deepseek-chat";

        cfg->setDeepseekApiKey(newKey);
        cfg->setDeepseekBaseUrl(newUrl);
        cfg->setDeepseekModel(newModel);
        cfg->save();

        m_apiKey  = newKey;
        m_baseUrl = newUrl;
        m_model   = newModel;
        m_configured = true;

        resetConversation();
        dlg->deleteLater();
        return true;
    }
    dlg->deleteLater();
    return false;
}

void DeepSeekClient::sendMessage(const QString &text)
{
    if (m_apiKey.isEmpty()) return;

    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = text;
    m_messages.append(userMsg);

    QJsonObject body;
    body["model"] = m_model;
    body["messages"] = m_messages;
    body["stream"] = false;
    body["temperature"] = 0.7;

    QJsonDocument doc(body);

    QUrl url(m_baseUrl);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());

    QNetworkReply *reply = m_manager->post(req, doc.toJson());
    connect(reply, &QNetworkReply::finished, this, &DeepSeekClient::onReplyFinished);

    emit thinking();
}

void DeepSeekClient::sendFileMessage(const QString &filePath, const QString &userText)
{
    if (m_apiKey.isEmpty()) return;

    QFileInfo fi(filePath);
    QString fileName = fi.fileName();
    qint64 fileSize = fi.size();

    // 读文本文件内容
    QString content;
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        content = QString::fromUtf8(file.readAll());
        file.close();
    }

    // 拼出给 AI 的提示
    QString prompt = QString::fromUtf8(
        "【用户发送了文件】\n文件名: %1\n大小: %2 字节\n\n"
        "文件内容:\n```\n%3\n```\n\n"
        "用户说: %4\n\n请分析文件内容并回答用户。"
    ).arg(fileName).arg(fileSize).arg(content, userText);

    sendMessage(prompt);
}

void DeepSeekClient::sendImageMessage(const QString &imagePath, const QString &userText)
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

    // deepseek-chat 不支持 image_url，改为描述性文本
    QString prompt = QString::fromUtf8(
        "【用户发送了一张图片】\n"
        "文件名: %1\n尺寸: %2 x %3 像素\n大小: %4\n\n"
        "%5"
    ).arg(fileName).arg(w).arg(h).arg(sizeDesc,
         userText.isEmpty()
             ? QString::fromUtf8("请根据文件名和尺寸推测图片可能的内容，并询问用户具体想了解什么。")
             : userText);

    sendMessage(prompt);
}

void DeepSeekClient::setSkillPrompt(const QString &suffix)
{
    m_skillSuffix = suffix;
}

void DeepSeekClient::resetConversation()
{
    m_messages = QJsonArray();
    QJsonObject sysMsg;
    sysMsg["role"] = "system";
    QString prompt = QString::fromUtf8(
        "你叫DeepSeek，一个耐心、靠谱的技术朋友。风格："
        "用中文，口语自然不油滑；"
        "回答先给结论再展开细节；"
        "有代码问题给出示例和原理；"
        "像资深同事一样讲清楚为什么，但不啰嗦。");
    if (!m_skillSuffix.isEmpty())
        prompt += m_skillSuffix;
    sysMsg["content"] = prompt;
    m_messages.append(sysMsg);
}

void DeepSeekClient::onReplyFinished()
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() != QNetworkReply::NoError) {
        QString errText = reply->errorString();
        QByteArray respBody = reply->readAll();
        QJsonDocument errDoc = QJsonDocument::fromJson(respBody);
        if (errDoc.isObject()) {
            QJsonObject errObj = errDoc.object();
            if (errObj.contains("error")) {
                QJsonObject errDetail = errObj["error"].toObject();
                errText = errDetail["message"].toString(errText);
            }
        }
        reply->deleteLater();
        // 移除失败的用户消息
        if (!m_messages.isEmpty()) m_messages.removeLast();
        emit errorOccurred(errText);
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject obj = doc.object();
    QJsonArray choices = obj["choices"].toArray();

    if (!choices.isEmpty()) {
        QString content = choices[0].toObject()["message"].toObject()["content"].toString();

        QJsonObject aiMsg;
        aiMsg["role"] = "assistant";
        aiMsg["content"] = content;
        m_messages.append(aiMsg);

        reply->deleteLater();
        emit replyReceived(content);
    } else {
        reply->deleteLater();
        emit errorOccurred("未收到有效回复");
    }
}

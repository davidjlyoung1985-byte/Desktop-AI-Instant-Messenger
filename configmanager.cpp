#include "configmanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>

// ===== 默认值（内部，不会暴露到源码中的明文 key） =====
static const char *kDefaultClaudeKey  = "";
static const char *kDefaultClaudeUrl  = "https://cc-vibe.com/v1";
static const char *kDefaultClaudeModel = "claude-sonnet-4-20250514";

static const char *kDefaultDsKey   = "";
static const char *kDefaultDsUrl   = "https://api.deepseek.com/v1/chat/completions";
static const char *kDefaultDsModel = "deepseek-chat";

// ─── 单例 ────────────────────────────────────────────────────────────

ConfigManager *ConfigManager::s_instance = nullptr;

ConfigManager *ConfigManager::instance()
{
    if (!s_instance) {
        s_instance = new ConfigManager(qApp);
        s_instance->load();
    }
    return s_instance;
}

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
{
}

// ─── 路径 ─────────────────────────────────────────────────────────────

QString ConfigManager::configFilePath() const
{
    // 使用 AppData 目录，不依赖于 app binary 目录
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/config.json";
}

// ─── 加载 / 保存 ──────────────────────────────────────────────────────

void ConfigManager::load()
{
    QFile f(configFilePath());
    if (!f.open(QIODevice::ReadOnly))
        return;

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();

    if (doc.isObject())
        m_data = doc.object();
}

void ConfigManager::save()
{
    QFile f(configFilePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;

    QJsonDocument doc(m_data);
    f.write(doc.toJson(QJsonDocument::Indented));
    f.close();

    emit configChanged();
}

// ─── Claude 存取 ──────────────────────────────────────────────────────

QString ConfigManager::claudeApiKey() const
{
    return m_data.value("claude/apiKey").toString(kDefaultClaudeKey);
}
void ConfigManager::setClaudeApiKey(const QString &key)
{
    m_data["claude/apiKey"] = key;
}

QString ConfigManager::claudeBaseUrl() const
{
    return m_data.value("claude/baseUrl").toString(kDefaultClaudeUrl);
}
void ConfigManager::setClaudeBaseUrl(const QString &url)
{
    m_data["claude/baseUrl"] = url;
}

QString ConfigManager::claudeModel() const
{
    return m_data.value("claude/model").toString(kDefaultClaudeModel);
}
void ConfigManager::setClaudeModel(const QString &model)
{
    m_data["claude/model"] = model;
}

// ─── DeepSeek 存取 ────────────────────────────────────────────────────

QString ConfigManager::deepseekApiKey() const
{
    return m_data.value("deepseek/apiKey").toString(kDefaultDsKey);
}
void ConfigManager::setDeepseekApiKey(const QString &key)
{
    m_data["deepseek/apiKey"] = key;
}

QString ConfigManager::deepseekBaseUrl() const
{
    return m_data.value("deepseek/baseUrl").toString(kDefaultDsUrl);
}
void ConfigManager::setDeepseekBaseUrl(const QString &url)
{
    m_data["deepseek/baseUrl"] = url;
}

QString ConfigManager::deepseekModel() const
{
    return m_data.value("deepseek/model").toString(kDefaultDsModel);
}
void ConfigManager::setDeepseekModel(const QString &model)
{
    m_data["deepseek/model"] = model;
}

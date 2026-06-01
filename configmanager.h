#pragma once
#include <QObject>
#include <QString>
#include <QJsonObject>

// ============ 统一配置管理器 ============
// 管理所有 API Key、URL、Model 等配置，持久化到 config.json
// 使用 QStandardPaths 存放配置，确保不同平台兼容

class ConfigManager : public QObject
{
    Q_OBJECT

public:
    static ConfigManager *instance();

    // ── Claude (cc-vibe.com) 配置 ──
    QString claudeApiKey()  const;
    void    setClaudeApiKey(const QString &key);
    QString claudeBaseUrl()  const;
    void    setClaudeBaseUrl(const QString &url);
    QString claudeModel()   const;
    void    setClaudeModel(const QString &model);

    // ── DeepSeek 配置 ──
    QString deepseekApiKey()  const;
    void    setDeepseekApiKey(const QString &key);
    QString deepseekBaseUrl()  const;
    void    setDeepseekBaseUrl(const QString &url);
    QString deepseekModel()   const;
    void    setDeepseekModel(const QString &model);

    // ── UI 设置 ──
    enum UiTheme { ThemeLight = 0, ThemeDark = 1, ThemeSystem = 2 };
    enum UiFontSize { FontSmall = 0, FontNormal = 1, FontLarge = 2, FontXLarge = 3 };

    UiTheme   uiTheme() const;
    void      setUiTheme(UiTheme theme);
    UiFontSize uiFontSize() const;
    void      setUiFontSize(UiFontSize size);
    bool      uiNotify() const;
    void      setUiNotify(bool on);
    bool      uiSound() const;
    void      setUiSound(bool on);
    bool      uiAutoLogin() const;
    void      setUiAutoLogin(bool on);
    bool      uiPrivacy() const;
    void      setUiPrivacy(bool on);

    // ── 持久化 ──
    void save();
    void load();

signals:
    void configChanged();

private:
    explicit ConfigManager(QObject *parent = nullptr);
    static ConfigManager *s_instance;

    QString configFilePath() const;

    QJsonObject m_data;
};


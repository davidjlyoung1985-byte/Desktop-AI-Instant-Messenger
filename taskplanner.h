#pragma once
#include <QObject>
#include <QJsonArray>
#include <QString>
#include <QVector>

class QNetworkAccessManager;
class QNetworkReply;

// ============ 任务规划器 ============
// 将用户需求分解为可执行步骤，逐步执行并追踪进度
// 独立于现有 AI 客户端，使用自己的 QNetworkAccessManager

class TaskPlanner : public QObject
{
    Q_OBJECT

public:
    /// 单个步骤
    struct Step {
        int     index = 0;
        QString title;          // 步骤名称
        QString description;    // 步骤说明
        enum State { Pending, Running, Completed, Failed };
        State   state = Pending;
        QString result;         // 执行结果
    };

    explicit TaskPlanner(QObject *parent = nullptr);

    /// 配置 API 连接信息
    void setApiConfig(const QString &baseUrl, const QString &apiKey, const QString &model);
    bool isConfigured() const { return !m_apiKey.isEmpty() && !m_baseUrl.isEmpty(); }

    /// 开始规划：将 goal 分解为步骤（通过 AI 生成）
    void plan(const QString &goal);

    /// 执行指定步骤（自动带上前面步骤的上下文）
    void executeStep(int index);

    /// 一键执行所有剩余步骤
    void executeAll();

    /// 取消当前操作
    void cancel();

    /// 重置状态
    void reset();

    const QVector<Step> &steps()  const { return m_steps; }
    const QString       &goal()   const { return m_goal; }
    bool  isPlanning()   const { return m_planning; }
    bool  isExecuting()  const { return m_executing; }
    int   currentStep()  const { return m_currentStep; }

signals:
    void planReady(const QVector<TaskPlanner::Step> &steps);
    void planFailed(const QString &error);
    void stepStarted(int index);
    void stepCompleted(int index, const QString &result);
    void stepFailed(int index, const QString &error);
    void allStepsCompleted();
    void thinking();    // 等待 API 响应时发出

private slots:
    void onPlanReplyFinished();
    void onStepReplyFinished();

private:
    QVector<Step> parsePlanResponse(const QString &response);
    void          postRequest(const QJsonArray &messages);
    void          sendPlanningPrompt();
    void          sendStepPrompt(int stepIndex);

    QNetworkAccessManager *m_manager = nullptr;

    // API 配置
    QString m_baseUrl;
    QString m_apiKey;
    QString m_model;

    // 状态
    QString       m_goal;
    QVector<Step> m_steps;
    int           m_currentStep  = -1;
    QStringList   m_stepResults;          // 每步执行结果（纯文本）
    bool          m_planning     = false;
    bool          m_executing    = false;
    bool          m_cancelled    = false;
};

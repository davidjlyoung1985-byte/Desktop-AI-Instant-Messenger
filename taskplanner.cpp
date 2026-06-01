#include "taskplanner.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>

// ============ 构造 ============

TaskPlanner::TaskPlanner(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{
}

// ============ 配置 ============

void TaskPlanner::setApiConfig(const QString &baseUrl, const QString &apiKey, const QString &model)
{
    m_baseUrl = baseUrl;
    m_apiKey  = apiKey;
    m_model   = model;
}

// ============ POST 请求封装 ============

void TaskPlanner::postRequest(const QJsonArray &messages)
{
    if (!isConfigured()) {
        emit planFailed("API 未配置，请先在设置中配置 AI 服务");
        return;
    }

    QJsonObject body;
    body["model"]    = m_model;
    body["messages"] = messages;
    body["stream"]   = false;

    QString endpoint = m_baseUrl;
    if (!endpoint.endsWith("/v1/chat/completions") && !endpoint.endsWith("/v1/chat/completions/"))
        endpoint += "/v1/chat/completions";

    QNetworkRequest req(QUrl(endpoint));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());

    QJsonDocument doc(body);
    m_manager->post(req, doc.toJson(QJsonDocument::Compact));
}

// ============ 规划：分解任务 ============

void TaskPlanner::plan(const QString &goal)
{
    if (goal.trimmed().isEmpty()) return;

    cancel();
    reset();
    m_goal     = goal.trimmed();
    m_planning = true;

    emit thinking();
    sendPlanningPrompt();
}

void TaskPlanner::sendPlanningPrompt()
{
    QJsonArray messages;

    // System prompt: 扮演任务规划器
    QJsonObject sys;
    sys["role"]    = "system";
    sys["content"] = QString::fromUtf8(
        "你是一个任务规划专家。请将用户的需求分解为清晰、可执行的步骤序列。\n\n"
        "规则：\n"
        "1. 每个步骤必须具体、可独立执行\n"
        "2. 步骤之间应有逻辑递进关系\n"
        "3. 步骤数量控制在 3-7 个\n"
        "4. 用中文回答\n\n"
        "输出格式（严格遵守）：\n"
        "步骤1: <标题> - <一句话描述>\n"
        "步骤2: <标题> - <一句话描述>\n"
        "...\n\n"
        "不要输出多余的解释，只输出步骤列表。"
    );
    messages.append(sys);

    // User message: 要分解的任务
    QJsonObject user;
    user["role"]    = "user";
    user["content"] = QString("请分解以下任务：\n\n%1").arg(m_goal);
    messages.append(user);

    // 连接回复处理（一次性的 —— 后续由 executeStep 重新连接）
    connect(m_manager, &QNetworkAccessManager::finished,
            this, &TaskPlanner::onPlanReplyFinished, Qt::SingleShotConnection);

    postRequest(messages);
}

void TaskPlanner::onPlanReplyFinished()
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();

    m_planning = false;

    if (m_cancelled) {
        m_cancelled = false;
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        emit planFailed(QString("规划失败: %1").arg(reply->errorString()));
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject obj = doc.object();

    // 解析 OpenAI 兼容格式
    QString content;
    QJsonArray choices = obj["choices"].toArray();
    if (!choices.isEmpty()) {
        QJsonObject choice = choices[0].toObject();
        QJsonObject message = choice["message"].toObject();
        content = message["content"].toString();
    }

    if (content.isEmpty()) {
        emit planFailed("规划失败: AI 返回了空内容");
        return;
    }

    m_steps = parsePlanResponse(content);
    if (m_steps.isEmpty()) {
        // 无法解析步骤，把原始回复当成一个单步骤
        Step s;
        s.index       = 0;
        s.title       = "执行";
        s.description = m_goal;
        m_steps.append(s);
    }

    emit planReady(m_steps);
}

QVector<TaskPlanner::Step> TaskPlanner::parsePlanResponse(const QString &response)
{
    QVector<Step> steps;

    // 匹配 "步骤N: 标题 - 描述" 或 "Step N: title - desc" 或 "N. title - desc"
    static QRegularExpression re(
        R"((?:步骤|Step\s*)?(\d+)[\.:\、\)]\s*(.+?)\s*[-–—]\s*(.+))",
        QRegularExpression::CaseInsensitiveOption
    );

    auto it = re.globalMatch(response);
    int idx = 0;
    while (it.hasNext()) {
        auto m = it.next();
        Step s;
        s.index       = idx++;
        s.title       = m.captured(2).trimmed();
        s.description = m.captured(3).trimmed();
        // 清理可能的尾随标记
        s.description.remove(QRegularExpression(R"([\)）]\s*$)"));
        steps.append(s);
    }

    // 如果没有匹配到，尝试匹配简单的 "1. xxx" 或 "- xxx" 列表
    if (steps.isEmpty()) {
        static QRegularExpression simpleRe(
            R"(^(?:\d+[\.\)、]\s*|\-\s+)(.+?)$)",
            QRegularExpression::MultilineOption
        );
        auto it2 = simpleRe.globalMatch(response);
        int idx2 = 0;
        while (it2.hasNext()) {
            auto m = it2.next();
            QString line = m.captured(1).trimmed();
            if (line.length() < 5) continue; // 跳过太短的行
            Step s;
            s.index       = idx2++;
            s.title       = line.left(line.indexOf('-') != -1
                                       ? line.indexOf('-')
                                       : qMin(30, line.length()));
            s.description = line;
            steps.append(s);
        }
    }

    return steps;
}

// ============ 执行步骤 ============

void TaskPlanner::executeStep(int index)
{
    if (index < 0 || index >= m_steps.size()) return;
    if (m_steps[index].state == Step::Completed) return;
    if (!isConfigured()) {
        emit stepFailed(index, "API 未配置");
        return;
    }

    m_executing    = true;
    m_currentStep  = index;
    m_steps[index].state = Step::Running;

    emit thinking();
    emit stepStarted(index);

    sendStepPrompt(index);
}

void TaskPlanner::executeAll()
{
    // 找到第一个未完成的步骤
    for (int i = 0; i < m_steps.size(); ++i) {
        if (m_steps[i].state == Step::Pending || m_steps[i].state == Step::Failed) {
            executeStep(i);
            return;
        }
    }
    // 全部完成
    m_executing = false;
    emit allStepsCompleted();
}

void TaskPlanner::sendStepPrompt(int stepIndex)
{
    const Step &step = m_steps[stepIndex];
    QJsonArray messages;

    // System prompt
    QJsonObject sys;
    sys["role"] = "system";
    sys["content"] = QString::fromUtf8(
        "你正在执行一个多步骤任务。请专注于当前步骤，只输出该步骤的完成结果。"
        "不要输出下一步的信息，不要输出多余的说明。回答要简洁、完整、可直接使用。"
    );
    messages.append(sys);

    // 构建上下文消息
    QString contextMsg;

    // 整体目标
    contextMsg += QString("【总体任务】\n%1\n\n").arg(m_goal);

    // 前面步骤的结果
    if (!m_stepResults.isEmpty()) {
        contextMsg += "【前面步骤的完成结果】\n";
        for (int i = 0; i < m_stepResults.size(); ++i) {
            if (!m_stepResults[i].isEmpty()) {
                contextMsg += QString("步骤 %1 完成: %2\n\n")
                    .arg(i + 1)
                    .arg(m_stepResults[i]);
            }
        }
    }

    // 当前步骤
    contextMsg += QString("【当前要执行的步骤】\n"
                          "步骤 %1: %2\n"
                          "%3\n\n"
                          "请完成上面这个步骤，输出完成结果。")
        .arg(stepIndex + 1)
        .arg(step.title)
        .arg(step.description);

    QJsonObject user;
    user["role"]    = "user";
    user["content"] = contextMsg;
    messages.append(user);

    // 连接回复处理（一次性）
    connect(m_manager, &QNetworkAccessManager::finished,
            this, &TaskPlanner::onStepReplyFinished, Qt::SingleShotConnection);

    postRequest(messages);
}

void TaskPlanner::onStepReplyFinished()
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();

    if (m_cancelled) {
        m_executing  = false;
        m_cancelled  = false;
        return;
    }

    int idx = m_currentStep;
    if (idx < 0 || idx >= m_steps.size()) {
        m_executing = false;
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        m_steps[idx].state  = Step::Failed;
        m_executing          = false;
        emit stepFailed(idx, reply->errorString());
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject obj = doc.object();

    QString content;
    QJsonArray choices = obj["choices"].toArray();
    if (!choices.isEmpty()) {
        QJsonObject choice = choices[0].toObject();
        QJsonObject message = choice["message"].toObject();
        content = message["content"].toString();
    }

    // 保存结果
    m_steps[idx].state  = Step::Completed;
    m_steps[idx].result = content;

    // 确保 m_stepResults 足够长
    while (m_stepResults.size() <= idx)
        m_stepResults.append({});
    m_stepResults[idx] = content;

    emit stepCompleted(idx, content);

    // 自动执行下一步
    bool hasNext = false;
    for (int i = idx + 1; i < m_steps.size(); ++i) {
        if (m_steps[i].state == Step::Pending) {
            hasNext = true;
            executeStep(i);
            break;
        }
    }

    if (!hasNext) {
        m_executing = false;
        emit allStepsCompleted();
    }
}

// ============ 取消 & 重置 ============

void TaskPlanner::cancel()
{
    m_cancelled = true;
    m_planning  = false;
    m_executing = false;

    if (m_currentStep >= 0 && m_currentStep < m_steps.size()) {
        if (m_steps[m_currentStep].state == Step::Running) {
            m_steps[m_currentStep].state = Step::Pending;
        }
    }
    m_currentStep = -1;
}

void TaskPlanner::reset()
{
    cancel();
    m_goal.clear();
    m_steps.clear();
    m_stepResults.clear();
    m_currentStep = -1;
}

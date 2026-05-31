#include "skillmanager.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

// ============ 内置预设技能 ============
static const Skill kDefaultSkills[] = {
    {"代码助手", "你是编程专家。回答时给出示例代码、解释原理、注明注意事项。"},
    {"翻译官",   "你把用户输入翻译成英文。如果用户输入已是英文，则翻译成中文。只输出译文。"},
    {"写作助手", "你帮助用户改进写作。检查语法、优化表达、提供更好的措辞建议。"},
    {"摘要专家", "把用户的输入总结成 3 条以内的要点，简洁清晰。"},
    {"幽默朋友", "你用幽默风趣的语气和用户聊天，可以讲笑话、玩梗，但保持友善。"},
};

SkillManager::SkillManager(QObject *parent) : QObject(parent) {}

QVector<Skill> SkillManager::activeSkills() const
{
    QVector<Skill> out;
    for (const auto &s : m_skills)
        if (s.enabled) out.append(s);
    return out;
}

void SkillManager::addSkill(const QString &name, const QString &prompt)
{
    m_skills.append({name, prompt, true});
    emit changed();
}

void SkillManager::updateSkill(int index, const QString &name, const QString &prompt)
{
    if (index < 0 || index >= m_skills.size()) return;
    m_skills[index].name   = name;
    m_skills[index].prompt = prompt;
    emit changed();
}

void SkillManager::removeSkill(int index)
{
    if (index < 0 || index >= m_skills.size()) return;
    m_skills.removeAt(index);
    emit changed();
}

void SkillManager::setEnabled(int index, bool on)
{
    if (index < 0 || index >= m_skills.size()) return;
    m_skills[index].enabled = on;
    emit changed();
}

QString SkillManager::buildSkillPrompt() const
{
    auto active = activeSkills();
    if (active.isEmpty()) return {};

    QString out = QString::fromUtf8(
        "\n\n## 当前激活的技能\n"
        "请同时遵循以下技能要求：\n");
    for (const auto &s : active) {
        out += QString::fromUtf8("- 【%1】%2\n")
                   .arg(s.name, s.prompt);
    }
    return out;
}

void SkillManager::saveToFile(const QString &path)
{
    QJsonArray arr;
    for (const auto &s : m_skills) {
        QJsonObject o;
        o["name"]    = s.name;
        o["prompt"]  = s.prompt;
        o["enabled"] = s.enabled;
        arr.append(o);
    }
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(arr).toJson());
    }
}

void SkillManager::loadFromFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) return;
    m_skills.clear();
    for (const auto &v : doc.array()) {
        QJsonObject o = v.toObject();
        m_skills.append({
            o["name"].toString(),
            o["prompt"].toString(),
            o["enabled"].toBool(true)
        });
    }
    if (m_skills.isEmpty()) loadDefaults();
    emit changed();
}

void SkillManager::loadDefaults()
{
    m_skills.clear();
    for (const auto &s : kDefaultSkills)
        m_skills.append(s);
    emit changed();
}

#pragma once
#include <QObject>
#include <QString>
#include <QVector>

// ============ 技能数据结构 ============
struct Skill {
    QString name;
    QString prompt;    // 提示词内容
    bool    enabled = true;
};

// ============ 技能管理器 —— 持久化存储 + 系统提示词拼装 ============
class SkillManager : public QObject
{
    Q_OBJECT
public:
    explicit SkillManager(QObject *parent = nullptr);

    const QVector<Skill> &skills() const { return m_skills; }
    QVector<Skill> activeSkills() const;   // 仅返回已启用的技能

    void addSkill(const QString &name, const QString &prompt);
    void updateSkill(int index, const QString &name, const QString &prompt);
    void removeSkill(int index);
    void setEnabled(int index, bool on);

    /// 把当前启用的技能拼成一份追加到系统提示词后面的文本
    QString buildSkillPrompt() const;

    /// 文件存取
    void saveToFile(const QString &path);
    void loadFromFile(const QString &path);

    /// 内置预设
    void loadDefaults();

signals:
    void changed();

private:
    QVector<Skill> m_skills;
};

#pragma once

#include <QMainWindow>
#include <QMap>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QStackedWidget;
class QMenu;
class QTcpServer;
class QTcpSocket;
class QTextEdit;

class ClaudeClient;
class DeepSeekClient;
class SkillManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    // ---- 三个按钮 ----
    void onBtnAddFriend();
    void onBtnCreateGroup();
    void onBtnSettings();
    void onSidebarSearch();

    // ---- 浏览器 ----
    void onBrowserNav();
    void onBrowserBack();

    // ---- 消息 / 聊天 ----
    void onMsgItemClicked(QListWidgetItem *item);
    void onChatBack();

    // ---- TCP 网络 ----
    void onStartListen();
    void onStopListen();
    void onConnectPeer();
    void onNewConnection();
    void onReadyRead();
    void onPeerDisconnected();

    // ---- DeepSeek ----
    void onDsThinking();
    void onDsReply(const QString &content);
    void onDsError(const QString &error);

    // ---- Claude ----
    void onClaudeThinking();
    void onClaudeReply(const QString &content);
    void onClaudeError(const QString &error);

    void showSkillDialog();   // 技能管理对话框
    void applyAppTheme();      // 应用主题样式

    // ---- 上下文菜单 ----
    void onMsgListContextMenu(const QPoint &pos);
    void onContactListContextMenu(const QPoint &pos);

private:
    // ===== 核心 ====
    void setupUI();
    void navigateTo(int pageIndex);
    void resetNavHighlight(QPushButton *active);
    void openInSystemBrowser(const QString &text);

    // ===== 部件创建 ====
    QWidget *createLeftSidebar();
    QWidget *createMessagePanel();
    QWidget *createContactPanel();
    QWidget *createMomentsPanel();
    QWidget *createSettingsPanel();
    QWidget *createBrowserPanel();
    QWidget *createNetworkBar();

    // ===== 聊天 ====
    QWidget *createChatDetailPanel(const QString &name, const QString &contactId, const QString &avatarColor);
    QString makeBubble(const QString &text, bool isMe,
                       const QString &avatarChar, const QString &avatarColor);
    void sendChatMessage(const QString &peerName, const QString &text);
    void displayReceivedMessage(const QString &peerName, const QString &text);

    // ===== 消息列表 ====
    void refreshMessagePreview(const QString &contactId);
    void refreshMsgList();                            // 重建消息列表（用于联系人变更后）

    // ===== 网络 ====
    QStringList localIPs() const;
    void updateNetworkStatus();

    // -------- 左侧边栏 --------
    QLineEdit   *m_sidebarSearch = nullptr;
    QPushButton *m_navMsg = nullptr;
    QPushButton *m_navContact = nullptr;
    QPushButton *m_navMoments = nullptr;
    QPushButton *m_navBrowser = nullptr;

    // -------- 顶部工具栏 --------
    QLineEdit    *m_searchBox = nullptr;
    QPushButton  *m_btnAddFriend = nullptr;
    QPushButton  *m_btnCreateGroup = nullptr;
    QPushButton  *m_btnSettings = nullptr;

    // -------- 网络工具栏 --------
    QWidget      *m_networkBar = nullptr;
    QLineEdit    *m_portEdit = nullptr;
    QPushButton  *m_btnListen = nullptr;
    QPushButton  *m_btnStopListen = nullptr;
    QLineEdit    *m_localNameEdit = nullptr;
    QLineEdit    *m_peerAddrEdit = nullptr;
    QPushButton  *m_btnConnectPeer = nullptr;
    QLabel       *m_netStatusLabel = nullptr;

    // -------- 主内容区 --------
    QStackedWidget *m_stacked = nullptr;
    QListWidget    *m_msgList = nullptr;
    QWidget        *m_settingsPanel = nullptr;
    QWidget        *m_browserPanel = nullptr;
    QLineEdit      *m_browserUrlEdit = nullptr;
    QLabel         *m_statusBar = nullptr;

    // -------- 浏览器历史 --------
    QStringList m_browserHistory;
    int         m_browserHistoryPos = -1;

    // -------- 聊天详情 --------
    QWidget     *m_chatDetailWidget = nullptr;
    QTextEdit   *m_chatInput = nullptr;
    QTextEdit   *m_chatDisplay = nullptr;
    QPushButton *m_chatSendBtn = nullptr;
    QCheckBox   *m_webSearchCheck = nullptr;
    QComboBox   *m_modelCombo = nullptr;   // 模型切换下拉框
    QString      m_currentChatPeer;
    QString      m_currentPeerId;
    QString      m_currentPeerColor;

    // -------- 联系人列表 --------
    QListWidget *m_contactList = nullptr;

    // -------- TCP 网络 --------
    QTcpServer *m_tcpServer = nullptr;
    QMap<QString, QTcpSocket*> m_peerSockets;
    quint16 m_listenPort = 12345;

    // -------- DeepSeek AI --------
    DeepSeekClient *m_dsClient = nullptr;

    // -------- Claude AI --------
    ClaudeClient *m_claudeClient = nullptr;

    // -------- 技能系统 --------
    SkillManager *m_skillManager = nullptr;
};

#include "mainwindow.h"
#include "constants.h"
#include "avatarutils.h"
#include "chathistory.h"
#include "claudeclient.h"
#include "configmanager.h"
#include "contactstore.h"
#include "deepseekclient.h"
#include "markdownutils.h"
#include "skillmanager.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>

#include <QHostInfo>
#include <QNetworkInterface>
#include <QTcpServer>
#include <QTcpSocket>
#include <QKeyEvent>

// ============ 辅助：打开系统浏览器 ============

void MainWindow::openInSystemBrowser(const QString &text)
{
    QString url;
    if (text.startsWith("http://") || text.startsWith("https://")) {
        url = text;
    } else if (text.contains('.') && !text.contains(' ')) {
        url = "https://" + text;
    } else {
        QString keyword = QUrl::toPercentEncoding(text);
        url = "https://www.baidu.com/s?wd=" + keyword;
    }
    QDesktopServices::openUrl(QUrl(url));
}

// ============ 构造 / 析构 ============

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_tcpServer(nullptr)
    , m_dsClient(new DeepSeekClient(this))
    , m_claudeClient(new ClaudeClient(this))
    , m_skillManager(new SkillManager(this))
{
    // 加载技能文件
    QString skillFile = QApplication::applicationDirPath() + "/skills.json";
    m_skillManager->loadFromFile(skillFile);
    if (m_skillManager->skills().isEmpty())
        m_skillManager->loadDefaults();

    connect(m_skillManager, &SkillManager::changed, this, [this, skillFile]{
        m_skillManager->saveToFile(skillFile);
        m_dsClient->setSkillPrompt(m_skillManager->buildSkillPrompt());
        m_claudeClient->setSkillPrompt(m_skillManager->buildSkillPrompt());
    });

    setupUI();
}

MainWindow::~MainWindow() = default;

// ============ 通用导航 ============

void MainWindow::navigateTo(int pageIndex)
{
    m_stacked->setCurrentIndex(pageIndex);
}

void MainWindow::resetNavHighlight(QPushButton *active)
{
    QString s = QString(
        "QPushButton { background: transparent; color: #a0a0a0; border: none; "
        "border-radius: 4px; font-size: 12px; }"
        "QPushButton:hover { background: %1; color: white; }"
    ).arg(kSidebarActive);
    m_navMsg->setStyleSheet(s);
    m_navContact->setStyleSheet(s);
    m_navMoments->setStyleSheet(s);
    m_navBrowser->setStyleSheet(s);

    if (active) {
        active->setStyleSheet(QString(
            "QPushButton { background: %1; color: white; border: none; "
            "border-radius: 4px; font-size: 12px; }"
        ).arg(kSidebarActive));
    }
}

// ============ UI 入口 ============

void MainWindow::setupUI()
{
    setWindowTitle("桌面AI应用集合");
    resize(1050, 680);
    setMinimumSize(820, 540);

    // ============ 左侧边栏 (宽度 64) ============
    auto *leftSide = createLeftSidebar();

    // ============ 右侧主体 ============
    // -- 顶部工具栏 --
    m_searchBox = new QLineEdit;
    m_searchBox->setPlaceholderText("搜索");
    m_searchBox->setFixedSize(180, 32);
    m_searchBox->setStyleSheet(QString(
        "QLineEdit {"
        "  border: 1px solid #d0d0d0; border-radius: 16px;"
        "  padding: 2px 14px; background: %1; font-size: 13px;"
        "}"
        "QLineEdit:focus { border-color: %2; }"
    ).arg(kWhite, kBtnBlue));

    auto makeToolBtn = [](const QString &text) {
        auto *btn = new QPushButton(text);
        btn->setFixedSize(88, 32);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString(
            "QPushButton {"
            "  background-color: %1; color: white; border: none;"
            "  border-radius: 4px; font-size: 13px; font-weight: bold;"
            "}"
            "QPushButton:hover { background-color: %2; }"
            "QPushButton:pressed { background-color: #0b8ec4; }"
        ).arg(kBtnBlue, kBtnBlueHover));
        return btn;
    };
    m_btnAddFriend   = makeToolBtn("添加");
    m_btnCreateGroup = makeToolBtn("群聊");
    m_btnSettings    = makeToolBtn("设置");

    auto *toolLayout = new QHBoxLayout;
    toolLayout->setContentsMargins(20, 8, 20, 8);
    toolLayout->addWidget(m_searchBox);
    toolLayout->addStretch();
    toolLayout->addWidget(m_btnAddFriend);
    toolLayout->addSpacing(8);
    toolLayout->addWidget(m_btnCreateGroup);
    toolLayout->addSpacing(8);
    toolLayout->addWidget(m_btnSettings);

    auto *toolbar = new QWidget;
    toolbar->setFixedHeight(54);
    toolbar->setLayout(toolLayout);
    toolbar->setStyleSheet(QString("background-color: %1;").arg(kToolbarBg));

    auto *divider = new QFrame;
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet("color: #e0e0e0;");

    // -- 网络工具栏 --
    m_networkBar = createNetworkBar();

    // -- 主内容区 (QStackedWidget) --
    m_stacked = new QStackedWidget;
    m_stacked->addWidget(createMessagePanel());    // 0: 消息列表
    m_stacked->addWidget(createSettingsPanel());   // 1: 设置
    m_stacked->addWidget(createBrowserPanel());    // 2: 浏览器
    m_stacked->addWidget(createContactPanel());    // 3: 联系人
    m_stacked->addWidget(createMomentsPanel());    // 4: 动态
    // 5: 聊天详情 — 动态创建

    // -- 底部状态栏 --
    m_statusBar = new QLabel;
    m_statusBar->setFixedHeight(32);
    m_statusBar->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_statusBar->setContentsMargins(20, 0, 0, 0);
    m_statusBar->setStyleSheet(QString(
        "font-size: 12px; color: #888; background: %1; "
        "border-top: 1px solid #e4e4e4;"
    ).arg(kLightGray));
    m_statusBar->setText(QString::fromUtf8("\xE2\x97\x8F 在线 - 桌面AI应用集合"));

    // ---- 组装右侧 ----
    auto *rightLayout = new QVBoxLayout;
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);
    rightLayout->addWidget(toolbar);
    rightLayout->addWidget(divider);
    rightLayout->addWidget(m_networkBar);
    rightLayout->addWidget(m_stacked, 1);
    rightLayout->addWidget(m_statusBar);

    auto *rightWidget = new QWidget;
    rightWidget->setLayout(rightLayout);

    // ============ 整体水平布局 ============
    auto *mainLayout = new QHBoxLayout;
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(leftSide);
    mainLayout->addWidget(rightWidget, 1);

    auto *central = new QWidget(this);
    central->setLayout(mainLayout);
    setCentralWidget(central);

    // ============ 信号 / 槽 ============
    connect(m_btnAddFriend,   &QPushButton::clicked, this, &MainWindow::onBtnAddFriend);
    connect(m_btnCreateGroup, &QPushButton::clicked, this, &MainWindow::onBtnCreateGroup);
    connect(m_btnSettings,    &QPushButton::clicked, this, &MainWindow::onBtnSettings);
    connect(m_sidebarSearch,  &QLineEdit::returnPressed, this, &MainWindow::onSidebarSearch);

    connect(m_navMsg, &QPushButton::clicked, this, [this]{
        navigateTo(0);
        resetNavHighlight(m_navMsg);
    });
    connect(m_navContact, &QPushButton::clicked, this, [this]{
        navigateTo(3);
        resetNavHighlight(m_navContact);
    });
    connect(m_navMoments, &QPushButton::clicked, this, [this]{
        navigateTo(4);
        resetNavHighlight(m_navMoments);
    });
    connect(m_navBrowser, &QPushButton::clicked, this, [this]{
        navigateTo(2);
        resetNavHighlight(m_navBrowser);
    });

    // ---- 网络信号 ----
    connect(m_btnListen,    &QPushButton::clicked, this, &MainWindow::onStartListen);
    connect(m_btnStopListen,&QPushButton::clicked, this, &MainWindow::onStopListen);
    connect(m_btnConnectPeer,&QPushButton::clicked, this, &MainWindow::onConnectPeer);
    connect(m_peerAddrEdit, &QLineEdit::returnPressed, this, &MainWindow::onConnectPeer);

    // ---- DeepSeek AI 信号 ----
    connect(m_dsClient, &DeepSeekClient::thinking,      this, &MainWindow::onDsThinking);
    connect(m_dsClient, &DeepSeekClient::replyReceived, this, &MainWindow::onDsReply);
    connect(m_dsClient, &DeepSeekClient::errorOccurred, this, &MainWindow::onDsError);

    // ---- Claude AI 信号 ----
    connect(m_claudeClient, &ClaudeClient::thinking,      this, &MainWindow::onClaudeThinking);
    connect(m_claudeClient, &ClaudeClient::replyReceived, this, &MainWindow::onClaudeReply);
    connect(m_claudeClient, &ClaudeClient::errorOccurred, this, &MainWindow::onClaudeError);
}

// ============ 左侧边栏 ============

QWidget *MainWindow::createLeftSidebar()
{
    auto *w = new QWidget;
    w->setFixedWidth(64);
    w->setStyleSheet(QString("background-color: %1;").arg(kSidebarBg));

    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(8, 12, 8, 12);
    lay->setSpacing(6);

    // ---- 头像 ----
    auto *avatar = new QLabel;
    avatar->setFixedSize(44, 44);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setStyleSheet(QString(
        "background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 %1, stop:1 #8e44ad);"
        "border-radius: 22px; color: white; font-size: 18px; font-weight: bold;"
    ).arg(kBtnBlue));
    avatar->setText("Q");

    // ---- 搜索栏 ----
    m_sidebarSearch = new QLineEdit;
    m_sidebarSearch->setPlaceholderText(QString::fromUtf8("\xF0\x9F\x94\x8D"));
    m_sidebarSearch->setFixedSize(48, 28);
    m_sidebarSearch->setAlignment(Qt::AlignCenter);
    m_sidebarSearch->setStyleSheet(QString(
        "QLineEdit {"
        "  background: #3a3f45; color: #ccc; border: 1px solid #4a4f55;"
        "  border-radius: 4px; font-size: 11px; padding: 2px 4px;"
        "}"
        "QLineEdit:focus { background: #444950; color: white; }"
    ));

    // ---- 导航按钮 ----
    auto makeNav = [&](const QString &icon, const QString &label) {
        auto *btn = new QPushButton(icon + "\n" + label);
        btn->setFixedSize(48, 52);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString(
            "QPushButton { background: transparent; color: #a0a0a0; border: none; "
            "border-radius: 4px; font-size: 12px; }"
            "QPushButton:hover { background: %1; color: white; }"
        ).arg(kSidebarActive));
        return btn;
    };

    m_navMsg     = makeNav(QString::fromUtf8("\xF0\x9F\x92\xAC"), "消息");
    m_navContact = makeNav(QString::fromUtf8("\xF0\x9F\x91\xA5"), "联系人");
    m_navMoments = makeNav(QString::fromUtf8("\xE2\xAD\x90"),     "动态");
    m_navBrowser = makeNav(QString::fromUtf8("\xF0\x9F\x8C\x90"), "浏览器");

    // 消息默认选中
    m_navMsg->setStyleSheet(QString(
        "QPushButton { background: %1; color: white; border: none; "
        "border-radius: 4px; font-size: 12px; }"
    ).arg(kSidebarActive));

    lay->addWidget(avatar,  0, Qt::AlignHCenter);
    lay->addSpacing(8);
    lay->addWidget(m_sidebarSearch, 0, Qt::AlignHCenter);
    lay->addSpacing(4);
    lay->addWidget(m_navMsg,     0, Qt::AlignHCenter);
    lay->addWidget(m_navContact, 0, Qt::AlignHCenter);
    lay->addWidget(m_navMoments, 0, Qt::AlignHCenter);
    lay->addWidget(m_navBrowser, 0, Qt::AlignHCenter);
    lay->addStretch();

    return w;
}

// ============ 消息面板 ============

QWidget *MainWindow::createMessagePanel()
{
    m_msgList = new QListWidget;
    m_msgList->setStyleSheet(QString(
        "QListWidget { border: none; background: %1; outline: none; }"
        "QListWidget::item { padding: 6px 0px; }"
        "QListWidget::item:hover { background: %2; }"
        "QListWidget::item:selected { background: %2; }"
    ).arg(kWhite, kMsgHoverBg));

    m_msgList->setIconSize(QSize(44, 44));
    m_msgList->setContextMenuPolicy(Qt::CustomContextMenu);

    auto *store = ContactStore::instance();
    auto *history = ChatHistory::instance();

    for (int i = 0; i < store->contactCount(); ++i) {
        const auto &c = store->allContacts()[i];
        int colorIdx = i % 10;
        QPixmap pix = makeAvatar(c.displayName, kAvatarColors[colorIdx].name());

        auto *item = new QListWidgetItem(QIcon(pix), "");
        item->setSizeHint(QSize(0, 62));
        item->setData(Qt::UserRole,     c.id);   // 存 contact ID
        item->setData(Qt::UserRole + 1, kAvatarColors[colorIdx].name());
        m_msgList->addItem(item);

        auto *w = new QWidget;
        auto *lay = new QVBoxLayout(w);
        lay->setContentsMargins(4, 6, 16, 6);
        lay->setSpacing(2);

        auto *header = new QHBoxLayout;
        auto *nameLbl = new QLabel(c.displayName);
        nameLbl->setStyleSheet("font-size: 14px; font-weight: bold; color: #222;");

        QString statusStr = c.online ? "在线" : "[离线]";
        QString statusColor = c.online ? "#4cd137" : "#aaaaaa";
        auto *statusLbl = new QLabel(statusStr);
        statusLbl->setStyleSheet(QString("font-size: 11px; color: %1;").arg(statusColor));

        header->addWidget(nameLbl);
        header->addWidget(statusLbl);
        header->addStretch();

        // 使用历史记录中的最后一条消息作为摘要
        QString summary = c.statusText;
        ChatMessage last = history->lastMessage(c.id);
        if (last.id > 0) {
            // 截取纯文本前 20 个字符
            QString preview = last.content;
            preview.remove(QRegularExpression("<[^>]*>"));
            preview = preview.left(20);
            if (last.content.length() > 20) preview += "...";
            summary = preview;
        }
        auto *summaryLbl = new QLabel(summary);
        summaryLbl->setStyleSheet("font-size: 12px; color: #999;");

        lay->addLayout(header);
        lay->addWidget(summaryLbl);

        m_msgList->setItemWidget(m_msgList->item(i), w);
    }

    connect(m_msgList, &QListWidget::itemClicked,
            this, &MainWindow::onMsgItemClicked);
    connect(m_msgList, &QWidget::customContextMenuRequested,
            this, &MainWindow::onMsgListContextMenu);

    return m_msgList;
}

// ============ 聊天详情面板 ============

void MainWindow::onMsgItemClicked(QListWidgetItem *item)
{
    if (!item) return;
    QString contactId = item->data(Qt::UserRole).toString();
    QString color     = item->data(Qt::UserRole + 1).toString();

    const Contact *contact = ContactStore::instance()->byId(contactId);
    if (!contact) return;

    QString name = contact->displayName;

    // DeepSeek 首次需配置
    if (contact->type == "ai_deepseek") {
        if (!m_dsClient->showConfigDialog(this))
            return;
    }
    // Claude 首次需配置
    if (contact->type == "ai_claude") {
        if (!m_claudeClient->showConfigDialog(this))
            return;
    }

    m_currentChatPeer = name;
    m_currentPeerId   = contactId;
    m_currentPeerColor = color;

    // 移除旧的聊天详情
    if (m_chatDetailWidget) {
        m_stacked->removeWidget(m_chatDetailWidget);
        m_chatDetailWidget->deleteLater();
        m_chatDetailWidget = nullptr;
    }

    m_chatDetailWidget = createChatDetailPanel(name, contactId, color);
    m_stacked->addWidget(m_chatDetailWidget);
    m_stacked->setCurrentWidget(m_chatDetailWidget);

    // ── 加载聊天历史 ──
    auto *history = ChatHistory::instance();
    auto msgs = history->loadMessages(contactId);
    if (!msgs.isEmpty()) {
        m_chatDisplay->clear();
        for (const auto &m : msgs) {
            if (m.isFromMe) {
                m_chatDisplay->append(makeBubble(m.content, true, m.senderName, kBtnBlue));
            } else {
                m_chatDisplay->append(makeBubble(m.content, false, m.senderName, color));
            }
        }
    }

    m_statusBar->setText(QString::fromUtf8(
        "\xF0\x9F\x92\xAC 正在与 %1 聊天"
    ).arg(name));
}

void MainWindow::onChatBack()
{
    m_stacked->setCurrentIndex(0);
    resetNavHighlight(m_navMsg);
    m_statusBar->setText(QString::fromUtf8("\xE2\x97\x8F 在线 - 桌面AI应用集合"));
}

QWidget *MainWindow::createChatDetailPanel(const QString &name,
                                           const QString &contactId,
                                           const QString &avatarColor)
{
    auto *w = new QWidget;
    w->setStyleSheet(QString("background: %1;").arg(kWhite));

    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    // ---- 顶部栏 ----
    auto *topBar = new QHBoxLayout;
    topBar->setContentsMargins(12, 8, 12, 8);

    auto *backBtn = new QPushButton(QString::fromUtf8("\xE2\x86\x90 返回"));
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setStyleSheet(QString(
        "QPushButton { border: none; background: transparent;"
        " color: %1; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { color: #0b8ec4; }"
    ).arg(kBtnBlue));
    connect(backBtn, &QPushButton::clicked, this, &MainWindow::onChatBack);

    // 好友头像 + 名称
    auto *avatarLbl = new QLabel;
    avatarLbl->setPixmap(makeAvatar(name, avatarColor, 36));
    auto *nameLbl = new QLabel(name);
    nameLbl->setStyleSheet("font-size: 16px; font-weight: bold; color: #222;");

    // 模型切换下拉框
    m_modelCombo = new QComboBox;
    m_modelCombo->addItem("DeepSeek", "deepseek");
    m_modelCombo->addItem("Claude",   "claude");
    m_modelCombo->setStyleSheet(
        "QComboBox { border: 1px solid #d0d0d0; border-radius: 6px;"
        " padding: 4px 8px; font-size: 13px; background: white; }"
        "QComboBox::drop-down { border: none; }"
    );
    m_modelCombo->setCursor(Qt::PointingHandCursor);
    // 根据联系人类型设置默认选中
    {
        const Contact *c = ContactStore::instance()->byId(contactId);
        if (c && c->type == "ai_claude")
            m_modelCombo->setCurrentIndex(1);
        else
            m_modelCombo->setCurrentIndex(0);
    }

    topBar->addWidget(backBtn);
    topBar->addStretch();
    topBar->addWidget(avatarLbl);
    topBar->addSpacing(8);
    topBar->addWidget(nameLbl);
    topBar->addStretch();
    topBar->addWidget(m_modelCombo);

    auto *topDiv = new QFrame;
    topDiv->setFrameShape(QFrame::HLine);
    topDiv->setStyleSheet("color: #e0e0e0;");

    // ---- 聊天消息显示区 ----
    m_chatDisplay = new QTextEdit;
    m_chatDisplay->setReadOnly(true);
    m_chatDisplay->setStyleSheet(QString(
        "QTextEdit { border: none; background: %1; padding: 16px;"
        " font-size: 14px; color: #333; }"
    ).arg(kLightGray));

    // 显示系统欢迎消息
    m_chatDisplay->setHtml(QString(
        "<div style='text-align:center; padding:20px; color:#aaa; font-size:12px;'>"
        "%1 — 聊天记录从这里开始</div>"
    ).arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm")));

    // ---- 输入区域 ----
    auto *inputLayout = new QHBoxLayout;
    inputLayout->setContentsMargins(12, 8, 12, 8);

    m_chatInput = new QTextEdit;
    m_chatInput->setPlaceholderText("输入消息... (回车发送, Shift+回车换行)");
    m_chatInput->setFixedHeight(64);
    m_chatInput->installEventFilter(this);  // 回车发送
    m_chatInput->setStyleSheet(QString(
        "QTextEdit { border: 1px solid #d0d0d0; border-radius: 8px;"
        " padding: 8px; background: %1; font-size: 14px; }"
        "QTextEdit:focus { border-color: %2; }"
    ).arg(kWhite, kBtnBlue));

    auto *sendBtn = new QPushButton("发送");
    sendBtn->setFixedSize(72, 40);
    sendBtn->setCursor(Qt::PointingHandCursor);
    sendBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: white; border: none;"
        " border-radius: 6px; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { background: %2; }"
    ).arg(kBtnBlue, kBtnBlueHover));
    m_chatSendBtn = sendBtn;  // 保存引用，供 eventFilter 触发

    // 发送消息逻辑
    connect(sendBtn, &QPushButton::clicked, this, [this, name, contactId]{
        QString text = m_chatInput->toPlainText().trimmed();
        if (text.isEmpty()) return;

        const Contact *contact = ContactStore::instance()->byId(contactId);
        QString myName = m_localNameEdit->text().trimmed();
        if (myName.isEmpty()) myName = "我";

        auto *history = ChatHistory::instance();

        // 保存自己发出的消息到历史
        {
            ChatMessage m;
            m.conversationId = contactId;
            m.senderName     = myName;
            m.content        = makeBubble(text, true, myName, kBtnBlue);
            m.isFromMe       = true;
            history->saveMessage(m);
        }

        // 根据模型下拉框决定用哪个 AI（AI 联系人才走 AI 分支）
        bool isAiContact = contact && (contact->type == "ai_deepseek" || contact->type == "ai_claude");
        QString selectedModel = m_modelCombo ? m_modelCombo->currentData().toString() : "deepseek";

        if (isAiContact && selectedModel == "deepseek" && m_dsClient->isConfigured()) {
            m_chatDisplay->append(makeBubble(text, true, myName, kBtnBlue));
            m_chatDisplay->append(QString(
                "<div id='dsThinking' style='text-align:left; margin:8px 0;'>"
                "<span style='background:#f0f0f0; color:#999; padding:8px 14px;"
                " border-radius:12px; font-size:13px; display:inline-block;'>"
                "思考中...</span></div>"
            ));
            m_chatInput->clear();
            m_dsClient->sendMessage(text);
        }
        else if (isAiContact && selectedModel == "claude" && m_claudeClient->isConfigured()) {
            m_chatDisplay->append(makeBubble(text, true, myName, kBtnBlue));
            m_chatInput->clear();

            if (m_webSearchCheck && m_webSearchCheck->isChecked()) {
                m_chatDisplay->append(QString(
                    "<div id='claudeThinking' style='text-align:left; margin:8px 0;'>"
                    "<span style='background:#e8f4e8; color:#27ae60; padding:8px 14px;"
                    " border-radius:12px; font-size:13px; display:inline-block;'>"
                    "\xF0\x9F\x8C\x90 联网搜索中...</span></div>"
                ));

                auto *searchMgr = new QNetworkAccessManager(this);
                QString encodedQuery = QUrl::toPercentEncoding(text);

                auto onSearchFinished = [this](QNetworkAccessManager *mgr,
                                                const QString &userText,
                                                const QString &ctx,
                                                const QString &errMsg = {}) {
                    mgr->deleteLater();
                    QString html2 = m_chatDisplay->toHtml();
                    html2.replace("<div id='claudeThinking' style='text-align:left; margin:8px 0;'>"
                                  "<span style='background:#e8f4e8; color:#27ae60; padding:8px 14px;"
                                  " border-radius:12px; font-size:13px; display:inline-block;'>"
                                  "\xF0\x9F\x8C\x90 联网搜索中...</span></div>", "");
                    m_chatDisplay->setHtml(html2);
                    m_chatDisplay->moveCursor(QTextCursor::End);

                    if (!ctx.isEmpty()) {
                        m_chatDisplay->append(QString(
                            "<div style='text-align:left; margin:6px 0;'>"
                            "<span style='background:#e8f4e8; color:#27ae60; padding:6px 12px;"
                            " border-radius:8px; font-size:12px; display:inline-block;'>"
                            "\xF0\x9F\x94\x8D 已搜索到 %1 条结果</span></div>"
                        ).arg(ctx.count('\n')));
                        QString searchPrompt = QString::fromUtf8(
                            "【以下来自联网搜索结果，请优先基于这些信息回答用户问题】\n\n%1\n"
                            "【搜索结果结束。请基于以上搜索到的信息，用中文回答用户问题。"
                            "如果搜索结果不相关或不充分，请如实告知用户，并给出你的了解。"
                            "回答时请引用搜索来源编号如[1][2]】\n\n"
                            "用户问题: %2"
                        ).arg(ctx, userText);
                        m_claudeClient->sendMessage(searchPrompt);
                    } else {
                        if (!errMsg.isEmpty()) {
                            m_chatDisplay->append(QString(
                                "<div style='text-align:left; margin:6px 0;'>"
                                "<span style='background:#ffe0e0; color:#c0392b; padding:6px 12px;"
                                " border-radius:8px; font-size:11px; display:inline-block;'>"
                                "\xE2\x9D\x8C 搜索连接失败，直接发送消息\n"
                                "<span style='font-size:10px; color:#999;'>%1</span></span></div>"
                            ).arg(errMsg.toHtmlEscaped()));
                        } else {
                            m_chatDisplay->append(QString(
                                "<div style='text-align:left; margin:6px 0;'>"
                                "<span style='background:#fff3cd; color:#856404; padding:6px 12px;"
                                " border-radius:8px; font-size:12px; display:inline-block;'>"
                                "\xE2\x9A\xA0 未搜到结果，直接发送</span></div>"
                            ));
                        }
                        m_claudeClient->sendMessage(userText);
                    }
                };

                auto doSearch = [this, searchMgr, text](const QUrl &url, const QString &engine,
                                                         auto onSuccess, auto onFail) {
                    QNetworkRequest req(url);
                    req.setRawHeader("User-Agent",
                        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"
                        " (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
                    req.setRawHeader("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
                    auto *reply = searchMgr->get(req);
                    connect(reply, &QNetworkReply::finished, this, [=]{
                        reply->deleteLater();
                        if (reply->error() == QNetworkReply::NoError) {
                            onSuccess(QString::fromUtf8(reply->readAll()));
                        } else {
                            onFail(reply->errorString());
                        }
                    });
                };

                auto parseBing = [](const QString &html) -> QString {
                    QString ctx;
                    static QRegularExpression bingRe(
                        R"(<li\s+class="b_algo"[^>]*>(.*?)</li>)",
                        QRegularExpression::DotMatchesEverythingOption);
                    auto it = bingRe.globalMatch(html);
                    int count = 0;
                    while (it.hasNext() && count < 5) {
                        auto m = it.next();
                        QString block = m.captured(1);
                        static QRegularExpression titleRe(R"(<h2[^>]*><a[^>]*>(.*?)</a></h2>)",
                            QRegularExpression::DotMatchesEverythingOption);
                        static QRegularExpression descRe(R"(<p[^>]*>(.*?)</p>)",
                            QRegularExpression::DotMatchesEverythingOption);
                        auto tm = titleRe.match(block);
                        auto dm = descRe.match(block);
                        QString title = tm.hasMatch() ? tm.captured(1).trimmed() : "";
                        QString desc  = dm.hasMatch() ? dm.captured(1).trimmed() : "";
                        title.replace(QRegularExpression("<[^>]+>"), "");
                        desc.replace(QRegularExpression("<[^>]+>"), "");
                        if (!desc.isEmpty())
                            ctx += QString("[%1] %2: %3\n").arg(++count).arg(title.isEmpty() ? "..." : title, desc);
                    }
                    return ctx;
                };

                auto parseDdg = [](const QString &html) -> QString {
                    QString ctx;
                    static QRegularExpression snippetRe(
                        R"(<a\s+class="result__snippet"[^>]*>((?:(?!</a>).)*)</a>)",
                        QRegularExpression::DotMatchesEverythingOption);
                    auto matches = snippetRe.globalMatch(html);
                    int count = 0;
                    while (matches.hasNext() && count < 5) {
                        auto m = matches.next();
                        QString raw = m.captured(1).trimmed();
                        raw.replace(QRegularExpression("<[^>]+>"), "");
                        raw = raw.trimmed();
                        if (!raw.isEmpty())
                            ctx += QString::fromUtf8("[%1] %2\n").arg(++count).arg(raw);
                    }
                    return ctx;
                };

                doSearch(QUrl(QString("https://www.bing.com/search?q=%1&setlang=zh-cn").arg(encodedQuery)),
                         "Bing",
                    [=](const QString &bhtml){
                        onSearchFinished(searchMgr, text, parseBing(bhtml));
                    },
                    [=](const QString &bErr){
                        doSearch(QUrl(QString("https://html.duckduckgo.com/html/?q=%1").arg(encodedQuery)),
                                 "DuckDuckGo",
                            [=](const QString &dhtml){
                                onSearchFinished(searchMgr, text, parseDdg(dhtml));
                            },
                            [=](const QString &ddgErr){
                                onSearchFinished(searchMgr, text, QString(),
                                    QString::fromUtf8("Bing: %1\nDuckDuckGo: %2").arg(bErr, ddgErr));
                            });
                    });
            } else {
                m_chatDisplay->append(QString(
                    "<div id='claudeThinking' style='text-align:left; margin:8px 0;'>"
                    "<span style='background:#f0f0f0; color:#999; padding:8px 14px;"
                    " border-radius:12px; font-size:13px; display:inline-block;'>"
                    "Claude 思考中...</span></div>"
                ));
                m_claudeClient->sendMessage(text);
            }
        }
        // 如果对方在线（有网络连接），通过网络发送
        else if (m_peerSockets.contains(name)) {
            sendChatMessage(name, text);
        } else {
            // 本地模拟
            m_chatDisplay->append(makeBubble(text, true, myName, kBtnBlue));

            QStringList replies = {
                "好的，收到！",
                "嗯嗯，了解了~",
                "哈哈，有意思",
                "稍等，我看看",
                "没问题！",
                "这个想法不错"
            };
            QString reply = replies[QRandomGenerator::global()->bounded(replies.size())];
            m_chatDisplay->append(makeBubble(reply, false, name, m_currentPeerColor));
            refreshMessagePreview(contactId);
        }

        m_chatInput->clear();
    });

    inputLayout->addWidget(m_chatInput, 1);
    inputLayout->addWidget(sendBtn);

    // ---- 文件/图片按钮栏 ----
    auto *attachBar = new QHBoxLayout;
    attachBar->setContentsMargins(12, 2, 12, 6);
    attachBar->setSpacing(8);

    auto makeAttach = [](const QString &text) {
        auto *btn = new QPushButton(text);
        btn->setFixedHeight(28);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString(
            "QPushButton {"
            "  background: %1; color: #888; border: 1px solid #ddd;"
            "  border-radius: 4px; font-size: 12px; padding: 0 10px;"
            "}"
            "QPushButton:hover { color: %2; border-color: %2; background: #eef7ff; }"
        ).arg(kWhite, kBtnBlue));
        return btn;
    };

    auto *fileBtn  = makeAttach(QString::fromUtf8("\xF0\x9F\x93\x8E 发送文件"));
    auto *imageBtn = makeAttach(QString::fromUtf8("\xF0\x9F\x96\xBC 发送图片"));
    auto *skillBtn = makeAttach(QString::fromUtf8("\xF0\x9F\xA7\xA9 技能"));

    // 联网搜索复选框
    m_webSearchCheck = new QCheckBox(QString::fromUtf8("\xF0\x9F\x8C\x90 联网搜索"));
    m_webSearchCheck->setChecked(false);
    m_webSearchCheck->setCursor(Qt::PointingHandCursor);
    m_webSearchCheck->setStyleSheet(QString(
        "QCheckBox { color: #888; font-size: 12px; spacing: 4px; }"
        "QCheckBox:hover { color: %1; }"
        "QCheckBox::indicator { width: 16px; height: 16px; }"
        "QCheckBox::indicator:checked { background: %1; border: 1px solid %1; border-radius: 3px; }"
    ).arg(kBtnBlue));

    // 发送文件
    connect(fileBtn, &QPushButton::clicked, this, [this, name, contactId]{
        QString fp = QFileDialog::getOpenFileName(this, "选择要发送的文件");
        if (fp.isEmpty()) return;
        QFileInfo fi(fp);
        QString fileName = fi.fileName();

        const Contact *contact = ContactStore::instance()->byId(contactId);

        m_chatDisplay->append(QString(
            "<div style='text-align:right; margin:6px 16px;'>"
            "<span style='background:#e8f4e8; color:#333; padding:6px 12px;"
            " border-radius:6px; font-size:12px; display:inline-block;'>"
            "\xF0\x9F\x93\x8E %1</span></div>"
        ).arg(fileName));

        QString userText = m_chatInput->toPlainText().trimmed();
        bool isAiFile = contact && (contact->type == "ai_deepseek" || contact->type == "ai_claude");
        QString selModel = m_modelCombo ? m_modelCombo->currentData().toString() : "deepseek";
        if (isAiFile && selModel == "deepseek" && m_dsClient->isConfigured()) {
            m_chatDisplay->append(QString(
                "<div style='text-align:left; margin:8px 0;'>"
                "<span style='background:#f0f0f0; color:#999; padding:8px 14px;"
                " border-radius:12px; font-size:13px; display:inline-block;'>"
                "分析文件中...</span></div>"
            ));
            m_dsClient->sendFileMessage(fp, userText.isEmpty()
                ? QString::fromUtf8("请分析这个文件的内容并总结。") : userText);
            m_chatInput->clear();
        } else if (isAiFile && selModel == "claude" && m_claudeClient->isConfigured()) {
            m_chatDisplay->append(QString(
                "<div style='text-align:left; margin:8px 0;'>"
                "<span style='background:#f0f0f0; color:#999; padding:8px 14px;"
                " border-radius:12px; font-size:13px; display:inline-block;'>"
                "Claude 分析文件中...</span></div>"
            ));
            m_claudeClient->sendFileMessage(fp, userText.isEmpty()
                ? QString::fromUtf8("请分析这个文件的内容并总结。") : userText);
            m_chatInput->clear();
        } else {
            m_statusBar->setText(QString("已选择文件: %1").arg(fileName));
        }
    });

    // 发送图片
    connect(imageBtn, &QPushButton::clicked, this, [this, name, contactId]{
        QString fp = QFileDialog::getOpenFileName(this, "选择图片",
            QString(), "图片 (*.png *.jpg *.jpeg *.gif *.bmp *.webp)");
        if (fp.isEmpty()) return;
        QFileInfo fi(fp);
        QString fileName = fi.fileName();

        const Contact *contact = ContactStore::instance()->byId(contactId);

        m_chatDisplay->append(QString(
            "<div style='text-align:right; margin:6px 16px;'>"
            "<span style='background:#e8f4e8; color:#333; padding:6px 12px;"
            " border-radius:6px; font-size:12px; display:inline-block;'>"
            "\xF0\x9F\x96\xBC %1</span></div>"
        ).arg(fileName));

        QString userText = m_chatInput->toPlainText().trimmed();
        bool isAiImg = contact && (contact->type == "ai_deepseek" || contact->type == "ai_claude");
        QString selModelImg = m_modelCombo ? m_modelCombo->currentData().toString() : "deepseek";
        if (isAiImg && selModelImg == "deepseek" && m_dsClient->isConfigured()) {
            m_chatDisplay->append(QString(
                "<div style='text-align:left; margin:8px 0;'>"
                "<span style='background:#f0f0f0; color:#999; padding:8px 14px;"
                " border-radius:12px; font-size:13px; display:inline-block;'>"
                "分析图片中...</span></div>"
            ));
            m_dsClient->sendImageMessage(fp, userText);
            m_chatInput->clear();
        } else if (isAiImg && selModelImg == "claude" && m_claudeClient->isConfigured()) {
            m_chatDisplay->append(QString(
                "<div style='text-align:left; margin:8px 0;'>"
                "<span style='background:#f0f0f0; color:#999; padding:8px 14px;"
                " border-radius:12px; font-size:13px; display:inline-block;'>"
                "Claude 分析图片中...</span></div>"
            ));
            m_claudeClient->sendImageMessage(fp, userText);
            m_chatInput->clear();
        } else {
            m_statusBar->setText(QString("已选择图片: %1").arg(fileName));
        }
    });

    attachBar->addWidget(fileBtn);
    attachBar->addWidget(imageBtn);
    attachBar->addWidget(m_webSearchCheck);
    attachBar->addWidget(skillBtn);
    attachBar->addStretch();

    // 技能管理
    connect(skillBtn, &QPushButton::clicked, this, &MainWindow::showSkillDialog);

    auto *bottomDiv = new QFrame;
    bottomDiv->setFrameShape(QFrame::HLine);
    bottomDiv->setStyleSheet("color: #e0e0e0;");

    lay->addLayout(topBar);
    lay->addWidget(topDiv);
    lay->addWidget(m_chatDisplay, 1);
    lay->addWidget(bottomDiv);
    lay->addLayout(inputLayout);
    lay->addLayout(attachBar);

    return w;
}

// ============ 联系人面板 ============

QWidget *MainWindow::createContactPanel()
{
    auto *outer = new QWidget;
    outer->setStyleSheet(QString("background: %1;").arg(kWhite));

    auto *outerLay = new QVBoxLayout(outer);
    outerLay->setContentsMargins(0, 0, 0, 0);
    outerLay->setSpacing(0);

    // 搜索栏
    auto *searchBar = new QHBoxLayout;
    searchBar->setContentsMargins(12, 8, 12, 4);

    auto *contactSearch = new QLineEdit;
    contactSearch->setPlaceholderText(QString::fromUtf8("\xF0\x9F\x94\x8D 搜索联系人..."));
    contactSearch->setFixedHeight(32);
    contactSearch->setStyleSheet(QString(
        "QLineEdit { border: 1px solid #d0d0d0; border-radius: 16px;"
        "  padding: 2px 14px; background: %1; font-size: 13px; }"
        "QLineEdit:focus { border-color: %2; }"
    ).arg(kToolbarBg, kBtnBlue));
    searchBar->addWidget(contactSearch);

    m_contactList = new QListWidget;
    m_contactList->setStyleSheet(QString(
        "QListWidget { border: none; background: %1; outline: none; }"
        "QListWidget::item { padding: 4px 0px; }"
        "QListWidget::item:hover { background: %2; }"
    ).arg(kWhite, kMsgHoverBg));
    m_contactList->setIconSize(QSize(44, 44));
    m_contactList->setContextMenuPolicy(Qt::CustomContextMenu);

    auto *store = ContactStore::instance();

    for (int i = 0; i < store->contactCount(); ++i) {
        const auto &c = store->allContacts()[i];
        int colorIdx = i % 10;
        QPixmap pix = makeAvatar(c.displayName, kAvatarColors[colorIdx].name());

        auto *item = new QListWidgetItem(QIcon(pix), "");
        item->setSizeHint(QSize(0, 62));
        item->setData(Qt::UserRole,     c.id);
        item->setData(Qt::UserRole + 1, kAvatarColors[colorIdx].name());

        auto *w = new QWidget;
        auto *lay = new QVBoxLayout(w);
        lay->setContentsMargins(4, 6, 16, 6);
        lay->setSpacing(2);

        auto *header = new QHBoxLayout;
        auto *nameLbl = new QLabel(c.displayName);
        nameLbl->setStyleSheet("font-size: 14px; font-weight: bold; color: #222;");

        auto *onlineDot = new QLabel(c.online
            ? QString::fromUtf8("\xE2\x97\x8F")
            : QString::fromUtf8("\xE2\x97\x8B"));
        onlineDot->setStyleSheet(QString("font-size: 10px; color: %1;")
            .arg(c.online ? "#4cd137" : "#aaaaaa"));

        header->addWidget(nameLbl);
        header->addWidget(onlineDot);
        header->addStretch();

        auto *sigLbl = new QLabel(c.statusText);
        sigLbl->setStyleSheet("font-size: 12px; color: #999;");

        lay->addLayout(header);
        lay->addWidget(sigLbl);

        m_contactList->addItem(item);
        m_contactList->setItemWidget(item, w);
    }

    connect(m_contactList, &QListWidget::itemClicked, this,
            &MainWindow::onMsgItemClicked);
    connect(m_contactList, &QWidget::customContextMenuRequested,
            this, &MainWindow::onContactListContextMenu);

    // 搜索过滤
    connect(contactSearch, &QLineEdit::textChanged, this, [this, contactSearch]{
        QString filter = contactSearch->text().trimmed();
        for (int i = 0; i < m_contactList->count(); ++i) {
            auto *item = m_contactList->item(i);
            if (!item) continue;
            QString id = item->data(Qt::UserRole).toString();
            const Contact *c = ContactStore::instance()->byId(id);
            bool match = !c || filter.isEmpty()
                || c->displayName.contains(filter, Qt::CaseInsensitive);
            item->setHidden(!match);
        }
    });

    outerLay->addLayout(searchBar);
    outerLay->addWidget(m_contactList, 1);

    return outer;
}

// ============ 动态面板 ============

QWidget *MainWindow::createMomentsPanel()
{
    auto *w = new QWidget;
    w->setStyleSheet(QString("background: %1;").arg(kWhite));

    auto *outerLayout = new QVBoxLayout(w);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // ---- 顶部标题栏 ----
    auto *titleBar = new QHBoxLayout;
    titleBar->setContentsMargins(20, 12, 20, 12);

    auto *titleLbl = new QLabel("朋友圈");
    titleLbl->setStyleSheet("font-size: 20px; font-weight: bold; color: #333;");

    auto *refreshBtn = new QPushButton(QString::fromUtf8("\xF0\x9F\x94\x84 刷新"));
    refreshBtn->setCursor(Qt::PointingHandCursor);
    refreshBtn->setStyleSheet(QString(
        "QPushButton { border: none; background: transparent;"
        " color: %1; font-size: 13px; }"
        "QPushButton:hover { color: #0b8ec4; }"
    ).arg(kBtnBlue));

    titleBar->addWidget(titleLbl);
    titleBar->addStretch();
    titleBar->addWidget(refreshBtn);

    auto *titleDiv = new QFrame;
    titleDiv->setFrameShape(QFrame::HLine);
    titleDiv->setStyleSheet("color: #e0e0e0;");

    // ---- 滚动区域 ----
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; background: white; }");

    auto *content = new QWidget;
    content->setStyleSheet("background: white;");
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    struct MomentItem {
        QString name;
        QString avatarColor;
        QString text;
        QString time;
        QStringList images;  // 用颜色方块模拟图片
    };

    const QVector<MomentItem> moments = {
        {"DeepSeek", "#12b7f5",
         "今天天气真好，适合出去走走！分享一组随手拍的照片~",
         "10 分钟前", {"#e74c3c", "#3498db", "#2ecc71"}},
        {"李四", "#8e44ad",
         "新版本终于发布了！感谢团队的努力付出，大家辛苦了！",
         "25 分钟前", {"#f39c12", "#1abc9c"}},
        {"王五", "#e74c3c",
         "推荐一本好书《设计模式》，受益匪浅",
         "1 小时前", {}},
        {"黄九", "#2ecc71",
         "周末骑行的快乐，50公里达成！",
         "2 小时前", {"#e67e22", "#3498db", "#9b59b6", "#1abc9c"}},
        {"赵六", "#27ae60",
         "今天学习了 Rust 语言，感觉很有趣！",
         "3 小时前", {}},
        {"吴十一","#3498db",
         "分享一下今天的美食：自己做的寿司，味道还不错",
         "5 小时前", {"#fbc531", "#e74c3c"}},
        {"马十五","#e67e22",
         "坚持阅读第100天，自律给我自由",
         "昨天", {}},
        {"冯十六","#1abc9c",
         "新买的机械键盘手感太棒了，打字速度提升了20%",
         "昨天", {"#12b7f5"}},
    };

    for (const auto &m : moments) {
        auto *card = new QWidget;
        card->setStyleSheet("background: white;");

        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(20, 12, 20, 12);
        cardLayout->setSpacing(6);

        // 头像 + 名称 + 时间
        auto *header = new QHBoxLayout;
        auto *avatarLbl = new QLabel;
        avatarLbl->setPixmap(makeAvatar(m.name, m.avatarColor, 44));
        header->addWidget(avatarLbl);

        auto *infoLayout = new QVBoxLayout;
        auto *nameLbl = new QLabel(m.name);
        nameLbl->setStyleSheet("font-size: 14px; font-weight: bold; color: #576b95;");
        auto *timeLbl = new QLabel(m.time);
        timeLbl->setStyleSheet("font-size: 11px; color: #aaa;");
        infoLayout->addWidget(nameLbl);
        infoLayout->addWidget(timeLbl);
        header->addLayout(infoLayout);
        header->addStretch();
        cardLayout->addLayout(header);

        // 文本内容
        auto *textLbl = new QLabel(m.text);
        textLbl->setWordWrap(true);
        textLbl->setStyleSheet("font-size: 14px; color: #333; margin: 4px 0;");
        cardLayout->addWidget(textLbl);

        // 图片模拟
        if (!m.images.isEmpty()) {
            auto *imgLayout = new QHBoxLayout;
            imgLayout->setSpacing(4);
            for (const auto &clr : m.images) {
                auto *imgBox = new QLabel;
                imgBox->setFixedSize(80, 80);
                imgBox->setStyleSheet(QString(
                    "background: %1; border-radius: 4px;"
                ).arg(clr));
                imgBox->setAlignment(Qt::AlignCenter);
                imgBox->setText(QString::fromUtf8("\xF0\x9F\x93\xB7"));
                imgLayout->addWidget(imgBox);
            }
            imgLayout->addStretch();
            cardLayout->addLayout(imgLayout);
        }

        // 操作栏
        auto *actions = new QHBoxLayout;
        auto makeAction = [](const QString &text) {
            auto *btn = new QPushButton(text);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setStyleSheet(
                "QPushButton { border: none; background: transparent;"
                " color: #999; font-size: 12px; padding: 2px 4px; }"
                "QPushButton:hover { color: #12b7f5; }");
            return btn;
        };
        actions->addWidget(makeAction(QString::fromUtf8("\xE2\x9D\xA4 赞")));
        actions->addWidget(makeAction(QString::fromUtf8("\xF0\x9F\x92\xAC 评论")));
        actions->addStretch();
        cardLayout->addLayout(actions);

        // 分割线
        auto *sep = new QFrame;
        sep->setFrameShape(QFrame::HLine);
        sep->setStyleSheet("color: #f0f0f0;");
        cardLayout->addWidget(sep);

        contentLayout->addWidget(card);
    }

    contentLayout->addStretch();
    scroll->setWidget(content);

    outerLayout->addLayout(titleBar);
    outerLayout->addWidget(titleDiv);
    outerLayout->addWidget(scroll, 1);

    return w;
}

// ============ 设置面板 ============

QWidget *MainWindow::createSettingsPanel()
{
    auto *w = new QWidget;
    w->setStyleSheet(QString("background: %1;").arg(kWhite));

    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(40, 30, 40, 30);
    lay->setSpacing(16);

    auto *title = new QLabel("设置");
    title->setStyleSheet("font-size: 22px; font-weight: bold; color: #333;");

    auto *sectionLine = new QFrame;
    sectionLine->setFrameShape(QFrame::HLine);
    sectionLine->setStyleSheet("color: #e0e0e0;");

    // 通用设置表单
    auto *form = new QFormLayout;
    form->setSpacing(12);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto styleLabel = [](const QString &text) {
        auto *lbl = new QLabel(text);
        lbl->setStyleSheet("font-size: 14px; color: #555;");
        return lbl;
    };

    // 主题选择
    auto *themeCombo = new QComboBox;
    themeCombo->addItems({"浅色模式", "深色模式", "跟随系统"});
    themeCombo->setCurrentIndex(0);
    themeCombo->setStyleSheet(QString(
        "QComboBox { border: 1px solid #d0d0d0; border-radius: 4px;"
        " padding: 4px 10px; font-size: 14px; background: white;"
        " min-width: 160px; }"
        "QComboBox:hover { border-color: %1; }"
    ).arg(kBtnBlue));
    form->addRow(styleLabel("主题皮肤："), themeCombo);

    // 字体大小
    auto *fontCombo = new QComboBox;
    fontCombo->addItems({"小", "标准", "大", "超大"});
    fontCombo->setCurrentIndex(1);
    fontCombo->setStyleSheet(themeCombo->styleSheet());
    form->addRow(styleLabel("字体大小："), fontCombo);

    // 通知开关
    auto *notifyCheck = new QCheckBox("接收新消息通知");
    notifyCheck->setChecked(true);
    notifyCheck->setStyleSheet(
        "QCheckBox { font-size: 14px; color: #555; spacing: 8px; }"
        "QCheckBox::indicator { width: 18px; height: 18px; }");
    form->addRow(styleLabel("消息通知："), notifyCheck);

    // 声音开关
    auto *soundCheck = new QCheckBox("收到消息时播放提示音");
    soundCheck->setChecked(true);
    soundCheck->setStyleSheet(notifyCheck->styleSheet());
    form->addRow(styleLabel("提示音："), soundCheck);

    // 启动时自动登录
    auto *autoLoginCheck = new QCheckBox("启动时自动登录");
    autoLoginCheck->setChecked(false);
    autoLoginCheck->setStyleSheet(notifyCheck->styleSheet());
    form->addRow(styleLabel("自动登录："), autoLoginCheck);

    // 隐私设置
    auto *privacyCheck = new QCheckBox("允许陌生人查看我的资料");
    privacyCheck->setChecked(false);
    privacyCheck->setStyleSheet(notifyCheck->styleSheet());
    form->addRow(styleLabel("隐私保护："), privacyCheck);

    // ---- 保存按钮 ----
    auto *saveBtn = new QPushButton("保存设置");
    saveBtn->setFixedSize(120, 36);
    saveBtn->setCursor(Qt::PointingHandCursor);
    saveBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: white; border: none;"
        " border-radius: 6px; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { background: %2; }"
    ).arg(kBtnBlue, kBtnBlueHover));

    auto *btnLayout = new QHBoxLayout;
    btnLayout->addStretch();
    btnLayout->addWidget(saveBtn);
    btnLayout->addStretch();

    // ---- 保存按钮逻辑 ----
    connect(saveBtn, &QPushButton::clicked, this, [this, themeCombo, fontCombo,
             notifyCheck, soundCheck, autoLoginCheck, privacyCheck]{
        auto *cfg = ConfigManager::instance();
        cfg->setClaudeModel(themeCombo->currentText());   // 复用存储做主题
        cfg->setDeepseekModel(fontCombo->currentText());  // 复用存储做字体
        cfg->setClaudeApiKey(notifyCheck->isChecked() ? "notify_on" : "notify_off");
        cfg->setDeepseekApiKey(soundCheck->isChecked() ? "sound_on" : "sound_off");
        cfg->setClaudeBaseUrl(autoLoginCheck->isChecked() ? "auto_login" : "");
        cfg->setDeepseekBaseUrl(privacyCheck->isChecked() ? "privacy_on" : "privacy_off");
        cfg->save();

        // 真正应用主题（简易实现：改背景色）
        if (themeCombo->currentIndex() == 1) {
            // 深色模式 — 这里只是模拟，实际需要整套样式替换
            m_statusBar->setStyleSheet(
                "font-size: 12px; color: #aaa; background: #333; border-top: 1px solid #444;");
        } else {
            m_statusBar->setStyleSheet(
                "font-size: 12px; color: #888; background: #f0f1f2; border-top: 1px solid #e4e4e4;");
        }

        QString summary = QString::fromUtf8(
            "\xE2\x9C\x85 设置已保存 — 主题:%1 | 字体:%2 | 通知:%3 | 提示音:%4"
        ).arg(themeCombo->currentText(),
              fontCombo->currentText(),
              notifyCheck->isChecked() ? "开" : "关",
              soundCheck->isChecked() ? "开" : "关");
        m_statusBar->setText(summary);
    });

    lay->addWidget(title);
    lay->addWidget(sectionLine);
    lay->addSpacing(8);
    lay->addLayout(form);
    lay->addSpacing(12);
    lay->addLayout(btnLayout);
    lay->addStretch();

    return w;
}

// ============ 浏览器面板 ============

QWidget *MainWindow::createBrowserPanel()
{
    auto *w = new QWidget;
    w->setStyleSheet(QString("background: %1;").arg(kWhite));

    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    // ---- 地址栏 + Go 按钮 ----
    auto *urlBar = new QHBoxLayout;
    urlBar->setContentsMargins(16, 12, 16, 12);
    urlBar->setSpacing(8);

    auto *backBtn = new QPushButton(QString::fromUtf8("\xE2\x86\x90"));
    backBtn->setFixedSize(36, 36);
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setToolTip("后退（浏览器历史）");
    backBtn->setStyleSheet(QString(
        "QPushButton { background: %1; border: 1px solid #d0d0d0; border-radius: 4px;"
        " font-size: 18px; color: #555; } QPushButton:hover { background: #e8e8e8; }"
        "QPushButton:disabled { color: #ccc; }"
    ).arg(kToolbarBg));
    connect(backBtn, &QPushButton::clicked, this, &MainWindow::onBrowserBack);

    m_browserUrlEdit = new QLineEdit;
    m_browserUrlEdit->setPlaceholderText("输入网址或搜索内容，回车打开系统浏览器...");
    m_browserUrlEdit->setStyleSheet(QString(
        "QLineEdit {"
        "  border: 1px solid #d0d0d0; border-radius: 18px;"
        "  padding: 6px 16px; background: %1; font-size: 14px;"
        "}"
        "QLineEdit:focus { border-color: %2; }"
    ).arg(kToolbarBg, kBtnBlue));
    m_browserUrlEdit->setFixedHeight(36);

    auto *goBtn = new QPushButton("打开");
    goBtn->setFixedSize(72, 36);
    goBtn->setCursor(Qt::PointingHandCursor);
    goBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: white; border: none;"
        " border-radius: 18px; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { background: %2; }"
    ).arg(kBtnBlue, kBtnBlueHover));

    urlBar->addWidget(backBtn);
    urlBar->addWidget(m_browserUrlEdit, 1);
    urlBar->addWidget(goBtn);

    // ---- 分隔线 ----
    auto *div = new QFrame;
    div->setFrameShape(QFrame::HLine);
    div->setStyleSheet("color: #e0e0e0;");

    // ---- 快捷链接区域 ----
    auto *quickGrid = new QGridLayout;
    quickGrid->setContentsMargins(40, 24, 40, 24);
    quickGrid->setSpacing(16);

    struct QuickLink {
        QString name;
        QString url;
        QString icon;
        QString bgColor;
    };
    const QVector<QuickLink> links = {
        {"百度",     "https://www.baidu.com",     "B", "#2932e1"},
        {"Google",   "https://www.google.com",    "G", "#4285f4"},
        {"B站",      "https://www.bilibili.com",  "B", "#fb7299"},
        {"GitHub",   "https://github.com",        "G", "#24292e"},
        {"知乎",     "https://www.zhihu.com",     "知", "#0066ff"},
        {"微博",     "https://www.weibo.com",     "微", "#e6162d"},
        {"淘宝",     "https://www.taobao.com",    "淘", "#ff5000"},
        {"QQ邮箱",   "https://mail.qq.com",       "邮", "#12b7f5"},
        {"CSDN",     "https://www.csdn.net",       "C", "#fc5531"},
        {"StackOverflow", "https://stackoverflow.com", "S", "#f48225"},
    };

    for (int i = 0; i < links.size(); ++i) {
        const auto &lk = links[i];

        auto *card = new QWidget;
        card->setFixedSize(120, 100);
        card->setCursor(Qt::PointingHandCursor);
        card->setStyleSheet(QString(
            "QWidget { background: %1; border-radius: 12px; }"
            "QWidget:hover { background: #f0f0f0; }"
        ).arg(kToolbarBg));

        auto *cardLay = new QVBoxLayout(card);
        cardLay->setContentsMargins(0, 0, 0, 0);
        cardLay->setSpacing(6);
        cardLay->setAlignment(Qt::AlignCenter);

        auto *iconLbl = new QLabel(lk.icon);
        iconLbl->setFixedSize(48, 48);
        iconLbl->setAlignment(Qt::AlignCenter);
        iconLbl->setStyleSheet(QString(
            "background: %1; border-radius: 24px; color: white;"
            " font-size: 20px; font-weight: bold;"
        ).arg(lk.bgColor));

        auto *nameLbl = new QLabel(lk.name);
        nameLbl->setAlignment(Qt::AlignCenter);
        nameLbl->setStyleSheet("font-size: 12px; color: #555;");

        cardLay->addWidget(iconLbl, 0, Qt::AlignHCenter);
        cardLay->addWidget(nameLbl, 0, Qt::AlignHCenter);

        quickGrid->addWidget(card, i / 5, i % 5);

        QString url = lk.url;
        auto *overlay = new QPushButton(card);
        overlay->setFixedSize(120, 100);
        overlay->setStyleSheet("background: transparent; border: none;");
        overlay->setCursor(Qt::PointingHandCursor);
        connect(overlay, &QPushButton::clicked, this, [this, url]{
            openInSystemBrowser(url);
        });
    }

    // ---- 快捷搜索栏 ----
    auto *searchBar = new QHBoxLayout;
    searchBar->setContentsMargins(40, 0, 40, 20);

    auto *searchLabel = new QLabel("快速搜索：");
    searchLabel->setStyleSheet("font-size: 13px; color: #888;");

    auto *quickSearch = new QLineEdit;
    quickSearch->setPlaceholderText("输入关键词，回车搜索...");
    quickSearch->setFixedHeight(34);
    quickSearch->setStyleSheet(QString(
        "QLineEdit { border: 1px solid #d0d0d0; border-radius: 17px;"
        " padding: 4px 16px; background: %1; font-size: 13px; }"
        "QLineEdit:focus { border-color: %2; }"
    ).arg(kToolbarBg, kBtnBlue));

    connect(quickSearch, &QLineEdit::returnPressed, this, [this, quickSearch]{
        QString text = quickSearch->text().trimmed();
        if (!text.isEmpty()) {
            openInSystemBrowser(text);
            quickSearch->clear();
        }
    });

    searchBar->addWidget(searchLabel);
    searchBar->addWidget(quickSearch, 1);

    // ---- 组装 ----
    lay->addLayout(urlBar);
    lay->addWidget(div);
    lay->addStretch();
    lay->addLayout(quickGrid);
    lay->addStretch();
    lay->addLayout(searchBar);

    // ---- 信号 ----
    connect(m_browserUrlEdit, &QLineEdit::returnPressed, this, &MainWindow::onBrowserNav);
    connect(goBtn, &QPushButton::clicked, this, &MainWindow::onBrowserNav);

    return w;
}

// ============ 左侧搜索框槽 ============

void MainWindow::onSidebarSearch()
{
    QString text = m_sidebarSearch->text().trimmed();
    if (text.isEmpty()) return;

    // 在联系人列表中搜索匹配项
    auto *store = ContactStore::instance();
    QStringList matches;
    for (const auto &c : store->allContacts()) {
        if (c.displayName.contains(text, Qt::CaseInsensitive) ||
            c.statusText.contains(text, Qt::CaseInsensitive)) {
            matches << c.displayName;
        }
    }

    if (!matches.isEmpty()) {
        // 导航到消息列表
        navigateTo(0);
        resetNavHighlight(m_navMsg);
        m_statusBar->setText(QString::fromUtf8(
            "\xE2\x9C\x85 找到 %1 个匹配: %2"
        ).arg(matches.size()).arg(matches.join(", ")));

        // 如果有精确匹配，自动打开聊天
        for (const auto &c : store->allContacts()) {
            if (c.displayName == text) {
                // 在消息列表中找对应的 item 点击
                for (int i = 0; i < m_msgList->count(); ++i) {
                    auto *item = m_msgList->item(i);
                    if (item && item->data(Qt::UserRole).toString() == c.id) {
                        onMsgItemClicked(item);
                        break;
                    }
                }
                break;
            }
        }
    } else {
        // 无匹配则打开系统浏览器搜索
        m_statusBar->setText(QString::fromUtf8(
            "\xE2\x97\x8F 未找到联系人: %1，已打开浏览器搜索"
        ).arg(text));
    }

    m_sidebarSearch->clear();
}

// ============ 浏览器面板槽 ============

void MainWindow::onBrowserNav()
{
    QString text = m_browserUrlEdit->text().trimmed();
    if (text.isEmpty()) return;

    // 添加到历史
    if (m_browserHistoryPos < m_browserHistory.size() - 1) {
        m_browserHistory = m_browserHistory.mid(0, m_browserHistoryPos + 1);
    }
    m_browserHistory.append(text);
    m_browserHistoryPos = m_browserHistory.size() - 1;

    openInSystemBrowser(text);
    m_statusBar->setText(QString::fromUtf8(
        "\xE2\x97\x8F 已在浏览器中打开: %1"
    ).arg(text));
}

void MainWindow::onBrowserBack()
{
    if (m_browserHistoryPos > 0) {
        m_browserHistoryPos--;
        QString prev = m_browserHistory[m_browserHistoryPos];
        m_browserUrlEdit->setText(prev);
        openInSystemBrowser(prev);
        m_statusBar->setText(QString::fromUtf8(
            "\xE2\x97\x8F 返回浏览: %1"
        ).arg(prev));
    }
}

// ============ 三个按钮的槽 ============

void MainWindow::onBtnAddFriend()
{
    auto *dlg = new QDialog(this);
    dlg->setWindowTitle("添加联系人");
    dlg->setFixedSize(380, 220);
    dlg->setStyleSheet(QString(
        "QDialog { background: %1; } QLabel { font-size: 14px; color: #555; } QLineEdit {"
        "  border: 1px solid #ddd; border-radius: 4px; padding: 6px 10px; font-size: 14px; }"
        "QLineEdit:focus { border-color: %2; }"
    ).arg(kWhite, kBtnBlue));

    auto *lay = new QFormLayout(dlg);
    lay->setContentsMargins(24, 20, 24, 20);
    lay->setSpacing(12);

    auto *nameEdit = new QLineEdit;
    nameEdit->setPlaceholderText("输入联系人昵称...");
    lay->addRow("昵称：", nameEdit);

    auto *sigEdit = new QLineEdit;
    sigEdit->setPlaceholderText("可选：个性签名");
    lay->addRow("签名：", sigEdit);

    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    btnBox->button(QDialogButtonBox::Ok)->setText("添加");
    btnBox->button(QDialogButtonBox::Ok)->setStyleSheet(QString(
        "QPushButton { background: %1; color: white; border: none; border-radius: 4px;"
        " padding: 6px 20px; font-size: 14px; } QPushButton:hover { background: %2; }"
    ).arg(kBtnBlue, kBtnBlueHover));
    btnBox->button(QDialogButtonBox::Cancel)->setText("取消");
    lay->addRow(btnBox);

    connect(btnBox, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

    if (dlg->exec() == QDialog::Accepted) {
        QString name = nameEdit->text().trimmed();
        if (name.isEmpty()) {
            QMessageBox::warning(this, "提示", "昵称不能为空");
            dlg->deleteLater();
            return;
        }

        // 生成唯一 ID
        QString id = "user_" + QString::number(QDateTime::currentSecsSinceEpoch());

        Contact c;
        c.displayName  = name;
        c.id           = id;
        c.avatarLetter = name.left(1);
        c.type         = "person";
        c.statusText   = sigEdit->text().trimmed().isEmpty()
                         ? "新朋友" : sigEdit->text().trimmed();
        c.online       = true;
        ContactStore::instance()->addContact(c);

        // 刷新消息列表
        int oldIdx = m_stacked->currentIndex();
        m_stacked->removeWidget(m_msgList);
        m_msgList->deleteLater();
        m_msgList = nullptr;
        m_stacked->insertWidget(0, createMessagePanel());
        m_stacked->setCurrentIndex(oldIdx);

        m_statusBar->setText(QString::fromUtf8(
            "\xE2\x9C\x85 已添加联系人: %1"
        ).arg(name));
    }
    dlg->deleteLater();
}

void MainWindow::onBtnCreateGroup()
{
    auto *store = ContactStore::instance();

    // 收集可以作为群成员的人（排除 AI 和已经存在的群）
    QStringList candidates;
    for (const auto &c : store->allContacts()) {
        if (c.type != "group")
            candidates << c.displayName;
    }

    auto *dlg = new QDialog(this);
    dlg->setWindowTitle("新建群聊");
    dlg->setFixedSize(400, 340);
    dlg->setStyleSheet(QString(
        "QDialog { background: %1; }"
        "QLabel { font-size: 14px; color: #555; }"
        "QLineEdit { border: 1px solid #ddd; border-radius: 4px;"
        "  padding: 6px 10px; font-size: 14px; }"
        "QLineEdit:focus { border-color: %2; }"
        "QListWidget { border: 1px solid #e0e0e0; border-radius: 4px;"
        "  font-size: 13px; }"
    ).arg(kWhite, kBtnBlue));

    auto *lay = new QVBoxLayout(dlg);
    lay->setContentsMargins(20, 16, 20, 16);
    lay->setSpacing(10);

    auto *nameEdit = new QLineEdit;
    nameEdit->setPlaceholderText("输入群聊名称...");
    lay->addWidget(new QLabel("群名称："));
    lay->addWidget(nameEdit);

    lay->addWidget(new QLabel("选择群成员（可多选）："));

    auto *memberList = new QListWidget;
    for (const auto &name : candidates) {
        auto *item = new QListWidgetItem(name);
        item->setCheckState(Qt::Unchecked);
        memberList->addItem(item);
    }
    lay->addWidget(memberList, 1);

    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    btnBox->button(QDialogButtonBox::Ok)->setText("创建");
    btnBox->button(QDialogButtonBox::Ok)->setStyleSheet(QString(
        "QPushButton { background: %1; color: white; border: none; border-radius: 4px;"
        " padding: 6px 20px; font-size: 14px; } QPushButton:hover { background: %2; }"
    ).arg(kBtnBlue, kBtnBlueHover));
    lay->addWidget(btnBox);

    connect(btnBox, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

    if (dlg->exec() == QDialog::Accepted) {
        QString name = nameEdit->text().trimmed();
        if (name.isEmpty()) {
            QMessageBox::warning(this, "提示", "群名称不能为空");
            dlg->deleteLater();
            return;
        }

        // 收集选中的成员
        QStringList members;
        for (int i = 0; i < memberList->count(); ++i) {
            auto *item = memberList->item(i);
            if (item->checkState() == Qt::Checked)
                members << item->text();
        }

        QString id = "group_" + QString::number(QDateTime::currentSecsSinceEpoch());
        QString memberStr = members.isEmpty()
            ? QString::fromUtf8("群聊") : members.join(", ");

        Contact c;
        c.displayName  = name;
        c.id           = id;
        c.avatarLetter = QString::fromUtf8("\xE7\xBE\xA4");  // "群"
        c.type         = "group";
        c.statusText   = QString::fromUtf8("%1 人 · %2")
                        .arg(members.size() + 1).arg(memberStr);
        c.online       = true;
        store->addContact(c);

        // 刷新消息列表
        int oldIdx = m_stacked->currentIndex();
        m_stacked->removeWidget(m_msgList);
        m_msgList->deleteLater();
        m_msgList = nullptr;
        m_stacked->insertWidget(0, createMessagePanel());
        m_stacked->setCurrentIndex(oldIdx);

        m_statusBar->setText(QString::fromUtf8(
            "\xE2\x9C\x85 群聊 \"%1\" 创建成功（%2人）"
        ).arg(name).arg(members.size() + 1));
    }
    dlg->deleteLater();
}

void MainWindow::onBtnSettings()
{
    m_stacked->setCurrentIndex(1);
    resetNavHighlight(nullptr);
    m_statusBar->setText(QString::fromUtf8("\xE2\x9A\x99 设置页面"));
}

// ============ DeepSeek AI 信号响应（胶水代码）============

void MainWindow::onDsThinking()
{
    // thinking 信号由 DeepSeekClient 发出时，气泡已由按钮 lambda 添加
}

void MainWindow::onDsReply(const QString &content)
{
    // 移除"思考中"提示
    QString html = m_chatDisplay->toHtml();
    html.replace("<div id='dsThinking' style='text-align:left; margin:8px 0;'>"
                 "<span style='background:#f0f0f0; color:#999; padding:8px 14px;"
                 " border-radius:12px; font-size:13px; display:inline-block;'>"
                 "思考中...</span></div>", "");
    m_chatDisplay->setHtml(html);
    m_chatDisplay->moveCursor(QTextCursor::End);
    QString bubble = makeBubble(content, false, "DeepSeek", "#12b7f5");
    m_chatDisplay->append(bubble);
    m_chatDisplay->moveCursor(QTextCursor::End);

    // ── 保存到聊天历史 ──
    ChatMessage m;
    m.conversationId = m_currentPeerId;
    m.senderName     = "DeepSeek";
    m.content        = bubble;
    m.isFromMe       = false;
    ChatHistory::instance()->saveMessage(m);
    refreshMessagePreview(m_currentPeerId);
}

void MainWindow::onDsError(const QString &error)
{
    QString html = m_chatDisplay->toHtml();
    html.replace("<div id='dsThinking' style='text-align:left; margin:8px 0;'>"
                 "<span style='background:#f0f0f0; color:#999; padding:8px 14px;"
                 " border-radius:12px; font-size:13px; display:inline-block;'>"
                 "思考中...</span></div>", "");
    m_chatDisplay->setHtml(html);
    m_chatDisplay->moveCursor(QTextCursor::End);
    m_chatDisplay->append(QString(
        "<div style='text-align:left; margin:8px 0;'>"
        "<span style='background:#ffe0e0; color:#c0392b; padding:8px 14px;"
        " border-radius:12px; font-size:13px; display:inline-block;'>"
        "错误: %1</span></div>"
    ).arg(error.toHtmlEscaped()));
}

// ============ Claude AI 信号响应 ============

void MainWindow::onClaudeThinking()
{
    // thinking 信号由 ClaudeClient 发出时，气泡已由按钮 lambda 添加
}

void MainWindow::onClaudeReply(const QString &content)
{
    // 移除"思考中"提示
    QString html = m_chatDisplay->toHtml();
    html.replace("<div id='claudeThinking' style='text-align:left; margin:8px 0;'>"
                 "<span style='background:#f0f0f0; color:#999; padding:8px 14px;"
                 " border-radius:12px; font-size:13px; display:inline-block;'>"
                 "Claude 思考中...</span></div>", "");
    m_chatDisplay->setHtml(html);
    m_chatDisplay->moveCursor(QTextCursor::End);
    QString bubble = makeBubble(content, false, QString::fromUtf8("\xe6\x9d\x8e\xe5\x9b\x9b"), "#8e44ad");
    m_chatDisplay->append(bubble);
    m_chatDisplay->moveCursor(QTextCursor::End);

    // ── 保存到聊天历史 ──
    ChatMessage m;
    m.conversationId = m_currentPeerId;
    m.senderName     = QString::fromUtf8("\xe6\x9d\x8e\xe5\x9b\x9b");
    m.content        = bubble;
    m.isFromMe       = false;
    ChatHistory::instance()->saveMessage(m);
    refreshMessagePreview(m_currentPeerId);
}

void MainWindow::onClaudeError(const QString &error)
{
    // 移除"思考中"提示
    QString html = m_chatDisplay->toHtml();
    html.replace("<div id='claudeThinking' style='text-align:left; margin:8px 0;'>"
                 "<span style='background:#f0f0f0; color:#999; padding:8px 14px;"
                 " border-radius:12px; font-size:13px; display:inline-block;'>"
                 "Claude 思考中...</span></div>", "");
    m_chatDisplay->setHtml(html);
    m_chatDisplay->moveCursor(QTextCursor::End);
    m_chatDisplay->append(QString(
        "<div style='text-align:left; margin:8px 0;'>"
        "<span style='background:#ffe0e0; color:#c0392b; padding:8px 14px;"
        " border-radius:12px; font-size:13px; display:inline-block;'>"
        "Claude 错误: %1</span></div>"
    ).arg(error.toHtmlEscaped()));
}

// ============ 微信风格气泡 ============

QString MainWindow::makeBubble(const QString &text, bool isMe,
                                const QString &avatarChar, const QString &avatarColor)
{
    QString bubbleBg  = isMe ? "#f8f8f8" : "#e5e5e5";
    QString bubbleFg  = isMe ? "#333"   : "#333";
    QString timeColor = isMe ? "#aaa"   : "#999";

    // Markdown → HTML 渲染
    QString rendered = MarkdownUtils::toHtml(text);
    if (rendered.isEmpty())
        rendered = text.toHtmlEscaped();

    // 时间戳
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm");

    // 头像
    QString avatar = QString(
        "<td width='36' style='padding:%1; vertical-align:top;'>"
        "<table cellpadding='0' cellspacing='0' border='0'><tr><td"
        " style='width:36px;height:36px;border-radius:18px;"
        "background:%2;color:white;text-align:center;"
        "font-size:13px;font-weight:bold;'>%3</td></tr></table>"
        "<div style='text-align:center;font-size:10px;color:%4;margin-top:2px;'>%5</div></td>"
    ).arg(isMe ? "0 0 0 8px" : "0 8px 0 0",
          avatarColor,
          avatarChar.left(1),
          timeColor,
          timestamp);

    // 气泡
    QString bubble = QString(
        "<td style='padding:0;vertical-align:top;'>"
        "<table cellpadding='0' cellspacing='0' border='0'><tr><td"
        " style='background:%1;color:%2;padding:10px 14px;border-radius:6px;"
        "font-size:14px;line-height:1.55;word-break:break-word;'>%3"
        "</td></tr></table></td>"
    ).arg(bubbleBg, bubbleFg, rendered);

    // 组合
    QString leftSide  = !isMe ? avatar + bubble : bubble + avatar;
    // spacer 确保右对齐时也能工作
    QString spacer = isMe ? "<td width='100%'></td>" : "";

    return QString(
        "<table width='100%%' cellpadding='0' cellspacing='0' border='0' style='margin:10px 0;'>"
        "<tr>%1%2<td width='16'></td></tr></table>"
    ).arg(spacer, leftSide);
}

// ============ 技能管理对话框 ============

void MainWindow::showSkillDialog()
{
    auto *dlg = new QDialog(this);
    dlg->setWindowTitle("技能管理");
    dlg->setFixedSize(500, 420);
    dlg->setStyleSheet(QString("QDialog { background: %1; }").arg(kWhite));

    auto *lay = new QVBoxLayout(dlg);
    lay->setContentsMargins(16, 16, 16, 16);
    lay->setSpacing(10);

    auto *title = new QLabel(QString::fromUtf8("\xF0\x9F\xA7\xA9 技能列表（勾选=激活，注入到 AI 系统提示词）"));
    title->setStyleSheet("font-size: 13px; color: #888;");
    lay->addWidget(title);

    // 技能列表
    auto *list = new QListWidget;
    list->setStyleSheet(QString(
        "QListWidget { border: 1px solid #e0e0e0; border-radius: 6px; background: %1; }"
        "QListWidget::item { padding: 4px 0; }"
    ).arg(kToolbarBg));

    auto refreshList = [&]{
        list->clear();
        const auto &skills = m_skillManager->skills();
        for (int i = 0; i < skills.size(); ++i) {
            auto *item = new QListWidgetItem;
            item->setSizeHint(QSize(0, 44));
            item->setData(Qt::UserRole, i);
            list->addItem(item);

            auto *w = new QWidget;
            auto *hl = new QHBoxLayout(w);
            hl->setContentsMargins(8, 4, 8, 4);
            hl->setSpacing(8);

            auto *cb = new QCheckBox(skills[i].name);
            cb->setChecked(skills[i].enabled);
            cb->setStyleSheet("font-size: 14px; font-weight: bold; color: #333;");
            connect(cb, &QCheckBox::toggled, this, [this, i](bool on){
                m_skillManager->setEnabled(i, on);
            });

            auto *desc = new QLabel(skills[i].prompt.left(40) + (skills[i].prompt.size() > 40 ? "..." : ""));
            desc->setStyleSheet("font-size: 12px; color: #aaa;");

            hl->addWidget(cb);
            hl->addWidget(desc, 1);
            list->setItemWidget(item, w);
        }
    };
    refreshList();
    lay->addWidget(list, 1);

    // 按钮栏
    auto *btns = new QHBoxLayout;
    btns->setSpacing(8);

    auto makeDlgBtn = [](const QString &text, const QString &bg) {
        auto *b = new QPushButton(text);
        b->setFixedHeight(32);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QString(
            "QPushButton { background: %1; color: white; border: none;"
            " border-radius: 4px; font-size: 13px; padding: 0 16px; }"
            "QPushButton:hover { opacity: 0.9; }"
        ).arg(bg));
        return b;
    };

    auto *addBtn    = makeDlgBtn("+ 添加", kBtnBlue);
    auto *editBtn   = makeDlgBtn("编辑", "#f39c12");
    auto *delBtn    = makeDlgBtn("删除", "#e74c3c");
    auto *resetBtn  = makeDlgBtn("恢复默认", "#888");
    auto *closeBtn  = makeDlgBtn("关闭", "#888");

    // 添加/编辑共用函数
    auto openEditor = [this, dlg, &refreshList](const QString &title, int editIdx = -1){
        auto *ed = new QDialog(dlg);
        ed->setWindowTitle(title);
        ed->setFixedSize(420, 260);
        ed->setStyleSheet(QString("QDialog { background: %1; }").arg(kWhite));

        auto *vl = new QVBoxLayout(ed);
        vl->setContentsMargins(16, 16, 16, 16);
        vl->setSpacing(10);

        auto *nameLbl = new QLabel("技能名称:");
        auto *nameEdit = new QLineEdit;
        nameEdit->setStyleSheet("border:1px solid #ddd; border-radius:4px; padding:6px 10px; font-size:14px;");

        auto *promptLbl = new QLabel("提示词 (告诉 AI 该怎么做):");
        auto *promptEdit = new QLineEdit;
        promptEdit->setStyleSheet(nameEdit->styleSheet());

        if (editIdx >= 0) {
            const auto &s = m_skillManager->skills()[editIdx];
            nameEdit->setText(s.name);
            promptEdit->setText(s.prompt);
        }

        auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        btnBox->button(QDialogButtonBox::Ok)->setText("保存");
        btnBox->button(QDialogButtonBox::Ok)->setStyleSheet(QString(
            "QPushButton { background: %1; color: white; border: none; border-radius: 4px;"
            " padding: 6px 20px; font-size: 14px; }"
        ).arg(kBtnBlue));

        connect(btnBox, &QDialogButtonBox::accepted, ed, [ed]{ ed->accept(); });
        connect(btnBox, &QDialogButtonBox::rejected, ed, &QDialog::reject);

        vl->addWidget(nameLbl);
        vl->addWidget(nameEdit);
        vl->addWidget(promptLbl);
        vl->addWidget(promptEdit);
        vl->addStretch();
        vl->addWidget(btnBox);

        if (ed->exec() == QDialog::Accepted) {
            QString nm = nameEdit->text().trimmed();
            QString pr = promptEdit->text().trimmed();
            if (nm.isEmpty()) {
                QMessageBox::warning(ed, "错误", "技能名称不能为空");
                ed->deleteLater();
                return;
            }
            if (editIdx >= 0)
                m_skillManager->updateSkill(editIdx, nm, pr);
            else
                m_skillManager->addSkill(nm, pr);
            refreshList();
        }
        ed->deleteLater();
    };

    connect(addBtn, &QPushButton::clicked, this, [&]{ openEditor("添加技能"); });
    connect(editBtn, &QPushButton::clicked, this, [&]{
        int row = list->currentRow();
        if (row < 0) { QMessageBox::information(dlg, "提示", "请先选择一个技能"); return; }
        int idx = list->currentItem()->data(Qt::UserRole).toInt();
        openEditor("编辑技能", idx);
    });
    connect(delBtn, &QPushButton::clicked, this, [&]{
        int row = list->currentRow();
        if (row < 0) { QMessageBox::information(dlg, "提示", "请先选择一个技能"); return; }
        int idx = list->currentItem()->data(Qt::UserRole).toInt();
        m_skillManager->removeSkill(idx);
        refreshList();
    });
    connect(resetBtn, &QPushButton::clicked, this, [&]{
        m_skillManager->loadDefaults();
        refreshList();
    });
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);

    btns->addWidget(addBtn);
    btns->addWidget(editBtn);
    btns->addWidget(delBtn);
    btns->addWidget(resetBtn);
    btns->addStretch();
    btns->addWidget(closeBtn);

    // 提示
    auto *hint = new QLabel(QString::fromUtf8(
        "提示: 激活的技能会在 DeepSeek 和 李四(Claude) 的对话中自动生效。技能保存在 skills.json 文件中。"));
    hint->setStyleSheet("font-size: 11px; color: #bbb;");
    lay->addLayout(btns);
    lay->addWidget(hint);

    dlg->exec();
    dlg->deleteLater();
}

// ============ 消息列表上下文菜单 ============

void MainWindow::onMsgListContextMenu(const QPoint &pos)
{
    auto *item = m_msgList->itemAt(pos);
    if (!item) return;

    QString contactId = item->data(Qt::UserRole).toString();
    const Contact *contact = ContactStore::instance()->byId(contactId);
    if (!contact) return;

    QMenu menu;
    menu.setStyleSheet(
        "QMenu { background: white; border: 1px solid #d0d0d0; border-radius: 6px; padding: 4px; }"
        "QMenu::item { padding: 8px 24px; font-size: 13px; }"
        "QMenu::item:hover { background: #eaf4fe; color: #12b7f5; }"
    );

    QAction *chatAction = menu.addAction(QString::fromUtf8("\xF0\x9F\x92\xAC 打开聊天"));
    menu.addSeparator();
    QAction *delAction = menu.addAction(QString::fromUtf8("\xE2\x9D\x8C 删除会话"));

    // 非内置联系人可删除
    if (contact->type == "ai_deepseek" || contact->type == "ai_claude")
        delAction->setEnabled(false);

    QAction *chosen = menu.exec(m_msgList->viewport()->mapToGlobal(pos));

    if (chosen == chatAction) {
        onMsgItemClicked(item);
    } else if (chosen == delAction) {
        auto confirm = QMessageBox::question(this, "确认删除",
            QString::fromUtf8("确定要删除与 \"%1\" 的会话吗？\n聊天记录也将被清除。")
            .arg(contact->displayName),
            QMessageBox::Yes | QMessageBox::No);
        if (confirm == QMessageBox::Yes) {
            ChatHistory::instance()->deleteConversation(contactId);
            ContactStore::instance()->removeContact(contactId);

            // 如果正在与该联系人聊天，返回消息列表
            if (m_currentPeerId == contactId) {
                onChatBack();
            }

            // 重建消息列表和联系人列表
            int oldIdx = m_stacked->currentIndex();
            m_stacked->removeWidget(m_msgList);
            m_msgList->deleteLater();
            m_msgList = nullptr;
            m_stacked->insertWidget(0, createMessagePanel());
            m_stacked->setCurrentIndex(oldIdx);

            m_statusBar->setText(QString::fromUtf8(
                "\xE2\x9C\x88 已删除: %1").arg(contact->displayName));
        }
    }
}

// ============ 联系人列表上下文菜单 ============

void MainWindow::onContactListContextMenu(const QPoint &pos)
{
    auto *item = m_contactList->itemAt(pos);
    if (!item) return;

    QString contactId = item->data(Qt::UserRole).toString();
    const Contact *contact = ContactStore::instance()->byId(contactId);
    if (!contact) return;

    QMenu menu;
    menu.setStyleSheet(
        "QMenu { background: white; border: 1px solid #d0d0d0; border-radius: 6px; padding: 4px; }"
        "QMenu::item { padding: 8px 24px; font-size: 13px; }"
        "QMenu::item:hover { background: #eaf4fe; color: #12b7f5; }"
    );

    QAction *chatAction = menu.addAction(QString::fromUtf8("\xF0\x9F\x92\xAC 开始聊天"));
    menu.addSeparator();
    QAction *delAction = menu.addAction(QString::fromUtf8("\xE2\x9D\x8C 删除联系人"));

    if (contact->type == "ai_deepseek" || contact->type == "ai_claude")
        delAction->setEnabled(false);

    QAction *chosen = menu.exec(m_contactList->viewport()->mapToGlobal(pos));

    if (chosen == chatAction) {
        // 在消息列表中找对应 item 点击
        for (int i = 0; i < m_msgList->count(); ++i) {
            auto *msgItem = m_msgList->item(i);
            if (msgItem && msgItem->data(Qt::UserRole).toString() == contactId) {
                onMsgItemClicked(msgItem);
                navigateTo(0);
                resetNavHighlight(m_navMsg);
                break;
            }
        }
    } else if (chosen == delAction) {
        auto confirm = QMessageBox::question(this, "确认删除",
            QString::fromUtf8("确定要删除联系人 \"%1\" 吗？").arg(contact->displayName),
            QMessageBox::Yes | QMessageBox::No);
        if (confirm == QMessageBox::Yes) {
            ContactStore::instance()->removeContact(contactId);
            if (m_currentPeerId == contactId) onChatBack();

            int oldIdx = m_stacked->currentIndex();
            m_stacked->removeWidget(m_msgList);
            m_msgList->deleteLater();
            m_msgList = nullptr;
            m_stacked->insertWidget(0, createMessagePanel());
            m_stacked->setCurrentIndex(oldIdx);

            m_statusBar->setText(QString::fromUtf8(
                "\xE2\x9C\x88 已删除: %1").arg(contact->displayName));
        }
    }
}

// ============ 刷新消息预览 ============

void MainWindow::refreshMessagePreview(const QString &contactId)
{
    if (!m_msgList) return;

    for (int i = 0; i < m_msgList->count(); ++i) {
        auto *item = m_msgList->item(i);
        if (!item || item->data(Qt::UserRole).toString() != contactId) continue;

        auto *w = m_msgList->itemWidget(item);
        if (!w) break;

        // 找所有 QLabel 子控件，更新摘要
        auto labels = w->findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly);
        for (auto *lbl : labels) {
            // 摘要标签（颜色为 #999 的）
            if (lbl->styleSheet().contains("#999")) {
                ChatMessage last = ChatHistory::instance()->lastMessage(contactId);
                if (last.id > 0) {
                    QString preview = last.content;
                    preview.remove(QRegularExpression("<[^>]*>"));
                    preview = preview.left(20);
                    if (last.content.length() > 20) preview += "...";
                    lbl->setText(preview);
                }
                break;
            }
        }
        break;
    }
}

// ============ 事件过滤器 - 回车发送消息 ============

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_chatInput && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            // Shift+Enter = 换行; 纯 Enter = 发送
            if (keyEvent->modifiers() & Qt::ShiftModifier) {
                return QMainWindow::eventFilter(obj, event);
            }
            if (m_chatSendBtn && m_chatSendBtn->isEnabled()) {
                m_chatSendBtn->click();
            }
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

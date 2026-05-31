#include "markdownutils.h"
#include <QMap>
#include <QRegularExpression>
#include <QStringList>

namespace {
    // ---- 颜色常量 ----
    const char *kCodeBg    = "#1e1e1e";
    const char *kCodeFg    = "#d4d4d4";
    const char *kInlineBg  = "#f0f0f0";
    const char *kInlineFg  = "#e74c3c";
    const char *kQuoteBg   = "#f5f7fa";
    const char *kQuoteBorder = "#12b7f5";
    const char *kLinkColor = "#12b7f5";
    const char *kHrColor   = "#e0e0e0";

    // ---- 语法高亮关键词表 ----
    QMap<QString, QStringList> keywordsMap;
    bool keywordsInit = false;

    void initKeywords() {
        if (keywordsInit) return;
        keywordsInit = true;
        keywordsMap["cpp"]    = {"auto","break","case","class","const","continue",
            "default","delete","do","else","enum","explicit","extern","false","for",
            "friend","goto","if","inline","int","long","namespace","new","noexcept",
            "nullptr","operator","override","private","protected","public","return",
            "short","signed","sizeof","static","struct","switch","template","this",
            "throw","true","try","typedef","typename","union","unsigned","using",
            "virtual","void","volatile","while","include","define","pragma","ifdef",
            "ifndef","endif","error","QString","QObject","QWidget","QVariant"};

        keywordsMap["python"] = {"False","None","True","and","as","assert","async",
            "await","break","class","continue","def","del","elif","else","except",
            "finally","for","from","global","if","import","in","is","lambda","nonlocal",
            "not","or","pass","raise","return","try","while","with","yield","self","cls",
            "print","range","len","str","int","float","list","dict","set","tuple","type"};

        keywordsMap["js"]     = {"async","await","break","case","catch","class","const",
            "continue","debugger","default","delete","do","else","enum","export","extends",
            "false","finally","for","function","if","implements","import","in","instanceof",
            "interface","let","new","null","of","package","private","protected","public",
            "return","static","super","switch","this","throw","true","try","typeof","var",
            "void","while","with","yield","undefined","NaN","console","document","window",
            "Promise","async","require","module","exports"};

        keywordsMap["java"]   = {"abstract","assert","boolean","break","byte","case",
            "catch","char","class","const","continue","default","do","double","else",
            "enum","extends","final","finally","float","for","goto","if","implements",
            "import","instanceof","int","interface","long","native","new","package",
            "private","protected","public","return","short","static","strictfp","super",
            "switch","synchronized","this","throw","throws","transient","try","void",
            "volatile","while","String","System","Object","Integer","List","Map","Set"};

        keywordsMap["go"]     = {"break","case","chan","const","continue","default",
            "defer","else","fallthrough","for","func","go","goto","if","import",
            "interface","map","package","range","return","select","struct","switch",
            "type","var","nil","true","false","make","new","len","cap","append","error",
            "string","int","bool","byte","rune","float64","int64","uint64","context"};

        keywordsMap["rust"]   = {"as","async","await","break","const","continue",
            "crate","dyn","else","enum","extern","false","fn","for","if","impl","in",
            "let","loop","match","mod","move","mut","pub","ref","return","self","Self",
            "static","struct","super","trait","true","type","unsafe","use","where",
            "while","yield","Some","None","Ok","Err","Box","Vec","String","Result","Option"};

        keywordsMap["sh"]     = {"alias","bg","bind","break","builtin","caller",
            "case","command","compgen","complete","continue","declare","dirs","disown",
            "echo","enable","eval","exec","exit","export","false","fc","fg","getopts",
            "hash","help","history","if","jobs","kill","let","local","logout","popd",
            "printf","pushd","pwd","read","return","set","shift","shopt","source",
            "suspend","test","times","trap","true","type","typeset","ulimit","umask",
            "unalias","unset","until","wait","while","then","else","elif","fi","for",
            "in","do","done","case","esac","function","select","time","cd","ls","cat",
            "grep","awk","sed","mkdir","rm","cp","mv","chmod","chown","sudo","apt","yum"};

        keywordsMap["bash"]   = keywordsMap["sh"];
        keywordsMap["shell"]  = keywordsMap["sh"];

        keywordsMap["c"]      = keywordsMap["cpp"];
        keywordsMap["h"]      = keywordsMap["cpp"];
        keywordsMap["cs"]     = {"abstract","as","base","bool","break","byte","case",
            "catch","char","checked","class","const","continue","decimal","default",
            "delegate","do","double","else","enum","event","explicit","extern","false",
            "finally","fixed","float","for","foreach","goto","if","implicit","in","int",
            "interface","internal","is","lock","long","namespace","new","null","object",
            "operator","out","override","params","private","protected","public","readonly",
            "ref","return","sbyte","sealed","short","sizeof","stackalloc","static",
            "string","struct","switch","this","throw","true","try","typeof","uint",
            "ulong","unchecked","unsafe","ushort","using","var","virtual","void",
            "volatile","while","async","await","record","var","dynamic","nameof"};

        keywordsMap["sql"]    = {"SELECT","FROM","WHERE","INSERT","INTO","VALUES",
            "UPDATE","SET","DELETE","CREATE","TABLE","ALTER","DROP","INDEX","VIEW",
            "JOIN","LEFT","RIGHT","INNER","OUTER","ON","AS","AND","OR","NOT","NULL",
            "IS","IN","LIKE","BETWEEN","ORDER","BY","ASC","DESC","GROUP","HAVING",
            "LIMIT","OFFSET","UNION","ALL","DISTINCT","COUNT","SUM","AVG","MIN","MAX",
            "CASE","WHEN","THEN","ELSE","END","EXISTS","PRIMARY","KEY","FOREIGN",
            "REFERENCES","CONSTRAINT","DEFAULT","CHECK","UNIQUE","CASCADE","BEGIN",
            "COMMIT","ROLLBACK","TRANSACTION","IF","INT","VARCHAR","TEXT","BOOLEAN",
            "DATE","TIMESTAMP","FLOAT","DOUBLE","DECIMAL","BLOB","AUTO_INCREMENT"};

        keywordsMap["json"]   = {};
        keywordsMap["xml"]    = {};
        keywordsMap["html"]   = {"html","head","body","div","span","p","a","img","ul",
            "ol","li","table","tr","td","th","thead","tbody","form","input","button",
            "select","option","textarea","label","h1","h2","h3","h4","h5","h6","script",
            "style","link","meta","title","header","footer","nav","section","article",
            "main","aside","strong","em","br","hr","pre","code","blockquote","iframe"};
        keywordsMap["css"]    = {};
    }
}

QString MarkdownUtils::highlightCode(const QString &code, const QString &language)
{
    initKeywords();

    // ---- 输出行 ----
    QStringList lines = code.split('\n');
    QStringList result;

    // 获取该语言的关键词
    QStringList keywords;
    QString lang = language.toLower().simplified();
    // 别名映射
    QMap<QString,QString> aliases = {
        {"c++","cpp"}, {"cxx","cpp"}, {"cc","cpp"}, {"hpp","cpp"},
        {"javascript","js"}, {"typescript","js"}, {"ts","js"},
        {"python","python"}, {"py","python"},
        {"java","java"},
        {"golang","go"},
        {"c#","cs"}, {"csharp","cs"},
        {"sh","sh"}, {"bash","bash"}, {"zsh","sh"},
        {"rust","rust"}, {"rs","rust"},
        {"sql","sql"},
        {"json","json"}, {"xml","xml"}, {"html","html"}, {"css","css"},
    };
    if (aliases.contains(lang)) lang = aliases[lang];
    if (keywordsMap.contains(lang)) keywords = keywordsMap[lang];

    // 判定是否需要用引号标记注释
    auto isCppFamily = [](const QString &l) {
        return l == "cpp" || l == "c" || l == "java" || l == "go" ||
               l == "rust" || l == "cs";
    };
    bool cStyleComment = isCppFamily(lang) || lang == "js";

    for (const QString &line : lines) {
        QString escaped = line.toHtmlEscaped();
        QString htmlLine;

        if (lang == "json" || lang == "xml" || lang == "html" || lang == "css") {
            // 纯文本渲染，不额外高亮
            htmlLine = escaped;
        } else if (lang == "sql") {
            // 关键字高亮 (大小写不敏感)
            htmlLine = escaped;
            for (const QString &kw : keywords) {
                QRegularExpression kwRe("\\b" + kw + "\\b", QRegularExpression::CaseInsensitiveOption);
                htmlLine.replace(kwRe, QString("<span style='color:#569cd6;'>%1</span>").arg(kw));
            }
            // 字符串
            htmlLine.replace(QRegularExpression("('(?:[^'\\\\]|\\\\.)*')"),
                "<span style='color:#ce9178;'>\\1</span>");
            // 数字
            htmlLine.replace(QRegularExpression("\\b(\\d+\\.?\\d*)\\b"),
                "<span style='color:#b5cea8;'>\\1</span>");
        } else {
            // 通用高亮管线
            htmlLine = escaped;

            // 1. 整行注释 (//) — 先处理
            if (cStyleComment) {
                htmlLine.replace(QRegularExpression("^(//.*)$"),
                    "<span style='color:#6a9955;'>\\1</span>");
                htmlLine.replace(QRegularExpression("^(#.*)$"),
                    "<span style='color:#6a9955;'>\\1</span>");
            } else if (lang == "python" || lang == "sh" || lang == "bash") {
                htmlLine.replace(QRegularExpression("^(\\s*#.*)$"),
                    "<span style='color:#6a9955;'>\\1</span>");
            }

            // 2. 字符串 (双引号)
            htmlLine.replace(QRegularExpression(R"("(?:[^"\\]|\\.)*")"),
                "<span style='color:#ce9178;'>\\0</span>");
            // 3. 字符串 (单引号)
            htmlLine.replace(QRegularExpression(R"('(?:[^'\\]|\\.)*')"),
                "<span style='color:#ce9178;'>\\0</span>");
            // 4. 反引号字符串 (JS/Go 模板)
            htmlLine.replace(QRegularExpression("`(?:[^`\\\\]|\\\\.)*`"),
                "<span style='color:#ce9178;'>\\0</span>");

            // 5. 行尾注释 (//)
            if (cStyleComment) {
                htmlLine.replace(QRegularExpression("(//.*)$"),
                    "<span style='color:#6a9955;'>\\1</span>");
            }

            // 6. 数字
            htmlLine.replace(QRegularExpression("\\b(\\d+\\.?\\d*f?)\\b"),
                "<span style='color:#b5cea8;'>\\1</span>");

            // 7. 关键词
            for (const QString &kw : keywords) {
                QRegularExpression kwRe("\\b" + QRegularExpression::escape(kw) + "\\b");
                htmlLine.replace(kwRe, QString("<span style='color:#569cd6;font-weight:bold;'>%1</span>").arg(kw));
            }

            // 8. 函数调用模式: word(
            htmlLine.replace(QRegularExpression("\\b([a-zA-Z_]\\w*)\\s*\\("),
                "<span style='color:#dcdcaa;'>\\1</span>(");
        }

        result.append(htmlLine);
    }

    return result.join('\n');
}

// ===== 内联处理（前向声明 + 定义） =====
static QString processInline(const QString &text);

QString MarkdownUtils::toHtml(const QString &markdown)
{
    QString md = markdown;

    // ---- 阶段 0: 提取并保护代码块 ----
    struct CodeBlock {
        QString lang;
        QString code;
    };
    QList<CodeBlock> codeBlocks;
    {
        static QRegularExpression codeRe(
            R"(```(\w*)\s*\n(.*?)\n\s*```)",
            QRegularExpression::DotMatchesEverythingOption);
        int idx = 0;
        auto it = codeRe.globalMatch(md);
        QList<QPair<int,int>> ranges;
        while (it.hasNext()) {
            auto m = it.next();
            CodeBlock cb;
            cb.lang = m.captured(1).trimmed();
            cb.code = m.captured(2);
            codeBlocks.append(cb);
            ranges.prepend({static_cast<int>(m.capturedStart(0)), static_cast<int>(m.capturedLength(0))});
        }
        for (int i = 0; i < ranges.size(); ++i) {
            md.replace(ranges[i].first, ranges[i].second,
                       QString("\x01CODE%1\x01").arg(i));
        }
    }

    // ---- 阶段 1: 块级元素 (需要整行匹配) ----
    QStringList lines = md.split('\n');
    QStringList output;
    bool inUl = false, inOl = false, inBlockquote = false;

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i];

        // 代码块占位符 → 不需要块处理，直接输出
        if (line.startsWith("\x01CODE")) {
            if (inUl)  { output.append("</ul>");  inUl  = false; }
            if (inOl)  { output.append("</ol>");  inOl  = false; }
            if (inBlockquote) { output.append("</div>"); inBlockquote = false; }
            output.append(line);
            continue;
        }

        // 空行 → 结束列表和引用
        if (line.trimmed().isEmpty()) {
            if (inUl)  { output.append("</ul>");  inUl  = false; }
            if (inOl)  { output.append("</ol>");  inOl  = false; }
            if (inBlockquote) { output.append("</div>"); inBlockquote = false; }
            output.append("");
            continue;
        }

        // 水平分割线
        static QRegularExpression hrRe("^[-*_]{3,}\\s*$");
        if (hrRe.match(line.trimmed()).hasMatch()) {
            if (inUl)  { output.append("</ul>");  inUl  = false; }
            if (inOl)  { output.append("</ol>");  inOl  = false; }
            output.append(QString("<hr style='border:none;border-top:1px solid %1;margin:8px 0;'>").arg(kHrColor));
            continue;
        }

        // 标题
        static QRegularExpression h1Re("^#{6}\\s+(.+)$");
        static QRegularExpression h2Re("^#{5}\\s+(.+)$");
        static QRegularExpression h3Re("^#{4}\\s+(.+)$");
        static QRegularExpression h4Re("^#{3}\\s+(.+)$");
        static QRegularExpression h5Re("^#{2}\\s+(.+)$");
        static QRegularExpression h6Re("^#{1}\\s+(.+)$");
        auto heading = [&](QRegularExpression &re, int size) -> bool {
            auto m = re.match(line);
            if (m.hasMatch()) {
                if (inUl)  { output.append("</ul>");  inUl  = false; }
                if (inOl)  { output.append("</ol>");  inOl  = false; }
                output.append(QString("<h%1 style='font-size:%2px;font-weight:bold;"
                    "color:#222;margin:10px 0 4px 0;'>%3</h%1>")
                    .arg(size).arg(24 - (size-1)*2).arg(m.captured(1).toHtmlEscaped()));
                return true;
            }
            return false;
        };
        if (heading(h1Re, 6)) continue;
        if (heading(h2Re, 5)) continue;
        if (heading(h3Re, 4)) continue;
        if (heading(h4Re, 3)) continue;
        if (heading(h5Re, 2)) continue;
        if (heading(h6Re, 1)) continue;

        // 引用
        if (line.trimmed().startsWith('>')) {
            if (!inBlockquote) {
                output.append(QString("<div style='background:%1;border-left:3px solid %2;"
                    "padding:6px 12px;margin:6px 0;border-radius:0 6px 6px 0;color:#555;'>")
                    .arg(kQuoteBg, kQuoteBorder));
                inBlockquote = true;
            }
            QString content = line.trimmed().mid(1).trimmed();
            // 对内联内容做 markdown 处理
            content = processInline(content);
            output.append(content + "<br>");
            continue;
        } else if (inBlockquote) {
            output.append("</div>");
            inBlockquote = false;
        }

        // 无序列表
        static QRegularExpression ulRe("^\\s*[-+*]\\s+(.+)$");
        auto ulm = ulRe.match(line);
        if (ulm.hasMatch()) {
            if (!inUl) { output.append("<ul style='margin:4px 0;padding-left:20px;'>"); inUl = true; }
            output.append(QString("<li style='margin:2px 0;'>%1</li>")
                .arg(processInline(ulm.captured(1))));
            continue;
        } else if (inUl) {
            output.append("</ul>"); inUl = false;
        }

        // 有序列表
        static QRegularExpression olRe("^\\s*\\d+\\.\\s+(.+)$");
        auto olm = olRe.match(line);
        if (olm.hasMatch()) {
            if (!inOl) { output.append("<ol style='margin:4px 0;padding-left:20px;'>"); inOl = true; }
            output.append(QString("<li style='margin:2px 0;'>%1</li>")
                .arg(processInline(olm.captured(1))));
            continue;
        } else if (inOl) {
            output.append("</ol>"); inOl = false;
        }

        // 普通段落
        output.append(QString("<p style='margin:4px 0;'>%1</p>").arg(processInline(line)));
    }

    // 关闭未闭合的块
    if (inUl) output.append("</ul>");
    if (inOl) output.append("</ol>");
    if (inBlockquote) output.append("</div>");

    // ---- 阶段 2: 恢复代码块 ----
    QString html = output.join('\n');
    for (int i = 0; i < codeBlocks.size(); ++i) {
        const auto &cb = codeBlocks[i];
        QString highlighted = highlightCode(cb.code, cb.lang);
        QString langLabel = cb.lang.isEmpty() ? "" :
            QString("<div style='color:#888;font-size:11px;padding:2px 0;'>%1</div>").arg(cb.lang);
        QString codeHtml = QString(
            "<div style='background:%1;border-radius:8px;margin:10px 0;overflow:hidden;'>"
            "%2"
            "<table cellpadding='0' cellspacing='0' border='0' width='100%%'><tr>"
            "<td style='background:#333;width:8px;'></td>"
            "<td style='padding:10px 14px;'><pre style='margin:0;color:%3;font-size:13px;"
            "line-height:1.55;font-family:\"Cascadia Code\",\"Fira Code\",Consolas,monospace;"
            "white-space:pre-wrap;word-break:break-word;'>%4</pre></td>"
            "</tr></table></div>"
        ).arg(kCodeBg, langLabel, kCodeFg, highlighted);
        html.replace(QString("\x01CODE%1\x01").arg(i).toHtmlEscaped(), codeHtml);
    }

    return html.trimmed();
}

// ===== 内联处理（不公开） =====
QString processInline(const QString &text)
{
    QString t = text.toHtmlEscaped();

    // 行内代码 `...`
    t.replace(QRegularExpression("`([^`]+)`"),
        QString("<code style='background:%1;color:%2;padding:1px 5px;"
                "border-radius:3px;font-family:Consolas,monospace;font-size:13px;'>\\1</code>")
        .arg(kInlineBg, kInlineFg));

    // 粗体 **...** 或 __...__
    t.replace(QRegularExpression(R"(\*\*(.+?)\*\*)"),
        "<b style='color:#111;'>\\1</b>");
    t.replace(QRegularExpression(R"(__(.+?)__)"),
        "<b style='color:#111;'>\\1</b>");

    // 斜体 *...* 或 _..._
    t.replace(QRegularExpression(R"(\*([^*]+)\*)"),
        "<i>\\1</i>");
    t.replace(QRegularExpression("_([^_]+)_"),
        "<i>\\1</i>");

    // 链接 [text](url)
    t.replace(QRegularExpression(R"(\[([^\]]+)\]\(([^)]+)\))"),
        QString("<a href='\\2' style='color:%1;text-decoration:none;'>\\1</a>"
                "<span style='color:#aaa;font-size:11px;'> (\\2)</span>")
        .arg(kLinkColor));

    return t;
}

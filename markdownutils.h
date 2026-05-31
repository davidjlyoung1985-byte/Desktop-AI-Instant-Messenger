#pragma once
#include <QString>

// ============ Markdown → HTML 渲染工具 ============
// 支持的语法:
//   # 标题    **粗体**  *斜体*  `行内代码`
//   ```lang  代码块（自动高亮）  > 引用
//   - 列表   1. 有序列表   [链接](url)   --- 分割线
//
// 代码高亮支持: C/C++, Python, JS/TS, Java, Go, Rust, Shell, SQL, JSON, HTML/XML

namespace MarkdownUtils {

/// 将 Markdown 文本转换为 QQ 聊天风格 HTML
/// 返回可直接插入 QTextEdit 的 HTML 片段
QString toHtml(const QString &markdown);

/// 对代码块做语法高亮
QString highlightCode(const QString &code, const QString &language);

} // namespace MarkdownUtils

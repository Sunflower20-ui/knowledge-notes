#include "markdown_highlighter.h"

MarkdownHighlighter::MarkdownHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    setupRules();
}

void MarkdownHighlighter::setupRules()
{
    // --- Base formats (Catppuccin Mocha palette) ---
    QTextCharFormat headerFormat;
    headerFormat.setForeground(QColor("#cba6f7"));
    headerFormat.setFontWeight(QFont::Bold);

    QTextCharFormat boldFormat;
    boldFormat.setFontWeight(QFont::Bold);
    boldFormat.setForeground(QColor("#fab387"));

    QTextCharFormat italicFormat;
    italicFormat.setFontItalic(true);
    italicFormat.setForeground(QColor("#89b4fa"));

    QTextCharFormat boldItalicFormat;
    boldItalicFormat.setFontWeight(QFont::Bold);
    boldItalicFormat.setFontItalic(true);
    boldItalicFormat.setForeground(QColor("#fab387"));

    QTextCharFormat codeFormat;
    codeFormat.setFontFamilies({QStringLiteral("Consolas"), QStringLiteral("Cascadia Code"), QStringLiteral("monospace")});
    codeFormat.setForeground(QColor("#a6e3a1"));
    codeFormat.setBackground(QColor("#313244"));

    QTextCharFormat linkFormat;
    linkFormat.setForeground(QColor("#89b4fa"));
    linkFormat.setFontUnderline(true);

    QTextCharFormat listFormat;
    listFormat.setForeground(QColor("#f9e2af"));

    QTextCharFormat quoteFormat;
    quoteFormat.setForeground(QColor("#a6adc8"));
    quoteFormat.setFontItalic(true);

    QTextCharFormat hrFormat;
    hrFormat.setForeground(QColor("#6c7086"));

    QTextCharFormat strikethroughFormat;
    strikethroughFormat.setFontStrikeOut(true);
    strikethroughFormat.setForeground(QColor("#6c7086"));

    // ============================================================
    // Rules (order matters — more specific first)
    // ============================================================

    // Horizontal rule: ---, ***, ___
    m_rules.append({
        QRegularExpression(QStringLiteral("^[ ]{0,3}([-*_])[ ]{0,2}(?:\\1[ ]{0,2}){2,}[ \t]*$")),
        hrFormat
    });

    // ATX Headers (# H1 through ###### H6)
    m_rules.append({
        QRegularExpression(QStringLiteral("^#{1,6}\\s+.*$")),
        headerFormat
    });

    // Bold + Italic: ***text***
    m_rules.append({
        QRegularExpression(QStringLiteral("\\*\\*\\*(.+?)\\*\\*\\*")),
        boldItalicFormat
    });

    // Bold: **text** or __text__
    m_rules.append({
        QRegularExpression(QStringLiteral("\\*\\*(.+?)\\*\\*")),
        boldFormat
    });
    m_rules.append({
        QRegularExpression(QStringLiteral("__(.+?)__")),
        boldFormat
    });

    // Italic: *text* or _text_
    m_rules.append({
        QRegularExpression(QStringLiteral("(?<!\\*)\\*(?!\\*)(.+?)(?<!\\*)\\*(?!\\*)")),
        italicFormat
    });
    m_rules.append({
        QRegularExpression(QStringLiteral("(?<!_)_(?!_)(.+?)(?<!_)_(?!_)")),
        italicFormat
    });

    // Strikethrough: ~~text~~
    m_rules.append({
        QRegularExpression(QStringLiteral("~~(.+?)~~")),
        strikethroughFormat
    });

    // Inline code: `code`
    m_rules.append({
        QRegularExpression(QStringLiteral("`([^`]+)`")),
        codeFormat
    });

    // Links: [text](url)
    m_rules.append({
        QRegularExpression(QStringLiteral("\\[([^\\]]+)\\]\\(([^)]+)\\)")),
        linkFormat
    });

    // Wiki links: [[title]]
    m_rules.append({
        QRegularExpression(QStringLiteral("\\[\\[([^\\]]+)\\]\\]")),
        linkFormat
    });

    // Unordered lists: -, *, +
    m_rules.append({
        QRegularExpression(QStringLiteral("^[ ]{0,3}[-*+][ ]+")),
        listFormat
    });

    // Ordered lists: 1. 2. etc.
    m_rules.append({
        QRegularExpression(QStringLiteral("^[ ]{0,3}\\d+\\.[ ]+")),
        listFormat
    });

    // Blockquotes: >
    m_rules.append({
        QRegularExpression(QStringLiteral("^>[ ]?")),
        quoteFormat
    });
}

void MarkdownHighlighter::highlightBlock(const QString &text)
{
    // --- Fenced code block state machine ---
    QRegularExpression fenceMatch(QStringLiteral("^```"));

    int prevState = previousBlockState();
    int currentState = prevState;

    if (fenceMatch.match(text).hasMatch()) {
        currentState = (prevState > 0) ? 0 : 1;
    }

    setCurrentBlockState(currentState);

    if (currentState > 0) {
        QTextCharFormat cbFmt;
        cbFmt.setFontFamilies({QStringLiteral("Consolas"), QStringLiteral("Cascadia Code"), QStringLiteral("monospace")});
        cbFmt.setForeground(QColor("#a6e3a1"));
        setFormat(0, text.length(), cbFmt);
        return;
    }

    // Apply regex rules
    for (const Rule &rule : m_rules) {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}

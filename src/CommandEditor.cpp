#include "CommandEditor.h"
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QTextBlock>

LineNumberArea::LineNumberArea(CommandEditor *editor) : QWidget(editor), editor_(editor) {
    setMouseTracking(true);
}
QSize LineNumberArea::sizeHint() const { return {editor_->gutterWidth(), 0}; }
void LineNumberArea::paintEvent(QPaintEvent *event) { editor_->paintGutter(event); }
void LineNumberArea::mousePressEvent(QMouseEvent *event) { editor_->gutterClick(event); }

CommandEditor::CommandEditor(QWidget *parent) : QPlainTextEdit(parent), gutter_(new LineNumberArea(this)) {
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setFont(QFont(QStringLiteral("Consolas"), 11));
    setFrameStyle(QFrame::NoFrame);
    setTabChangesFocus(false);
    setRangeLabels(QStringLiteral("Set Start (green)"), QStringLiteral("Set End (red)"), QStringLiteral("Clear Start and End"));
    connect(this, &QPlainTextEdit::blockCountChanged, this, &CommandEditor::updateGutterWidth);
    connect(this, &QPlainTextEdit::updateRequest, this, [this](const QRect &rect, int dy) {
        if (dy) gutter_->scroll(0, dy); else gutter_->update(0, rect.y(), gutter_->width(), rect.height());
        if (rect.contains(viewport()->rect())) updateGutterWidth();
    });
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, [this] {
        highlightCurrentLine(); gutter_->update();
    });
    connect(this, &QPlainTextEdit::textChanged, this, [this] {
        clearRange(); highlightCurrentLine();
    });
    updateGutterWidth();
    highlightCurrentLine();
}

int CommandEditor::gutterWidth() const {
    int digits = 2;
    for (int n = qMax(1, blockCount()); n >= 100; n /= 10) ++digits;
    return 24 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
}
void CommandEditor::updateGutterWidth() { setViewportMargins(gutterWidth(), 0, 0, 0); }
void CommandEditor::resizeEvent(QResizeEvent *event) {
    QPlainTextEdit::resizeEvent(event);
    gutter_->setGeometry(contentsRect().left(), contentsRect().top(), gutterWidth(), contentsRect().height());
}
void CommandEditor::highlightCurrentLine() {
    QTextEdit::ExtraSelection selection;
    selection.format.setBackground(QColor(230, 237, 251));
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.cursor = textCursor();
    selection.cursor.clearSelection();
    setExtraSelections({selection});
}
void CommandEditor::goToLine(int line) {
    QTextBlock block = document()->findBlockByNumber(line);
    if (!block.isValid()) return;
    QTextCursor cursor(block);
    setTextCursor(cursor);
    centerCursor();
}
void CommandEditor::setStartLine(int line) {
    startLine_ = line; gutter_->update(); emit rangeChanged();
}
void CommandEditor::setEndLine(int line) {
    endLine_ = line; gutter_->update(); emit rangeChanged();
}
void CommandEditor::clearRange() {
    if (startLine_ == -1 && endLine_ == -1) return;
    startLine_ = endLine_ = -1;
    gutter_->update(); emit rangeChanged();
}
void CommandEditor::setRangeLabels(QString start, QString end, QString clear) {
    startText_ = std::move(start); endText_ = std::move(end); clearText_ = std::move(clear);
}
void CommandEditor::paintGutter(QPaintEvent *event) {
    QPainter painter(gutter_);
    painter.fillRect(event->rect(), QColor(247, 249, 252));
    QTextBlock block = firstVisibleBlock();
    int number = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            if (number == currentLine()) painter.fillRect(0, top, gutter_->width(), bottom - top, QColor(230, 237, 251));
            painter.setPen(number == currentLine() ? QColor(68, 88, 151) : QColor(145, 154, 167));
            painter.drawText(20, top, gutter_->width() - 25, fontMetrics().height(),
                             Qt::AlignRight | Qt::AlignVCenter, QString::number(number + 1));
            if (number == startLine_ || number == endLine_) {
                painter.setRenderHint(QPainter::Antialiasing);
                QRect dot(5, top + (fontMetrics().height() - 9) / 2, 9, 9);
                painter.setPen(Qt::NoPen);
                if (number == startLine_) { painter.setBrush(QColor(58, 164, 104)); painter.drawEllipse(dot); }
                if (number == endLine_) {
                    painter.setBrush(QColor(211, 89, 91));
                    if (number == startLine_) painter.drawPie(dot, 270 * 16, 180 * 16);
                    else painter.drawEllipse(dot);
                }
            }
        }
        block = block.next(); ++number; top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
    }
    painter.setPen(QColor(225, 230, 238));
    painter.drawLine(gutter_->width() - 1, 0, gutter_->width() - 1, height());
}
void CommandEditor::gutterClick(QMouseEvent *event) {
    if (isReadOnly()) return;
    QTextBlock block = firstVisibleBlock();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    while (block.isValid()) {
        const int bottom = top + qRound(blockBoundingRect(block).height());
        if (event->pos().y() < bottom) break;
        block = block.next(); top = bottom;
    }
    if (!block.isValid()) return;
    contextLine_ = block.blockNumber();
    goToLine(contextLine_);
    if (event->button() == Qt::LeftButton) { setFocus(); return; }
    if (event->button() != Qt::RightButton) return;
    QMenu menu(this);
    menu.addAction(startText_, this, [this] { setStartLine(contextLine_); });
    menu.addAction(endText_, this, [this] { setEndLine(contextLine_); });
    menu.addSeparator();
    auto *clear = menu.addAction(clearText_, this, &CommandEditor::clearRange);
    clear->setEnabled(startLine_ >= 0 || endLine_ >= 0);
    menu.exec(event->globalPosition().toPoint());
}

#pragma once
#include <QPlainTextEdit>

class CommandEditor;

class LineNumberArea : public QWidget {
public:
    explicit LineNumberArea(CommandEditor *editor);
    QSize sizeHint() const override;
protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
private:
    CommandEditor *editor_;
};

class CommandEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit CommandEditor(QWidget *parent = nullptr);
    int currentLine() const { return textCursor().blockNumber(); }
    int startLine() const { return startLine_; }
    int endLine() const { return endLine_; }
    void goToLine(int line);
    void setStartLine(int line);
    void setEndLine(int line);
    void clearRange();
    int gutterWidth() const;
    void paintGutter(QPaintEvent *event);
    void gutterClick(QMouseEvent *event);
    void setRangeLabels(QString start, QString end, QString clear);
signals:
    void rangeChanged();
protected:
    void resizeEvent(QResizeEvent *event) override;
private:
    void updateGutterWidth();
    void highlightCurrentLine();
    LineNumberArea *gutter_;
    int startLine_ = -1;
    int endLine_ = -1;
    int contextLine_ = -1;
    QString startText_;
    QString endText_;
    QString clearText_;
};

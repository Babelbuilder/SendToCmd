#include "MainWindow.h"
#include "core/SendValidator.h"
#include "platform/NativeWindow.h"
#include <QActionGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QCursor>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QSaveFile>
#include <QStringDecoder>
#include <QTextBlock>
#include <QToolButton>
#include <QVBoxLayout>
#include <utility>

namespace {
class SendButton : public QToolButton {
public:
    explicit SendButton(QWidget *parent = nullptr) : QToolButton(parent) { setCursor(Qt::PointingHandCursor); }
    bool stopIcon = false;
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        const QPoint c = rect().center();
        if (stopIcon) {
            p.setBrush(isEnabled() ? (isDown() ? QColor(23, 106, 67) : underMouse() ? QColor(31, 127, 79) : QColor(40, 145, 89))
                                   : QColor(153, 181, 164));
            p.drawRoundedRect(QRect(c.x() - 8, c.y() - 8, 16, 16), 3, 3);
            return;
        }

        // Three folded faces follow the supplied paper-plane silhouette.
        const auto shade = [this](QColor color) {
            if (!isEnabled()) return QColor(153, 181, 164);
            return isDown() ? color.darker(120) : underMouse() ? color.lighter(110) : color;
        };
        p.save();
        p.translate((width() - 30.0) / 2.0, (height() - 30.0) / 2.0);
        p.scale(30.0 / 512.0, 30.0 / 512.0);
        p.setBrush(shade(QColor(QStringLiteral("#37966c"))));
        p.drawPolygon(QPolygonF{QPointF(0, 145), QPointF(512, 0), QPointF(367, 512),
                                QPointF(242, 387), QPointF(87, 425), QPointF(125, 270)});
        p.setBrush(shade(QColor(QStringLiteral("#4fad7e"))));
        p.drawPolygon(QPolygonF{QPointF(0, 145), QPointF(512, 0), QPointF(125, 270)});
        p.setBrush(shade(QColor(QStringLiteral("#267952"))));
        p.drawPolygon(QPolygonF{QPointF(125, 270), QPointF(512, 0), QPointF(242, 387), QPointF(87, 425)});
        p.restore();
    }
};

class TargetPicker : public QToolButton {
public:
    explicit TargetPicker(QWidget *parent = nullptr) : QToolButton(parent) { setCursor(Qt::CrossCursor); }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        const QColor color = isEnabled() ? (underMouse() ? QColor(19, 112, 139) : QColor(48, 97, 129))
                                         : QColor(158, 172, 185);
        p.setPen(QPen(color, 1.9, Qt::SolidLine, Qt::RoundCap));
        const QPointF c = rect().center();
        p.drawEllipse(c, 10, 10);
        p.drawLine(QPointF(c.x() - 6, c.y()), QPointF(c.x() + 6, c.y()));
        p.drawLine(QPointF(c.x(), c.y() - 6), QPointF(c.x(), c.y() + 6));
    }
};
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), settings_(QStringLiteral("SendToCmd"), QStringLiteral("SendToCmd")),
      target_(this), autoEngine_(this) {
    chinese_ = settings_.value(QStringLiteral("language"), QStringLiteral("en")).toString() == QStringLiteral("zh");
#ifdef Q_OS_MACOS
    pasteShortcut_ = PasteShortcut::MetaV;
#elif defined(Q_OS_LINUX)
    pasteShortcut_ = PasteShortcut::CtrlShiftV;
#endif
    method_ = settings_.value(QStringLiteral("inputMethod"), QStringLiteral("clipboard")).toString() == QStringLiteral("keyboard")
        ? InputMethod::Keyboard : InputMethod::Clipboard;
    pasteShortcut_ = PasteShortcut(qBound(0, settings_.value(QStringLiteral("pasteShortcut"), int(pasteShortcut_)).toInt(), 4));
    customShortcut_ = QKeySequence::fromString(settings_.value(QStringLiteral("customPasteShortcut")).toString(), QKeySequence::PortableText);
    if (pasteShortcut_ == PasteShortcut::Custom && customShortcut_.isEmpty()) pasteShortcut_ = PasteShortcut::CtrlV;
    timing_.focusDelayMs = settings_.value(QStringLiteral("focusDelayMs"), 100).toInt();
    timing_.focusDelayMs = qBound(20, timing_.focusDelayMs, 2000);
    target_.setInputMethod(method_); target_.setPasteShortcut(pasteShortcut_);
    target_.setCustomPasteShortcut(customShortcut_); target_.setTiming(timing_);

    resize(1050, 700); setMinimumSize(780, 420);
    auto *root = new QWidget(this);
    auto *layout = new QVBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(0);
    auto *top = new QWidget(root); top->setObjectName(QStringLiteral("top"));
    auto *topLayout = new QHBoxLayout(top);
    topLayout->setContentsMargins(8, 0, 10, 0); topLayout->setSpacing(8);
    auto *menuBar = new QMenuBar(top); menuBar->setNativeMenuBar(false);
    fileMenu_ = menuBar->addMenu(QString()); editMenu_ = menuBar->addMenu(QString());
    settingsMenu_ = menuBar->addMenu(QString());
    auto add = [](QMenu *menu, const QKeySequence &shortcut, auto slot) {
        QAction *action = menu->addAction(QString());
        action->setShortcut(shortcut); QObject::connect(action, &QAction::triggered, slot);
        return action;
    };
    fileActions_ << add(fileMenu_, QKeySequence::New, [this] { newFile(); })
                 << add(fileMenu_, QKeySequence::Open, [this] { openFile(); })
                 << add(fileMenu_, QKeySequence::Save, [this] { saveFile(false); })
                 << add(fileMenu_, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S), [this] { saveFile(true); });
    fileMenu_->addSeparator();
    fileActions_ << add(fileMenu_, {}, [this] { close(); });
    editActions_ << add(editMenu_, QKeySequence::Undo, [this] { editor_->undo(); })
                 << add(editMenu_, QKeySequence::Cut, [this] { editor_->cut(); })
                 << add(editMenu_, QKeySequence::Copy, [this] { editor_->copy(); })
                 << add(editMenu_, QKeySequence::Paste, [this] { editor_->paste(); })
                 << add(editMenu_, QKeySequence::SelectAll, [this] { editor_->selectAll(); })
                 << add(editMenu_, QKeySequence::Find, [this] { findText(); });
    languageMenu_ = settingsMenu_->addMenu(QString());
    auto *languageGroup = new QActionGroup(this); languageGroup->setExclusive(true);
    englishAction_ = languageMenu_->addAction(QString()); englishAction_->setCheckable(true); languageGroup->addAction(englishAction_);
    chineseAction_ = languageMenu_->addAction(QString()); chineseAction_->setCheckable(true); languageGroup->addAction(chineseAction_);
    connect(englishAction_, &QAction::triggered, this, [this] { setLanguage(false); });
    connect(chineseAction_, &QAction::triggered, this, [this] { setLanguage(true); });
    methodMenu_ = settingsMenu_->addMenu(QString());
    auto *methodGroup = new QActionGroup(this); methodGroup->setExclusive(true);
    clipboardAction_ = methodMenu_->addAction(QString()); clipboardAction_->setCheckable(true); methodGroup->addAction(clipboardAction_);
    keyboardAction_ = methodMenu_->addAction(QString()); keyboardAction_->setCheckable(true); methodGroup->addAction(keyboardAction_);
    connect(clipboardAction_, &QAction::triggered, this, [this] {
        method_ = InputMethod::Clipboard; target_.setInputMethod(method_); settings_.setValue(QStringLiteral("inputMethod"), QStringLiteral("clipboard"));
    });
    connect(keyboardAction_, &QAction::triggered, this, [this] {
        method_ = InputMethod::Keyboard; target_.setInputMethod(method_); settings_.setValue(QStringLiteral("inputMethod"), QStringLiteral("keyboard"));
    });
    pasteMenu_ = settingsMenu_->addMenu(QString());
    auto *pasteGroup = new QActionGroup(this); pasteGroup->setExclusive(true);
    for (const auto &name : {"Ctrl+V", "Ctrl+Shift+V", "Shift+Insert", "Cmd+V"}) {
        auto *action = pasteMenu_->addAction(QString::fromLatin1(name));
        action->setCheckable(true); pasteGroup->addAction(action); pasteActions_ << action;
        connect(action, &QAction::triggered, this, [this, action] {
            pasteShortcut_ = PasteShortcut(pasteActions_.indexOf(action));
            target_.setPasteShortcut(pasteShortcut_); settings_.setValue(QStringLiteral("pasteShortcut"), int(pasteShortcut_));
        });
    }
    auto *customPaste = pasteMenu_->addAction(QString());
    customPaste->setCheckable(true); pasteGroup->addAction(customPaste); pasteActions_ << customPaste;
    connect(customPaste, &QAction::triggered, this, [this] {
        bool ok = false;
        const QString entered = QInputDialog::getText(this, t("Custom paste shortcut", "自定义粘贴快捷键"),
            t("Shortcut (for example Ctrl+Shift+Insert):", "快捷键（例如 Ctrl+Shift+Insert）："), QLineEdit::Normal,
            customShortcut_.toString(QKeySequence::PortableText), &ok);
        if (!ok) { retranslate(); return; }
        const QKeySequence sequence = QKeySequence::fromString(entered, QKeySequence::PortableText);
        if (sequence.count() != 1 || sequence[0].key() == Qt::Key_unknown ||
            sequence[0].keyboardModifiers() == Qt::NoModifier) {
            error(t("Enter one shortcut with at least one modifier.", "请输入带至少一个修饰键的单组快捷键。"));
            retranslate(); return;
        }
        customShortcut_ = sequence;
        pasteShortcut_ = PasteShortcut::Custom;
        target_.setCustomPasteShortcut(sequence); target_.setPasteShortcut(pasteShortcut_);
        settings_.setValue(QStringLiteral("customPasteShortcut"), sequence.toString(QKeySequence::PortableText));
        settings_.setValue(QStringLiteral("pasteShortcut"), int(pasteShortcut_));
        retranslate();
    });
    settingsMenu_->addSeparator();
    focusDelayAction_ = settingsMenu_->addAction(QString());
    connect(focusDelayAction_, &QAction::triggered, this, [this] {
        bool ok = false;
        int value = QInputDialog::getInt(this, t("Focus delay", "焦点等待"), t("Milliseconds:", "毫秒："),
                                         timing_.focusDelayMs, 20, 2000, 10, &ok);
        if (ok) { timing_.focusDelayMs = value; target_.setTiming(timing_); settings_.setValue(QStringLiteral("focusDelayMs"), value); }
    });
    topLayout->addWidget(menuBar);
    targetLabel_ = new QLabel(top); targetLabel_->setMinimumWidth(100);
    targetLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    targetLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    targetLabel_->installEventFilter(this);
    bindLabel_ = new QLabel(top);
    picker_ = new TargetPicker(top); picker_->setFixedSize(34, 30);
    picker_->installEventFilter(this);
    topLayout->addWidget(targetLabel_, 1); topLayout->addWidget(bindLabel_); topLayout->addWidget(picker_);
    layout->addWidget(top);

    editor_ = new CommandEditor(root); layout->addWidget(editor_, 1);
    auto *footer = new QWidget(root); footer->setObjectName(QStringLiteral("footer"));
    auto *footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(12, 6, 12, 6); footerLayout->setSpacing(7);
    statusLabel_ = new QLabel(footer); statusLabel_->setMinimumWidth(80);
    intervalLabel_ = new QLabel(footer); secondLabel_ = new QLabel(footer);
    interval_ = new QDoubleSpinBox(footer); interval_->setRange(0.1, 60); interval_->setDecimals(1);
    interval_->setSingleStep(0.1); interval_->setValue(1.0); interval_->setFixedWidth(70);
    autoCheck_ = new QCheckBox(footer);
    sendButton_ = new SendButton(footer); sendButton_->setFixedSize(48, 38);
    footerLayout->addWidget(statusLabel_, 1); footerLayout->addWidget(intervalLabel_);
    footerLayout->addWidget(interval_); footerLayout->addWidget(secondLabel_);
    footerLayout->addSpacing(14); footerLayout->addWidget(autoCheck_); footerLayout->addWidget(sendButton_);
    layout->addWidget(footer);
    setCentralWidget(root);
    setStyleSheet(QStringLiteral(
        "#top,#footer{background:#f8fafd;} #top{border-bottom:1px solid #e6ebf1;}"
        "#footer{border-top:1px solid #e1e7ef;} QMenuBar{background:transparent;}"
        "QPlainTextEdit{background:white;color:#263340;} QLabel{color:#53677f;}"));

    connect(editor_, &QPlainTextEdit::modificationChanged, this, &MainWindow::updateTitle);
    connect(editor_, &QPlainTextEdit::cursorPositionChanged, this, [this] {
        if (!busy_ && !autoEngine_.running()) statusLabel_->setText(t("Line ", "第 ") + QString::number(editor_->currentLine() + 1) + t("", " 行"));
    });
    connect(autoCheck_, &QCheckBox::toggled, this, &MainWindow::updateSendButton);
    connect(sendButton_, &QToolButton::clicked, this, &MainWindow::sendRequested);
    connect(&autoEngine_, &AutoSendEngine::lineSent, this, [this](int line) {
        editor_->goToLine(line + 1); statusLabel_->setText(t("Sent line ", "已发送第") + QString::number(line + 1) + t("", "行"));
    });
    connect(&autoEngine_, &AutoSendEngine::finished, this, &MainWindow::onAutoFinished);
    auto *f8 = new QShortcut(QKeySequence(Qt::Key_F8), this);
    connect(f8, &QShortcut::activated, this, &MainWindow::sendRequested);
    retranslate();
    if (!NativeWindow::supported()) statusLabel_->setText(t(
        "External window control is unavailable on this desktop.", "当前桌面环境不支持外部窗口控制。"));
    editor_->setFocus();
}

QString MainWindow::t(const char *english, const char *chinese) const {
    return QString::fromUtf8(chinese_ ? chinese : english);
}
void MainWindow::retranslate() {
    fileMenu_->setTitle(t("File", "文件")); editMenu_->setTitle(t("Edit", "编辑"));
    settingsMenu_->setTitle(t("Settings", "设置")); languageMenu_->setTitle(t("Language", "语言"));
    methodMenu_->setTitle(t("Input method", "输入方式")); pasteMenu_->setTitle(t("Terminal paste shortcut", "终端粘贴快捷键"));
    const QStringList file{t("New", "新建"), t("Open...", "打开..."), t("Save", "保存"),
                           t("Save As...", "另存为..."), t("Exit", "退出")};
    for (int i = 0; i < file.size(); ++i) fileActions_[i]->setText(file[i]);
    const QStringList edit{t("Undo", "撤销"), t("Cut", "剪切"), t("Copy", "复制"),
                           t("Paste", "粘贴"), t("Select All", "全选"), t("Find...", "查找...")};
    for (int i = 0; i < edit.size(); ++i) editActions_[i]->setText(edit[i]);
    englishAction_->setText(QStringLiteral("English")); chineseAction_->setText(QStringLiteral("中文"));
    englishAction_->setChecked(!chinese_); chineseAction_->setChecked(chinese_);
    clipboardAction_->setText(t("Clipboard paste", "剪贴板粘贴")); keyboardAction_->setText(t("Unicode keyboard", "Unicode 键盘输入"));
    clipboardAction_->setChecked(method_ == InputMethod::Clipboard); keyboardAction_->setChecked(method_ == InputMethod::Keyboard);
    pasteActions_[2]->setText(t("Shift+Insert (MobaXterm)", "Shift+Insert（MobaXterm 推荐）"));
    pasteActions_[4]->setText(t("Custom...", "自定义...") + (customShortcut_.isEmpty()
        ? QString() : QStringLiteral(" (") + customShortcut_.toString(QKeySequence::NativeText) + QLatin1Char(')')));
    if (int(pasteShortcut_) >= 0 && int(pasteShortcut_) < pasteActions_.size()) pasteActions_[int(pasteShortcut_)]->setChecked(true);
    focusDelayAction_->setText(t("Focus delay...", "焦点等待..."));
    bindLabel_->setText(t("Bind terminal", "绑定终端"));
    picker_->setToolTip(t("Hold, drag to a terminal window, then release", "按住拖到终端窗口，松开绑定"));
    intervalLabel_->setText(t("Interval", "间隔")); secondLabel_->setText(t("s", "秒"));
    autoCheck_->setText(t("Auto send", "自动发送"));
    editor_->setRangeLabels(t("Set Start (green)", "设为起点（绿点）"),
                            t("Set End (red)", "设为终点（红点）"), t("Clear Start and End", "清除起点和终点"));
    updateTitle(); updateTargetLabel(); updateSendButton();
    statusLabel_->setText(t("Line ", "第 ") + QString::number(editor_->currentLine() + 1) + t("", " 行"));
}
void MainWindow::setLanguage(bool chinese) {
    if (autoEngine_.running()) autoEngine_.stop();
    chinese_ = chinese;
    settings_.setValue(QStringLiteral("language"), chinese ? QStringLiteral("zh") : QStringLiteral("en"));
    retranslate();
}
void MainWindow::updateTitle() {
    setWindowTitle(QStringLiteral("SendToCmd — ") + (editor_->document()->isModified() ? QStringLiteral("*") : QString()) +
                   (currentPath_.isEmpty() ? t("Untitled", "未命名") : QFileInfo(currentPath_).fileName()));
}
void MainWindow::updateTargetLabel() {
    QString full = target_.window().valid() ? target_.window().title : t("Not bound", "未绑定");
    const QString prefix = t("Target: ", "目标：");
    const auto metrics = targetLabel_->fontMetrics();
    targetLabel_->setText(prefix + metrics.elidedText(full, Qt::ElideRight,
        qMax(0, targetLabel_->width() - metrics.horizontalAdvance(prefix) - 4)));
    targetLabel_->setToolTip(full);
}
void MainWindow::updateSendButton() {
    auto *button = static_cast<SendButton *>(sendButton_);
    button->stopIcon = autoEngine_.running(); button->update();
    sendButton_->setToolTip(autoEngine_.running() ? t("Stop auto send (F8)", "停止自动发送 (F8)")
        : autoCheck_->isChecked() ? t("Auto send range (F8)", "自动发送范围 (F8)")
                                  : t("Send current line (F8)", "发送当前行 (F8)"));
    sendButton_->setAccessibleName(sendButton_->toolTip());
}
void MainWindow::setBusy(bool busy) {
    busy_ = busy;
    editor_->setReadOnly(busy);
    picker_->setEnabled(!busy);
    autoCheck_->setEnabled(!busy);
    interval_->setEnabled(!busy);
    sendButton_->setEnabled(!busy || autoEngine_.running());
    methodMenu_->setEnabled(!busy); pasteMenu_->setEnabled(!busy);
    focusDelayAction_->setEnabled(!busy);
    fileMenu_->setEnabled(!busy || autoEngine_.running());
    editMenu_->setEnabled(!busy || autoEngine_.running());
    languageMenu_->setEnabled(!busy || autoEngine_.running());
    updateSendButton();
}
void MainWindow::error(const QString &message) {
    QString display = message;
    if (chinese_) {
        const auto translated = [this, &message](const char *english, const char *zh) {
            return message == QString::fromUtf8(english) ? t(english, zh) : QString();
        };
        for (const auto &pair : {
            std::pair{"Line exceeds 4096 UTF-16 code units.", "单行超过 4096 个 UTF-16 字符单元。"},
            std::pair{"Line contains a tab or control character.", "单行包含制表符或控制字符。"},
            std::pair{"Bind a target and set a valid start and end line.", "请绑定目标并设置有效的起点和终点。"},
            std::pair{"The target window is unavailable. Bind it again.", "目标窗口不可用，请重新绑定。"},
            std::pair{"Could not activate the target window.", "无法激活目标窗口。"},
            std::pair{"Target did not gain focus; sending was canceled.", "目标窗口未获得焦点，已取消发送。"},
            std::pair{"Could not send Enter.", "无法发送回车。"},
            std::pair{"Enter was sent, but target focus changed; check before retrying.", "回车已发送，但目标焦点发生变化；重试前请检查。"},
            std::pair{"Unicode keyboard input failed; check the target before retrying.", "Unicode 键盘输入失败；重试前请检查目标窗口。"},
            std::pair{"Target lost focus after text input; Enter was not sent.", "文本输入后目标失焦；未发送回车。"},
            std::pair{"Text may have arrived, but Enter failed; check the target.", "文本可能已到达，但回车发送失败；请检查目标窗口。"},
            std::pair{"Target focus or temporary clipboard changed; sending stopped.", "目标焦点或临时剪贴板发生变化，发送已停止。"},
            std::pair{"Could not write text to the Windows clipboard.", "无法将文本写入 Windows 剪贴板。"},
            std::pair{"Paste shortcut failed; check the target before retrying.", "粘贴快捷键发送失败；重试前请检查目标窗口。"},
            std::pair{"Target lost focus after paste; Enter was not sent.", "粘贴后目标失焦；未发送回车。"},
            std::pair{"Paste may have arrived, but Enter failed; check the target.", "内容可能已粘贴，但回车发送失败；请检查目标窗口。"},
            std::pair{"Input was sent, but target focus changed; check before retrying.", "输入已发送，但目标焦点发生变化；重试前请检查。"},
            std::pair{"A send is already in progress.", "已有发送操作正在进行。"},
            std::pair{"Target lost focus or closed; auto send stopped.", "目标窗口失焦或关闭，自动发送已停止。"}
        }) {
            QString value = translated(pair.first, pair.second);
            if (!value.isEmpty()) { display = value; break; }
        }
        if (message.startsWith(QStringLiteral("Line ")) && message.contains(QLatin1Char(':'))) {
            const auto split = message.indexOf(QLatin1Char(':'));
            const QString number = message.mid(5, split - 5);
            const QString reason = message.mid(split + 2);
            display = QStringLiteral("第%1行：%2").arg(number,
                reason == QStringLiteral("Line exceeds 4096 UTF-16 code units.")
                    ? QStringLiteral("超过 4096 个 UTF-16 字符单元。")
                    : QStringLiteral("包含制表符或控制字符。"));
        }
    }
    QMessageBox::warning(this, QStringLiteral("SendToCmd"), display);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (watched == targetLabel_ && event->type() == QEvent::Resize) updateTargetLabel();
    if (watched != picker_) return QMainWindow::eventFilter(watched, event);
    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton) { picking_ = true; picker_->grabMouse(); return true; }
    }
    if (event->type() == QEvent::MouseButtonRelease && picking_) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton) {
            const QPoint point = QCursor::pos();
            picking_ = false; picker_->releaseMouse();
            WindowIdentity selected = NativeWindow::at(point);
            if (!selected.valid()) { error(NativeWindow::supported() ? t("No usable target window was selected.", "未选中可用的目标窗口。") :
                t("External window control requires X11 or macOS Accessibility permission.", "外部窗口控制需要 X11 会话或 macOS 辅助功能权限。")); return true; }
#ifdef Q_OS_WIN
            if (!settings_.contains(QStringLiteral("pasteShortcut"))) {
                const bool mobaXterm = selected.executableName.startsWith(QStringLiteral("MobaXterm"), Qt::CaseInsensitive) ||
                    selected.title.contains(QStringLiteral("MobaXterm"), Qt::CaseInsensitive);
                pasteShortcut_ = mobaXterm ? PasteShortcut::ShiftInsert : PasteShortcut::CtrlV;
                target_.setPasteShortcut(pasteShortcut_);
                pasteActions_[int(pasteShortcut_)]->setChecked(true);
            }
#endif
            target_.bind(std::move(selected)); updateTargetLabel();
            statusLabel_->setText(t("Bound: ", "已绑定：") + target_.window().title);
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

QStringList MainWindow::documentLines() const {
    QStringList lines;
    for (QTextBlock block = editor_->document()->begin(); block.isValid(); block = block.next()) lines << block.text();
    return lines;
}
void MainWindow::sendRequested() {
    if (autoEngine_.running()) { autoEngine_.stop(); return; }
    if (busy_) return;
    if (!autoCheck_->isChecked()) { sendCurrentLine(); return; }
    const int first = editor_->startLine(), last = editor_->endLine();
    if (first < 0 || last < first) { error(t("Set a valid start and end line from the line-number menu.", "请在行号菜单设置有效的起点和终点。")); return; }
    autoEngine_.start(documentLines(), first, last, int(interval_->value() * 1000), &target_);
    if (autoEngine_.running()) setBusy(true);
}
void MainWindow::sendCurrentLine() {
    const int line = editor_->currentLine();
    const QString command = editor_->textCursor().block().text();
    auto validation = SendValidator::validateLine(command);
    if (!validation.ok) { error(validation.error); return; }
    if (!target_.isAvailable()) { error(t("Bind a usable target window first.", "请先绑定可用的目标窗口。")); return; }
    setBusy(true);
    const quint64 generation = ++manualGeneration_;
    target_.sendLine(command, [this, line, generation](SendResult result) {
        if (generation != manualGeneration_) return;
        setBusy(false);
        if (!result.success) { error(result.error); return; }
        if (line + 1 < editor_->document()->blockCount()) editor_->goToLine(line + 1);
        statusLabel_->setText(t("Sent line ", "已发送第") + QString::number(line + 1) + t("", "行"));
    });
}
void MainWindow::onAutoFinished(bool completed, const QString &message) {
    setBusy(false);
    if (!message.isEmpty()) error(message);
    else if (!completed) statusLabel_->setText(t("Auto send stopped", "已停止自动发送"));
    updateSendButton();
}

bool MainWindow::confirmDiscard() {
    if (!editor_->document()->isModified()) return true;
    auto answer = QMessageBox::question(this, QStringLiteral("SendToCmd"),
        t("Save changes to the current file?", "是否保存当前文件？"),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    if (answer == QMessageBox::Cancel) return false;
    return answer == QMessageBox::No || saveFile(false);
}
void MainWindow::newFile() {
    if (busy_ && !autoEngine_.running()) return;
    if (autoEngine_.running()) autoEngine_.stop();
    if (!confirmDiscard()) return;
    editor_->setPlainText({}); editor_->clearRange(); editor_->document()->setModified(false);
    currentPath_.clear(); updateTitle();
}
void MainWindow::openFile() {
    if (busy_ && !autoEngine_.running()) return;
    if (autoEngine_.running()) autoEngine_.stop();
    if (!confirmDiscard()) return;
    const QString path = QFileDialog::getOpenFileName(this, t("Open", "打开"), {}, t("Text files (*.txt);;All files (*)", "文本文件 (*.txt);;所有文件 (*)"));
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { error(file.errorString()); return; }
    QByteArray bytes = file.readAll();
    if (bytes.startsWith("\xEF\xBB\xBF")) bytes.remove(0, 3);
    QStringDecoder decoder(QStringDecoder::Utf8);
    QString content = decoder.decode(bytes);
    if (decoder.hasError()) { error(t("The file is not valid UTF-8.", "文件不是有效的 UTF-8 编码。")); return; }
    editor_->setPlainText(content); editor_->clearRange(); editor_->document()->setModified(false);
    currentPath_ = path; editor_->goToLine(0); updateTitle();
}
bool MainWindow::saveFile(bool saveAs) {
    QString path = currentPath_;
    if (path.isEmpty() || saveAs) {
        path = QFileDialog::getSaveFileName(this, t("Save As", "另存为"), path,
            t("Text files (*.txt);;All files (*)", "文本文件 (*.txt);;所有文件 (*)"));
        if (path.isEmpty()) return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) { error(file.errorString()); return false; }
    const QByteArray bytes = editor_->toPlainText().toUtf8();
    if (file.write(bytes) != bytes.size() || !file.commit()) { error(file.errorString()); return false; }
    currentPath_ = path; editor_->document()->setModified(false); updateTitle();
    statusLabel_->setText(t("Saved: ", "已保存：") + path);
    return true;
}
void MainWindow::findText() {
    if (busy_ && !autoEngine_.running()) return;
    if (autoEngine_.running()) autoEngine_.stop();
    QDialog dialog(this);
    dialog.setWindowTitle(t("Find", "查找"));
    dialog.setFixedSize(390, 100);
    auto *layout = new QVBoxLayout(&dialog);
    auto *query = new QLineEdit(&dialog);
    query->setPlaceholderText(t("Text to find", "查找内容"));
    auto *next = new QPushButton(t("Find Next", "查找下一个"), &dialog);
    layout->addWidget(query);
    layout->addWidget(next, 0, Qt::AlignRight);
    connect(next, &QPushButton::clicked, &dialog, [this, query, &dialog] {
        if (query->text().isEmpty()) return;
        if (!editor_->find(query->text())) {
            editor_->moveCursor(QTextCursor::Start);
            if (!editor_->find(query->text())) QMessageBox::information(&dialog, t("Find", "查找"), t("No match found.", "未找到匹配内容。"));
        }
    });
    connect(query, &QLineEdit::returnPressed, next, &QPushButton::click);
    dialog.exec();
}
void MainWindow::closeEvent(QCloseEvent *event) {
    if (autoEngine_.running()) autoEngine_.stop();
    if (!confirmDiscard()) { event->ignore(); return; }
    ++manualGeneration_; target_.cancel();
    event->accept();
}

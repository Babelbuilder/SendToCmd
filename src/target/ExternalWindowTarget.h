#pragma once
#include "ICommandTarget.h"
#include "../platform/NativeWindow.h"
#include <QMimeData>
#include <QObject>
#include <memory>

enum class InputMethod { Clipboard, Keyboard };

struct InputTiming {
    int focusDelayMs = 100;
    int clipboardReadyMs = 30;
    int enterDelayMs = 100;
    int restoreClipboardDelayMs = 200;
};

class ExternalWindowTarget : public QObject, public ICommandTarget {
    Q_OBJECT
public:
    explicit ExternalWindowTarget(QObject *parent = nullptr);
    ~ExternalWindowTarget() override;
    void bind(WindowIdentity window);
    const WindowIdentity &window() const { return window_; }
    void setInputMethod(InputMethod method) { method_ = method; }
    void setPasteShortcut(PasteShortcut shortcut) { shortcut_ = shortcut; }
    void setCustomPasteShortcut(QKeySequence shortcut) { customShortcut_ = std::move(shortcut); }
    void setTiming(InputTiming timing) { timing_ = timing; }
    bool isAvailable() const override;
    bool canContinue() const override;
    void sendLine(const QString &line, Completion completion) override;
    void cancel() override;
private:
    void finish(SendResult result);
    void restoreClipboard();
    bool ownsTemporaryClipboard() const;
    static std::unique_ptr<QMimeData> copyMime(const QMimeData *source);
    WindowIdentity window_;
    InputMethod method_ = InputMethod::Clipboard;
    PasteShortcut shortcut_ = PasteShortcut::CtrlV;
    QKeySequence customShortcut_;
    InputTiming timing_;
    Completion completion_;
    std::unique_ptr<QMimeData> savedClipboard_;
    QByteArray clipboardToken_;
#ifdef Q_OS_WIN
    quint32 clipboardSequence_ = 0;
#endif
    quint64 generation_ = 0;
    bool busy_ = false;
    bool pasteIssued_ = false;
    bool pendingRestore_ = false;
};

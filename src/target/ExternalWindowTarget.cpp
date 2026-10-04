#include "ExternalWindowTarget.h"
#include <QApplication>
#include <QClipboard>
#include <QGuiApplication>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <QWidget>
#include <cstring>
#endif

namespace { constexpr auto TokenFormat = "application/x-sendtocmd-clipboard-token"; }

#ifdef Q_OS_WIN
namespace {
bool setNativeClipboardText(QWidget *owner, const QString &text, quint32 &sequence) {
    if (!owner) return false;
    const SIZE_T bytes = SIZE_T(text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!memory) return false;
    auto *buffer = static_cast<wchar_t *>(GlobalLock(memory));
    if (!buffer) { GlobalFree(memory); return false; }
    std::memcpy(buffer, text.utf16(), SIZE_T(text.size()) * sizeof(wchar_t));
    buffer[text.size()] = L'\0';
    GlobalUnlock(memory);

    // A non-null owner is required for SetClipboardData after EmptyClipboard.
    if (!OpenClipboard(reinterpret_cast<HWND>(owner->winId()))) {
        GlobalFree(memory);
        return false;
    }
    const bool written = EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, memory);
    CloseClipboard();
    if (!written) { GlobalFree(memory); return false; }
    sequence = GetClipboardSequenceNumber();
    return sequence != 0;
}
}
#endif

ExternalWindowTarget::ExternalWindowTarget(QObject *parent) : QObject(parent) {}
ExternalWindowTarget::~ExternalWindowTarget() { restoreClipboard(); }

void ExternalWindowTarget::bind(WindowIdentity window) {
    cancel();
    window_ = std::move(window);
}

bool ExternalWindowTarget::isAvailable() const {
    return NativeWindow::supported() && window_.valid() && NativeWindow::exists(window_);
}
bool ExternalWindowTarget::canContinue() const {
    return isAvailable() && NativeWindow::isForeground(window_);
}

std::unique_ptr<QMimeData> ExternalWindowTarget::copyMime(const QMimeData *source) {
    auto copy = std::make_unique<QMimeData>();
    if (!source) return copy;
    for (const auto &format : source->formats()) copy->setData(format, source->data(format));
    if (source->hasText() && !copy->hasText()) copy->setText(source->text());
    if (source->hasHtml() && !copy->hasHtml()) copy->setHtml(source->html());
    if (source->hasUrls() && !copy->hasUrls()) copy->setUrls(source->urls());
    if (source->hasImage() && !copy->hasImage()) copy->setImageData(source->imageData());
    return copy;
}

bool ExternalWindowTarget::ownsTemporaryClipboard() const {
#ifdef Q_OS_WIN
    return clipboardSequence_ != 0 && GetClipboardSequenceNumber() == clipboardSequence_;
#else
    const auto *data = QGuiApplication::clipboard()->mimeData();
    return data && data->data(TokenFormat) == clipboardToken_ && !clipboardToken_.isEmpty();
#endif
}

void ExternalWindowTarget::restoreClipboard() {
    if (savedClipboard_ && ownsTemporaryClipboard())
        QGuiApplication::clipboard()->setMimeData(savedClipboard_.release());
    savedClipboard_.reset();
    clipboardToken_.clear();
#ifdef Q_OS_WIN
    clipboardSequence_ = 0;
#endif
}

void ExternalWindowTarget::finish(SendResult result) {
    restoreClipboard();
    busy_ = false;
    pasteIssued_ = false;
    pendingRestore_ = false;
    auto completion = std::move(completion_);
    completion_ = {};
    if (completion) completion(std::move(result));
}

void ExternalWindowTarget::cancel() {
    ++generation_;
    completion_ = {};
    if (pasteIssued_ && savedClipboard_) {
        if (!pendingRestore_) {
            pendingRestore_ = true;
            QTimer::singleShot(timing_.restoreClipboardDelayMs, this, [this] {
                restoreClipboard();
                pasteIssued_ = false;
                pendingRestore_ = false;
                busy_ = false;
            });
        }
    } else {
        restoreClipboard();
        pasteIssued_ = false;
        busy_ = false;
    }
}

void ExternalWindowTarget::sendLine(const QString &line, Completion completion) {
    if (busy_) { completion(SendResult::fail(QStringLiteral("A send is already in progress."))); return; }
    if (!isAvailable()) { completion(SendResult::fail(QStringLiteral("The target window is unavailable. Bind it again."))); return; }
    if (!NativeWindow::activate(window_)) { completion(SendResult::fail(QStringLiteral("Could not activate the target window."))); return; }
    busy_ = true;
    completion_ = std::move(completion);
    const auto generation = ++generation_;
    QTimer::singleShot(timing_.focusDelayMs, this, [this, generation, line] {
        if (generation != generation_) return;
        if (!isAvailable() || !NativeWindow::isForeground(window_)) {
            finish(SendResult::fail(QStringLiteral("Target did not gain focus; sending was canceled.")));
            return;
        }
        if (line.isEmpty()) {
            if (!NativeWindow::enter()) finish(SendResult::fail(QStringLiteral("Could not send Enter.")));
            else finish(isAvailable() && NativeWindow::isForeground(window_) ? SendResult::ok()
                : SendResult::fail(QStringLiteral("Enter was sent, but target focus changed; check before retrying.")));
            return;
        }
        if (method_ == InputMethod::Keyboard) {
            if (!NativeWindow::sendUnicode(line)) {
                finish(SendResult::fail(QStringLiteral("Unicode keyboard input failed; check the target before retrying.")));
                return;
            }
            if (!isAvailable() || !NativeWindow::isForeground(window_)) {
                finish(SendResult::fail(QStringLiteral("Target lost focus after text input; Enter was not sent.")));
                return;
            }
            if (!NativeWindow::enter()) finish(SendResult::fail(QStringLiteral("Text may have arrived, but Enter failed; check the target.")));
            else finish(isAvailable() && NativeWindow::isForeground(window_) ? SendResult::ok()
                : SendResult::fail(QStringLiteral("Input was sent, but target focus changed; check before retrying.")));
            return;
        }
        auto *clipboard = QGuiApplication::clipboard();
        savedClipboard_ = copyMime(clipboard->mimeData());
#ifdef Q_OS_WIN
        if (!setNativeClipboardText(qobject_cast<QWidget *>(parent()), line, clipboardSequence_)) {
            finish(SendResult::fail(QStringLiteral("Could not write text to the Windows clipboard.")));
            return;
        }
#else
        clipboardToken_ = QUuid::createUuid().toByteArray();
        auto data = std::make_unique<QMimeData>();
        data->setText(line);
        data->setData(TokenFormat, clipboardToken_);
        clipboard->setMimeData(data.release());
#endif
        QTimer::singleShot(timing_.clipboardReadyMs, this, [this, generation] {
            if (generation != generation_) return;
            if (!isAvailable() || !NativeWindow::isForeground(window_) || !ownsTemporaryClipboard()) {
                finish(SendResult::fail(QStringLiteral("Target focus or temporary clipboard changed; sending stopped.")));
                return;
            }
            if (!NativeWindow::paste(shortcut_, customShortcut_)) {
                finish(SendResult::fail(QStringLiteral("Paste shortcut failed; check the target before retrying.")));
                return;
            }
            pasteIssued_ = true;
            QTimer::singleShot(timing_.enterDelayMs, this, [this, generation] {
                if (generation != generation_) return;
                if (!isAvailable() || !NativeWindow::isForeground(window_)) {
                    finish(SendResult::fail(QStringLiteral("Target lost focus after paste; Enter was not sent.")));
                    return;
                }
                if (!NativeWindow::enter()) {
                    finish(SendResult::fail(QStringLiteral("Paste may have arrived, but Enter failed; check the target.")));
                    return;
                }
                if (!isAvailable() || !NativeWindow::isForeground(window_)) {
                    finish(SendResult::fail(QStringLiteral("Input was sent, but target focus changed; check before retrying.")));
                    return;
                }
                QTimer::singleShot(timing_.restoreClipboardDelayMs, this, [this, generation] {
                    if (generation == generation_) finish(SendResult::ok());
                });
            });
        });
    });
}

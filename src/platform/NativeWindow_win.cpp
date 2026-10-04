#include "NativeWindow.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <QCoreApplication>
#include <array>

namespace {
HWND handle(const WindowIdentity &w) { return reinterpret_cast<HWND>(w.id); }
DWORD keyboardFlags(WORD code, DWORD flags) {
    // Insert in the navigation cluster carries an E0 prefix. Without it,
    // terminal shortcut handlers may see the numpad Insert instead.
    if (code == VK_INSERT || code == VK_DELETE || code == VK_HOME || code == VK_END ||
        code == VK_PRIOR || code == VK_NEXT || code == VK_LEFT || code == VK_RIGHT ||
        code == VK_UP || code == VK_DOWN || code == VK_LWIN || code == VK_RWIN)
        flags |= KEYEVENTF_EXTENDEDKEY;
    return flags;
}
bool key(WORD code, DWORD flags = 0) {
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = code;
    input.ki.dwFlags = keyboardFlags(code, flags);
    return SendInput(1, &input, sizeof(input)) == 1;
}
bool chord(WORD first, WORD second, WORD code) {
    std::array<INPUT, 6> input{};
    int count = 0;
    auto append = [&](WORD virtualKey, DWORD flags) {
        auto &event = input[size_t(count++)];
        event.type = INPUT_KEYBOARD;
        event.ki.wVk = virtualKey;
        event.ki.dwFlags = keyboardFlags(virtualKey, flags);
    };
    append(first, 0);
    if (second) append(second, 0);
    append(code, 0);
    append(code, KEYEVENTF_KEYUP);
    if (second) append(second, KEYEVENTF_KEYUP);
    append(first, KEYEVENTF_KEYUP);
    if (SendInput(UINT(count), input.data(), sizeof(INPUT)) == UINT(count)) return true;
    // A partial SendInput must not leave a shortcut modifier held down.
    key(code, KEYEVENTF_KEYUP);
    if (second) key(second, KEYEVENTF_KEYUP);
    key(first, KEYEVENTF_KEYUP);
    return false;
}
WORD virtualKey(Qt::Key keyCode) {
    const int keyValue = int(keyCode);
    if ((keyValue >= Qt::Key_A && keyValue <= Qt::Key_Z) ||
        (keyValue >= Qt::Key_0 && keyValue <= Qt::Key_9)) return WORD(keyValue);
    if (keyCode >= Qt::Key_F1 && keyCode <= Qt::Key_F12) return WORD(VK_F1 + keyValue - Qt::Key_F1);
    switch (keyCode) {
    case Qt::Key_Insert: return VK_INSERT;
    case Qt::Key_Delete: return VK_DELETE;
    case Qt::Key_Space: return VK_SPACE;
    case Qt::Key_Return: case Qt::Key_Enter: return VK_RETURN;
    case Qt::Key_Tab: return VK_TAB;
    default: return 0;
    }
}
bool customChord(const QKeySequence &sequence) {
    if (sequence.count() != 1) return false;
    const auto combination = sequence[0];
    const WORD code = virtualKey(combination.key());
    if (!code) return false;
    const auto modifiers = combination.keyboardModifiers();
    WORD held[4]{}; int count = 0;
    if (modifiers & Qt::ControlModifier) held[count++] = VK_CONTROL;
    if (modifiers & Qt::AltModifier) held[count++] = VK_MENU;
    if (modifiers & Qt::ShiftModifier) held[count++] = VK_SHIFT;
    if (modifiers & Qt::MetaModifier) held[count++] = VK_LWIN;
    int pressed = 0;
    for (int i = 0; i < count; ++i) {
        if (!key(held[i])) break;
        ++pressed;
    }
    if (pressed != count) {
        for (int i = pressed - 1; i >= 0; --i) key(held[i], KEYEVENTF_KEYUP);
        return false;
    }
    const bool down = key(code), up = key(code, KEYEVENTF_KEYUP);
    bool released = true;
    for (int i = count - 1; i >= 0; --i) released = key(held[i], KEYEVENTF_KEYUP) && released;
    return released && down && up;
}
}

bool NativeWindow::supported() { return true; }
QString NativeWindow::supportMessage() { return {}; }
WindowIdentity NativeWindow::at(const QPoint &point) {
    Q_UNUSED(point);
    POINT p{};
    if (!GetCursorPos(&p)) return {};
    HWND window = GetAncestor(WindowFromPoint(p), GA_ROOT);
    if (!window || !IsWindowVisible(window)) return {};
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    if (!pid || pid == GetCurrentProcessId()) return {};
    const int length = GetWindowTextLengthW(window);
    if (!length) return {};
    std::wstring title(size_t(length) + 1, L'\0');
    title.resize(size_t(GetWindowTextW(window, title.data(), int(title.size()))));
    if (title.empty()) return {};
    QString executableName;
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process) {
        std::wstring path(32768, L'\0');
        DWORD pathLength = DWORD(path.size());
        if (QueryFullProcessImageNameW(process, 0, path.data(), &pathLength)) {
            path.resize(pathLength);
            const size_t slash = path.find_last_of(L"\\/");
            executableName = QString::fromStdWString(path.substr(slash == std::wstring::npos ? 0 : slash + 1));
        }
        CloseHandle(process);
    }
    return {reinterpret_cast<quintptr>(window), qint64(pid), QString::fromStdWString(title), executableName};
}
bool NativeWindow::exists(const WindowIdentity &w) {
    if (!w.valid() || !IsWindow(handle(w))) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(handle(w), &pid);
    return qint64(pid) == w.processId;
}
bool NativeWindow::activate(const WindowIdentity &w) {
    if (!exists(w)) return false;
    if (IsIconic(handle(w))) ShowWindow(handle(w), SW_RESTORE);
    SetForegroundWindow(handle(w));
    return true;
}
bool NativeWindow::isForeground(const WindowIdentity &w) {
    return exists(w) && GetAncestor(GetForegroundWindow(), GA_ROOT) == handle(w);
}
bool NativeWindow::paste(PasteShortcut shortcut, const QKeySequence &custom) {
    switch (shortcut) {
    case PasteShortcut::CtrlV: return chord(VK_CONTROL, 0, 'V');
    case PasteShortcut::CtrlShiftV: return chord(VK_CONTROL, VK_SHIFT, 'V');
    case PasteShortcut::ShiftInsert: return chord(VK_SHIFT, 0, VK_INSERT);
    case PasteShortcut::MetaV: return false;
    case PasteShortcut::Custom: return customChord(custom);
    }
    return false;
}
bool NativeWindow::sendUnicode(const QString &text) {
    for (QChar ch : text) {
        INPUT input[2]{};
        for (auto &event : input) { event.type = INPUT_KEYBOARD; event.ki.wScan = ch.unicode(); }
        input[0].ki.dwFlags = KEYEVENTF_UNICODE;
        input[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        if (SendInput(2, input, sizeof(INPUT)) != 2) return false;
    }
    return true;
}
bool NativeWindow::enter() { return key(VK_RETURN) && key(VK_RETURN, KEYEVENTF_KEYUP); }

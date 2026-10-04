#include "NativeWindow.h"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QByteArray>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <vector>

namespace {
bool x11() { return QGuiApplication::platformName() == QStringLiteral("xcb"); }
bool xError = false;
int ignoreXError(Display *, XErrorEvent *) { xError = true; return 0; }
Display *open() { return x11() ? XOpenDisplay(nullptr) : nullptr; }
Window root(Display *display) { return DefaultRootWindow(display); }
Window parent(Display *display, Window child) {
    Window r = None, p = None, *children = nullptr;
    unsigned count = 0;
    if (!XQueryTree(display, child, &r, &p, &children, &count)) return None;
    if (children) XFree(children);
    return p;
}
bool property(Display *display, Window window, const char *name, unsigned long &value) {
    Atom atom = XInternAtom(display, name, True);
    if (atom == None) return false;
    Atom type = None; int format = 0; unsigned long count = 0, remaining = 0;
    unsigned char *data = nullptr;
    const int result = XGetWindowProperty(display, window, atom, 0, 1, False, AnyPropertyType,
                                           &type, &format, &count, &remaining, &data);
    if (result != Success || !data || !count) { if (data) XFree(data); return false; }
    value = format == 32 ? *reinterpret_cast<unsigned long *>(data) : 0;
    XFree(data);
    return format == 32;
}
QString title(Display *display, Window window) {
    Atom atom = XInternAtom(display, "_NET_WM_NAME", True);
    if (atom != None) {
        Atom type = None; int format = 0; unsigned long count = 0, remaining = 0;
        unsigned char *data = nullptr;
        if (XGetWindowProperty(display, window, atom, 0, 2048, False, AnyPropertyType,
                               &type, &format, &count, &remaining, &data) == Success && data) {
            QString name = QString::fromUtf8(reinterpret_cast<char *>(data), qsizetype(count));
            XFree(data);
            if (!name.isEmpty()) return name;
        }
    }
    char *legacy = nullptr;
    if (XFetchName(display, window, &legacy) && legacy) {
        QString name = QString::fromLocal8Bit(legacy);
        XFree(legacy);
        return name;
    }
    return {};
}
bool fake(Display *display, KeySym symbol) {
    KeyCode code = XKeysymToKeycode(display, symbol);
    if (!code) return false;
    const bool a = XTestFakeKeyEvent(display, code, True, CurrentTime);
    const bool b = XTestFakeKeyEvent(display, code, False, CurrentTime);
    return a && b;
}
bool chord(Display *display, KeySym first, KeySym second, KeySym key) {
    KeyCode a = XKeysymToKeycode(display, first);
    KeyCode b = second ? XKeysymToKeycode(display, second) : 0;
    KeyCode c = XKeysymToKeycode(display, key);
    if (!a || !c || (second && !b)) return false;
    if (!XTestFakeKeyEvent(display, a, True, CurrentTime)) return false;
    if (b && !XTestFakeKeyEvent(display, b, True, CurrentTime)) {
        XTestFakeKeyEvent(display, a, False, CurrentTime);
        XFlush(display);
        return false;
    }
    const bool down = XTestFakeKeyEvent(display, c, True, CurrentTime);
    const bool up = XTestFakeKeyEvent(display, c, False, CurrentTime);
    if (b) XTestFakeKeyEvent(display, b, False, CurrentTime);
    XTestFakeKeyEvent(display, a, False, CurrentTime);
    XFlush(display);
    return down && up;
}
bool customChord(Display *display, const QKeySequence &sequence) {
    if (sequence.count() != 1) return false;
    const auto combination = sequence[0];
    const auto modifiers = combination.keyboardModifiers();
    const int keyValue = int(combination.key());
    KeySym symbol = NoSymbol;
    if (keyValue >= Qt::Key_A && keyValue <= Qt::Key_Z) symbol = KeySym(keyValue + 32);
    else if (keyValue >= Qt::Key_0 && keyValue <= Qt::Key_9) symbol = KeySym(keyValue);
    else if (keyValue >= Qt::Key_F1 && keyValue <= Qt::Key_F12) symbol = KeySym(XK_F1 + keyValue - Qt::Key_F1);
    else if (keyValue == Qt::Key_Insert) symbol = XK_Insert;
    else if (keyValue == Qt::Key_Delete) symbol = XK_Delete;
    else if (keyValue == Qt::Key_Space) symbol = XK_space;
    else if (keyValue == Qt::Key_Return || keyValue == Qt::Key_Enter) symbol = XK_Return;
    else if (keyValue == Qt::Key_Tab) symbol = XK_Tab;
    const KeyCode code = XKeysymToKeycode(display, symbol);
    if (!code) return false;
    KeyCode held[4]{}; int count = 0;
    if (modifiers & Qt::ControlModifier) held[count++] = XKeysymToKeycode(display, XK_Control_L);
    if (modifiers & Qt::AltModifier) held[count++] = XKeysymToKeycode(display, XK_Alt_L);
    if (modifiers & Qt::ShiftModifier) held[count++] = XKeysymToKeycode(display, XK_Shift_L);
    if (modifiers & Qt::MetaModifier) held[count++] = XKeysymToKeycode(display, XK_Super_L);
    for (int i = 0; i < count; ++i) if (!held[i]) return false;
    int pressed = 0;
    for (int i = 0; i < count; ++i) {
        if (!XTestFakeKeyEvent(display, held[i], True, CurrentTime)) break;
        ++pressed;
    }
    if (pressed != count) {
        for (int i = pressed - 1; i >= 0; --i) XTestFakeKeyEvent(display, held[i], False, CurrentTime);
        XFlush(display);
        return false;
    }
    const bool down = XTestFakeKeyEvent(display, code, True, CurrentTime);
    const bool up = XTestFakeKeyEvent(display, code, False, CurrentTime);
    for (int i = count - 1; i >= 0; --i) XTestFakeKeyEvent(display, held[i], False, CurrentTime);
    XFlush(display);
    return down && up;
}
bool inWindowTree(Display *display, Window child, Window ancestor) {
    while (child != None && child != root(display)) {
        if (child == ancestor) return true;
        child = parent(display, child);
    }
    return false;
}
}

bool NativeWindow::supported() { return x11(); }
QString NativeWindow::supportMessage() {
    return QStringLiteral("External window control requires an X11 session. Wayland blocks other-app window selection and input injection.");
}
WindowIdentity NativeWindow::at(const QPoint &point) {
    Display *display = open();
    if (!display) return {};
    Window rootWindow = root(display), hit = None;
    int x = 0, y = 0; unsigned mask = 0;
    Window resultRoot = None;
    if (!XQueryPointer(display, rootWindow, &resultRoot, &hit, &x, &y, &x, &y, &mask)) { XCloseDisplay(display); return {}; }
    Q_UNUSED(point);
    // Root usually reports a window-manager frame. Follow the pointer into
    // its child windows before walking back up to the client with _NET_WM_PID.
    Window candidate = hit;
    while (candidate != None) {
        Window child = None;
        if (!XQueryPointer(display, candidate, &resultRoot, &child, &x, &y, &x, &y, &mask) || child == None)
            break;
        candidate = child;
    }
    while (candidate != None && candidate != rootWindow) {
        unsigned long pid = 0;
        if (property(display, candidate, "_NET_WM_PID", pid) && pid) {
            QString name = title(display, candidate);
            XCloseDisplay(display);
            if (pid == static_cast<unsigned long>(QCoreApplication::applicationPid()) || name.isEmpty()) return {};
            return {quintptr(candidate), qint64(pid), name};
        }
        candidate = parent(display, candidate);
    }
    XCloseDisplay(display);
    return {};
}
bool NativeWindow::exists(const WindowIdentity &w) {
    Display *display = open();
    if (!display || !w.valid()) { if (display) XCloseDisplay(display); return false; }
    xError = false;
    auto *previous = XSetErrorHandler(ignoreXError);
    XWindowAttributes attributes{};
    unsigned long pid = 0;
    bool ok = XGetWindowAttributes(display, Window(w.id), &attributes) &&
              property(display, Window(w.id), "_NET_WM_PID", pid) && qint64(pid) == w.processId;
    XSync(display, False);
    XSetErrorHandler(previous);
    XCloseDisplay(display);
    return ok && !xError;
}
bool NativeWindow::activate(const WindowIdentity &w) {
    if (!exists(w)) return false;
    Display *display = open();
    if (!display) return false;
    Atom active = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
    XEvent event{};
    event.xclient.type = ClientMessage;
    event.xclient.window = Window(w.id);
    event.xclient.message_type = active;
    event.xclient.format = 32;
    event.xclient.data.l[0] = 1; // application request
    const bool sent = XSendEvent(display, root(display), False,
        SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XFlush(display);
    XCloseDisplay(display);
    return sent;
}
bool NativeWindow::isForeground(const WindowIdentity &w) {
    Display *display = open();
    if (!display || !w.valid()) { if (display) XCloseDisplay(display); return false; }
    unsigned long active = 0;
    bool ok = property(display, root(display), "_NET_ACTIVE_WINDOW", active) &&
              Window(active) == Window(w.id);
    if (!ok) {
        Window focus = None; int revert = 0;
        XGetInputFocus(display, &focus, &revert);
        ok = inWindowTree(display, focus, Window(w.id));
    }
    XCloseDisplay(display);
    return ok;
}
bool NativeWindow::paste(PasteShortcut shortcut, const QKeySequence &custom) {
    Display *display = open();
    if (!display) return false;
    bool ok = false;
    switch (shortcut) {
    case PasteShortcut::CtrlV: ok = chord(display, XK_Control_L, 0, XK_v); break;
    case PasteShortcut::CtrlShiftV: ok = chord(display, XK_Control_L, XK_Shift_L, XK_v); break;
    case PasteShortcut::ShiftInsert: ok = chord(display, XK_Shift_L, 0, XK_Insert); break;
    case PasteShortcut::MetaV: break;
    case PasteShortcut::Custom: ok = customChord(display, custom); break;
    }
    XCloseDisplay(display);
    return ok;
}
bool NativeWindow::sendUnicode(const QString &text) {
    Display *display = open();
    if (!display) return false;
    struct Key { KeyCode code; bool shift; };
    std::vector<Key> keys;
    // XTest sends keycodes. Resolve the active keymap before emitting anything, so
    // an unsupported character cannot cause a partially typed command.
    for (QChar ch : text) {
        if (ch.isSurrogate()) { XCloseDisplay(display); return false; }
        KeySym symbol = ch.unicode() < 128 ? KeySym(ch.unicode()) : KeySym(0x01000000 | ch.unicode());
        KeyCode code = XKeysymToKeycode(display, symbol);
        if (!code) { XCloseDisplay(display); return false; }
        int count = 0;
        KeySym *mapped = XGetKeyboardMapping(display, code, 1, &count);
        bool found = false, shift = false;
        for (int i = 0; mapped && i < qMin(count, 2); ++i) {
            if (mapped[i] == symbol) { found = true; shift = i == 1; break; }
        }
        if (mapped) XFree(mapped);
        if (!found) { XCloseDisplay(display); return false; }
        keys.push_back({code, shift});
    }
    const KeyCode shiftCode = XKeysymToKeycode(display, XK_Shift_L);
    bool ok = true;
    for (const auto &entry : keys) {
        if (entry.shift && (!shiftCode || !XTestFakeKeyEvent(display, shiftCode, True, CurrentTime))) {
            ok = false; break;
        }
        const bool down = XTestFakeKeyEvent(display, entry.code, True, CurrentTime);
        const bool up = XTestFakeKeyEvent(display, entry.code, False, CurrentTime);
        if (entry.shift) XTestFakeKeyEvent(display, shiftCode, False, CurrentTime);
        if (!down || !up) { ok = false; break; }
    }
    XFlush(display);
    XCloseDisplay(display);
    return ok;
}
bool NativeWindow::enter() {
    Display *display = open();
    if (!display) return false;
    const bool ok = fake(display, XK_Return);
    XFlush(display);
    XCloseDisplay(display);
    return ok;
}

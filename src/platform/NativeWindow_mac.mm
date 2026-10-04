#include "NativeWindow.h"
#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>

namespace {
bool postKey(CGKeyCode code, CGEventFlags modifiers = 0) {
    CGEventRef down = CGEventCreateKeyboardEvent(nullptr, code, true);
    CGEventRef up = CGEventCreateKeyboardEvent(nullptr, code, false);
    if (!down || !up) { if (down) CFRelease(down); if (up) CFRelease(up); return false; }
    CGEventSetFlags(down, modifiers);
    CGEventSetFlags(up, modifiers);
    CGEventPost(kCGHIDEventTap, down);
    CGEventPost(kCGHIDEventTap, up);
    CFRelease(down); CFRelease(up);
    return true;
}
bool customChord(const QKeySequence &sequence) {
    if (sequence.count() != 1) return false;
    const auto combination = sequence[0];
    const int key = int(combination.key());
    CGKeyCode code = UINT16_MAX;
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        static constexpr CGKeyCode letters[] = {0, 11, 8, 2, 14, 3, 5, 4, 34, 38, 40, 37, 46,
                                                  45, 31, 35, 12, 15, 1, 17, 32, 9, 13, 7, 16, 6};
        code = letters[key - Qt::Key_A];
    } else if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        static constexpr CGKeyCode digits[] = {29, 18, 19, 20, 21, 23, 22, 26, 28, 25};
        code = digits[key - Qt::Key_0];
    } else if (key == Qt::Key_Space) code = 49;
    else if (key == Qt::Key_Return || key == Qt::Key_Enter) code = 36;
    else if (key == Qt::Key_Tab) code = 48;
    else if (key == Qt::Key_Delete) code = 51;
    if (code == UINT16_MAX) return false;
    const auto modifiers = combination.keyboardModifiers();
    CGEventFlags flags = 0;
    if (modifiers & Qt::ControlModifier) flags |= kCGEventFlagMaskControl;
    if (modifiers & Qt::AltModifier) flags |= kCGEventFlagMaskAlternate;
    if (modifiers & Qt::ShiftModifier) flags |= kCGEventFlagMaskShift;
    if (modifiers & Qt::MetaModifier) flags |= kCGEventFlagMaskCommand;
    return postKey(code, flags);
}
}

bool NativeWindow::supported() { return AXIsProcessTrusted(); }
QString NativeWindow::supportMessage() {
    return QStringLiteral("Allow SendToCmd in System Settings → Privacy & Security → Accessibility, then restart it.");
}
WindowIdentity NativeWindow::at(const QPoint &point) {
    Q_UNUSED(point);
    CFArrayRef windows = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
    if (!windows) return {};
    WindowIdentity found;
    CGEventRef cursorEvent = CGEventCreate(nullptr);
    if (!cursorEvent) { CFRelease(windows); return {}; }
    const CGPoint p = CGEventGetLocation(cursorEvent);
    CFRelease(cursorEvent);
    for (CFIndex i = 0; i < CFArrayGetCount(windows); ++i) {
        NSDictionary *info = (__bridge NSDictionary *)CFArrayGetValueAtIndex(windows, i);
        NSNumber *pid = info[(id)kCGWindowOwnerPID];
        NSNumber *number = info[(id)kCGWindowNumber];
        NSNumber *layer = info[(id)kCGWindowLayer];
        NSDictionary *bounds = info[(id)kCGWindowBounds];
        CGRect rect{};
        if (!pid || !number || layer.intValue != 0 || pid.longLongValue == NSProcessInfo.processInfo.processIdentifier ||
            !CGRectMakeWithDictionaryRepresentation((__bridge CFDictionaryRef)bounds, &rect) || !CGRectContainsPoint(rect, p)) continue;
        NSString *title = info[(id)kCGWindowName];
        NSString *owner = info[(id)kCGWindowOwnerName];
        found = {quintptr(number.unsignedLongLongValue), pid.longLongValue,
                 QString::fromNSString(title.length ? title : owner)};
        break;
    }
    CFRelease(windows);
    return found;
}
bool NativeWindow::exists(const WindowIdentity &w) {
    if (!w.valid()) return false;
    CFArrayRef windows = CGWindowListCopyWindowInfo(kCGWindowListOptionIncludingWindow, CGWindowID(w.id));
    if (!windows) return false;
    bool found = false;
    for (CFIndex i = 0; i < CFArrayGetCount(windows); ++i) {
        NSDictionary *info = (__bridge NSDictionary *)CFArrayGetValueAtIndex(windows, i);
        NSNumber *pid = info[(id)kCGWindowOwnerPID];
        NSNumber *number = info[(id)kCGWindowNumber];
        if (number.unsignedLongLongValue == w.id && pid.longLongValue == w.processId) { found = true; break; }
    }
    CFRelease(windows);
    return found;
}
bool NativeWindow::activate(const WindowIdentity &w) {
    if (!supported() || !exists(w)) return false;
    NSRunningApplication *app = [NSRunningApplication runningApplicationWithProcessIdentifier:pid_t(w.processId)];
    return app && [app activateWithOptions:NSApplicationActivateIgnoringOtherApps];
}
bool NativeWindow::isForeground(const WindowIdentity &w) {
    if (!exists(w) || NSWorkspace.sharedWorkspace.frontmostApplication.processIdentifier != w.processId) return false;
    CFArrayRef windows = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
    if (!windows) return false;
    bool front = false;
    for (CFIndex i = 0; i < CFArrayGetCount(windows); ++i) {
        NSDictionary *info = (__bridge NSDictionary *)CFArrayGetValueAtIndex(windows, i);
        NSNumber *pid = info[(id)kCGWindowOwnerPID];
        NSNumber *number = info[(id)kCGWindowNumber];
        NSNumber *layer = info[(id)kCGWindowLayer];
        if (pid.longLongValue == w.processId && layer.intValue == 0) {
            front = number.unsignedLongLongValue == w.id;
            break;
        }
    }
    CFRelease(windows);
    return front;
}
bool NativeWindow::paste(PasteShortcut shortcut, const QKeySequence &custom) {
    if (!supported()) return false;
    if (shortcut == PasteShortcut::Custom) return customChord(custom);
    if (shortcut == PasteShortcut::MetaV) return postKey(9, kCGEventFlagMaskCommand);
    return false;
}
bool NativeWindow::sendUnicode(const QString &text) {
    if (!supported()) return false;
    const auto *units = reinterpret_cast<const UniChar *>(text.utf16());
    for (qsizetype offset = 0; offset < text.size();) {
        qsizetype length = qMin<qsizetype>(20, text.size() - offset);
        if (offset + length < text.size() && text.at(offset + length - 1).isHighSurrogate() &&
            text.at(offset + length).isLowSurrogate()) --length;
        const UniCharCount count = UniCharCount(length);
        CGEventRef down = CGEventCreateKeyboardEvent(nullptr, 0, true);
        CGEventRef up = CGEventCreateKeyboardEvent(nullptr, 0, false);
        if (!down || !up) { if (down) CFRelease(down); if (up) CFRelease(up); return false; }
        CGEventKeyboardSetUnicodeString(down, count, units + offset);
        CGEventKeyboardSetUnicodeString(up, count, units + offset);
        CGEventPost(kCGHIDEventTap, down); CGEventPost(kCGHIDEventTap, up);
        CFRelease(down); CFRelease(up);
        offset += length;
    }
    return true;
}
bool NativeWindow::enter() { return supported() && postKey(36); }

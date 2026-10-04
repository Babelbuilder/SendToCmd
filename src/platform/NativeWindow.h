#pragma once
#include <QPoint>
#include <QKeySequence>
#include <QString>

enum class PasteShortcut { CtrlV, CtrlShiftV, ShiftInsert, MetaV, Custom };

struct WindowIdentity {
    quintptr id = 0;
    qint64 processId = 0;
    QString title;
    QString executableName;
    bool valid() const { return id != 0 && processId != 0; }
};

class NativeWindow {
public:
    static bool supported();
    static QString supportMessage();
    static WindowIdentity at(const QPoint &screenPoint);
    static bool exists(const WindowIdentity &window);
    static bool activate(const WindowIdentity &window);
    static bool isForeground(const WindowIdentity &window);
    static bool paste(PasteShortcut shortcut, const QKeySequence &custom = {});
    static bool sendUnicode(const QString &text);
    static bool enter();
};

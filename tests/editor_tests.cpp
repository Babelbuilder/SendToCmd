#include "../src/CommandEditor.h"
#include <QApplication>
#include <cassert>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    CommandEditor editor;
    editor.setPlainText(QStringLiteral("pwd\n\nls"));
    assert(editor.document()->blockCount() == 3);
    editor.goToLine(1);
    assert(editor.currentLine() == 1);
    editor.setStartLine(0);
    editor.setEndLine(2);
    assert(editor.startLine() == 0 && editor.endLine() == 2);
    editor.insertPlainText(QStringLiteral("echo"));
    assert(editor.startLine() == -1 && editor.endLine() == -1);
    return 0;
}

#include "../src/MainWindow.h"
#include <QApplication>
#include <QTabWidget>
#include <QTabBar>
#include <QToolButton>
#include <QTemporaryDir>
#include <QFile>
#include <QMessageBox>
#include <QAbstractButton>
#include <QTimer>
#include <QCloseEvent>
#include <QLabel>
#include <stdexcept>
#include <iostream>

static void check(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
class RecordingTarget : public ICommandTarget {
public:
    int sent = 0;
    int cancelled = 0;
    bool isAvailable() const override { return true; }
    bool canContinue() const override { return true; }
    void sendLine(const QString &, Completion completion) override {
        ++sent; completion(SendResult::ok());
    }
    void cancel() override { ++cancelled; }
};
class MainWindowTests {
public:
    static void answerDialog(QMessageBox::StandardButton answer) {
        QTimer::singleShot(0, [answer] {
            auto *dialog = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (!dialog) throw std::runtime_error("Expected save confirmation");
            dialog->button(answer)->click();
        });
    }
    static void run() {
        MainWindow window;
        check(window.tabs_->count() == 1, "Startup must have a blank tab");
        auto *first = window.editor_;
        first->setPlainText(QStringLiteral("pwd\n\nls\nuname"));
        first->document()->setModified(true);
        first->goToLine(2); first->setStartLine(0); first->setEndLine(3);
        check(window.tabs_->tabText(0).startsWith('*'), "Dirty tab indicator missing");
        window.newFile();
        check(window.tabs_->count() == 2, "New must retain the dirty first document");
        auto *second = window.editor_;
        second->setPlainText(QStringLiteral("echo second\nwhoami"));
        second->goToLine(1); second->setStartLine(1); second->setEndLine(1);
        window.tabs_->setCurrentIndex(0);
        check(window.editor_ == first && first->currentLine() == 2, "Cursor was not preserved");
        check(first->startLine() == 0 && first->endLine() == 3, "First range was not preserved");
        check(second->startLine() == 1 && second->endLine() == 1, "Ranges must be independent");
        check(second->toPlainText().startsWith("echo second"), "Content was not preserved");
        window.tabs_->tabBar()->moveTab(0, 1);
        check(window.editor_ == first, "Reordering tabs changed the active document");
        QTemporaryDir directory;
        check(directory.isValid(), "Temporary directory failed");
        const QString path = directory.filePath(QStringLiteral("commands.txt"));
        QFile file(path); check(file.open(QIODevice::WriteOnly), "Cannot create fixture");
        file.write("echo loaded\nls"); file.close();
        check(window.openDocument(path), "Open failed");
        auto *loaded = window.editor_;
        check(window.tabs_->count() == 3 && loaded->toPlainText() == "echo loaded\nls", "Open replaced another tab");
        check(window.openDocument(path) && window.tabs_->count() == 3, "Duplicate open created another tab");
        if (qEnvironmentVariableIsSet("SENDTOCMD_TEST_SCREENSHOT")) {
            window.show(); QApplication::processEvents();
            check(window.grab().save(qEnvironmentVariable("SENDTOCMD_TEST_SCREENSHOT")), "Preview capture failed");
        }
        loaded->insertPlainText(QStringLiteral("# "));
        check(window.saveFile(false), "Save failed");
        check(!loaded->document()->isModified(), "Saved tab is still dirty");
        check(first->document()->isModified(), "Save reset another tab's dirty state");
        check(file.open(QIODevice::ReadOnly) && file.readAll().startsWith("# echo loaded"), "Save wrote wrong content");
        file.close();
        loaded->insertPlainText(QStringLiteral("# more "));
        answerDialog(QMessageBox::Yes);
        static_cast<QToolButton *>(window.tabs_->tabBar()->tabButton(window.tabs_->indexOf(loaded), QTabBar::RightSide))->click();
        check(window.tabs_->count() == 2, "Save confirmation did not close the document");
        check(file.open(QIODevice::ReadOnly) && file.readAll().contains("# more "), "Close did not save changes");
        file.close();
        window.tabs_->setCurrentWidget(first);
        answerDialog(QMessageBox::Cancel);
        window.closeTab(window.tabs_->indexOf(first));
        check(window.tabs_->count() == 2 && window.editor_ == first, "Cancel lost the dirty document");
        answerDialog(QMessageBox::No);
        window.closeTab(window.tabs_->indexOf(first));
        check(window.tabs_->count() == 1 && window.editor_ == second, "Closing tab selected wrong document");
        second->document()->setModified(false);
        window.closeTab(0);
        check(window.tabs_->count() == 1 && window.editor_->toPlainText().isEmpty(), "Last close must create blank tab");
        window.editor_->insertPlainText(QStringLiteral("unsaved"));
        window.newFile();
        answerDialog(QMessageBox::Cancel);
        QCloseEvent cancelled; window.closeEvent(&cancelled);
        check(!cancelled.isAccepted() && window.tabs_->count() == 2, "Exit ignored inactive dirty tab");
        window.setBusy(true);
        const quint64 generation = window.manualGeneration_;
        window.tabs_->setCurrentIndex(1);
        check(!window.busy_ && window.manualGeneration_ > generation, "Tab switch did not cancel in-flight send");
        check(!window.editor_->isReadOnly(), "Tab switch left editor locked");
        RecordingTarget target;
        window.editor_->setPlainText(QStringLiteral("one\ntwo\nthree"));
        window.autoEngine_.start(window.documentLines(), 0, 2, 1000, &target);
        window.setBusy(true);
        check(window.autoEngine_.running() && target.sent == 1, "Auto-send fixture did not start");
        window.tabs_->setCurrentIndex(0);
        check(!window.autoEngine_.running() && target.cancelled == 1 && target.sent == 1,
              "Switching tabs did not stop the original auto-send range");
    }
};
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    try { MainWindowTests::run(); }
    catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
    return 0;
}

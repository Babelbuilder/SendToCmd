#pragma once
#include <QMainWindow>
#include <QSettings>
#include <memory>
#include "CommandEditor.h"
#include "core/AutoSendEngine.h"
#include "target/ExternalWindowTarget.h"

class QAction;
class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QMenu;
class QToolButton;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
private:
    QString t(const char *english, const char *chinese) const;
    void retranslate();
    void updateTitle();
    void updateTargetLabel();
    void updateSendButton();
    void setBusy(bool busy);
    void sendRequested();
    void sendCurrentLine();
    void onAutoFinished(bool completed, const QString &error);
    void setLanguage(bool chinese);
    bool confirmDiscard();
    void newFile();
    void openFile();
    bool saveFile(bool saveAs);
    void findText();
    QStringList documentLines() const;
    void error(const QString &message);

    CommandEditor *editor_ = nullptr;
    QLabel *targetLabel_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QLabel *intervalLabel_ = nullptr;
    QLabel *secondLabel_ = nullptr;
    QLabel *bindLabel_ = nullptr;
    QToolButton *picker_ = nullptr;
    QToolButton *sendButton_ = nullptr;
    QCheckBox *autoCheck_ = nullptr;
    QDoubleSpinBox *interval_ = nullptr;
    QMenu *fileMenu_ = nullptr;
    QMenu *editMenu_ = nullptr;
    QMenu *settingsMenu_ = nullptr;
    QMenu *languageMenu_ = nullptr;
    QMenu *methodMenu_ = nullptr;
    QMenu *pasteMenu_ = nullptr;
    QAction *englishAction_ = nullptr;
    QAction *chineseAction_ = nullptr;
    QAction *clipboardAction_ = nullptr;
    QAction *keyboardAction_ = nullptr;
    QAction *focusDelayAction_ = nullptr;
    QList<QAction *> fileActions_;
    QList<QAction *> editActions_;
    QList<QAction *> pasteActions_;
    QSettings settings_;
    ExternalWindowTarget target_;
    AutoSendEngine autoEngine_;
    QString currentPath_;
    bool chinese_ = false;
    bool picking_ = false;
    bool busy_ = false;
    quint64 manualGeneration_ = 0;
    InputMethod method_ = InputMethod::Clipboard;
    PasteShortcut pasteShortcut_ = PasteShortcut::CtrlV;
    QKeySequence customShortcut_;
    InputTiming timing_;
};

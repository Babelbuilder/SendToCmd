#pragma once
#include <QObject>
#include <QStringList>
#include <QTimer>
#include "../target/ICommandTarget.h"

class AutoSendEngine : public QObject {
    Q_OBJECT
public:
    explicit AutoSendEngine(QObject *parent = nullptr);
    bool running() const { return running_; }
    void start(QStringList lines, int first, int last, int intervalMs, ICommandTarget *target);
    void stop();
signals:
    void lineSent(int zeroBasedLine);
    void finished(bool completed, const QString &error);
private:
    void sendNext();
    QStringList lines_;
    int next_ = 0;
    int last_ = 0;
    int intervalMs_ = 1000;
    bool running_ = false;
    bool sentAny_ = false;
    ICommandTarget *target_ = nullptr;
    QTimer timer_;
};

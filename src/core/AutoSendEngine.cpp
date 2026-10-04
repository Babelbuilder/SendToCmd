#include "AutoSendEngine.h"
#include "SendValidator.h"

AutoSendEngine::AutoSendEngine(QObject *parent) : QObject(parent) {
    timer_.setSingleShot(true);
    connect(&timer_, &QTimer::timeout, this, &AutoSendEngine::sendNext);
}

void AutoSendEngine::start(QStringList lines, int first, int last, int intervalMs, ICommandTarget *target) {
    if (running_) return;
    if (!target || !target->isAvailable() || first < 0 || last < first || last >= lines.size()) {
        emit finished(false, QStringLiteral("Bind a target and set a valid start and end line."));
        return;
    }
    for (int i = first; i <= last; ++i) {
        const auto result = SendValidator::validateLine(lines.at(i));
        if (!result.ok) {
            emit finished(false, QStringLiteral("Line %1: %2").arg(i + 1).arg(result.error));
            return;
        }
    }
    lines_ = std::move(lines);
    next_ = first;
    last_ = last;
    intervalMs_ = intervalMs;
    target_ = target;
    running_ = true;
    sentAny_ = false;
    sendNext();
}

void AutoSendEngine::stop() {
    if (!running_) return;
    running_ = false;
    timer_.stop();
    if (target_) target_->cancel();
    emit finished(false, {});
}

void AutoSendEngine::sendNext() {
    if (!running_) return;
    if (sentAny_ && !target_->canContinue()) {
        running_ = false;
        emit finished(false, QStringLiteral("Target lost focus or closed; auto send stopped."));
        return;
    }
    const int line = next_;
    target_->sendLine(lines_.at(line), [this, line](SendResult result) {
        if (!running_) return;
        if (!result.success) {
            running_ = false;
            emit finished(false, result.error);
            return;
        }
        emit lineSent(line);
        sentAny_ = true;
        if (line == last_) {
            running_ = false;
            emit finished(true, {});
            return;
        }
        next_ = line + 1;
        timer_.start(intervalMs_);
    });
}

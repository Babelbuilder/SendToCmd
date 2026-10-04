#pragma once
#include <QString>
#include <functional>

struct SendResult {
    bool success = false;
    QString error;
    static SendResult ok() { return {true, {}}; }
    static SendResult fail(const QString &message) { return {false, message}; }
};

class ICommandTarget {
public:
    using Completion = std::function<void(SendResult)>;
    virtual ~ICommandTarget() = default;
    virtual bool isAvailable() const = 0;
    virtual bool canContinue() const { return isAvailable(); }
    virtual void sendLine(const QString &line, Completion completion) = 0;
    virtual void cancel() = 0;
};

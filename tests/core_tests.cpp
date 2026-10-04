#include "../src/core/AutoSendEngine.h"
#include "../src/core/SendValidator.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <cassert>

class FakeTarget : public ICommandTarget {
public:
    QStringList sent;
    bool available = true;
    bool foreground = true;
    bool loseFocusAfterFirst = false;
    int failAt = -1;
    bool isAvailable() const override { return available; }
    bool canContinue() const override { return available && foreground; }
    void sendLine(const QString &line, Completion completion) override {
        sent << line;
        if (loseFocusAfterFirst && sent.size() == 1) foreground = false;
        if (sent.size() == failAt) completion(SendResult::fail(QStringLiteral("test failure")));
        else completion(SendResult::ok());
    }
    void cancel() override {}
};

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    assert(SendValidator::validateLine({}).ok);
    assert(SendValidator::validateLine(QString(4096, QLatin1Char('x'))).ok);
    assert(!SendValidator::validateLine(QString(4097, QLatin1Char('x'))).ok);
    assert(!SendValidator::validateLine(QStringLiteral("a\tb")).ok);
    assert(SendValidator::validateLine(QStringLiteral("echo 中文")).ok);

    AutoSendEngine engine;
    FakeTarget target;
    QEventLoop loop;
    bool completed = false;
    QObject::connect(&engine, &AutoSendEngine::finished, &loop, [&](bool success, const QString &) {
        completed = success; loop.quit();
    });
    engine.start({QStringLiteral("skip"), QStringLiteral("pwd"), {}, QStringLiteral("ls")}, 1, 3, 1, &target);
    if (engine.running()) loop.exec();
    assert(completed);
    assert(target.sent == QStringList({QStringLiteral("pwd"), {}, QStringLiteral("ls")}));

    target.sent.clear(); target.failAt = 2; completed = true;
    engine.start({QStringLiteral("pwd"), QStringLiteral("ls"), QStringLiteral("uname")}, 0, 2, 1, &target);
    if (engine.running()) loop.exec();
    assert(!completed && target.sent.size() == 2);

    target.sent.clear(); target.failAt = -1; completed = true;
    engine.start({QStringLiteral("pwd"), QStringLiteral("ls")}, 1, 0, 1, &target);
    assert(!completed && target.sent.isEmpty());

    target.loseFocusAfterFirst = true; target.foreground = true; completed = true;
    engine.start({QStringLiteral("pwd"), QStringLiteral("ls")}, 0, 1, 1, &target);
    if (engine.running()) loop.exec();
    assert(!completed && target.sent == QStringList({QStringLiteral("pwd")}));
    return 0;
}

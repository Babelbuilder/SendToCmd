#pragma once
#include <QString>

struct ValidationResult {
    bool ok = true;
    QString error;
};

class SendValidator {
public:
    static ValidationResult validateLine(const QString &line);
};

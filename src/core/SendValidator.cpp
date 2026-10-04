#include "SendValidator.h"

ValidationResult SendValidator::validateLine(const QString &line) {
    if (line.size() > 4096)
        return {false, QStringLiteral("Line exceeds 4096 UTF-16 code units.")};
    for (QChar ch : line) {
        if (ch.isNull() || ch.category() == QChar::Other_Control)
            return {false, QStringLiteral("Line contains a tab or control character.")};
    }
    return {};
}

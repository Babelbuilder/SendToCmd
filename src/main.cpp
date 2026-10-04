#include "MainWindow.h"
#include <QApplication>
#include <QIcon>
#include <QPixmap>

static QIcon applicationIcon() {
    QIcon icon;
    for (int size : {16, 24, 32, 48, 64, 128, 256}) {
        icon.addPixmap(QPixmap(QStringLiteral(":/icons/icons/sendtocmd-%1.png").arg(size)));
    }
    return icon;
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("SendToCmd"));
    app.setOrganizationName(QStringLiteral("SendToCmd"));
    app.setWindowIcon(applicationIcon());
    MainWindow window;
    window.show();
    return app.exec();
}

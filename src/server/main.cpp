#include <QtWidgets/QApplication>

#include "server_window.hpp"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    ServerWindow window;
    window.show();
    return app.exec();
}

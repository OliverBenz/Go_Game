#include "game.hpp"
#include "window.hpp"

#include <QApplication>

int main(int argc, char** argv) {
    // Game game(800, 800);
    // game.run();

    QApplication app(argc, argv);

    MainWindow window;
    window.show();

    return app.exec();
}
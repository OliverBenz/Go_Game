#include "window.hpp"

#include "game.hpp"

#include <QApplication>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    // A simple central widget to show something
    QWidget* central = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(central);

    setCentralWidget(central);

    setWindowTitle("Qt6 Window");
    resize(400, 300);
}

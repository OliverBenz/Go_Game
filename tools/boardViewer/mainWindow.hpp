#pragma once

#include "gui/boardWidget.hpp"

#include <QImage>
#include <QMainWindow>
#include <QWidget>

#include <filesystem>
#include <nlohmann/json_fwd.hpp>
#include <opencv2/core/mat.hpp>

class QLabel;
class QPushButton;

namespace tengen {

//! Displays a test image (pipeline result overlaid + .json ground truth) next to the dotBW ground truth board.
//! To be added: openCV mats, custom serialization functions, sgf files.
class MainWindow : public QMainWindow {
	Q_OBJECT

public:
	explicit MainWindow(QWidget* parent = nullptr);
	~MainWindow() override;

private slots:
	void onOpenImageClicked();

private:
	void buildLayout();
	void loadImage(const std::filesystem::path& imagePath);

private:
	QLabel* m_imageLabel{nullptr};
	QPushButton* m_openButton{nullptr};
	gui::BoardWidget* m_boardWidget{nullptr};
};

} // namespace tengen

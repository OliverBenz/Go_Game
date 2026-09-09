#include "mainWindow.hpp"

#include "core/serializer.hpp"

#include "vision/core/boardFinder.hpp"
#include "vision/core/gridFinder.hpp"
#include "vision/core/stoneFinder.hpp"

#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>

#include <QFileDialog>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QShortcut>
#include <QVBoxLayout>

#include <fstream>
#include <vector>

using namespace tengen::vision::core;

namespace tengen {

static QImage matToQImage(const cv::Mat& mat) {
	if (mat.empty()) {
		return {};
	}
	cv::Mat rgb;
	cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
	return QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888).copy();
}

static std::vector<cv::Point2f> parsePoints(const nlohmann::json& array) {
	std::vector<cv::Point2f> points;
	points.reserve(array.size());
	for (const auto& p: array) {
		points.emplace_back(p.at(0).get<float>(), p.at(1).get<float>());
	}
	return points;
}

//! Map the vision pipeline's stone state to the model's board stone type (same semantics, different enums).
static Board::Stone toBoardStone(StoneState state) {
	switch (state) {
	case StoneState::Black:
		return Board::Stone::Black;
	case StoneState::White:
		return Board::Stone::White;
	case StoneState::Empty:
	default:
		return Board::Stone::Empty;
	}
}

//! Run the pipeline and draw detected stones + .json ground truth corners onto a copy of the original image.
//! If groundTruth is provided and matches the detected board size, mismatching intersections are circled thick red.
static cv::Mat buildOverlayImage(const cv::Mat& original, const std::filesystem::path& imagePath, const Board* groundTruth) {
	cv::Mat overlay = original.clone();

	const WarpResult warped = warpToBoard(original);
	if (!isValidBoard(warped)) {
		return overlay;
	}
	const BoardGeometry geometry = analyseGeometry(warped);
	if (!isValidGeometry(geometry)) {
		return overlay;
	}
	const RectifiedBoard rectified = transformImage(original, geometry);
	if (!isValidRectifiedBoard(rectified)) {
		return overlay;
	}
	const StoneResult stoneRes = analyseBoard(rectified);

	// Project detected intersections (in rectified/B space) back into the original image space.
	std::vector<cv::Point2f> intersectionsOriginal;
	cv::perspectiveTransform(geometry.intersections, intersectionsOriginal, geometry.H.inv());

	const unsigned N      = geometry.boardSize;
	const bool canCompare = groundTruth != nullptr && groundTruth->size() == N;

	for (std::size_t i = 0; i < intersectionsOriginal.size(); ++i) {
		const cv::Point pt(cv::saturate_cast<int>(intersectionsOriginal[i].x), cv::saturate_cast<int>(intersectionsOriginal[i].y));
		if (stoneRes.success && i < stoneRes.stones.size()) {
			switch (stoneRes.stones[i]) {
			case StoneState::Black:
				cv::circle(overlay, pt, 10, cv::Scalar(0, 0, 0), cv::FILLED, cv::LINE_AA);
				cv::circle(overlay, pt, 10, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
				break;
			case StoneState::White:
				cv::circle(overlay, pt, 10, cv::Scalar(255, 255, 255), cv::FILLED, cv::LINE_AA);
				cv::circle(overlay, pt, 10, cv::Scalar(0, 0, 0), 1, cv::LINE_AA);
				break;
			case StoneState::Empty:
				cv::circle(overlay, pt, 3, cv::Scalar(0, 255, 0), 1, cv::LINE_AA);
				break;
			}

			// Compare detected stone against ground truth. Intersection i maps to Coord{x, y} = {i / N, i % N}
			if (canCompare) {
				const Coord c{static_cast<unsigned>(i / N), static_cast<unsigned>(i % N)};
				const Board::Stone expected = groundTruth->get(c);
				if (toBoardStone(stoneRes.stones[i]) != expected) {
					cv::circle(overlay, pt, 18, cv::Scalar(0, 0, 255), 4, cv::LINE_AA);
				}
			}
		}
	}

	// Overlay .json ground truth (already defined in original image space), if present.
	std::filesystem::path jsonPath = imagePath;
	jsonPath.replace_extension(".json");
	std::ifstream jsonFile(jsonPath);
	if (jsonFile.is_open()) {
		const nlohmann::json j = nlohmann::json::parse(jsonFile, nullptr, /*allow_exceptions=*/false);
		if (!j.is_discarded()) {
			for (const cv::Point2f& p: parsePoints(j.value("boardCorners", nlohmann::json::array()))) {
				cv::drawMarker(overlay, p, cv::Scalar(0, 215, 255), cv::MARKER_TILTED_CROSS, 24, 3, cv::LINE_AA);
			}
			for (const cv::Point2f& p: parsePoints(j.value("gridCorners", nlohmann::json::array()))) {
				cv::drawMarker(overlay, p, cv::Scalar(255, 255, 0), cv::MARKER_CROSS, 20, 2, cv::LINE_AA);
			}
		}
	}

	return overlay;
}


MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
	setWindowTitle("Board Viewer");
	buildLayout();

	auto* escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
	connect(escShortcut, &QShortcut::activated, this, &QMainWindow::close);
}

MainWindow::~MainWindow() = default;

void MainWindow::buildLayout() {
	auto* rootWidget = new QWidget(this);
	auto* rootLayout = new QHBoxLayout(rootWidget);

	m_imageLabel = new QLabel("No image loaded.", rootWidget);
	m_imageLabel->setAlignment(Qt::AlignCenter);
	m_imageLabel->setMinimumSize(400, 400);

	m_openButton     = new QPushButton("Open Image...", rootWidget);
	auto* leftLayout = new QVBoxLayout();
	leftLayout->addWidget(m_openButton);
	leftLayout->addWidget(m_imageLabel, 1);

	m_boardWidget = new gui::BoardWidget(rootWidget);

	rootLayout->addLayout(leftLayout, 1);
	rootLayout->addWidget(m_boardWidget, 1);

	connect(m_openButton, &QPushButton::clicked, this, &MainWindow::onOpenImageClicked);

	setCentralWidget(rootWidget);
}

void MainWindow::onOpenImageClicked() {
	const QString fileName = QFileDialog::getOpenFileName(this, "Open Test Image", QString{PATH_TEST_IMG}, "Images (*.png *.jpg *.jpeg)");
	if (fileName.isEmpty()) {
		return;
	}
	loadImage(fileName.toStdString());
}

void MainWindow::loadImage(const std::filesystem::path& imagePath) {
	const cv::Mat original = cv::imread(imagePath.string());
	if (original.empty()) {
		return;
	}

	std::filesystem::path txtPath = imagePath;
	txtPath.replace_extension(".txt");
	Board board(0u);
	const bool hasBoard = readBoard(txtPath, board);

	const cv::Mat overlay = buildOverlayImage(original, imagePath, hasBoard ? &board : nullptr);
	m_imageLabel->setPixmap(QPixmap::fromImage(matToQImage(overlay)).scaled(m_imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

	if (hasBoard) {
		m_boardWidget->setBoard(board);
	}
}

} // namespace tengen

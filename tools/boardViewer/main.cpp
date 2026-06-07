#include "../../src/vision/devtools/include/vision/devtools/syntheticBoard.hpp"
#include "core/serializer.hpp"
#include "gui/boardWidget.hpp"

#include "vision/devtools/syntheticBoard.hpp"

#include <QApplication>
#include <QShortcut>

// Used for quickly visualising stuff I work on.
// Only supports visualising dotBW format for now. To be added: Real Images, openCV mats, custom serialization functions, sgf files.
int main(int argc, char* argv[]) {
	auto res = tengen::vision::devtools::makeCanonicalBoardImage(9, 300);
	cv::imshow("img", res);
	cv::waitKey(0);
#ifdef APP
	QApplication application(argc, argv);

	tengen::Board board(9u);
	if (!tengen::readBoard(std::filesystem::path{PATH_TEST_IMG} / "example.txt", board)) {
		return -1;
	}

	tengen::gui::BoardWidget boardWidget;
	boardWidget.setBoard(board);
	boardWidget.resize(800, 800);

	QShortcut escShortcut(QKeySequence(Qt::Key_Escape), &boardWidget);
	QObject::connect(&escShortcut, &QShortcut::activated, &application, &QApplication::quit);

	boardWidget.show();
	return application.exec();
#endif
}

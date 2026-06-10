#pragma once

#include <QDialog>

class QComboBox;

namespace tengen::gui {

class BotDialog : public QDialog {
	Q_OBJECT

public:
	explicit BotDialog(QWidget* parent = nullptr);

	unsigned boardSize() const;
	unsigned difficultyIndex() const;
	bool humanPlaysBlack() const;

private:
	QComboBox* m_boardSize{nullptr};
	QComboBox* m_difficulty{nullptr};
	QComboBox* m_colour{nullptr};
};

} // namespace tengen::gui

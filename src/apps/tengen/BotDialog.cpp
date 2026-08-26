#include "BotDialog.hpp"

#include "Logging.hpp"
#include "model/player.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <fcntl.h>

namespace tengen::gui {

BotDialog::BotDialog(QWidget* parent)
    : QDialog(parent) {
	setWindowTitle("New Bot Game");

	m_boardSize = new QComboBox(this);
	m_boardSize->addItem("9x9", 9u);
	m_boardSize->addItem("13x13", 13u);
	m_boardSize->addItem("19x19", 19u);
	m_boardSize->setCurrentIndex(0);

	m_difficulty = new QComboBox(this);
	m_difficulty->addItem("Easy", static_cast<int>(Difficulty::Easy));
	m_difficulty->addItem("Medium", static_cast<int>(Difficulty::Medium));
	m_difficulty->addItem("Hard", static_cast<int>(Difficulty::Hard));
	m_difficulty->setCurrentIndex(1);

	m_colour = new QComboBox(this);
	m_colour->addItem("Black", static_cast<int>(Player::Black));
	m_colour->addItem("White", static_cast<int>(Player::White));
	m_colour->setCurrentIndex(0);

	auto* form = new QFormLayout();
	form->addRow(tr("Board size:"), m_boardSize);
	form->addRow(tr("Difficulty:"), m_difficulty);
	form->addRow(tr("Your color:"), m_colour);

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	auto* layout = new QVBoxLayout(this);
	layout->addLayout(form);
	layout->addWidget(buttons);
}

unsigned BotDialog::boardSize() const {
	const unsigned boardSize = m_boardSize->currentData().toUInt();

	if (boardSize != 9 && boardSize != 13 && boardSize != 19) {
		Logger().Log(Logging::LogLevel::Error, "Invalid board size selected in Bot game. Choosing 9x9.");
		return 9u;
	}
	return boardSize;
}

Difficulty BotDialog::difficulty() const {
	const int difficulty = m_difficulty->currentData().toInt();

	if (difficulty < static_cast<int>(Difficulty::Easy) || difficulty > static_cast<int>(Difficulty::Hard)) {
		Logger().Log(Logging::LogLevel::Error, "Invalid difficulty value in Bot game selected. Choosing Easy.");
		return Difficulty::Easy;
	}
	return static_cast<Difficulty>(difficulty);
}

bool BotDialog::humanPlaysBlack() const {
	const int player = m_colour->currentData().toInt();

	if (player != static_cast<int>(Player::White) && player != static_cast<int>(Player::Black)) {
		Logger().Log(Logging::LogLevel::Error, "Invalid player selection in Bot game. Choosing Black.");
		return true;
	}
	return static_cast<Player>(player) == Player::Black;
}

} // namespace tengen::gui

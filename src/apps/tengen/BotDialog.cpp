#include "BotDialog.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>

namespace tengen::gui {

BotDialog::BotDialog(QWidget* parent) : QDialog(parent) {
	setWindowTitle("New Bot Game");

	m_boardSize = new QComboBox(this);
	m_boardSize->addItem("9x9", 9u);
	m_boardSize->addItem("13x13", 13u);
	m_boardSize->addItem("19x19", 19u);
	m_boardSize->setCurrentIndex(0);

	m_difficulty = new QComboBox(this);
	m_difficulty->addItem("Easy", 0u);
	m_difficulty->addItem("Medium", 1u);
	m_difficulty->addItem("Hard", 2u);
	m_difficulty->setCurrentIndex(1);

	m_colour = new QComboBox(this);
	m_colour->addItem("Black", true);
	m_colour->addItem("White", false);
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
	return m_boardSize->currentData().toUInt();
}

unsigned BotDialog::difficultyIndex() const {
	return m_difficulty->currentData().toUInt();
}

bool BotDialog::humanPlaysBlack() const {
	return m_colour->currentData().toBool();
}

} // namespace tengen::gui

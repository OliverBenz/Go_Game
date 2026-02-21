#include "HostDialog.hpp"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace go::gui {

HostDialog::HostDialog(QWidget* parent) : QDialog(parent) {
	setWindowTitle("Host Server");

	auto* label = new QLabel(tr("Board size:"), this);

	m_btn9  = new QPushButton("9", this);
	m_btn13 = new QPushButton("13", this);
	m_btn19 = new QPushButton("19", this);

	m_btn9->setCheckable(true);
	m_btn13->setCheckable(true);
	m_btn19->setCheckable(true);

	auto* group = new QButtonGroup(this);
	group->setExclusive(true);
	group->addButton(m_btn9, 9);
	group->addButton(m_btn13, 13);
	group->addButton(m_btn19, 19);

	m_btn13->setChecked(true);

	auto* buttonRow = new QHBoxLayout();
	buttonRow->addWidget(m_btn9);
	buttonRow->addWidget(m_btn13);
	buttonRow->addWidget(m_btn19);
	buttonRow->addStretch();

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	auto* mainLayout = new QVBoxLayout(this);
	mainLayout->addWidget(label);
	mainLayout->addLayout(buttonRow);
	mainLayout->addWidget(buttons);
}

unsigned HostDialog::boardSize() const {
	if (m_btn9->isChecked()) {
		return 9u;
	}
	if (m_btn19->isChecked()) {
		return 19u;
	}
	return 13u;
}

} // namespace go::gui

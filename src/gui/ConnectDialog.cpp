#include "ConnectDialog.hpp"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpressionValidator>
#include <QVBoxLayout>

namespace go::ui {

ConnectDialog::ConnectDialog(QWidget* parent) : QDialog(parent) {
	setWindowTitle("Connect to Server");

	// IP label + input field
	auto* formLayout = new QHBoxLayout;
	auto* ipLabel    = new QLabel(tr("IP:"), this);

	ipEdit = new QLineEdit(this);
	ipEdit->setPlaceholderText(tr("e.g. 192.168.1.100"));

	// Optional: basic IP validation
	QRegularExpression ipRegex(R"(^(25[0-5]|2[0-4]\d|[01]?\d\d?)\.)"
	                           R"((25[0-5]|2[0-4]\d|[01]?\d\d?)\.)"
	                           R"((25[0-5]|2[0-4]\d|[01]?\d\d?)\.)"
	                           R"((25[0-5]|2[0-4]\d|[01]?\d\d?)$)");
	ipEdit->setValidator(new QRegularExpressionValidator(ipRegex, this));

	formLayout->addWidget(ipLabel);
	formLayout->addWidget(ipEdit);

	// OK / Cancel buttons
	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	// --- Main layout ---
	auto* mainLayout = new QVBoxLayout(this);
	mainLayout->addLayout(formLayout);
	mainLayout->addWidget(buttons);
}

QString ConnectDialog::ipAddress() const {
	return ipEdit->text();
}

} // namespace go::ui

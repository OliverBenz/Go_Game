#pragma once

#include <QDialog>

class QLineEdit;

namespace go::ui {

class ConnectDialog : public QDialog {
	Q_OBJECT

public:
	explicit ConnectDialog(QWidget* parent = nullptr);

	QString ipAddress() const;

private:
	QLineEdit* ipEdit;
};

} // namespace go::ui

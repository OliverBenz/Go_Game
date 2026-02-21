#pragma once

#include <QDialog>

class QLineEdit;

namespace tengen::gui {

class ConnectDialog : public QDialog {
	Q_OBJECT

public:
	explicit ConnectDialog(QWidget* parent = nullptr);

	QString ipAddress() const;

private:
	QLineEdit* ipEdit;
};

} // namespace tengen::gui

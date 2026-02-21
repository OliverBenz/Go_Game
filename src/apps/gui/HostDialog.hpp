#pragma once

#include <QDialog>

class QPushButton;

namespace tengen::gui {

class HostDialog : public QDialog {
	Q_OBJECT

public:
	explicit HostDialog(QWidget* parent = nullptr);

	unsigned boardSize() const;

private:
	QPushButton* m_btn9;
	QPushButton* m_btn13;
	QPushButton* m_btn19;
};

} // namespace tengen::gui

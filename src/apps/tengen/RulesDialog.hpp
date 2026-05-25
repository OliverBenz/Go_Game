#pragma once

#include <QDialog>

namespace tengen::gui {

class RulesDialog : public QDialog {
	Q_OBJECT

public:
	explicit RulesDialog(QWidget* parent = nullptr);
};

} // namespace tengen::gui

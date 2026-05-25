#include "RulesDialog.hpp"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

namespace tengen::gui {

static QString rulesDocumentPath() {
	return QDir(QCoreApplication::applicationDirPath()).filePath("help/rules.html");
}

RulesDialog::RulesDialog(QWidget* parent) : QDialog(parent) {
	setWindowTitle(tr("Rules of Go"));
	resize(720, 600);

	auto* browser        = new QTextBrowser(this);
	const QString source = rulesDocumentPath();
	browser->setOpenExternalLinks(true); // We link to our sources.

	if (QFileInfo::exists(source)) {
		browser->setSource(QUrl::fromLocalFile(source));
	} else {
		browser->setHtml(tr("<h2>Rules unavailable</h2><p>Could not find <code>%1</code>.</p>").arg(source.toHtmlEscaped()));
	}

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	auto* mainLayout = new QVBoxLayout(this);
	mainLayout->addWidget(browser);
	mainLayout->addWidget(buttons);
}

} // namespace tengen::gui

#include "ChatPresenter.hpp"

#include "gui/chatWidget.hpp"

#include <QMetaObject>
#include <QObject>

#include <format>
#include <vector>

namespace tengen {

ChatPresenter::ChatPresenter(app::IChatSession& chat, gui::ChatWidget& chatWidget)
    : m_chat(chat), m_chatWidget(chatWidget) {
	QObject::connect(&m_chatWidget, &gui::ChatWidget::chatEvent, this, &ChatPresenter::onChatRequested);

	m_chat.subscribe(this, app::AS_NewChat);
	onAppEvent(app::AS_NewChat);
}

ChatPresenter::~ChatPresenter() {
	m_chat.unsubscribe(this);
}

void ChatPresenter::onChatRequested(const std::string& message) {
	m_chat.chat(message);
}

void ChatPresenter::onAppEvent(const app::AppSignal signal) {
	if (signal != app::AS_NewChat) {
		return;
	}

	const auto messageEntries = m_chat.getChatSince(m_lastChatMessageId);
	if (messageEntries.empty()) {
		return;
	}

	std::vector<std::string> lines;
	lines.reserve(messageEntries.size());
	for (const auto& entry: messageEntries) {
		lines.emplace_back(std::format("{}: {}", toString(entry.player), entry.message));
		m_lastChatMessageId = entry.messageId;
	}

	auto* widget = &m_chatWidget;
	QMetaObject::invokeMethod(
	        widget,
	        [widget, lines = std::move(lines)]() {
		        for (const auto& line: lines) {
			        widget->appendMessage(line);
		        }
	        },
	        Qt::QueuedConnection);
}

} // namespace tengen

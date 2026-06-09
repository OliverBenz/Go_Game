#pragma once

#include "engine/IEngineSession.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace tengen::engine {

//! Bootstrap data specific to a KataGo-backed engine session.
struct KataGoLaunchConfig {
	std::string executablePath{};
	std::string configPath{};
	std::string modelPath{};
	std::optional<std::string> humanModelPath{};
	std::vector<std::string> extraArgs{};
};

//! KataGo adapter using the GTP surface for interactive play.
//! The real subprocess transport is intentionally deferred; this class establishes the stable seam first.
class KataGoSession : public IEngineSession {
public:
	explicit KataGoSession(KataGoLaunchConfig launchConfig);
	~KataGoSession() override;

	KataGoSession(const KataGoSession&)            = delete;
	KataGoSession& operator=(const KataGoSession&) = delete;
	KataGoSession(KataGoSession&&)                 = delete;
	KataGoSession& operator=(KataGoSession&&)      = delete;

	bool newGame(const GameConfig& config) override;
	bool recordMove(const Move& move) override;
	std::optional<Decision> requestMove(Player player) override;

	SessionState state() const override;
	std::string lastError() const override;

	void shutdown() override;

private:
	class Implementation;
	std::unique_ptr<Implementation> m_pimpl;
};

} // namespace tengen::engine

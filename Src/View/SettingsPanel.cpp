#include "view/SettingsPanel.h"

#include <string>
#include <vector>

#include "game/Game.h"
#include "util/Logger.h"

namespace {
	constexpr float GEAR_SIZE{ 0.03f };
	constexpr float GEAR_PAD{ 0.012f };
	constexpr float PANEL_W{ 0.44f };
	constexpr float PANEL_H{ 0.30f };
	constexpr float ROW_H{ 0.052f };
	constexpr float ROW_GAP{ 0.012f };

	sf::Vector2f center(sf::FloatRect rect) {
		return { rect.left + rect.width / 2.f, rect.top + rect.height / 2.f };
	}
}

SettingsPanel::SettingsPanel(Window* window) : p_window{ window } {
	if (!m_font.loadFromFile("assets/Fonts/Minecraft.ttf")) {
		LOG(Level::ERROR) << "Failed to load font" << std::endl;
	}
}

sf::FloatRect SettingsPanel::gearBounds() const {
	sf::Vector2f size{ static_cast<float>(p_window->getSize().x), static_cast<float>(p_window->getSize().y) };
	float s = size.y * GEAR_SIZE;
	return { size.x - s - size.x * GEAR_PAD, size.y * GEAR_PAD, s, s };
}

sf::FloatRect SettingsPanel::panelRect() const {
	sf::Vector2f size{ static_cast<float>(p_window->getSize().x), static_cast<float>(p_window->getSize().y) };
	float w = size.x * PANEL_W;
	float h = size.y * PANEL_H;
	return { (size.x - w) / 2.f, (size.y - h) / 2.f, w, h };
}

sf::FloatRect SettingsPanel::panelBounds() const {
	return panelRect();
}

bool SettingsPanel::isOverButton(sf::Vector2f point) const {
	return gearBounds().contains(point);
}

void SettingsPanel::layoutRows(std::vector<Row>& rows) const {
	sf::FloatRect panel = panelRect();
	const float rowStep = panel.height * ROW_H + panel.height * ROW_GAP;
	float y = panel.top + panel.height * 0.22f;
	for (Row& row : rows) {
		row.bounds = { panel.left + panel.width * 0.12f, y, panel.width * 0.76f, panel.height * ROW_H };
		y += rowStep;
	}
}

void SettingsPanel::hover(sf::Vector2f point) {
	m_hoverGear = gearBounds().contains(point);
	m_hoverRow = -1;
	if (m_open) {
		std::vector<Row> rows{ { {}, "", false }, { {}, "", false } };
		layoutRows(rows);
		for (std::size_t i = 0; i < rows.size(); ++i) {
			if (rows[i].bounds.contains(point)) {
				m_hoverRow = static_cast<int>(i);
				break;
			}
		}
	}
}

bool SettingsPanel::handleClick(sf::Vector2f point, Game& game) {
	std::vector<Row> rows{ { {}, "Fly", game.getPlayer().isFlying() },
		{ {}, "AI player", game.isBotOnline() } };
	layoutRows(rows);

	for (const Row& row : rows) {
		if (!row.bounds.contains(point)) {
			continue;
		}
		if (row.label == "Fly") {
			game.toggleFlyFromSettings();
		} else if (row.label == "AI player") {
			game.toggleBotFromSettings();
		}
		return true;
	}

	// A click outside the panel closes it.
	if (m_open && !panelRect().contains(point)) {
		m_open = false;
		return true;
	}
	return false;
}

void SettingsPanel::draw(Game& game) {
	sf::FloatRect gear = gearBounds();
	sf::RectangleShape gearRect{ { gear.width, gear.height } };
	gearRect.setPosition({ gear.left, gear.top });
	gearRect.setFillColor(m_hoverGear ? sf::Color{ 46, 92, 138 } : sf::Color{ 32, 64, 96 });
	gearRect.setOutlineThickness(2.f);
	gearRect.setOutlineColor(sf::Color::White);
	p_window->draw(gearRect);

	sf::Text gearLabel;
	gearLabel.setFont(m_font);
	gearLabel.setString("Settings");
	gearLabel.setCharacterSize(static_cast<unsigned int>(gear.height * 0.5f));
	gearLabel.setFillColor(sf::Color::White);
	const sf::FloatRect gearText = gearLabel.getLocalBounds();
	gearLabel.setOrigin({ gearText.left + gearText.width / 2.f, gearText.top + gearText.height / 2.f });
	gearLabel.setPosition(center(gear));
	p_window->draw(gearLabel);

	if (!m_open)
		return;

	sf::FloatRect panel = panelRect();
	sf::RectangleShape bg{ { panel.width, panel.height } };
	bg.setPosition({ panel.left, panel.top });
	bg.setFillColor(sf::Color{ 16, 20, 26, 235 });
	bg.setOutlineThickness(2.f);
	bg.setOutlineColor(sf::Color{ 120, 150, 180 });
	p_window->draw(bg);

	sf::Text title;
	title.setFont(m_font);
	title.setString("Settings");
	title.setCharacterSize(static_cast<unsigned int>(panel.height * 0.1f));
	title.setFillColor(sf::Color::White);
	const sf::FloatRect titleBounds = title.getLocalBounds();
	title.setOrigin({ titleBounds.left + titleBounds.width / 2.f, titleBounds.top + titleBounds.height / 2.f });
	title.setPosition({ panel.left + panel.width / 2.f, panel.top + panel.height * 0.1f });
	p_window->draw(title);

	std::vector<Row> rows{ { {}, "Fly", game.getPlayer().isFlying() },
		{ {}, "AI player", game.isBotOnline() } };
	layoutRows(rows);

	for (std::size_t i = 0; i < rows.size(); ++i) {
		sf::FloatRect r = rows[i].bounds;
		bool on = rows[i].on;
		bool hover = m_open && static_cast<int>(i) == m_hoverRow;
		sf::RectangleShape rowRect{ { r.width, r.height } };
		rowRect.setPosition({ r.left, r.top });
		rowRect.setFillColor(hover ? sf::Color{ 40, 60, 84 } : sf::Color{ 26, 32, 42 });
		rowRect.setOutlineThickness(1.f);
		rowRect.setOutlineColor(sf::Color{ 90, 110, 130 });
		p_window->draw(rowRect);

		sf::Text label;
		label.setFont(m_font);
		label.setString(rows[i].label);
		label.setCharacterSize(static_cast<unsigned int>(r.height * 0.5f));
		label.setFillColor(sf::Color::White);
		label.setPosition({ r.left + r.height * 0.3f, r.top + (r.height - label.getLocalBounds().height) / 2.f });
		p_window->draw(label);

		sf::Text state;
		state.setFont(m_font);
		state.setString(on ? "on" : "off");
		state.setCharacterSize(static_cast<unsigned int>(r.height * 0.5f));
		state.setFillColor(on ? sf::Color{ 110, 220, 120 } : sf::Color{ 220, 110, 110 });
		const sf::FloatRect stateBounds = state.getLocalBounds();
		state.setPosition({ r.left + r.width - stateBounds.width - r.height * 0.3f,
			r.top + (r.height - stateBounds.height) / 2.f });
		p_window->draw(state);
	}

	sf::Text hint;
	hint.setFont(m_font);
	hint.setString("AI player listens on port 8765. Say @bot in chat to command it.");
	hint.setCharacterSize(static_cast<unsigned int>(panel.height * 0.05f));
	hint.setFillColor(sf::Color{ 150, 160, 175 });
	const sf::FloatRect hintBounds = hint.getLocalBounds();
	hint.setOrigin({ hintBounds.left + hintBounds.width / 2.f, hintBounds.top + hintBounds.height / 2.f });
	hint.setPosition({ panel.left + panel.width / 2.f, panel.top + panel.height * 0.87f });
	p_window->draw(hint);
}
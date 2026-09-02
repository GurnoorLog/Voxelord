#include "MainMenu.h"

#include <cctype>

#include "Util/Logger.h"

MainMenu::MainMenu(Window* window) : p_window{ window } {
	if (!m_font.loadFromFile("Data/Fonts/arial.ttf")) {
		LOG(Level::ERROR) << "Failed to load font" << std::endl;
	}
	m_title.setFont(m_font);
	m_title.setString("VoxLord");
	m_title.setFillColor(sf::Color::White);

	m_buttons = {
		{ "Create World", MenuAction::CREATE_WORLD, true, {} },
		{ "Join", MenuAction::JOIN, true, {} },
		{ "Quit", MenuAction::QUIT, true, {} },
	};
}

MenuAction MainMenu::handleEvent(const sf::Event& event) {
	switch (event.type) {
	case sf::Event::MouseMoved: {
		m_hovered = buttonAt({ static_cast<float>(event.mouseMove.x), static_cast<float>(event.mouseMove.y) });
		return MenuAction::NONE;
	}
	case sf::Event::MouseButtonPressed:
		if (event.mouseButton.button == sf::Mouse::Left) {
			int index = buttonAt({ static_cast<float>(event.mouseButton.x), static_cast<float>(event.mouseButton.y) });
			if (index >= 0 && m_buttons[index].enabled) {
				if (m_buttons[index].action == MenuAction::JOIN && !m_joinMode) {
					// First click opens the address box; a second click confirms.
					m_joinMode = true;
					m_addressActive = true;
					return MenuAction::NONE;
				}
				return m_buttons[index].action;
			}
			// Clicking the address box keeps it focused.
			if (m_joinMode && m_addressBounds.contains({ static_cast<float>(event.mouseButton.x), static_cast<float>(event.mouseButton.y) })) {
				m_addressActive = true;
			}
		}
		return MenuAction::NONE;
	case sf::Event::KeyPressed:
		if (!m_joinMode)
			return MenuAction::NONE;
		if (event.key.code == sf::Keyboard::Return) {
			m_joinMode = false;
			m_addressActive = false;
			return MenuAction::JOIN;
		}
		if (event.key.code == sf::Keyboard::Escape) {
			m_joinMode = false;
			m_addressActive = false;
		}
		return MenuAction::NONE;
	case sf::Event::TextEntered:
		if (!m_joinMode || !m_addressActive)
			return MenuAction::NONE;
		if (event.text.unicode == 8) { // Backspace
			if (!m_address.empty())
				m_address.pop_back();
		} else if (event.text.unicode < 128) {
			char c = static_cast<char>(event.text.unicode);
			if (std::isprint(static_cast<unsigned char>(c)) && m_address.size() < 48)
				m_address.push_back(c);
		}
		return MenuAction::NONE;
	default:
		return MenuAction::NONE;
	}
}

void MainMenu::draw() {
	layout();

	// Title, sized to the window and centered above the buttons.
	sf::Vector2f size{ static_cast<float>(p_window->getSize().x), static_cast<float>(p_window->getSize().y) };
	m_title.setCharacterSize(static_cast<unsigned int>(size.y * 0.12f));
	const sf::FloatRect titleBounds = m_title.getLocalBounds();
	m_title.setOrigin({ titleBounds.left + titleBounds.width / 2.f, titleBounds.top + titleBounds.height / 2.f });
	m_title.setPosition({ size.x / 2.f, size.y * 0.34f });
	p_window->draw(m_title);

	for (std::size_t i = 0; i < m_buttons.size(); ++i) {
		const Button& button = m_buttons[i];

		sf::RectangleShape rect{ { button.bounds.width, button.bounds.height } };
		rect.setPosition({ button.bounds.left, button.bounds.top });
		rect.setOutlineThickness(2.f);
		if (button.enabled && static_cast<int>(i) == m_hovered) {
			rect.setFillColor(sf::Color{ 46, 92, 138 });
			rect.setOutlineColor(sf::Color::White);
		} else if (button.enabled) {
			rect.setFillColor(sf::Color{ 32, 64, 96 });
			rect.setOutlineColor(sf::Color{ 120, 150, 180 });
		} else {
			rect.setFillColor(sf::Color{ 28, 28, 34 });
			rect.setOutlineColor(sf::Color{ 70, 70, 80 });
		}
		p_window->draw(rect);

		sf::Text label;
		label.setFont(m_font);
		label.setString(button.label);
		label.setCharacterSize(static_cast<unsigned int>(button.bounds.height * 0.55f));
		label.setFillColor(button.enabled ? sf::Color::White : sf::Color{ 110, 110, 120 });
		const sf::FloatRect textBounds = label.getLocalBounds();
		label.setOrigin({ textBounds.left + textBounds.width / 2.f, textBounds.top + textBounds.height / 2.f });
		label.setPosition({ button.bounds.left + button.bounds.width / 2.f,
			button.bounds.top + button.bounds.height / 2.f });
		p_window->draw(label);
	}

	if (m_joinMode) {
		sf::RectangleShape addrRect{ { m_addressBounds.width, m_addressBounds.height } };
		addrRect.setPosition({ m_addressBounds.left, m_addressBounds.top });
		addrRect.setFillColor(sf::Color{ 12, 14, 18 });
		addrRect.setOutlineThickness(2.f);
		addrRect.setOutlineColor(m_addressActive ? sf::Color::White : sf::Color{ 90, 90, 100 });
		p_window->draw(addrRect);

		sf::Text addrText;
		addrText.setFont(m_font);
		addrText.setString(m_address + (m_addressActive ? "_" : ""));
		addrText.setCharacterSize(static_cast<unsigned int>(m_addressBounds.height * 0.55f));
		addrText.setFillColor(sf::Color::White);
		const sf::FloatRect addrBounds = addrText.getLocalBounds();
		addrText.setOrigin({ 0.f, addrBounds.top + addrBounds.height / 2.f });
		addrText.setPosition({ m_addressBounds.left + 10.f, m_addressBounds.top + m_addressBounds.height / 2.f });
		p_window->draw(addrText);

		sf::Text hint;
		hint.setFont(m_font);
		hint.setString("Address (host or host:port) - Enter to join, Esc to cancel");
		hint.setCharacterSize(static_cast<unsigned int>(m_addressBounds.height * 0.35f));
		hint.setFillColor(sf::Color{ 150, 160, 175 });
		hint.setPosition({ m_addressBounds.left, m_addressBounds.top + m_addressBounds.height + 5.f });
		p_window->draw(hint);
	}
}

sf::Vector2f MainMenu::buttonSize() const {
	sf::Vector2f size{ static_cast<float>(p_window->getSize().x), static_cast<float>(p_window->getSize().y) };
	return { size.x * 0.22f, size.y * 0.065f };
}

void MainMenu::layout() {
	sf::Vector2f size{ static_cast<float>(p_window->getSize().x), static_cast<float>(p_window->getSize().y) };
	const sf::Vector2f dims = buttonSize();
	const float spacing = size.y * 0.014f;
	const float totalHeight = dims.y * static_cast<float>(m_buttons.size()) + spacing * static_cast<float>(m_buttons.size() - 1);
	float y = (size.y * 0.50f) - totalHeight / 2.f;
	for (Button& button : m_buttons) {
		button.bounds = { (size.x - dims.x) / 2.f, y, dims.x, dims.y };
		y += dims.y + spacing;
	}

	if (!m_buttons.empty()) {
		const Button& last = m_buttons.back();
		m_addressBounds = {
			(size.x - dims.x) / 2.f, last.bounds.top + last.bounds.height + size.y * 0.03f,
			dims.x, size.y * 0.05f
		};
	}
}

int MainMenu::buttonAt(sf::Vector2f point) const {
	for (std::size_t i = 0; i < m_buttons.size(); ++i) {
		if (m_buttons[i].bounds.contains(point))
			return static_cast<int>(i);
	}
	return -1;
}
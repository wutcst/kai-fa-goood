#pragma once

#include <SFML/Graphics.hpp>
#include <string>

namespace fireice {

class UiHelper {
public:
    bool loadFont();

    void drawPanel(sf::RenderWindow& window, const sf::FloatRect& area, sf::Color fill, float alpha = 200.0f) const;
    void drawText(sf::RenderWindow& window, const std::string& text, float x, float y, unsigned size,
                  sf::Color color = sf::Color::White) const;
    void drawCenteredText(sf::RenderWindow& window, const std::string& text, float centerX, float y, unsigned size,
                          sf::Color color = sf::Color::White) const;
    void drawButton(sf::RenderWindow& window, const sf::FloatRect& area, const std::string& label,
                    bool highlighted, sf::Color accent) const;

    bool hasFont() const { return fontLoaded_; }

private:
    mutable sf::Font font_;
    bool fontLoaded_ = false;
};

} // namespace fireice

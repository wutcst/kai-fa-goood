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
    void drawButton(sf::RenderWindow& window, const sf::FloatRect& area, const std::string& label, bool highlighted,
                    sf::Color accent) const;
    void drawImageButton(sf::RenderWindow& window, const sf::FloatRect& area, const sf::Texture& texture,
                         bool enabled = true, bool highlighted = true) const;
    void drawImageButtonWithHint(sf::RenderWindow& window, const sf::FloatRect& area, const sf::Texture& texture,
                                 const std::string& hint, bool enabled = true, bool highlighted = true) const;
    void drawOutlinedText(sf::RenderWindow& window, const std::string& text, float x, float y, unsigned size,
                          sf::Color fill, sf::Color outline, float outlineThickness = 2.0f) const;
    void drawOutlinedCenteredText(sf::RenderWindow& window, const std::string& text, float centerX, float y,
                                  unsigned size, sf::Color fill, sf::Color outline,
                                  float outlineThickness = 2.0f) const;
    void drawMultilineText(sf::RenderWindow& window, const std::string& text, float x, float y, unsigned size,
                           sf::Color color, float lineSpacing = 6.0f) const;
    float drawWrappedText(sf::RenderWindow& window, const std::string& text, float x, float y, unsigned size,
                          sf::Color color, float maxWidth, float lineSpacing = 4.0f) const;
    void drawTitleMenuItem(sf::RenderWindow& window, const std::string& text, float centerX, float y, unsigned size,
                           bool selected) const;

    bool hasFont() const { return fontLoaded_; }

private:
    mutable sf::Font font_;
    bool fontLoaded_ = false;
};

}  // namespace fireice

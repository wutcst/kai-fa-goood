#include "UiHelper.hpp"

namespace fireice {

namespace {

const char* kFontCandidates[] = {
    "C:/Windows/Fonts/msyh.ttc",
    "C:/Windows/Fonts/msyhbd.ttc",
    "C:/Windows/Fonts/simhei.ttf",
    "C:/Windows/Fonts/arial.ttf",
};

} // namespace

bool UiHelper::loadFont() {
    for (const char* path : kFontCandidates) {
        if (font_.loadFromFile(path)) {
            fontLoaded_ = true;
            return true;
        }
    }
    fontLoaded_ = false;
    return false;
}

void UiHelper::drawPanel(sf::RenderWindow& window, const sf::FloatRect& area, sf::Color fill, float alpha) const {
    sf::RectangleShape panel({area.width, area.height});
    panel.setPosition(area.left, area.top);
    fill.a = static_cast<sf::Uint8>(alpha);
    panel.setFillColor(fill);
    panel.setOutlineThickness(2.0f);
    panel.setOutlineColor(sf::Color(255, 255, 255, 60));
    window.draw(panel);
}

void UiHelper::drawText(sf::RenderWindow& window, const std::string& text, float x, float y, unsigned size,
                        sf::Color color) const {
    if (!fontLoaded_) {
        sf::RectangleShape placeholder({static_cast<float>(text.size() * 8), static_cast<float>(size)});
        placeholder.setPosition(x, y);
        placeholder.setFillColor(color);
        window.draw(placeholder);
        return;
    }

    sf::Text label(text, font_, size);
    label.setFillColor(color);
    label.setPosition(x, y);
    window.draw(label);
}

void UiHelper::drawCenteredText(sf::RenderWindow& window, const std::string& text, float centerX, float y,
                                unsigned size, sf::Color color) const {
    if (!fontLoaded_) {
        drawText(window, text, centerX - static_cast<float>(text.size() * 4), y, size, color);
        return;
    }

    sf::Text label(text, font_, size);
    label.setFillColor(color);
    const sf::FloatRect bounds = label.getLocalBounds();
    label.setOrigin(bounds.left + bounds.width / 2.0f, bounds.top);
    label.setPosition(centerX, y);
    window.draw(label);
}

void UiHelper::drawButton(sf::RenderWindow& window, const sf::FloatRect& area, const std::string& label,
                          bool highlighted, sf::Color accent) const {
    sf::RectangleShape button({area.width, area.height});
    button.setPosition(area.left, area.top);
    button.setFillColor(highlighted ? accent : sf::Color(accent.r / 2, accent.g / 2, accent.b / 2, 220));
    button.setOutlineThickness(2.0f);
    button.setOutlineColor(sf::Color::White);
    window.draw(button);

    drawCenteredText(window, label, area.left + area.width / 2.0f, area.top + area.height / 2.0f - 14.0f, 22,
                     sf::Color::White);
}

} // namespace fireice

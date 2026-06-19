#include "UiHelper.hpp"

#include <algorithm>

namespace fireice {

namespace {

sf::String toSfString(const std::string& text) {
    return sf::String::fromUtf8(text.begin(), text.end());
}

const char* kFontCandidates[] = {
    "C:/Windows/Fonts/msyh.ttc",
    "C:/Windows/Fonts/msyhbd.ttc",
    "C:/Windows/Fonts/simsun.ttc",
    "C:/Windows/Fonts/simhei.ttf",
};

}  // namespace

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
    sf::RectangleShape shadow({area.width, area.height});
    shadow.setPosition(area.left + 5.0f, area.top + 7.0f);
    shadow.setFillColor(sf::Color(0, 0, 0, 70));
    window.draw(shadow);

    sf::RectangleShape panel({area.width, area.height});
    panel.setPosition(area.left, area.top);
    fill.a = static_cast<sf::Uint8>(alpha);
    panel.setFillColor(fill);
    panel.setOutlineThickness(2.0f);
    panel.setOutlineColor(sf::Color(255, 244, 205, 95));
    window.draw(panel);

    sf::RectangleShape highlight({std::max(0.0f, area.width - 8.0f), 2.0f});
    highlight.setPosition(area.left + 4.0f, area.top + 4.0f);
    highlight.setFillColor(sf::Color(255, 255, 255, 42));
    window.draw(highlight);
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

    sf::Text label(toSfString(text), font_, size);
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

    sf::Text label(toSfString(text), font_, size);
    label.setFillColor(color);
    const sf::FloatRect bounds = label.getLocalBounds();
    label.setOrigin(bounds.left + bounds.width / 2.0f, bounds.top);
    label.setPosition(centerX, y);
    window.draw(label);
}

void UiHelper::drawButton(sf::RenderWindow& window, const sf::FloatRect& area, const std::string& label,
                          bool highlighted, sf::Color accent) const {
    const sf::Color base = highlighted ? accent : sf::Color(accent.r / 3, accent.g / 3, accent.b / 3, 210);
    const sf::Color top(std::min(255, static_cast<int>(base.r) + 36), std::min(255, static_cast<int>(base.g) + 36),
                        std::min(255, static_cast<int>(base.b) + 36), highlighted ? 255 : 210);
    const sf::Color bottom(static_cast<sf::Uint8>(base.r * 0.70f), static_cast<sf::Uint8>(base.g * 0.70f),
                           static_cast<sf::Uint8>(base.b * 0.70f), highlighted ? 255 : 210);

    sf::RectangleShape shadow({area.width, area.height});
    shadow.setPosition(area.left + 4.0f, area.top + 5.0f);
    shadow.setFillColor(sf::Color(0, 0, 0, highlighted ? 95 : 55));
    window.draw(shadow);

    sf::RectangleShape button({area.width, area.height});
    button.setPosition(area.left, area.top);
    button.setFillColor(bottom);
    button.setOutlineThickness(highlighted ? 3.0f : 2.0f);
    button.setOutlineColor(highlighted ? sf::Color(255, 245, 190) : sf::Color(150, 150, 150, 130));
    window.draw(button);

    sf::RectangleShape upper({area.width - 6.0f, area.height * 0.48f});
    upper.setPosition(area.left + 3.0f, area.top + 3.0f);
    upper.setFillColor(top);
    window.draw(upper);

    sf::RectangleShape shine({area.width - 18.0f, 3.0f});
    shine.setPosition(area.left + 9.0f, area.top + 7.0f);
    shine.setFillColor(sf::Color(255, 255, 255, highlighted ? 80 : 35));
    window.draw(shine);

    drawOutlinedCenteredText(window, label, area.left + area.width / 2.0f, area.top + area.height / 2.0f - 15.0f,
                             highlighted ? 22 : 21, highlighted ? sf::Color::White : sf::Color(205, 210, 215),
                             sf::Color(30, 35, 42), highlighted ? 1.8f : 1.2f);
}
void UiHelper::drawImageButton(sf::RenderWindow& window, const sf::FloatRect& area, const sf::Texture& texture,
                               bool enabled, bool highlighted) const {
    if (texture.getSize().x == 0 || texture.getSize().y == 0) {
        return;
    }

    sf::RectangleShape shadow({area.width, area.height});
    shadow.setPosition(area.left + 4.0f, area.top + 5.0f);
    shadow.setFillColor(sf::Color(0, 0, 0, enabled ? 80 : 45));
    window.draw(shadow);

    sf::RectangleShape frame({area.width, area.height});
    frame.setPosition(area.left, area.top);
    frame.setFillColor(enabled ? sf::Color(42, 50, 62, 205) : sf::Color(36, 36, 42, 160));
    frame.setOutlineThickness(highlighted && enabled ? 3.0f : 2.0f);
    frame.setOutlineColor(highlighted && enabled ? sf::Color(255, 230, 135, 220) : sf::Color(120, 130, 145, 130));
    window.draw(frame);

    sf::RectangleShape sheen({area.width - 12.0f, 3.0f});
    sheen.setPosition(area.left + 6.0f, area.top + 6.0f);
    sheen.setFillColor(sf::Color(255, 255, 255, highlighted && enabled ? 70 : 30));
    window.draw(sheen);

    sf::Sprite sprite(texture);
    const float scale = std::min((area.width - 16.0f) / static_cast<float>(texture.getSize().x),
                                 (area.height - 12.0f) / static_cast<float>(texture.getSize().y));
    const float drawW = static_cast<float>(texture.getSize().x) * scale;
    const float drawH = static_cast<float>(texture.getSize().y) * scale;
    sprite.setScale(scale, scale);
    sprite.setPosition(area.left + (area.width - drawW) / 2.0f, area.top + (area.height - drawH) / 2.0f);

    if (!enabled) {
        sprite.setColor(sf::Color(120, 120, 120, 165));
    } else if (highlighted) {
        sprite.setColor(sf::Color::White);
    } else {
        sprite.setColor(sf::Color(210, 215, 220, 230));
    }

    window.draw(sprite);
}
void UiHelper::drawImageButtonWithHint(sf::RenderWindow& window, const sf::FloatRect& area, const sf::Texture& texture,
                                       const std::string& hint, bool enabled, bool highlighted) const {
    drawImageButton(window, area, texture, enabled, highlighted);
    if (!hint.empty()) {
        drawCenteredText(window, hint, area.left + area.width / 2.0f, area.top + area.height + 4.0f, 14,
                         enabled ? sf::Color(230, 230, 230) : sf::Color(150, 150, 150));
    }
}

void UiHelper::drawOutlinedText(sf::RenderWindow& window, const std::string& text, float x, float y, unsigned size,
                                sf::Color fill, sf::Color outline, float outlineThickness) const {
    if (!fontLoaded_) {
        drawText(window, text, x, y, size, fill);
        return;
    }

    sf::Text label(toSfString(text), font_, size);
    label.setFillColor(fill);
    label.setOutlineColor(outline);
    label.setOutlineThickness(outlineThickness);
    label.setPosition(x, y);
    window.draw(label);
}

void UiHelper::drawOutlinedCenteredText(sf::RenderWindow& window, const std::string& text, float centerX, float y,
                                        unsigned size, sf::Color fill, sf::Color outline,
                                        float outlineThickness) const {
    if (!fontLoaded_) {
        drawCenteredText(window, text, centerX, y, size, fill);
        return;
    }

    sf::Text label(toSfString(text), font_, size);
    label.setFillColor(fill);
    label.setOutlineColor(outline);
    label.setOutlineThickness(outlineThickness);
    const sf::FloatRect bounds = label.getLocalBounds();
    label.setOrigin(bounds.left + bounds.width / 2.0f, bounds.top);
    label.setPosition(centerX, y);
    window.draw(label);
}

void UiHelper::drawTitleMenuItem(sf::RenderWindow& window, const std::string& text, float centerX, float y,
                                 unsigned size, bool selected) const {
    const sf::Color fill = selected ? sf::Color(255, 245, 120) : sf::Color(255, 220, 80);
    const sf::Color outline = selected ? sf::Color(120, 60, 10) : sf::Color(80, 40, 10);
    const float thickness = selected ? 3.5f : 2.5f;
    const unsigned drawSize = selected ? size + 4 : size;
    drawOutlinedCenteredText(window, text, centerX, y, drawSize, fill, outline, thickness);

    if (selected) {
        sf::RectangleShape cursor({18.0f, 4.0f});
        cursor.setFillColor(sf::Color(255, 245, 120));
        float textHalfWidth = static_cast<float>(text.size() * 9);
        float midY = y + 18.0f;
        if (fontLoaded_) {
            sf::Text measure(toSfString(text), font_, drawSize);
            const sf::FloatRect bounds = measure.getLocalBounds();
            textHalfWidth = bounds.width / 2.0f;
            midY = y + bounds.height / 2.0f;
        }
        cursor.setOrigin(0.0f, 2.0f);
        cursor.setPosition(centerX - textHalfWidth - 28.0f, midY);
        window.draw(cursor);
        cursor.setPosition(centerX + textHalfWidth + 10.0f, midY);
        window.draw(cursor);
    }
}

}  // namespace fireice

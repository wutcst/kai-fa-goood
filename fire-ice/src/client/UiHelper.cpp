#include "UiHelper.hpp"

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
    sf::RectangleShape button({area.width, area.height});
    button.setPosition(area.left, area.top);
    button.setFillColor(highlighted ? accent : sf::Color(accent.r / 2, accent.g / 2, accent.b / 2, 220));
    button.setOutlineThickness(2.0f);
    button.setOutlineColor(sf::Color::White);
    window.draw(button);

    drawCenteredText(window, label, area.left + area.width / 2.0f, area.top + area.height / 2.0f - 14.0f, 22,
                     sf::Color::White);
}

void UiHelper::drawImageButton(sf::RenderWindow& window, const sf::FloatRect& area, const sf::Texture& texture,
                               bool enabled, bool highlighted) const {
    if (texture.getSize().x == 0 || texture.getSize().y == 0) {
        return;
    }

    sf::Sprite sprite(texture);
    const float scale = std::min(area.width / static_cast<float>(texture.getSize().x),
                                 area.height / static_cast<float>(texture.getSize().y));
    const float drawW = static_cast<float>(texture.getSize().x) * scale;
    const float drawH = static_cast<float>(texture.getSize().y) * scale;
    sprite.setScale(scale, scale);
    sprite.setPosition(area.left + (area.width - drawW) / 2.0f, area.top + (area.height - drawH) / 2.0f);

    if (!enabled) {
        sprite.setColor(sf::Color(130, 130, 130, 180));
    } else if (highlighted) {
        sprite.setColor(sf::Color::White);
    } else {
        sprite.setColor(sf::Color(200, 200, 200, 220));
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

void UiHelper::drawMultilineText(sf::RenderWindow& window, const std::string& text, float x, float y, unsigned size,
                                 sf::Color color, float lineSpacing) const {
    float curY = y;
    std::string line;
    for (char ch : text) {
        if (ch == '\n') {
            if (!line.empty()) {
                drawText(window, line, x, curY, size, color);
                curY += static_cast<float>(size) + lineSpacing;
                line.clear();
            }
            continue;
        }
        line.push_back(ch);
    }
    if (!line.empty()) {
        drawText(window, line, x, curY, size, color);
    }
}

float UiHelper::drawWrappedText(sf::RenderWindow& window, const std::string& text, float x, float y, unsigned size,
                                sf::Color color, float maxWidth, float lineSpacing) const {
    if (text.empty()) {
        return y;
    }

    if (!fontLoaded_) {
        drawText(window, text, x, y, size, color);
        return y + static_cast<float>(size) + lineSpacing;
    }

    auto measureLine = [&](const std::string& line) {
        sf::Text probe(toSfString(line), font_, size);
        return probe.getLocalBounds().width;
    };

    float curY = y;
    std::string current;
    for (std::size_t i = 0; i < text.size();) {
        const unsigned char lead = static_cast<unsigned char>(text[i]);
        std::size_t charLen = 1;
        if (lead >= 0xF0) {
            charLen = 4;
        } else if (lead >= 0xE0) {
            charLen = 3;
        } else if (lead >= 0xC0) {
            charLen = 2;
        }

        const std::string piece = text.substr(i, charLen);
        i += charLen;

        if (piece == "\n") {
            drawText(window, current, x, curY, size, color);
            curY += static_cast<float>(size) + lineSpacing;
            current.clear();
            continue;
        }

        const std::string candidate = current + piece;
        if (!current.empty() && measureLine(candidate) > maxWidth) {
            drawText(window, current, x, curY, size, color);
            curY += static_cast<float>(size) + lineSpacing;
            current = piece;
        } else {
            current = candidate;
        }
    }

    if (!current.empty()) {
        drawText(window, current, x, curY, size, color);
        curY += static_cast<float>(size) + lineSpacing;
    }

    return curY;
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

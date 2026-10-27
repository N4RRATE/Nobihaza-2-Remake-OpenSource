#include "local_text_fixtures.h"
#include "dialogue.h"
#include "dialogue_renderer.h"
#include "input_prompt.h"
#include "scene_renderer.h"
#include <SFML/Graphics/RenderTexture.hpp>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}

void reveal(Dialogue& dialogue) {
    for (int step = 0; step < 512 && dialogue.view().phase != DialoguePhase::AwaitingInput; ++step) {
        dialogue.update(false, false);
    }
    require(dialogue.view().phase == DialoguePhase::AwaitingInput, "Message did not finish typing");
}

void checkExFontModel() {
    MessageDefinition literal;
    literal.lines = {{"", {{MessageTokenKind::Text, decodeMessageText(localSourceText("g9_presentation_checks.0")), 0}}}};
    Dialogue oldPackage;
    oldPackage.request(literal);
    reveal(oldPackage);
    require(oldPackage.view().lines.front() == decodeMessageText(localSourceText("g9_presentation_checks.1")) && oldPackage.view().exfontImage.empty() && oldPackage.view().exfontIndices.empty(),
            "Old Text tokens must keep the old package contract");

    MessageDefinition typed;
    typed.exfontImage = "g9-message-exfont";
    typed.lines = {{"", {{MessageTokenKind::Color, {}, 3}, {MessageTokenKind::Text, decodeMessageText(localSourceText("g9_presentation_checks.2")), 0},
                        {MessageTokenKind::ExFont, {}, 32}, {MessageTokenKind::Text, decodeMessageText(localSourceText("g9_presentation_checks.3")), 0}}}};
    for (int speed : {1, 2}) {
        auto bitmapMessage = typed;
        bitmapMessage.lines.front().tokens.insert(bitmapMessage.lines.front().tokens.begin(), {MessageTokenKind::Speed, {}, speed});
        auto textMessage = bitmapMessage;
        textMessage.exfontImage.clear();
        textMessage.lines.front().tokens[3] = {MessageTokenKind::Text, decodeMessageText(localSourceText("g9_presentation_checks.4")), 0};
        Dialogue bitmap, text;
        const auto request = bitmap.request(bitmapMessage);
        text.request(textMessage);
        for (int step = 0; step < 80; ++step) {
            bitmap.update(false,false);
            text.update(false,false);
            require(bitmap.view().phase == text.view().phase && bitmap.view().lines.front().size() == text.view().lines.front().size(),
                    "An ExFont glyph must retain one full-width character's timing");
        }
        require(bitmap.view().lines.front() == decodeMessageText(localSourceText("g9_presentation_checks.5")) && bitmap.view().exfontIndices.front() == std::vector<int>{-1,32,-1},
                "Explicit ExFont metadata must retain glyph order without a Unicode substitute");
        require(bitmap.view().colors.front() == std::vector<int>{3,3,3}, "ExFont lost the current source text color");
        require(bitmap.cancel(request), "ExFont message must remain cancellable");
        bitmap.update(false,false);
        require(bitmap.view().exfontIndices.front().empty(), "Closing retained stale ExFont glyphs");
        bitmap.invalidate();
        require(bitmap.view().exfontImage.empty() && bitmap.view().exfontIndices.empty(), "New scene retained an ExFont binding");
    }
    for (int index : {-1, 52}) {
        auto invalid = typed;
        invalid.lines.front().tokens[2].value = index;
        bool rejected = false;
        try { Dialogue candidate; candidate.request(invalid); } catch (const std::runtime_error&) { rejected = true; }
        require(rejected, "Out-of-range ExFont index was accepted");
    }
    for (int index : {0, 51}) {
        auto boundary = typed;
        boundary.lines.front().tokens[2].value = index;
        Dialogue candidate;
        candidate.request(boundary);
        reveal(candidate);
        require(candidate.view().exfontIndices.front()[1] == index, "Valid first/last atlas glyph was rejected");
    }
    auto malformed = typed;
    malformed.lines.front().tokens[2].text = decodeMessageText(localSourceText("g9_presentation_checks.6"));
    bool malformedRejected = false;
    try { Dialogue candidate; candidate.request(malformed); } catch (const std::runtime_error&) { malformedRejected = true; }
    require(malformedRejected, "An ExFont token carrying unrelated literal text was accepted");
    typed.exfontImage.clear();
    bool rejected = false;
    try { Dialogue candidate; candidate.request(typed); } catch (const std::runtime_error&) { rejected = true; }
    require(rejected, "Explicit ExFont token without an atlas binding was accepted");
}

void checkExFontPixels(const sf::Font& font) {
    // Independently inspected CN EXFONT index32, not a normal-font heart character.
    const std::string rows[] = {
        "............", "..##...##...", ".#..#.#..#..", "#....#....#.",
        "#.........#.", "#.........#.", ".#.......#..", "..#.....#...",
        "...#...#....", "....#.#.....", ".....#......", "............"
    };
    sf::Image mask({156,48},sf::Color::Transparent);
    for (unsigned y = 0; y < 12; ++y) {
        for (unsigned x = 0; x < 12; ++x) {
            if (rows[y][x] == '#') { mask.setPixel({72+x,24+y},sf::Color::White); }
        }
    }
    std::map<std::string,sf::Texture> images;
    require(images["g9-message-exfont"].loadFromImage(mask), "Cannot upload original ExFont mask fixture");
    sf::Image palette({160,80},sf::Color::White);
    const sf::Color textColor(225,70,125), background(18,39,61);
    palette.setPixel({56,55},textColor);
    DialogueRenderer renderer(font);
    renderer.setMedia(images,palette);
    MessageDefinition message;
    message.display = {true, 2, true, false};
    message.exfontImage = "g9-message-exfont";
    message.lines = {{"", {{MessageTokenKind::Color, {}, 3}, {MessageTokenKind::ExFont, {}, 32}}}};
    Dialogue dialogue;
    dialogue.request(message);
    reveal(dialogue);
    for (unsigned scale : {1u, 3u}) {
        sf::RenderTexture target({320*scale,240*scale});
        auto view = makeSceneView(target.getSize(), {320,240});
        view.setCenter({620,520});
        target.setView(view);
        target.clear(background);
        renderer.draw(target,dialogue.view());
        require(target.getView().getCenter() == view.getCenter(), "ExFont moved the world camera");
        target.display();
        const auto pixels = target.getTexture().copyToImage();
        for (unsigned y = 0; y < 13; ++y) {
            for (unsigned x = 0; x < 13; ++x) {
                const bool ink = x < 12 && y < 12 && rows[y][x] == '#';
                const bool shadow = x > 0 && y > 0 && rows[y-1][x-1] == '#';
                const auto expected = ink ? textColor : shadow ? sf::Color::Black : background;
                require(pixels.getPixel({(8+x)*scale,(170+y)*scale}) == expected,
                        "ExFont pixels differ from the original mask, source color or 12px position");
            }
        }
        auto malformed = dialogue.view();
        malformed.exfontImage = "absent";
        bool rejected = false;
        try { renderer.draw(target,malformed); } catch (const std::runtime_error&) { rejected = true; }
        require(rejected && target.getView().getCenter() == view.getCenter(), "Missing atlas changed the active draw target");
        malformed = dialogue.view();
        malformed.exfontIndices.front().front() = 52;
        rejected = false;
        try { renderer.draw(target,malformed); } catch (const std::runtime_error&) { rejected = true; }
        require(rejected, "Renderer accepted an invalid ExFont glyph index");
        ChoicePrompt question;
        question.display = message.display;
        question.question = dialogue.view();
        question.options = {{"",{{MessageTokenKind::Text,decodeMessageText(localSourceText("g9_presentation_checks.7")),0}}}, {"",{{MessageTokenKind::Text,decodeMessageText(localSourceText("g9_presentation_checks.8")),0}}}};
        InputPrompt prompt;
        prompt.request(question,1);
        renderer.draw(target,prompt.view());
        require(prompt.view().lines.size() == 3 && prompt.view().exfontIndices.front().front() == 32,
                "Appending choice rows lost the existing question's bitmap glyph");
    }
    require(images.at("g9-message-exfont").loadFromImage(sf::Image({12,12},sf::Color::White)), "Cannot prepare wrong atlas fixture");
    sf::RenderTexture target({320,240});
    bool rejected = false;
    try { renderer.draw(target,dialogue.view()); } catch (const std::runtime_error&) { rejected = true; }
    require(rejected, "Renderer accepted an atlas with the wrong dimensions");
}

void render(const std::filesystem::path& fontPath, const std::filesystem::path& output, const DialogueView& source) {
    sf::Font font;
    require(font.openFromFile(fontPath), "Cannot load reviewed font");
    checkExFontPixels(font);
    DialogueRenderer renderer(font);
    DialogueView message;
    message.phase = DialoguePhase::AwaitingInput;
    message.display = {true, 1, true, false};
    message.lines = {std::u32string(50, U'.') + decodeMessageText(localSourceText("g9_presentation_checks.9"))};
    std::filesystem::create_directories(output);
    for (const auto size : {sf::Vector2u{960,720}, sf::Vector2u{1000,600}, sf::Vector2u{400,360}}) {
        sf::RenderTexture target(size);
        auto view = makeSceneView(size, {320,240});
        view.setCenter({620,520});
        target.setView(view);
        const auto viewport = target.getViewport(view);
        const float scale = viewport.size.y / 240.f;
        const int left = viewport.position.x + static_cast<int>(8*scale);
        const int right = left + static_cast<int>(304*scale);
        const int top = viewport.position.y + static_cast<int>((80+8)*scale);
        const int bottom = top + static_cast<int>(64*scale);
        const sf::Color background(18,39,61);
        target.clear(background);
        renderer.draw(target, message);
        require(target.getView().getCenter() == view.getCenter(), "Dialogue changed the world camera");
        target.display();
        const auto image = target.getTexture().copyToImage();
        int visible = 0;
        for (unsigned y = 0; y < size.y; ++y) {
            for (unsigned x = 0; x < size.x; ++x) {
                if (image.getPixel({x,y}) == background) { continue; }
                ++visible;
                require(int(x) >= left && int(x) < right && int(y) >= top && int(y) < bottom,
                        "Dialogue pixels escaped the original contents bitmap");
            }
        }
        require(visible > 0, "Clipped dialogue lost all text");
        require(image.saveToFile(output / (std::to_string(size.x) + "x" + std::to_string(size.y) + ".png")),
                "Cannot save dialogue evidence");
        view.setCenter({160,120});
        target.setView(view);
        target.clear(background);
        renderer.draw(target,message);
        target.display();
        const auto stationary = target.getTexture().copyToImage();
        for (unsigned y = 0; y < size.y; ++y) {
            for (unsigned x = 0; x < size.x; ++x) {
                require(image.getPixel({x,y}) == stationary.getPixel({x,y}), "Dialogue moved with the world camera");
            }
        }
        target.clear(background);
        renderer.draw(target,source);
        target.display();
        require(target.getTexture().copyToImage().saveToFile(output / ("ce159-" + std::to_string(size.x) + "x" + std::to_string(size.y) + ".png")),
                "Cannot save original CE159 reading");
    }
}
}

int main(int argc, char** argv) {
    try {
        checkExFontModel();
        // CN1.00 CE159 C30-33, I187 reading; copied from the actual G7 package without editing text.
        const std::u32string reading[] = {
            decodeMessageText(localSourceText("g9_presentation_checks.10")),
            decodeMessageText(localSourceText("g9_presentation_checks.11")),
            decodeMessageText(localSourceText("g9_presentation_checks.12")),
            decodeMessageText(localSourceText("g9_presentation_checks.13"))
        };
        MessageDefinition message;
        message.display = {true, 1, true, false};
        for (const auto& text : reading) {
            message.lines.push_back({"", {{MessageTokenKind::InstantLine, {}, 0}, {MessageTokenKind::Text, text, 0}}});
        }
        Dialogue dialogue;
        const auto first = dialogue.request(message);
        reveal(dialogue);
        const auto sourceView = dialogue.view();
        require(dialogue.view().lines.size() == 4, "Reading must preserve its four source lines");
        for (std::size_t line = 0; line < 4; ++line) {
            require(dialogue.view().lines[line] == reading[line], "Reading lost original text or punctuation");
        }
        require(dialogue.view().display == message.display, "Reading lost its centered transparent display");
        require(dialogue.cancel(first), "Reading must remain cancellable");
        for (int step = 0; step < 7; ++step) { dialogue.update(false, false); }
        const auto cancelled = dialogue.takeCompletion();
        require(cancelled && cancelled->result == MessageResult::Cancelled, "Reading cancellation did not complete");
        const auto second = dialogue.request(message);
        require(second != first && !dialogue.cancel(first), "Repeated reading reused a stale request");
        dialogue.invalidate();
        require(!dialogue.active() && !dialogue.takeCompletion(), "Scene invalidation retained an old reading");

        MessageDefinition ordinary;
        ordinary.lines = {{"", {{MessageTokenKind::Text, std::u32string(25, U'国'), 0}}}};
        dialogue.request(ordinary);
        reveal(dialogue);
        require(dialogue.view().lines.front() == std::u32string(25, U'国'), "Ordinary 50-cell Chinese line regressed");
        dialogue.invalidate();
        ordinary.lines.front().tokens.front().text = std::u32string(50, U'.') + decodeMessageText(localSourceText("g9_presentation_checks.14"));
        dialogue.request(ordinary);
        reveal(dialogue);
        require(dialogue.view().lines.front().back() == U'国', "Final partially clipped full-width glyph was discarded");
        dialogue.invalidate();
        ordinary.lines.front().tokens.front().text += decodeMessageText(localSourceText("g9_presentation_checks.15"));
        bool offscreenRejected = false;
        try { dialogue.request(ordinary); } catch (const std::runtime_error&) { offscreenRejected = true; }
        require(offscreenRejected, "A fully offscreen following glyph must be rejected");
        ordinary.lines.front().tokens.front().text = std::u32string(100, U'国');
        bool rejected = false;
        try { dialogue.request(ordinary); } catch (const std::runtime_error&) { rejected = true; }
        require(rejected, "An unbounded line must still be rejected");
        require(argc == 3, "Expected font and evidence directory");
        render(argv[1], argv[2], sourceView);
        std::cout << "G9 presentation dialogue checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

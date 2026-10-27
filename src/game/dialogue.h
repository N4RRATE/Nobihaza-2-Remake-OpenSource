#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct MessageDisplay{
    bool transparent{};
    int position{2}; // top / middle / bottom
    bool fixedPosition{};
    bool continueEvents{};
    bool operator==(const MessageDisplay&) const = default;
};
enum class MessageTokenKind{ Text, InstantLine, Speed, ShortWait, LongWait, Color, InstantOff, AutoClose, WaitInput, LiteralSlash, Variable, ExFont };
struct MessageToken{
    MessageTokenKind kind{};
    std::u32string text;
    int value{};
};
struct MessageLine{
    std::string raw;
    std::vector<MessageToken> tokens;
};
struct MessageDefinition{
    MessageDisplay display{false,2,true,false};
    MessageDisplay restoreDisplay;
    std::vector<MessageLine> lines;
    int shortWaitUpdates{16};
    std::string faceImage;
    int faceIndex{};
    bool faceRight{};
    bool faceFlipped{};
    std::string exfontImage;
};
std::u32string decodeUiText(const std::string& utf8);
std::u32string decodeMessageText(const std::string& utf8);
int messageCharacterWidth(char32_t value);
using MessageRequestId = std::uint64_t;
enum class MessageResult{ Completed, Cancelled };
struct MessageCompletion{ MessageRequestId request; MessageResult result; };
enum class DialoguePhase{ Hidden, Opening, Typing, AwaitingInput, Closing, Cooldown };
struct DialogueCursor{int line{};int column{};int width{1};bool numeric{};int firstChoiceLine{};};
struct DialogueView{
    DialoguePhase phase{DialoguePhase::Hidden};
    MessageDisplay display;
    std::vector<std::u32string> lines;
    MessageRequestId request{};
    std::optional<DialogueCursor> cursor;
    std::vector<std::vector<int>> colors;
    std::string faceImage;
    int faceIndex{};
    bool faceRight{};
    bool faceFlipped{};
    std::string exfontImage;
    // Parallel to lines: -1 is ordinary text, otherwise a 12px bitmap glyph index.
    std::vector<std::vector<int>> exfontIndices;
    unsigned animationFrame{};
    int visibleHeight{80};
    bool showPause{};
};

class Dialogue{
public:
    Dialogue() = default;
    Dialogue(const Dialogue&) = delete;
    Dialogue& operator=(const Dialogue&) = delete;
    MessageRequestId request(const MessageDefinition& message);
    void update(bool confirmPressed, bool cancelPressed);
    bool cancel(MessageRequestId request);
    void invalidate();
    bool active() const;
    const DialogueView& view() const;
    std::optional<MessageCompletion> takeCompletion();
private:
    struct Glyph{ int update; std::size_t line; char32_t value; int color; int exfontIndex{-1}; };
    void beginClose(MessageResult result);
    DialogueView view_;
    std::vector<Glyph> glyphs_;
    std::size_t nextGlyph_{};
    bool autoClose_{};bool paused_{};std::vector<int> pauses_;std::size_t nextPause_{};
    int readyUpdate_{};
    int textUpdate_{};
    int transitionUpdates_{};
    MessageResult result_{MessageResult::Completed};
    std::optional<MessageCompletion> completion_;
};

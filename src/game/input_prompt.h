#pragma once
#include "dialogue.h"
#include "input_buffer.h"
#include <variant>

struct PromptIdentity{
    std::uint64_t scene{};
    std::uint64_t request{};
    bool operator==(const PromptIdentity&) const=default;
};
struct ChoicePrompt{
    std::vector<MessageLine> options;
    int cancelType{}; // source: 0 disabled, 1..4 chooses that option, 5 separate branch
    MessageDisplay display{false,2,true,false};
    std::vector<bool> enabled; // empty means all options enabled
    std::optional<DialogueView> question; // completed text shares the four-line choice window
};
struct NumberPrompt{
    int digits{4};
    int initial{};
    MessageDisplay display{false,2,true,false};
};
struct ChoiceSelected{int index{};bool viaCancel{};};
struct ChoiceCancelled{};
struct NumberSubmitted{int value{};};
using PromptResult=std::variant<ChoiceSelected,ChoiceCancelled,NumberSubmitted>;
struct PromptCompletion{PromptIdentity identity;PromptResult result;};
enum class PromptFeedback{None,Cursor,Decision,Cancel};

// UI state only. The requesting flow validates identity and owns gameplay commits.
class InputPrompt{
public:
    InputPrompt()=default;
    InputPrompt(const InputPrompt&)=delete;
    InputPrompt& operator=(const InputPrompt&)=delete;
    PromptIdentity request(const ChoicePrompt& definition,std::uint64_t scene);
    PromptIdentity request(const NumberPrompt& definition,std::uint64_t scene);
    PromptFeedback update(const InputFrame& input);
    void invalidate(std::uint64_t scene);
    bool active() const{return view_.phase!=DialoguePhase::Hidden;}
    const DialogueView& view() const{return view_;}
    std::optional<PromptCompletion> takeCompletion(PromptIdentity expected);
private:
    void begin(DialogueView view,std::uint64_t scene);
    void complete(PromptResult result);
    void refreshNumber();
    DialogueView view_;
    PromptIdentity identity_;
    std::optional<PromptCompletion> completion_;
    std::vector<bool> enabled_;
    bool numeric_{};
    int selection_{};
    int cancelType_{};
    int choiceOffset_{},choiceCount_{};
    int digits_{};
    int number_{};
};

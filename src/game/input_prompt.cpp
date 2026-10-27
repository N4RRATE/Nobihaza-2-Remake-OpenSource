#include "input_prompt.h"
#include <atomic>
#include <algorithm>
#include <stdexcept>
#include <utility>
namespace{
    bool pressed(const InputFrame& input,InputAction action){return input.buttons[static_cast<std::size_t>(action)].pressed;}
    void validateDisplay(const MessageDisplay& display){
        if(display.position<0||display.position>2||!display.fixedPosition){throw std::runtime_error("Unsupported prompt display");}
    }
}
void InputPrompt::begin(DialogueView view,std::uint64_t scene){
    if(active()||completion_){throw std::runtime_error("Prompt already active or result unconsumed");}
    if(!scene){throw std::runtime_error("Prompt needs scene identity");}
    static std::atomic<std::uint64_t> next{1};identity_={scene,next.fetch_add(1)};
    view.phase=DialoguePhase::AwaitingInput;view.request=identity_.request;view.showPause=false;view.visibleHeight=80;view.animationFrame=0;view_=std::move(view);
}
PromptIdentity InputPrompt::request(const ChoicePrompt& definition,std::uint64_t scene){
    const int count=static_cast<int>(definition.options.size());
    if(count<1||count>4||definition.cancelType<0||definition.cancelType>5||
       (definition.cancelType>count&&definition.cancelType!=5)){throw std::runtime_error("Invalid choice definition");}
    if(!definition.enabled.empty()&&definition.enabled.size()!=definition.options.size()){throw std::runtime_error("Invalid choice availability");}
    validateDisplay(definition.display);DialogueView view=definition.question.value_or(DialogueView{});view.display=definition.display;
    if(view.lines.size()+definition.options.size()>4||view.lines.size()!=view.colors.size()){throw std::runtime_error("Choice question exceeds message window");}
    const int offset=static_cast<int>(view.lines.size());
    int widest=1;
    for(const auto& option:definition.options){
        std::u32string text;std::vector<int> colors;int color=0,width=0;
        for(const auto& token:option.tokens){
            if(token.kind==MessageTokenKind::Color&&token.value>=0&&token.value<20){color=token.value;}
            else if(token.kind==MessageTokenKind::Text){
                for(auto glyph:token.text){width+=messageCharacterWidth(glyph);text+=glyph;colors.push_back(color);}
            }else{throw std::runtime_error("Unsupported choice label token");}
        }
        if(text.empty()||width>50){throw std::runtime_error("Invalid choice label length");}
        widest=std::max(widest,width);view.lines.push_back(std::move(text));view.colors.push_back(std::move(colors));
    }
    view.cursor=DialogueCursor{offset,0,widest,false,offset};begin(std::move(view),scene);
    enabled_=definition.enabled;choiceOffset_=offset;choiceCount_=count;
    numeric_=false;selection_=0;cancelType_=definition.cancelType;return identity_;
}
PromptIdentity InputPrompt::request(const NumberPrompt& definition,std::uint64_t scene){
    // F3 needs four digits; bounded 1..6 supports known RPG2000 unsigned requests.
    if(definition.digits<1||definition.digits>6){throw std::runtime_error("Unsupported numeric digit count");}
    int limit=1;for(int i=0;i<definition.digits;++i){limit*=10;}
    if(definition.initial<0||definition.initial>=limit){throw std::runtime_error("Numeric initial value outside digits");}
    validateDisplay(definition.display);DialogueView view;view.display=definition.display;
    begin(std::move(view),scene);numeric_=true;digits_=definition.digits;number_=definition.initial;
    selection_=digits_-1;refreshNumber();return identity_;
}
void InputPrompt::refreshNumber(){
    auto text=std::to_string(number_);text=std::string(digits_-text.size(),'0')+text;
    view_.lines={decodeMessageText(text)};view_.colors={std::vector<int>(digits_,0)};
    view_.cursor=DialogueCursor{0,selection_,1,true};
}
void InputPrompt::complete(PromptResult result){
    completion_=PromptCompletion{identity_,std::move(result)};view_={};
}
PromptFeedback InputPrompt::update(const InputFrame& input){
    if(!active()){return PromptFeedback::None;}
    view_.animationFrame=(view_.animationFrame+1)%840;
    // Cancellation wins over confirmation for choices, as in the original UI.
    if(!numeric_&&pressed(input,InputAction::Cancel)){
        if(cancelType_==0){return PromptFeedback::None;}
        if(cancelType_==5){complete(ChoiceCancelled{});}else{complete(ChoiceSelected{cancelType_-1,true});}
        return PromptFeedback::Cancel;
    }
    if(pressed(input,InputAction::Confirm)){
        if(numeric_){complete(NumberSubmitted{number_});}else{if(!enabled_.empty()&&!enabled_.at(selection_)){return PromptFeedback::None;}complete(ChoiceSelected{selection_,false});}
        return PromptFeedback::Decision;
    }
    bool changed=false;
    if(numeric_){
        if(pressed(input,InputAction::Up)||pressed(input,InputAction::Down)){
            int place=1;for(int i=selection_+1;i<digits_;++i){place*=10;}
            const int digit=number_/place%10;int next=digit;
            if(pressed(input,InputAction::Up)){next=(next+1)%10;}
            if(pressed(input,InputAction::Down)){next=(next+9)%10;}
            number_+=(next-digit)*place;changed=true;
        }
        if(pressed(input,InputAction::Right)){selection_=(selection_+1)%digits_;changed=true;}
        if(pressed(input,InputAction::Left)){selection_=(selection_+digits_-1)%digits_;changed=true;}
        refreshNumber();
    }else{
        const int count=choiceCount_;
        if(pressed(input,InputAction::Down)){selection_=(selection_+1)%count;changed=true;}
        if(pressed(input,InputAction::Up)){selection_=(selection_+count-1)%count;changed=true;}
        view_.cursor->line=choiceOffset_+selection_;
    }
    return changed?PromptFeedback::Cursor:PromptFeedback::None;
}
void InputPrompt::invalidate(std::uint64_t scene){
    if(identity_.scene!=scene){return;}view_={};completion_.reset();identity_={};
}
std::optional<PromptCompletion> InputPrompt::takeCompletion(PromptIdentity expected){
    if(!completion_||completion_->identity!=expected){return std::nullopt;}
    return std::exchange(completion_,std::nullopt);
}

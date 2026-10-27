#include "input_prompt.h"
#include <iostream>
#include <stdexcept>
void require(bool value,const char* reason){if(!value){throw std::runtime_error(reason);}}
InputFrame key(InputAction a){InputFrame f;f.buttons[static_cast<int>(a)].pressed=true;return f;}
MessageLine label(const std::string& text){return {text,{{MessageTokenKind::Text,decodeMessageText(text),0}}};}
int main(){
    InputPrompt prompt;
    const auto id=prompt.request(NumberPrompt{4,0},31);
    require(prompt.view().lines.at(0)==U"0000","leading zeroes are visible");
    require(prompt.view().cursor->column==3,"numeric input starts at units");
    prompt.update(key(InputAction::Cancel));require(prompt.active()&&!prompt.takeCompletion(id),"X never submits or cancels number input");
    for(int i=0;i<4;++i){prompt.update(key(InputAction::Down));prompt.update(key(InputAction::Left));}
    require(prompt.view().lines.at(0)==U"9999","each digit wraps independently");
    prompt.update(key(InputAction::Confirm));prompt.update(key(InputAction::Confirm));
    auto done=prompt.takeCompletion(id);require(done&&std::get<NumberSubmitted>(done->result).value==9999,"one confirmation commits once");
    require(!prompt.takeCompletion(id),"completion consumed once");
    auto next=prompt.request(NumberPrompt{4,0},32);prompt.update(key(InputAction::Confirm));
    require(!prompt.takeCompletion(id)&&prompt.takeCompletion(next),"old identity cannot consume new result");
    next=prompt.request(NumberPrompt{1,9},32);prompt.update(key(InputAction::Up));prompt.update(key(InputAction::Confirm));
    require(std::get<NumberSubmitted>(prompt.takeCompletion(next)->result).value==0,"single digit 9 wraps to zero");
    ChoicePrompt choice{{label("保留"),label("丢弃")},2};
    next=prompt.request(choice,32);prompt.update(key(InputAction::Cancel));
    require(std::get<ChoiceSelected>(prompt.takeCompletion(next)->result).index==1,"source cancel type 2 chooses second branch");
    choice.cancelType=0;next=prompt.request(choice,32);prompt.update(key(InputAction::Cancel));require(prompt.active(),"cancel forbidden");
    prompt.update(key(InputAction::Down));prompt.update(key(InputAction::Confirm));require(std::get<ChoiceSelected>(prompt.takeCompletion(next)->result).index==1,"choice navigation confirms selection");
    choice.cancelType=5;next=prompt.request(choice,32);prompt.update(key(InputAction::Cancel));require(std::holds_alternative<ChoiceCancelled>(prompt.takeCompletion(next)->result),"separate cancel branch is typed");
    next=prompt.request(choice,33);prompt.update(key(InputAction::Confirm));prompt.invalidate(33);require(!prompt.takeCompletion(next),"scene cancellation invalidates queued completion");
    next=prompt.request(choice,34);prompt.invalidate(33);require(prompt.active(),"stale scene cannot cancel new request");prompt.invalidate(34);require(!prompt.active(),"owner can invalidate active request");
    bool failed=false;try{prompt.request(NumberPrompt{0,0},34);}catch(const std::runtime_error&){failed=true;}require(failed&&!prompt.active(),"invalid definition preserves idle model");
    choice.display.continueEvents=true;next=prompt.request(choice,35);
    require(prompt.active()&&prompt.view().display.continueEvents,"Source choice may continue event motion without accepting player movement");prompt.invalidate(35);
    std::cout<<"prompt request/result checks passed\n";
}

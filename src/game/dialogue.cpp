#include "dialogue.h"
#include <atomic>
#include <algorithm>
#include <stdexcept>
#include <utility>

MessageRequestId Dialogue::request(const MessageDefinition& message){
    if(active() || completion_){ throw std::runtime_error("Dialogue request already active or unconsumed"); }
    if(message.lines.empty() || message.lines.size() > 4 || message.shortWaitUpdates != 16 ||
       message.display.position < 0 || message.display.position > 2){
        throw std::runtime_error("Invalid message page or timing");
    }
    if(!message.display.fixedPosition){
        throw std::runtime_error("Unsupported message display: resolved fixed position required");
    }
    std::vector<Glyph> prepared;autoClose_=false;paused_=false;pauses_.clear();nextPause_=0;
    int tick = 1;
    int speed = 1;
    int color = 0;
    for(std::size_t line=0;line<message.lines.size();++line){
        bool instant = false;
        bool printable = false;
        int halfCount = 0;
        const auto& tokens = message.lines[line].tokens;
        int lineWidth = 0;
        if(tokens.size() > 1024){throw std::runtime_error("Too many message tokens");}
        for(std::size_t t=0;t<tokens.size();++t){
            const auto& token = tokens[t];
            if(token.kind != MessageTokenKind::Text && token.kind != MessageTokenKind::InstantLine &&
               token.kind != MessageTokenKind::Speed && token.kind != MessageTokenKind::ShortWait &&
               token.kind != MessageTokenKind::LongWait && token.kind != MessageTokenKind::Color && token.kind != MessageTokenKind::InstantOff && token.kind != MessageTokenKind::AutoClose && token.kind != MessageTokenKind::WaitInput && token.kind != MessageTokenKind::LiteralSlash && token.kind != MessageTokenKind::ExFont){
                throw std::runtime_error("Unsupported message token");
            }
            if(token.kind == MessageTokenKind::Speed && (token.value < 0 || token.value > 20)){
                throw std::runtime_error("Unsupported message speed");
            }
            const bool exfont = token.kind == MessageTokenKind::ExFont;
            if(exfont && (token.value < 0 || token.value >= 52 || !token.text.empty() || message.exfontImage.empty())){
                throw std::runtime_error("Invalid ExFont glyph or missing atlas binding");
            }
            if(token.kind==MessageTokenKind::AutoClose){autoClose_=true;continue;}
            if(token.kind==MessageTokenKind::WaitInput){pauses_.push_back(tick++);continue;}
            if(token.kind==MessageTokenKind::LiteralSlash){
                if(lineWidth * 6 >= 320 - 16){throw std::runtime_error("Message line exceeds logical width");}
                prepared.push_back({tick++,line,U'\\',color});++lineWidth;continue;
            }
            if(token.kind == MessageTokenKind::Text || exfont){
                const auto count = exfont ? std::size_t{1} : token.text.size();
                for(std::size_t i=0;i<count;++i){
                    const auto c = exfont ? U' ' : token.text[i];
                    const int width = exfont ? 2 : messageCharacterWidth(c);
                    // The original 304px contents bitmap clips a final partial glyph, including CE159 C33.
                    if(lineWidth * 6 >= 320 - 16){throw std::runtime_error("Message line exceeds logical width");}
                    lineWidth += width;
                    prepared.push_back({tick,line,c,color,exfont ? token.value : -1});
                    const bool lastInLine = i+1 == count && t+1 == tokens.size();
                    if(!instant){
                        if(lastInLine && line+1 == message.lines.size()){ tick += 2; }
                        else if(speed > 1){ tick += speed * width / 2 + 1; }
                        else{ tick += width / 2 + ((width % 2) && ((halfCount % 2) || lastInLine)); }
                    }
                    halfCount = width == 1 && speed == 1 ? halfCount+1 : (halfCount/2+1)*2;
                    printable = true;
                }
            }else{
                if(!instant){
                    tick += token.kind == MessageTokenKind::ShortWait ? message.shortWaitUpdates+std::clamp(speed-16,0,4) : (token.kind == MessageTokenKind::LongWait ? 61 : 0);
                    if(speed == 1){tick += halfCount % 2;}
                }
                if(halfCount > 0){halfCount = speed == 1 ? halfCount+1 : (halfCount/2+1)*2;}
                printable = false;
                if(token.kind == MessageTokenKind::InstantLine){instant = true;}
                if(token.kind == MessageTokenKind::InstantOff){instant = false;}
                if(token.kind == MessageTokenKind::Color){
                    if(token.value<0||token.value>19){throw std::runtime_error("Invalid palette color");}
                    color=token.value;
                }
                if(token.kind == MessageTokenKind::Speed){speed = std::clamp(token.value,1,20);}
            }
        }
        const bool last = line+1 == message.lines.size();
        tick += instant ? (last ? 2 : 0) : (!printable ? (last ? 2 : 1) : 0);
    }
    static std::atomic<MessageRequestId> serial{1};
    view_ = {};view_.phase=DialoguePhase::Opening;view_.display=message.display;view_.request=serial.fetch_add(1);
    view_.lines.resize(message.lines.size());
    view_.colors.resize(message.lines.size());
    view_.exfontImage=message.exfontImage;
    if(!message.exfontImage.empty()){view_.exfontIndices.resize(message.lines.size());}
    view_.faceImage=message.faceImage; view_.faceIndex=message.faceIndex;
    view_.faceRight=message.faceRight; view_.faceFlipped=message.faceFlipped;
    glyphs_ = std::move(prepared); nextGlyph_ = 0; textUpdate_ = 0; transitionUpdates_ = 7;
    view_.visibleHeight=0;
    readyUpdate_ = tick;
    return view_.request;
}
void Dialogue::update(bool confirmPressed,bool cancelPressed){
    view_.animationFrame=(view_.animationFrame+1)%840;
    view_.showPause=view_.phase==DialoguePhase::AwaitingInput||paused_;
    if(view_.phase == DialoguePhase::Closing){
        view_.visibleHeight=(transitionUpdates_-1)*80/6;
        if(--transitionUpdates_ == 0){view_.phase = DialoguePhase::Cooldown;}
        return;
    }
    if(view_.phase == DialoguePhase::Cooldown){
        completion_ = MessageCompletion{view_.request,result_};
        view_.phase = DialoguePhase::Hidden; return;
    }
    if(view_.phase == DialoguePhase::AwaitingInput){
        if(confirmPressed || cancelPressed){beginClose(MessageResult::Completed);}
        return;
    }
    if(view_.phase == DialoguePhase::Opening){
        view_.visibleHeight=(8-transitionUpdates_)*80/7;
        if(--transitionUpdates_ > 0){return;}
        view_.phase = DialoguePhase::Typing;
    }
    if(view_.phase == DialoguePhase::Typing){
        if(paused_){if(!confirmPressed&&!cancelPressed){return;}paused_=false;view_.showPause=false;return;}
        ++textUpdate_;
        while(nextGlyph_ < glyphs_.size() && glyphs_[nextGlyph_].update <= textUpdate_){
            const auto& glyph = glyphs_[nextGlyph_++]; view_.lines[glyph.line] += glyph.value;
            view_.colors[glyph.line].push_back(glyph.color);
            if(!view_.exfontIndices.empty()){view_.exfontIndices[glyph.line].push_back(glyph.exfontIndex);}
        }
        if(nextPause_<pauses_.size()&&textUpdate_>=pauses_[nextPause_]){++nextPause_;paused_=true;view_.showPause=true;view_.animationFrame=0;return;}
        if(textUpdate_ >= readyUpdate_){
            if(autoClose_){beginClose(MessageResult::Completed);return;}
            view_.phase = DialoguePhase::AwaitingInput;
            view_.showPause=true;
            view_.animationFrame=0;
            if(confirmPressed || cancelPressed){beginClose(MessageResult::Completed);}
        }
    }
}
void Dialogue::beginClose(MessageResult result){
    result_ = result; view_.phase = DialoguePhase::Closing; transitionUpdates_ = 6;
    view_.showPause=false;
    for(auto& line : view_.lines){line.clear();}
    for(auto& line : view_.colors){line.clear();}
    for(auto& line : view_.exfontIndices){line.clear();}
}
bool Dialogue::cancel(MessageRequestId request){
    if(!active() || view_.request != request || view_.phase == DialoguePhase::Closing ||
       view_.phase == DialoguePhase::Cooldown){return false;}
    beginClose(MessageResult::Cancelled); return true;
}
bool Dialogue::active() const{return view_.phase != DialoguePhase::Hidden;}
const DialogueView& Dialogue::view() const{return view_;}
std::optional<MessageCompletion> Dialogue::takeCompletion(){return std::exchange(completion_,{});}

int messageCharacterWidth(char32_t value){
    // Fixed logical text cells, independent of high-resolution font rasterization.
    return value < 0x100 ? 1 : 2;
}

namespace{
std::u32string decodeUtf8(const std::string& utf8,bool message){
    std::u32string result;
    for(std::size_t i=0;i<utf8.size();){
        const auto first = static_cast<unsigned char>(utf8[i++]);
        char32_t code{}; int extra{};
        if(first < 0x80){code = first;}
        else if(first >= 0xc2 && first <= 0xdf){code = first & 31;extra = 1;}
        else if(first >= 0xe0 && first <= 0xef){code = first & 15;extra = 2;}
        else if(first >= 0xf0 && first <= 0xf4){code = first & 7;extra = 3;}
        else{throw std::runtime_error("Invalid UTF-8 message text");}
        for(int j=0;j<extra;++j){
            if(i == utf8.size()){throw std::runtime_error("Truncated UTF-8 message text");}
            const auto byte = static_cast<unsigned char>(utf8[i++]);
            if((byte & 0xc0) != 0x80){throw std::runtime_error("Invalid UTF-8 continuation");}
            code = (code << 6) | (byte & 63);
        }
        if((extra == 1 && code < 0x80) || (extra == 2 && code < 0x800) ||
           (extra == 3 && code < 0x10000) || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)){
            throw std::runtime_error("Invalid UTF-8 code point");
        }
        if(message&&(code < 32 || code == 127 || code == U'\\')){
            throw std::runtime_error("Unsupported literal message control character");
        }
        result += !message&&(code<32||code==127)?U' ':code;
    }
    return result;
}

}
std::u32string decodeMessageText(const std::string& utf8){return decodeUtf8(utf8,true);}
std::u32string decodeUiText(const std::string& utf8){return decodeUtf8(utf8,false);}
void Dialogue::invalidate(){view_=DialogueView{};glyphs_.clear();completion_.reset();nextGlyph_=0;readyUpdate_=textUpdate_=transitionUpdates_=0;}

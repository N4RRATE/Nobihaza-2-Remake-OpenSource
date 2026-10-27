#include "menu_controller.h"
#include <algorithm>
#include <utility>
namespace{
bool pressed(const InputFrame& i,InputAction a){return i.buttons[static_cast<std::size_t>(a)].pressed;}
std::string term(const CombatContent& c,int id){return c.media.at("terms").at(std::to_string(id)).at("display");}
SoundDefinition ammunitionSound(const CombatContent& c,int form){
    const auto& binding=c.catalog.at("weapon_bindings").at(std::to_string(form));
    const auto& reload=c.actionProfiles.at(binding.at("reload").get<std::string>());
    for(const auto& id:reload.at("cue_order")){
        const auto& cue=c.cue(id.get<std::string>());
        if(cue.at("kind")=="forced_route"){
            for(const auto& step:cue.at("steps")){if(step.at("kind")=="sound"){return readSound(step.at("sound"));}}
        }
    }
    throw std::runtime_error("Missing ammunition change sound");
}
}
MenuController::MenuController(const CombatContent& c,GameSession& s,std::optional<ToolUseRequest> tool,std::optional<PromptIdentity> mix):MenuController(c,s,tool?std::vector<ToolUseRequest>{*tool}:std::vector<ToolUseRequest>{},mix){}
MenuController::MenuController(const CombatContent& c,GameSession& s,std::vector<ToolUseRequest> tools,std::optional<PromptIdentity> mix):content_(c),session_(s),tools_(std::move(tools)),mix_(mix){session_.organizeAmmo();}
void MenuController::enter(MenuPage p,int selection){page_=p;selection_=selection;notice_.clear();}
void MenuController::notice(std::string text){previous_=page_;notice_=std::move(text);page_=MenuPage::Notice;sound_="buzzer";}
void MenuController::returnFromReading(){session_.organizeAmmo();enter(MenuPage::Root);}
MenuView MenuController::view() const{
    MenuView v;v.page=page_;v.selection=selection_;v.notice=notice_;
    const auto itemName=[&](int id){return id?content_.items.at(id).name:std::string("－");};
    switch(page_){
    case MenuPage::Root:
        v.rows={{0,term(content_,106)},{1,term(content_,107)},{2,term(content_,108)},{3,term(content_,110),false},{4,term(content_,112)}};break;
    case MenuPage::Items:
        v.title=term(content_,106);v.columns=2;
        for(const auto& [id,d]:content_.items){if(session_.count(id)>0){
            bool contextual=!content_.expansion.tool.is_null()&&id==content_.expansion.tool.at("active_item");
            if(content_.catalog.contains("context_tools")){for(const auto& tool:content_.catalog.at("context_tools")){contextual|=id==tool.at("usable_item");}}
            v.rows.push_back({id,d.name+" : "+std::to_string(session_.count(id)),session_.canSelectItem(id)&&(!contextual||std::any_of(tools_.begin(),tools_.end(),[&](const auto& tool){return tool.item==id;}))});
        }}
        if(!v.rows.empty()){v.description=content_.items.at(v.rows.at(std::min(selection_,static_cast<int>(v.rows.size())-1)).id).description;}break;
    case MenuPage::Skills:
        v.title=term(content_,107);
        for(int id:session_.skills()){const auto& d=content_.skills.at(id);v.rows.push_back({id,d.name+"  "+std::to_string(d.tpCost),session_.skillUsable(d)&&d.type==3&&(content_.expansion.mixing.is_null()||id!=content_.expansion.mixing.at("enabled_skill")||mix_.has_value())});}
        if(!v.rows.empty()){v.description=content_.skills.at(v.rows.at(std::min(selection_,static_cast<int>(v.rows.size())-1)).id).description;}break;
    case MenuPage::Slots:
        v.title=term(content_,108);
        for(const auto& slot:content_.catalog.at("menu").at("slots")){const int n=slot.at("slot");v.rows.push_back({n,term(content_,slot.at("term"))+"  "+itemName(session_.equipped(n))});}break;
    case MenuPage::Equipment:
        v.columns=2;v.title=term(content_,135+slot_);v.rows.push_back({0,"卸下"});
        for(const auto& [id,d]:content_.items){if(d.equipmentType==slot_&&session_.count(id)>0){v.rows.push_back({id,d.name,session_.canEquip(id,slot_)});}}
        if(selection_<static_cast<int>(v.rows.size())&&v.rows[selection_].id){v.description=content_.items.at(v.rows[selection_].id).description;}break;
    case MenuPage::TitleConfirm:
        v.title=term(content_,151);v.rows={{0,term(content_,152)},{1,term(content_,153)}};break;
    case MenuPage::Notice:v.title="开发范围";break;
    case MenuPage::Target:v.title=content_.items.at(item_).name;v.rows={{item_,content_.media.at("ui").at("menu").at("actor_name")}};break;
    }
    if(page_==MenuPage::Slots||page_==MenuPage::Equipment){
        v.currentStats=session_.equipmentStats();
        for(const auto& slot:content_.catalog.at("menu").at("slots")){const int n=slot.at("slot");v.equipmentSlots.push_back({n,term(content_,slot.at("term"))+"  "+itemName(session_.equipped(n))});}
        if(page_==MenuPage::Equipment&&selection_<static_cast<int>(v.rows.size())){v.previewStats=session_.equipmentStats(v.rows[selection_].id,slot_);}
    }
    return v;
}
std::optional<MenuResult> MenuController::takeResult(){return std::exchange(result_,std::nullopt);}
std::string MenuController::takeSound(){return std::exchange(sound_,{});}
std::optional<SoundDefinition> MenuController::takeActionSound(){return std::exchange(actionSound_,std::nullopt);}
void MenuController::update(const InputFrame& rawInput){
    auto input=rawInput;
    if(result_){return;}
    if(suppressConfirm_){const auto& button=input.buttons[static_cast<int>(InputAction::Confirm)];if(button.pressed||button.held){input.buttons[static_cast<int>(InputAction::Confirm)].pressed=false;}else{suppressConfirm_=false;}}
    if(page_==MenuPage::Notice){if(pressed(input,InputAction::Cancel)||pressed(input,InputAction::Confirm)){page_=previous_;notice_.clear();sound_="cancel";}return;}
    if(pressed(input,InputAction::Cancel)){
        sound_="cancel";
        switch(page_){case MenuPage::Root:result_=MenuResult{MenuResult::Kind::Close};break;
        case MenuPage::Equipment:enter(MenuPage::Slots,slot_-1);break;
        case MenuPage::Target:enter(MenuPage::Items,listSelection_);break;
        default:enter(MenuPage::Root);break;}return;
    }
    auto v=view();if(v.rows.empty()){return;}
    const int size=static_cast<int>(v.rows.size());selection_=std::clamp(selection_,0,size-1);
    int delta=0;
    if(pressed(input,InputAction::Down)){delta=v.columns;}else if(pressed(input,InputAction::Up)){delta=-v.columns;}
    else if(v.columns==2&&pressed(input,InputAction::Right)){delta=1;}else if(v.columns==2&&pressed(input,InputAction::Left)){delta=-1;}
    if(delta){selection_=(selection_+size+delta)%size;sound_="cursor";return;}
    if(!pressed(input,InputAction::Confirm)){return;}
    const auto row=v.rows.at(selection_);if(!row.enabled){sound_="buzzer";return;}sound_="decision";
    switch(page_){
    case MenuPage::Root:
        switch(row.id){case 0:enter(MenuPage::Items);break;case 1:enter(MenuPage::Skills);break;case 2:enter(MenuPage::Slots);break;case 4:enter(MenuPage::TitleConfirm);break;}break;
    case MenuPage::Slots:slot_=row.id;enter(MenuPage::Equipment);break;
    case MenuPage::Equipment:
        if(session_.equip(row.id,slot_)){enter(MenuPage::Slots,slot_-1);}else{sound_="buzzer";}break;
    case MenuPage::TitleConfirm:
        if(row.id==0){result_=MenuResult{MenuResult::Kind::Title};}else{enter(MenuPage::Root,4);}break;
    case MenuPage::Skills:
        if(mix_&&!content_.expansion.mixing.is_null()&&row.id==content_.expansion.mixing.at("enabled_skill")){result_=MenuResult{MenuResult::Kind::Mix,row.id,{},mix_};}
        else if(session_.castSkill(content_.skills.at(row.id))==SkillUseResult::Applied){sound_="skill:"+std::to_string(row.id);suppressConfirm_=true;result_=MenuResult{MenuResult::Kind::Close};}
        else{sound_="buzzer";}break;
    case MenuPage::Items:
        item_=row.id;listSelection_=selection_;
        {
            std::vector<int> forms;
            int ammunition=0;
            for(const auto& [id,binding]:content_.catalog.at("weapon_bindings").items()){
                if(binding.at("kind")=="gl"){
                    const int form=std::stoi(id);forms.push_back(form);
                    if(content_.items.at(form).looseItem==item_){ammunition=form;}
                }
            }
            if(ammunition){
                const auto sound=ammunitionSound(content_,ammunition);
                if(session_.selectWeaponAmmunition(item_,forms)==ItemUseResult::Applied){enter(MenuPage::Root);suppressConfirm_=true;sound_.clear();actionSound_=sound;}
                else{sound_="buzzer";}
                break;
            }
        }
        if(const auto tool=std::find_if(tools_.begin(),tools_.end(),[&](const auto& tool){return tool.item==item_;});tool!=tools_.end()){result_=MenuResult{MenuResult::Kind::Tool,item_,*tool};}
        else if(content_.items.at(item_).equipmentType==6){enter(MenuPage::Target);}
        else{
            bool found=false;for(const auto& [id,reading]:content_.catalog.at("readings").items()){if(reading.at("item")==item_){found=true;break;}}
            if(found){result_=MenuResult{MenuResult::Kind::Read,item_};}else{notice("此物品的行动尚未接入；未消耗物品。");}
        }break;
    case MenuPage::Target:{
        const auto result=session_.useMedicine(item_);sound_=result==ItemUseResult::Applied?"item":"buzzer";
        suppressConfirm_=true;
        if(session_.count(item_)==0){enter(MenuPage::Items,listSelection_);const int size=static_cast<int>(view().rows.size());selection_=std::min(selection_,std::max(0,size-1));}
        break;
    }
    default:break;
    }
}

#pragma once
#include "file_menu.h"
class SaveController{
public:
    SaveController(const CombatContent&,GameSession&,CombatScene&,SaveFiles&,const SaveRequest&,const SavedPresentation&);
    void update(const InputFrame&);
    const Dialogue& dialogue() const{return dialogue_;}
    const DialogueView& promptView() const{return combined_;}
    std::optional<FileMenuView> fileView() const;
    bool finished() const{return finished_;}
    bool direct() const{return request_.kind=="direct_unscored";}
    std::vector<PresentationEvent> takePresentation();
private:
    void prepare();void finish(bool committed);void sound(const std::string&);
    const CombatContent& content_;GameSession& session_;CombatScene& scene_;SaveFiles& files_;
    SavedPresentation sourcePresentation_;bool preparationFailed_{};std::string preparationError_;
    SaveSnapshot candidate_;SaveRequest request_;std::uint64_t generation_;Dialogue dialogue_;InputPrompt prompt_;PromptIdentity promptId_;
    DialogueView combined_;std::vector<std::u32string> questionLines_;std::vector<std::vector<int>> questionColors_;
    std::unique_ptr<FileMenu> filesUi_;bool askingChoice_{},finished_{};std::vector<PresentationEvent> presentation_;
};

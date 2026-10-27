// Presentation values remain outside gameplay owners; resources are rebuilt by ID.
namespace {
nlohmann::json visualColor(sf::Color c){return {c.r,c.g,c.b,c.a};}
sf::Color visualColor(const nlohmann::json& j){return {j[0].get<std::uint8_t>(),j[1].get<std::uint8_t>(),j[2].get<std::uint8_t>(),j[3].get<std::uint8_t>()};}
nlohmann::json visualPosition(sf::Vector2f v){return {v.x,v.y};}
sf::Vector2f visualPosition(const nlohmann::json& j){return {j[0].get<float>(),j[1].get<float>()};}
}
nlohmann::json CombatPresentation::warehouseVisual() const{
 using Json=nlohmann::json;Json pictures=Json::array(),effects=Json::array(),flashes=Json::array();
 for(const auto& [id,p]:pictures_){pictures.push_back({id,p.image,p.mapFixed,p.worldAnchored,p.eraseOnMapChange,p.rotation,p.rotationSpeed,visualPosition(p.position),p.zoom,p.opacity,visualColor(p.color),visualPosition(p.startPosition),visualPosition(p.targetPosition),p.startZoom,p.targetZoom,p.startOpacity,p.targetOpacity,p.startWave,p.targetWave,visualColor(p.startColor),visualColor(p.targetColor),p.remaining,p.total,p.wave,0,p.phase});}
 for(const auto& e:effects_){effects.push_back({e.id,{e.anchor.x,e.anchor.y},e.age,e.animation,e.actor,e.clipChecked,e.clipped});}for(const auto& [id,f]:flashes_){flashes.push_back({id,visualColor(f.color),f.remaining,f.total});}
 return {{"pictures",pictures},{"effects",effects},{"flashes",flashes},{"system",systemImage_},{"panorama",panorama_},{"screen",{black_,blackTarget_,shakeTicks_,shakeAge_,musicVolume_,musicFade_,musicFadeTotal_,toneRemaining_,toneDuration_,weatherType_,weatherStrength_,shakeStrength_,shakeSpeed_,flashRemaining_,flashDuration_,screenFlash_.toInteger(),rememberFadedMusic_?1:0}},{"tone",{saturation_,startSaturation_,targetSaturation_,0,0,0,screenTone_,startTone_,targetTone_}},{"pending",Json::array()}};
}
void CombatPresentation::restoreWarehouseVisual(const nlohmann::json& j){
 if(j.is_null()){return;}validateWarehouseVisual(content_,j,musicDescription_);pictures_.clear();effects_.clear();flashes_.clear();systemImage_=j.at("system");palette_=images_.at(systemImage_).copyToImage();panorama_=j.at("panorama");
 for(const auto& v:j.at("pictures")){Picture p;p.image=v[1];p.mapFixed=v[2];p.worldAnchored=v[3];p.eraseOnMapChange=v[4];p.rotation=v[5];p.rotationSpeed=v[6];p.position=visualPosition(v[7]);p.zoom=v[8];p.opacity=v[9];p.color=visualColor(v[10]);p.startPosition=visualPosition(v[11]);p.targetPosition=visualPosition(v[12]);p.startZoom=v[13];p.targetZoom=v[14];p.startOpacity=v[15];p.targetOpacity=v[16];p.startWave=v[17];p.targetWave=v[18];p.startColor=visualColor(v[19]);p.targetColor=visualColor(v[20]);p.remaining=v[21];p.total=v[22];p.wave=v[23];p.phase=v[25];pictures_.emplace(v[0].get<int>(),p);}
 for(const auto& v:j.at("effects")){effects_.push_back({v[0],{v[1][0],v[1][1]},v[2],v[3],v[4],v[5],v[6]});}for(const auto& v:j.at("flashes")){flashes_.emplace(v[0].get<std::string>(),Flash{visualColor(v[1]),v[2],v[3]});}
 const auto& v=j.at("screen");black_=v[0];blackTarget_=v[1];shakeTicks_=v[2];shakeAge_=v[3];musicVolume_=v[4];musicFade_=v[5];musicFadeTotal_=v[6];toneRemaining_=v[7];toneDuration_=v[8];weatherType_=v[9];weatherStrength_=v[10];shakeStrength_=v[11];shakeSpeed_=v[12];flashRemaining_=v[13];flashDuration_=v[14];screenFlash_=sf::Color(v[15].get<std::uint32_t>());rememberFadedMusic_=v[16].get<int>()!=0;const auto& t=j.at("tone");saturation_=t[0];startSaturation_=t[1];targetSaturation_=t[2];screenTone_=t[6].get<std::array<float,3>>();startTone_=t[7].get<std::array<float,3>>();targetTone_=t[8].get<std::array<float,3>>();
 // Capture uses the preceding rendered step plus this committed request step.
 // Complete that step once here; the Load caller skips advancement on adoption.
 std::vector<PresentationEvent> pending;for(const auto& e:j.at("pending")){PresentationEvent event;event.kind=e[0]==0?PresentationEvent::Kind::Cue:e[0]==1?PresentationEvent::Kind::Effect:PresentationEvent::Kind::Animation;event.id=e[1];event.position={e[2][0],e[2][1]};event.actor=e[3];if(!e[4].is_null()){event.sampledScreenPosition=GridPosition{e[4][0],e[4][1]};}if(!e[5].is_null()){event.pictureMapOrigin=GridPosition{e[5][0],e[5][1]};}pending.push_back(event);}update(pending);
}

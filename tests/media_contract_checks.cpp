#include "combat_presentation.h"
#include "content_hash.h"
#include <SFML/Audio/InputSoundFile.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <array>
#include <functional>
#include <set>
#include <stdexcept>

namespace {
void need(bool ok,const std::string& message){if(!ok){throw std::runtime_error(message);}}
void selfCheck(const std::filesystem::path& output,const std::filesystem::path& fontFile){
    std::filesystem::create_directories(output);
    sf::Image image({160,80},sf::Color::White);need(image.saveToFile(output/"pixel.png"),"fixture image");
    CombatContent c;c.directory=output;c.media={{"images",{{"system",{{"file","pixel.png"},{"size_px",{160,80}}}}}},{"ui",{{"menu",{{"system_image","system"}}}}},{"effects",nlohmann::json::object()},{"audio",nlohmann::json::object()}};
    sf::Font font;need(font.openFromFile(fontFile),"fixture font");
    CombatPresentation current(c,font);
    c.media["audio"]["bgm-broken"]={{"file","missing.wav"},{"source_file","Music/test.mid"},{"source",{{"location","Map9999 E1P1 C2"}}}};
    bool rejected=false;try{CombatPresentation candidate(c,font);}catch(const std::exception& e){rejected=true;std::cout<<"reject missing BGM: "<<e.what()<<'\n';}
    need(rejected,"Candidate accepted a missing BGM before scene adoption");
    std::ofstream(output/"bad.wav")<<"not-a-wave";
    c.media["audio"]["bgm-broken"]["file"]="bad.wav";
    rejected=false;try{CombatPresentation candidate(c,font);}catch(const std::exception& e){rejected=true;std::cout<<"reject malformed BGM header: "<<e.what()<<'\n';}
    need(rejected,"Candidate accepted malformed BGM header");
    std::array<std::int16_t,32> samples{};sf::SoundBuffer pcm;
    need(pcm.loadFromSamples(samples.data(),samples.size(),1,8000,{sf::SoundChannel::Mono}),"fixture PCM");need(pcm.saveToFile(output/"valid.wav"),"fixture WAV");
    auto& track=c.media["audio"]["bgm-broken"];track["file"]="valid.wav";track["loop_samples"]={{"start",0},{"end",33}};track["initial_sample_compatibility"]=0;
    rejected=false;try{CombatPresentation candidate(c,font);}catch(const std::exception& e){rejected=true;std::cout<<"reject loop beyond actual PCM: "<<e.what()<<'\n';}
    need(rejected,"Candidate accepted loop beyond actual PCM");
    track["loop_samples"]["end"]=32;{CombatPresentation candidate(c,font);}
    SavedPresentation saved;saved.music={"bgm-broken",0,100,50};current.prepareRestoredState(saved);
    track["file"]="missing-after-preparation.wav";
    PresentationEvent next;next.kind=PresentationEvent::Kind::Music;next.sound=saved.music;
    rejected=false;try{current.update({next},false);}catch(const std::exception& e){rejected=true;std::cout<<"late BGM failure retains old music: "<<e.what()<<'\n';}
    need(rejected&&current.saveState().music==saved.music,"Late decode failure replaced current music");
    c.media["audio"]=nlohmann::json::object();c.media["images"]["system"]["size_px"]={2,1};
    rejected=false;try{CombatPresentation candidate(c,font);}catch(const std::exception& e){rejected=true;std::cout<<"reject wrong dimensions: "<<e.what()<<'\n';}need(rejected,"Candidate accepted wrong image size");
    c.media["images"]["system"]["file"]="missing.png";
    rejected=false;try{CombatPresentation candidate(c,font);}catch(const std::exception& e){rejected=true;std::cout<<"reject missing image: "<<e.what()<<'\n';}need(rejected,"Candidate accepted missing image");
    c.media["images"]["system"]["file"]="pixel.png";c.media["images"]["system"]["size_px"]={160,80};c.replaceMediaDefinitions("cues",{{"bad-source-cue",{{"kind","unhandled"},{"source",{{"location","Map9999 E2P1 C4"}}}}}});
    rejected=false;try{validateSceneContent(c,{});}catch(const std::exception& e){rejected=true;need(std::string(e.what()).find("Unknown media cue: bad-source-cue")!=std::string::npos,"Unknown cue failed at unrelated boundary");std::cout<<"reject unknown cue: "<<e.what()<<'\n';}need(rejected,"Unknown cue accepted");
    sf::Font badFont;need(!badFont.openFromFile(output/"missing-font.otf")&&!badFont.openFromFile(output/"bad.wav"),"Bad font accepted");
    current.update({},false);need(current.images().contains("system"),"Candidate failure destroyed current presentation");
    std::cout<<"small media candidate checks PASS\n";
}

void inspect(const std::filesystem::path& package,const std::filesystem::path& output,const std::string& baseline){
    using Json=nlohmann::json;
    Json prior; if(baseline!="actual"){std::ifstream in(baseline);need(bool(in),"Cannot read passed decode baseline");in>>prior;}
    const auto content=loadCombatContent(package);const auto& c=content;
    Json report={{"entry",std::filesystem::absolute(package).string()},{"compatibility",c.compatibility},{"files",Json::object()},{"media",c.resolvedMedia()},{"maps",Json::object()},{"sprites",Json::object()},{"texts",Json::array()}};
    unsigned reusedImages=0,reusedAudio=0,decodedImages=0,decodedAudio=0;
    auto resource=[&](const std::filesystem::path& file,const std::string& use,const std::vector<unsigned>& size=std::vector<unsigned>{}){
        const auto name=file.lexically_relative(c.directory).generic_string();auto& row=report["files"][name];
        if(row.is_null()){
            need(!std::filesystem::is_symlink(file)&&std::filesystem::is_regular_file(file),"Missing/unsafe resource: "+use+" "+file.string());
            std::ifstream in(file,std::ios::binary);need(bool(in),"Cannot read "+file.string());row={{"sha256",contentSha256(readContentBytes(in))},{"uses",Json::array()}};
        }
        if(!size.empty()){
            if(!row.contains("decoded_size_px")){
                if(!prior.is_null()&&prior.at("files").contains(name)&&prior.at("files").at(name).at("sha256")==row.at("sha256")&&prior.at("files").at(name).contains("decoded_size_px")){
                    row["decoded_size_px"]=prior.at("files").at(name).at("decoded_size_px");++reusedImages;
                }else{sf::Image image;need(image.loadFromFile(file),"Cannot decode "+use+" "+file.string());row["decoded_size_px"]={image.getSize().x,image.getSize().y};++decodedImages;}
            }
            need(row.at("decoded_size_px")==size,"Wrong dimensions: "+use+" "+file.string());
        }
        row["uses"].push_back(use);return name;
    };
    const auto& sprites=*c.maps.begin()->second.sprites;
    for(const auto& [id,s]:sprites){const auto name=resource(s.texturePath,"sprites/"+id,{static_cast<unsigned>(s.textureWidth),static_cast<unsigned>(s.textureHeight)});report["sprites"][id]={{"file",name},{"size_px",{s.textureWidth,s.textureHeight}}};}
    std::set<std::filesystem::path> fonts;
    for(const auto& [id,map]:c.maps){
        const auto prefix="maps/"+std::to_string(id);const std::vector<unsigned> size{static_cast<unsigned>(map.width*map.tileSize),static_cast<unsigned>(map.height*map.tileSize)};
        auto& row=report["maps"][std::to_string(id)];row={{"size_tiles",{map.width,map.height}},{"background",resource(map.backgroundPath,prefix+"/background",size)},{"foreground",resource(map.foregroundPath,prefix+"/foreground",size)}};
        for(const auto& tile:map.animatedTiles){resource(tile.file,prefix+"/animated/"+std::to_string(tile.id),{static_cast<unsigned>(tile.width),static_cast<unsigned>(tile.height)});}
        for(const auto& replacement:map.upperTileReplacements){resource(replacement.image,prefix+"/upper",{16,16});if(!replacement.underlay.empty()){resource(replacement.underlay,prefix+"/underlay",{16,16});}for(const auto& file:replacement.cellUnderlays){resource(file,prefix+"/cell-underlay",{16,16});}}
        if(map.lowerTiles){for(const auto& [index,file]:map.lowerTiles->images){resource(file,prefix+"/lower/"+std::to_string(index),{16,16});}}
        if(map.defaultPanorama){row["default_panorama"]=map.defaultPanorama->image;}
        fonts.insert(map.fontPath);
    }
    for(const auto& [id,image]:c.media.at("images").items()){resource(c.directory/image.at("file").get<std::string>(),"images/"+id,image.at("size_px").get<std::vector<unsigned>>());}
    std::array<std::int16_t,32768> block{};
    for(const auto& [id,audio]:c.media.at("audio").items()){
        const auto file=c.directory/audio.at("file").get<std::string>();const auto name=resource(file,"audio/"+id);
        auto& row=report["media"]["audio"][id];
        if(!prior.is_null()&&prior.at("files").contains(name)&&prior.at("files").at(name).at("sha256")==report.at("files").at(name).at("sha256")&&prior.at("media").at("audio").contains(id)){
            auto reviewed=prior.at("media").at("audio").at(id);const auto samples=reviewed.at("decoded_samples");const auto channels=reviewed.at("decoded_channels");const auto rate=reviewed.at("decoded_rate_hz");
            reviewed.erase("decoded_samples");reviewed.erase("decoded_channels");reviewed.erase("decoded_rate_hz");
            if(reviewed==audio&&samples.get<std::uint64_t>()>0){row["decoded_samples"]=samples;row["decoded_channels"]=channels;row["decoded_rate_hz"]=rate;++reusedAudio;continue;}
        }
        sf::InputSoundFile input;need(input.openFromFile(file),"Cannot decode audio/"+id+" "+audio.dump());
        std::uint64_t decoded=0;while(const auto n=input.read(block.data(),block.size())){decoded+=n;}
        need(decoded&&decoded==input.getSampleCount(),"Incomplete audio decode: "+id+" "+file.string());
        row["decoded_samples"]=decoded;row["decoded_channels"]=input.getChannelCount();row["decoded_rate_hz"]=input.getSampleRate();++decodedAudio;
        if(audio.contains("loop_samples")||audio.contains("clear_midi")){
            std::optional<MusicLoop> loop;if(audio.contains("loop_samples")){const auto& l=audio.at("loop_samples");loop=MusicLoop{audio.at("initial_sample_compatibility"),l.at("start"),l.at("end")};}
            PredecodedMusic prepared(file,loop,false,audio.value("clear_midi",Json{}));
        }
    }
    std::u32string codepoints=U"0123456789 +-/%()：盒弹匣备用当前卸下主武器辅助选择确认返回";
    std::function<void(const Json&)> text=[&](const Json& value){
        if(value.is_string()){const auto decoded=decodeUiText(value.get<std::string>());codepoints+=decoded;}
        else if(value.is_object()){for(const auto& [key,v]:value.items()){if(key!="source"&&key!="source_pages"&&key!="source_index"&&key!="source_actor_fields"&&key!="original_fields"){text(v);}}}
        else if(value.is_array()){for(const auto& v:value){text(v);}}
    };
    report["consumer_models"]={{"catalog",c.catalog},{"full",c.resolvedFullDefinition()},{"branch",c.branch},{"rules",c.rules},{"interactions",c.interactions},{"transitions",c.transitions},{"action_profiles",c.actionProfiles}};
    text(report["consumer_models"]);text(c.media.at("ui"));text(c.media.at("terms"));text(c.media.at("actors"));
    auto reference=[&](const std::string& id,const std::string& path){if(!id.empty()){report["consumer_references"][id].push_back({{"definition_path",path}});}};
    for(const auto& [id,route]:c.routes){for(std::size_t i=0;i<route.steps.size();++i){reference(route.steps[i].sprite,"routes/"+id+"/"+std::to_string(i)+"/sprite");reference(route.steps[i].sound.resource,"routes/"+id+"/"+std::to_string(i)+"/sound");}}
    for(const auto& [id,object]:c.objects){for(const auto& variant:object.variants){const auto path="objects/"+id+"/page"+std::to_string(variant.page);if(variant.definition.spriteId){reference(*variant.definition.spriteId,path+"/sprite");}for(const auto& step:variant.autonomousRoute.steps){reference(step.sprite,path+"/autonomous/sprite");reference(step.sound.resource,path+"/autonomous/sound");}}}
    for(const auto& [id,message]:c.messages){reference(message.message.faceImage,"messages/"+id+"/face");reference(message.message.exfontImage,"messages/"+id+"/exfont");for(const auto& line:message.message.lines){for(const auto& token:line.tokens){if(token.kind==MessageTokenKind::ExFont){report["exfont_glyphs"][id].push_back(token.value);}}}}
    for(const auto& [id,item]:c.items){codepoints+=decodeUiText(item.name);codepoints+=decodeUiText(item.description);}
    for(const auto& [id,message]:c.messages){for(const auto& line:message.message.lines){for(const auto& token:line.tokens){if(token.kind==MessageTokenKind::Text){codepoints+=token.text;}}}}
    std::set<char32_t> unique(codepoints.begin(),codepoints.end());unsigned glyphs=0;
    for(const auto& file:fonts){resource(file,"font");sf::Font font;need(font.openFromFile(file),"Invalid font: "+file.string());for(const auto cp:unique){if(cp>=32&&cp!=127){need(font.hasGlyph(cp),"Missing font glyph U+"+std::to_string(static_cast<unsigned>(cp))+" "+file.string());++glyphs;}}}
    const auto license=c.directory/"FONT-LICENSE.txt";resource(license,"font/license");need(std::filesystem::file_size(license)>0,"Empty font license");
    report["glyphs_checked"]=glyphs;
    report["decode_delta"]={{"baseline",baseline},{"images_reused",reusedImages},{"images_decoded",decodedImages},{"audio_reused",reusedAudio},{"audio_decoded",decodedAudio}};
    // Construct the real candidate as well; this uploads shared images and prepares SE, without playback.
    sf::Font font;need(font.openFromFile(*fonts.begin()),"candidate font");CombatPresentation prepared(c,font);
    std::filesystem::create_directories(output);std::ofstream result(output/"bindings.json");result<<report.dump(2);need(bool(result),"Cannot write bindings");
    std::cout<<"Actual "<<c.compatibility<<": maps="<<c.maps.size()<<" sprites="<<sprites.size()<<" images="<<c.media.at("images").size()<<" audio="<<c.media.at("audio").size()<<" animations="<<c.media.at("animations").size()<<" files="<<report.at("files").size()<<" glyphs="<<glyphs<<" PASS (no playback)\n";
}
}
int main(int argc,char** argv){try{
    need(argc==4,"media_contract_checks --self-check output font | package output reserved");
    if(std::string(argv[1])=="--self-check"){selfCheck(argv[2],argv[3]);return 0;}
    inspect(argv[1],argv[2],argv[3]);return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

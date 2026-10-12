#include "chienpo/audio.hpp"
#include <sounds/SoundManager.h>
#include <sounds/Logging.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <algorithm>
#include <memory>
namespace chienpo {
namespace {
using Playing = ad::sounds::Handle<ad::sounds::PlayingSoundCue>;
using Cue = ad::sounds::Handle<ad::sounds::SoundCue>;
struct ActiveSound { Playing handle; std::string name,asset; float gain=.3f; };
struct AudioImpl {
 std::unique_ptr<ad::sounds::SoundManager> manager;
 std::map<std::pair<std::string,bool>,Cue> cues;
 std::vector<ActiveSound> active;
};
ad::sounds::PlayingSoundCue* resolve(const Playing& handle) {
 // SoundManager's public handles can be recycled; compare the unique ID, not only the slot.
 auto found=ad::sounds::mPlayingCues.find(handle);
 return found!=ad::sounds::mPlayingCues.end() && found->second && found->second->id==handle.mUniqueId?found->second.get():nullptr;
}
}
std::string sound_asset_name(const std::string& asset) {
 auto stem=std::filesystem::path(asset).stem().string();
 if(stem=="player_death") stem="end_death_bzzzt";
 if(stem=="win") stem="end_win";
 if(stem=="warpzone") stem="warpzone_temp";
 // ad::sounds currently has no raw PCM decoder. The bundled PCM assets
 // were converted to 48 kHz stereo Ogg; legacy game .pcm references resolve here.
 return stem+".ogg";
}
bool audio_open(Audio& audio) {
 if(audio.implementation) return audio.available;
 ad::sounds::initializeLogging();
 auto impl=std::make_unique<AudioImpl>();
 try {
  impl->manager=std::make_unique<ad::sounds::SoundManager>(std::vector<ad::sounds::SoundCategory>{0});
  if(!alcGetCurrentContext()) { audio.error="No OpenAL output device. Use --no-audio on a silent host."; return false; }
  audio.available=true; audio.implementation=impl.release(); return true;
 } catch(const std::exception& e) { audio.error=e.what(); return false; }
}
void audio_stop(Audio& audio,const std::string& name) {
 if(!audio.implementation) return;
 auto& impl=*static_cast<AudioImpl*>(audio.implementation);
 if(name.empty()) { impl.manager->stopAllSound(); impl.active.clear(); return; }
 for(auto it=impl.active.begin();it!=impl.active.end();) {
  if(it->name==name || it->asset==sound_asset_name(name)) {
   if(resolve(it->handle)) impl.manager->stopSound(it->handle);
   it=impl.active.erase(it);
  } else ++it;
 }
}
void audio_update(Audio& audio,Game& game) {
 auto events=std::move(game.m_sound_events); game.m_sound_events.clear();
 if(!audio.available || !audio.implementation) return;
 auto& impl=*static_cast<AudioImpl*>(audio.implementation);
 audio.muted=game.m_no_sound;
 for(const auto& event:events) {
  if(event.type==SoundEvent::Type::StopAll) { audio_stop(audio); continue; }
  if(event.type==SoundEvent::Type::StopOne) { audio_stop(audio,event.name); continue; }
  if(audio.muted) continue;
  try {
   const auto asset=sound_asset_name(event.asset);
   const auto path=game.m_paths.music/asset;
   if(!std::filesystem::is_regular_file(path)) { game.send_error("Sound asset missing: "+path.string()); continue; }
   const auto key=std::make_pair(path.string(),event.loop);
   auto cue=impl.cues.find(key);
   if(cue==impl.cues.end()) {
    const auto data=impl.manager->createData(path);
    const auto handle=impl.manager->createSoundCue({{data,{event.loop?-1:0}}},0,0);
    cue=impl.cues.emplace(key,handle).first;
   }
   const auto handle=impl.manager->playSound(cue->second);
   impl.active.push_back({handle,event.name,asset,static_cast<float>(event.gain)});
  } catch(const std::exception& e) { game.send_error(std::string("Audio: ")+e.what()); }
 }
 for(auto it=impl.active.begin();it!=impl.active.end();) {
  if(auto* cue=resolve(it->handle)) {
   auto volume=audio.volumes.find(it->asset);
   // ad::sounds lacks a public per-playing-cue gain setter. This narrow adapter uses
   // its exposed handle object; update() applies option.gain to the OpenAL source.
   cue->option.gain=audio.muted?0.f:std::clamp(volume==audio.volumes.end()?it->gain:volume->second,0.f,1.f);
   ++it;
  } else it=impl.active.erase(it);
 }
 impl.manager->update();
}
void audio_close(Audio& audio) {
 if(audio.implementation) {
  audio_stop(audio);
  delete static_cast<AudioImpl*>(audio.implementation);
  audio.implementation=nullptr;
 }
 audio.available=false;
}
} // namespace chienpo

#include "chienpo/types.hpp"
#include <algorithm>
#include <utility>

namespace chienpo {
std::string_view to_string(EntityType value) {
 switch(value) {
 case EntityType::nil: return "Nil";
 case EntityType::frame: return "Frame";
 case EntityType::sprite: return "Sprite";
 case EntityType::point: return "Point";
 case EntityType::player: return "Player";
 case EntityType::neon_boss: return "Boss";
 case EntityType::platform: return "Platform";
 case EntityType::move_platform: return "Moving Platform";
 case EntityType::masking_box: return "Masking box";
 case EntityType::laser: return "Laser";
 case EntityType::laser_projectile: return "Laser proj";
 case EntityType::boss_projectile: return "Boss projectiles";
 case EntityType::projection_bound: return "Projection bound";
 case EntityType::scaling_bound: return "Scaling bound";
 case EntityType::decor: return "Decor";
 case EntityType::spawner: return "Spawner";
 case EntityType::portal: return "Portal";
 case EntityType::scene_root_node: return "Scene Root";
 case EntityType::device: return "Screen";
 case EntityType::device_parent: return "device_parent";
 case EntityType::trigger: return "trigger";
 } return "";
}
bool from_string(std::string_view text, EntityType& value) {
 if(text=="Nil" || text=="nil") { value=EntityType::nil; return true; }
 if(text=="Frame" || text=="frame") { value=EntityType::frame; return true; }
 if(text=="Sprite" || text=="sprite") { value=EntityType::sprite; return true; }
 if(text=="Point" || text=="point") { value=EntityType::point; return true; }
 if(text=="Player" || text=="player") { value=EntityType::player; return true; }
 if(text=="Boss" || text=="neon_boss") { value=EntityType::neon_boss; return true; }
 if(text=="Platform" || text=="platform") { value=EntityType::platform; return true; }
 if(text=="Moving Platform" || text=="move_platform") { value=EntityType::move_platform; return true; }
 if(text=="Masking box" || text=="masking_box") { value=EntityType::masking_box; return true; }
 if(text=="Laser" || text=="laser") { value=EntityType::laser; return true; }
 if(text=="Laser proj" || text=="laser_projectile") { value=EntityType::laser_projectile; return true; }
 if(text=="Boss projectiles" || text=="boss_projectile") { value=EntityType::boss_projectile; return true; }
 if(text=="Projection bound" || text=="projection_bound") { value=EntityType::projection_bound; return true; }
 if(text=="Scaling bound" || text=="scaling_bound") { value=EntityType::scaling_bound; return true; }
 if(text=="Decor" || text=="decor") { value=EntityType::decor; return true; }
 if(text=="Spawner" || text=="spawner") { value=EntityType::spawner; return true; }
 if(text=="Portal" || text=="portal") { value=EntityType::portal; return true; }
 if(text=="Scene Root" || text=="scene_root_node") { value=EntityType::scene_root_node; return true; }
 if(text=="Screen" || text=="device") { value=EntityType::device; return true; }
 if(text=="device_parent" || text=="device_parent") { value=EntityType::device_parent; return true; }
 if(text=="trigger" || text=="trigger") { value=EntityType::trigger; return true; }
 return false;
}
std::string_view to_string(TriggerActionType value) {
 switch(value) {
 case TriggerActionType::death: return "Death";
 case TriggerActionType::shot: return "Shot";
 case TriggerActionType::activated: return "Activated";
 case TriggerActionType::deactivated: return "Deactivated";
 case TriggerActionType::on_count: return "On count";
 case TriggerActionType::on_start: return "On press start";
 } return "";
}
bool from_string(std::string_view text, TriggerActionType& value) {
 if(text=="Death" || text=="death") { value=TriggerActionType::death; return true; }
 if(text=="Shot" || text=="shot") { value=TriggerActionType::shot; return true; }
 if(text=="Activated" || text=="activated") { value=TriggerActionType::activated; return true; }
 if(text=="Deactivated" || text=="deactivated") { value=TriggerActionType::deactivated; return true; }
 if(text=="On count" || text=="on_count") { value=TriggerActionType::on_count; return true; }
 if(text=="On press start" || text=="on_start") { value=TriggerActionType::on_start; return true; }
 return false;
}
std::string_view to_string(TriggerType value) {
 switch(value) {
 case TriggerType::no_trigger: return "No trigger";
 case TriggerType::win: return "Win";
 case TriggerType::activate: return "Activate";
 case TriggerType::deactivate: return "Deactivate";
 case TriggerType::add_to_count: return "Add to count";
 case TriggerType::remove_from_count: return "Remove from count";
 case TriggerType::lose: return "Lose";
 case TriggerType::start_game: return "Start game";
 } return "";
}
bool from_string(std::string_view text, TriggerType& value) {
 if(text=="No trigger" || text=="no_trigger") { value=TriggerType::no_trigger; return true; }
 if(text=="Win" || text=="win") { value=TriggerType::win; return true; }
 if(text=="Activate" || text=="activate") { value=TriggerType::activate; return true; }
 if(text=="Deactivate" || text=="deactivate") { value=TriggerType::deactivate; return true; }
 if(text=="Add to count" || text=="add_to_count") { value=TriggerType::add_to_count; return true; }
 if(text=="Remove from count" || text=="remove_from_count") { value=TriggerType::remove_from_count; return true; }
 if(text=="Lose" || text=="lose") { value=TriggerType::lose; return true; }
 if(text=="Start game" || text=="start_game") { value=TriggerType::start_game; return true; }
 return false;
}
std::string_view to_string(SceneType value) {
 switch(value) {
 case SceneType::nil_scene: return "Nil";
 case SceneType::projection: return "Projection";
 case SceneType::scaling: return "Scaling";
 case SceneType::game: return "Game";
 case SceneType::editor: return "Editor";
 case SceneType::game_over: return "Lose";
 case SceneType::game_win: return "Win";
 } return "";
}
bool from_string(std::string_view text, SceneType& value) {
 if(text=="Nil" || text=="nil_scene") { value=SceneType::nil_scene; return true; }
 if(text=="Projection" || text=="projection") { value=SceneType::projection; return true; }
 if(text=="Scaling" || text=="scaling") { value=SceneType::scaling; return true; }
 if(text=="Game" || text=="game") { value=SceneType::game; return true; }
 if(text=="Editor" || text=="editor") { value=SceneType::editor; return true; }
 if(text=="Lose" || text=="game_over") { value=SceneType::game_over; return true; }
 if(text=="Win" || text=="game_win") { value=SceneType::game_win; return true; }
 return false;
}
SceneType string_to_scene_type(std::string_view value) {
 if(value=="over") return SceneType::game_over;
 if(value=="win") return SceneType::game_win;
 SceneType result=SceneType::nil_scene; from_string(value,result); return result;
}
std::string_view scene_type_to_string(SceneType value) {
 switch(value) {
 case SceneType::projection:return "projection";
 case SceneType::scaling:return "scaling";
 case SceneType::game:return "game";
 case SceneType::editor:return "editor";
 case SceneType::game_over:return "over";
 case SceneType::game_win:return "win";
 default:return "nil";
 }
}
Things::Things() { reset(); }
Things::Things(const Things& other) { *this=other; }
Things& Things::operator=(const Things& other) {
 if(this!=&other) { m_things=other.m_things; m_used=other.m_used;
 m_next_empty_slot=other.m_next_empty_slot; m_first_free=other.m_first_free; rebind(); }
 return *this;
}
void Things::rebind() { for(auto& thing:m_things) thing.m_things=this; }
void Things::reset() {
 for(auto& thing:m_things) { thing=Thing{}; thing.m_things=this; }
 m_used.fill(false); m_used[0]=true; m_next_empty_slot=1; m_first_free=0;
 m_things[0].m_active=false;
}
Thing_idx Things::add(EntityType kind) {
 if(kind==EntityType::nil) return 0;
 Thing_idx slot=m_first_free;
 if(slot) {
  if(slot>=MAX_THINGS || m_used[slot]) return 0;
  m_first_free=m_things[slot].m_next_free;
 } else if(m_next_empty_slot<MAX_THINGS) slot=m_next_empty_slot++;
 if(!slot) return 0;
 m_things[slot]=Thing{};
 auto& t=m_things[slot]; t.m_things=this; t.m_idx=slot; t.m_kind=kind;
 t.m_name=std::string(to_string(kind))+" "+std::to_string(slot);
 m_used[slot]=true; return slot;
}
Thing_idx Things::add_thing(const Thing& thing) {
 // Copy first because the input may be a reference into this pool.
 Thing copy=thing; auto slot=add(copy.m_kind); if(!slot) return 0;
 copy.m_idx=slot; copy.m_things=this; copy.m_next_free=0;
 copy.parent_idx=copy.first_child_idx=copy.next_child_idx=copy.prev_child_idx=0;
 copy.new_parent_after_transaction=0;
 m_things[slot]=std::move(copy); return slot;
}
void Things::remove(Thing_idx idx) {
 if(!ent_valid(get(idx))) return;
 for(auto child:children(idx)) remove_child(*this,child);
 remove_child(*this,idx);
 // Clear incoming links before recycling the slot. Otherwise a portal or trigger
 // could silently target an unrelated entity allocated at the same index later.
 for(auto& thing:m_things) {
  for(auto* reference:{&thing.m_moving_platform_idx,&thing.m_portal_destination_idx,
                       &thing.m_proj_thing_idx,&thing.m_scale_thing_idx,
                       &thing.new_parent_after_transaction})
   if(*reference==idx) *reference=0;
  for(auto& trigger:thing.m_triggers)
   trigger.m_trigger_target.erase(
    std::remove(trigger.m_trigger_target.begin(),trigger.m_trigger_target.end(),idx),
    trigger.m_trigger_target.end());
 }
 m_things[idx]=Thing{}; m_things[idx].m_things=this;
 m_things[idx].m_next_free=m_first_free; m_first_free=idx; m_used[idx]=false;
}
Thing& Things::get(Thing_idx idx) {
 return m_things[idx>0 && idx<MAX_THINGS && m_used[idx]?idx:0];
}
const Thing& Things::get(Thing_idx idx) const {
 return m_things[idx>0 && idx<MAX_THINGS && m_used[idx]?idx:0];
}
std::vector<Thing_idx> Things::children(Thing_idx parent,bool active_only) const {
 std::vector<Thing_idx> result;
 const auto& p=get(parent); if(!ent_valid(p)) return result;
 auto current=p.first_child_idx;
 std::array<bool,MAX_THINGS> visited{};
 while(current && current<MAX_THINGS && !visited[current]) {
  visited[current]=true; const auto& c=get(current);
  if(!ent_valid(c) || c.parent_idx!=parent) break;
  if(!active_only || c.m_active) result.push_back(current);
  current=c.next_child_idx;
 }
 return result;
}
void Things::Iterator::skip() {
 while(index<pool->m_next_empty_slot && (!pool->m_used[index] || !ent_valid(pool->m_things[index]) || !pool->m_things[index].m_active)) ++index;
}
Things::Iterator Things::begin() { Iterator it{this,1}; it.skip(); return it; }
Things::Iterator Things::end() { return {this,m_next_empty_slot}; }
bool ent_valid(const Thing& t) { return t.m_idx>0 && t.m_kind!=EntityType::nil; }
Thing& Thing::parent() { return m_things->get(parent_idx); }
Thing& Thing::first_child() { return m_things->get(first_child_idx); }
Thing& Thing::prev_child() { return m_things->get(prev_child_idx); }
Thing& Thing::last_child() { return m_things->get(last_child_idx()); }
Thing_idx Thing::last_child_idx() const { return m_things->get(first_child_idx).prev_child_idx; }
Thing& Thing::next_child(bool active) {
 auto current=next_child_idx;
 for(std::size_t guard=0;current && guard<MAX_THINGS;++guard) {
  auto& candidate=m_things->get(current);
  if(!ent_valid(candidate)) break;
  if(!active || candidate.m_active) return candidate;
  current=candidate.next_child_idx;
  if(current==next_child_idx) break;
 }
 return m_things->get(0);
}
bool add_child(Things& things,Thing_idx parent_idx,Thing_idx child_idx) {
 auto& p=things.get(parent_idx); auto& c=things.get(child_idx);
 if(!ent_valid(p)||!ent_valid(c)||parent_idx==child_idx) return false;
 auto ancestor=parent_idx;
 for(std::size_t guard=0;ancestor;++guard) {
  if(guard>=MAX_THINGS || ancestor==child_idx) return false;
  ancestor=things.get(ancestor).parent_idx;
 }
 remove_child(things,child_idx);
 c.new_parent_after_transaction=0;
 if(!p.first_child_idx) {
  p.first_child_idx=child_idx; c.prev_child_idx=child_idx; c.next_child_idx=child_idx;
 } else {
  auto& first=things.get(p.first_child_idx); auto& last=things.get(first.prev_child_idx);
  if(!ent_valid(first)||!ent_valid(last)) return false;
  c.prev_child_idx=last.m_idx; c.next_child_idx=first.m_idx;
  last.next_child_idx=child_idx; first.prev_child_idx=child_idx;
 }
 c.parent_idx=parent_idx; return true;
}
void remove_child(Things& things,Thing_idx idx) {
 auto& c=things.get(idx); if(!ent_valid(c)) return;
 auto& p=things.get(c.parent_idx);
 if(ent_valid(p)) {
  auto& next=things.get(c.next_child_idx); auto& prev=things.get(c.prev_child_idx);
  if(ent_valid(next)) next.prev_child_idx=c.prev_child_idx;
  if(ent_valid(prev)) prev.next_child_idx=c.next_child_idx;
  if(p.first_child_idx==idx) p.first_child_idx=c.next_child_idx==idx?0:c.next_child_idx;
 }
 c.parent_idx=c.next_child_idx=c.prev_child_idx=0;
}
void remove_hierarchy(Things& things,Thing_idx root) {
 if(!ent_valid(things.get(root))) return;
 // Iterative post-order deletion bounds recursion even for adversarial levels.
 std::vector<Thing_idx> order{root}; std::array<bool,MAX_THINGS> seen{}; seen[root]=true;
 for(std::size_t i=0;i<order.size();++i) for(auto child:things.children(order[i]))
  if(!seen[child]) { seen[child]=true; order.push_back(child); }
 for(auto it=order.rbegin();it!=order.rend();++it) things.remove(*it);
}
Thing& get_scene_from_device(Things& things,const Thing& device,SceneType type) {
 for(auto idx:things.children(device.m_idx)) if(things.get(idx).m_scene_type==type) return things.get(idx);
 return things.get(0);
}
Thing& get_current_scene(Things& things,Thing_idx device) {
 const auto& d=things.get(device);
 return d.m_scene_stack.empty()?things.get(0):get_scene_from_device(things,d,d.m_scene_stack.back());
}
bool set_device_to_scene(Things& things,Thing_idx device,SceneType type) {
 auto& d=things.get(device); if(!ent_valid(get_scene_from_device(things,d,type))) return false;
 d.m_scene_stack={type}; return true;
}
void reset_thing_frame_lifetime_value(const GameContext& context,Thing& thing) { thing.m_intensity_modifier=context.m_base_intensity_modifier; }
void pop_box_in_thing(Thing& t,const Box& b) { t.xmin=b.xmin; t.xmax=b.xmax; t.ymin=b.ymin; t.ymax=b.ymax; }
void pop_pos_in_thing(Thing& t,const Vec2& p) { t.x=p.x; t.y=p.y; }
bool Things::validate(std::string* error) const {
 const auto fail=[&](const std::string& why){ if(error) *error=why; return false; };
 if(m_next_empty_slot<1 || m_next_empty_slot>MAX_THINGS) return fail("Invalid pool extent");
 if(!m_used[0] || m_things[0].m_idx || m_things[0].m_kind!=EntityType::nil) return fail("Invalid sentinel");
 std::array<bool,MAX_THINGS> free{}; auto cursor=m_first_free;
 while(cursor) {
  if(cursor>=m_next_empty_slot || m_used[cursor] || free[cursor]) return fail("Invalid free list");
  free[cursor]=true; cursor=m_things[cursor].m_next_free;
 }
 for(Thing_idx i=1;i<MAX_THINGS;++i) {
  const auto& t=m_things[i];
  if(!m_used[i]) { if(i<m_next_empty_slot && !free[i]) return fail("Missing free slot"); continue; }
  if(i>=m_next_empty_slot || t.m_idx!=i || !ent_valid(t)) return fail("Invalid live slot");
  if(t.parent_idx) {
   const auto& p=get(t.parent_idx); const auto& next=get(t.next_child_idx); const auto& prev=get(t.prev_child_idx);
   if(!ent_valid(p)||!ent_valid(next)||!ent_valid(prev)||next.prev_child_idx!=i||prev.next_child_idx!=i) return fail("Broken sibling links");
   auto siblings=children(t.parent_idx);
   if(std::find(siblings.begin(),siblings.end(),i)==siblings.end()) return fail("Unreachable child");
  } else if(t.next_child_idx || t.prev_child_idx) return fail("Detached sibling links");
  if(t.first_child_idx && get(t.first_child_idx).parent_idx!=i) return fail("Invalid first child");
  auto ancestor=t.parent_idx;
  for(std::size_t depth=0;ancestor;++depth) {
   if(depth>=MAX_THINGS || ancestor==i) return fail("Hierarchy cycle");
   ancestor=get(ancestor).parent_idx;
  }
 }
 return true;
}
void transform_command_state(CommandState& state,const Input& input) {
 const auto update=[](Command& c,bool pressed) { c.positive_edge=!c.pressed&&pressed; c.negative_edge=c.pressed&&!pressed; c.pressed=pressed; };
 update(state.next_entity,input.next_entity);
 update(state.prev_entity,input.prev_entity);
 update(state.next,input.next);
 update(state.prev,input.prev);
 update(state.up,input.up);
 update(state.down,input.down);
 update(state.left,input.left);
 update(state.right,input.right);
 update(state.confirm,input.confirm);
 update(state.shift,input.shift);
 update(state.cancel,input.cancel);
 update(state.delete_key,input.delete_key);
 update(state.jump,input.jump);
 update(state.add_platform,input.add_platform);
 update(state.add_moving_platform,input.add_moving_platform);
 update(state.add_masking_box,input.add_masking_box);
 update(state.add_neon_boss,input.add_neon_boss);
 update(state.add_portal,input.add_portal);
 update(state.add_decor,input.add_decor);
 update(state.add_laser,input.add_laser);
 update(state.add_trigger_thing,input.add_trigger_thing);
 update(state.switch_mode,input.switch_mode);
 update(state.scale_up,input.scale_up);
 update(state.scale_down,input.scale_down);
 update(state.reset_scale,input.reset_scale);
 update(state.loop_platform,input.loop_platform);
}
void remove_command_edges(CommandState& state) {
 state.next_entity.positive_edge=state.next_entity.negative_edge=false;
 state.prev_entity.positive_edge=state.prev_entity.negative_edge=false;
 state.next.positive_edge=state.next.negative_edge=false;
 state.prev.positive_edge=state.prev.negative_edge=false;
 state.up.positive_edge=state.up.negative_edge=false;
 state.down.positive_edge=state.down.negative_edge=false;
 state.left.positive_edge=state.left.negative_edge=false;
 state.right.positive_edge=state.right.negative_edge=false;
 state.confirm.positive_edge=state.confirm.negative_edge=false;
 state.shift.positive_edge=state.shift.negative_edge=false;
 state.cancel.positive_edge=state.cancel.negative_edge=false;
 state.delete_key.positive_edge=state.delete_key.negative_edge=false;
 state.jump.positive_edge=state.jump.negative_edge=false;
 state.add_platform.positive_edge=state.add_platform.negative_edge=false;
 state.add_moving_platform.positive_edge=state.add_moving_platform.negative_edge=false;
 state.add_masking_box.positive_edge=state.add_masking_box.negative_edge=false;
 state.add_neon_boss.positive_edge=state.add_neon_boss.negative_edge=false;
 state.add_portal.positive_edge=state.add_portal.negative_edge=false;
 state.add_decor.positive_edge=state.add_decor.negative_edge=false;
 state.add_laser.positive_edge=state.add_laser.negative_edge=false;
 state.add_trigger_thing.positive_edge=state.add_trigger_thing.negative_edge=false;
 state.switch_mode.positive_edge=state.switch_mode.negative_edge=false;
 state.scale_up.positive_edge=state.scale_up.negative_edge=false;
 state.scale_down.positive_edge=state.scale_down.negative_edge=false;
 state.reset_scale.positive_edge=state.reset_scale.negative_edge=false;
 state.loop_platform.positive_edge=state.loop_platform.negative_edge=false;
}
} // namespace chienpo

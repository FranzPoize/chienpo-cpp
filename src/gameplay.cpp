#include "chienpo/gameplay.hpp"
#include "chienpo/render.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <random>

namespace chienpo {
namespace {
constexpr double pi=3.14159265358979323846;
constexpr double epsilon=1e-9;
double positive(double value,double fallback) { return std::isfinite(value) && value>epsilon?value:fallback; }
int wrap(int value,int size) { return size>0?(value%size+size)%size:0; }
Box bounds(const Thing& thing) {
 const double x1=thing.x+thing.xmin*thing.m_scale,x2=thing.x+thing.xmax*thing.m_scale;
 const double y1=thing.y+thing.ymin*thing.m_scale,y2=thing.y+thing.ymax*thing.m_scale;
 return {std::min(x1,x2),std::max(x1,x2),std::min(y1,y2),std::max(y1,y2)};
}
bool overlaps(const Box& a,const Box& b) {
 return a.xmax>b.xmin && a.xmin<b.xmax && a.ymax>b.ymin && a.ymin<b.ymax;
}
Vec2 penetration(const Box& a,const Box& b) {
 const double offsets[]{b.xmin-a.xmax,b.xmax-a.xmin,b.ymin-a.ymax,b.ymax-a.ymin};
 int best=0; for(int i=1;i<4;++i) if(std::abs(offsets[i])<std::abs(offsets[best])) best=i;
 return best<2?Vec2{offsets[best],0}:Vec2{0,offsets[best]};
}
Vec2 world_size(const Thing& thing) {
 const auto& device=thing.m_things->get(thing.parent_idx).parent();
 return {1/positive(std::abs(device.m_base_scale_x),1),1/positive(std::abs(device.m_base_scale_y),1)};
}
bool solid(EntityType kind) { return kind==EntityType::platform || kind==EntityType::move_platform || kind==EntityType::masking_box; }
void animation(Thing& thing,const char* state,const char* asset,double ticks,bool loop=true) {
 thing.m_animation_path_map[state].push_back({asset,loop,ticks});
}
double default_scale(const Game& game,EntityType kind) {
 switch(kind) {
 case EntityType::player:return positive(game.m_context.m_base_player_scale,.02);
 case EntityType::neon_boss:case EntityType::laser:return .05;
 case EntityType::portal:case EntityType::decor:return .2;
 default:return 1;
 }
}
void move_platform(Game& game,Thing& platform) {
 platform.m_speed_x=platform.m_speed_y=0;
 platform.m_path_point_size=static_cast<int>(platform.m_path_point.size());
 const int size=platform.m_path_point_size;
 if(size<2 || game.m_delta_time<=0) return;
 platform.m_current_node=wrap(platform.m_current_node,size);
 const Vec2 old{platform.x,platform.y};
 double remaining=std::max(0.0,game.m_context.m_moving_platform_speed)*game.m_delta_time;
 // Bound iterations even for a path containing only identical points.
 for(int steps=0;remaining>epsilon && steps<size*2+8;++steps) {
  if(!platform.m_loop) {
   if(platform.m_current_node==0) platform.m_direction=0;
   if(platform.m_current_node==size-1) platform.m_direction=1;
  }
  const int next=wrap(platform.m_current_node+(platform.m_direction? -1:1),size);
  const auto target=platform.m_path_point[static_cast<std::size_t>(next)];
  const double dx=target.x-platform.x,dy=target.y-platform.y,distance=std::hypot(dx,dy);
  if(distance<=remaining+epsilon) {
   platform.x=target.x; platform.y=target.y; platform.m_current_node=next;
   remaining-=distance;
  } else {
   platform.x+=dx/distance*remaining; platform.y+=dy/distance*remaining; remaining=0;
  }
 }
 platform.m_speed_x=(platform.x-old.x)/game.m_delta_time;
 platform.m_speed_y=(platform.y-old.y)/game.m_delta_time;
}
Vec2 platform_speed(Game& game,const Thing& player) {
 const auto& platform=game.m_things.get(player.m_moving_platform_idx);
 if(!ent_valid(platform) || !platform.m_active || platform.m_kind!=EntityType::move_platform) return {};
 return {platform.m_speed_x,platform.m_speed_y};
}
void kill_player(Game& game,Thing& player) {
 if(player.m_dead) return;
 player.m_dead=true; player.m_climbing=0;
 change_animation_state(player,"dead");
 player.m_trigger_events.push_back(TriggerActionType::death);
 game.queue_sound("player_death","end_death_bzzzt.pcm");
}
void player_contacts(Game& game,Thing& player,const Box& previous,bool was_grounded,const CommandState& command) {
 bool touching_wall=false,in_portal=false;
 Thing_idx riding=0;
 for(const auto idx:game.m_things.children(player.parent_idx,true)) {
  auto& entity=game.m_things.get(idx);
  if(idx==player.m_idx || !entity.m_collision) continue;
  const auto box=bounds(entity),pbox=bounds(player);
  const bool collision=overlaps(pbox,box);
  if(entity.m_kind==EntityType::platform || entity.m_kind==EntityType::move_platform) {
   const double platform_dy=entity.m_kind==EntityType::move_platform?entity.m_speed_y*game.m_delta_time:0;
   const double relative_speed=player.m_speed_y-(entity.m_kind==EntityType::move_platform?entity.m_speed_y:0);
   const bool horizontal=pbox.xmax>box.xmin && pbox.xmin<box.xmax;
   const bool crossing=previous.ymax<=box.ymin-platform_dy+.0001 && pbox.ymax>=box.ymin-.0001;
   const bool resting=was_grounded && player.m_jumps==0 && std::abs(pbox.ymax-box.ymin)<.003;
   if(horizontal && (crossing || resting) && (relative_speed>=-epsilon || resting)) {
    player.y+=box.ymin-pbox.ymax; player.m_speed_y=0;
    player.m_grounded=1; player.m_jumps=0;
    if(entity.m_kind==EntityType::move_platform) riding=idx;
   }
   continue;
  }
  if(!collision) continue;
  switch(entity.m_kind) {
  case EntityType::masking_box: {
   const auto offset=penetration(pbox,box);
   if(offset.x!=0) {
    touching_wall=true;
    const bool pushing=(offset.x<0 && command.right.pressed)||(offset.x>0 && command.left.pressed);
    if(pushing && !player.m_dead) player.m_should_start_climbing=1;
    if(pushing || player.m_climbing) player.m_facing=offset.x<0?1:0;
    player.x+=offset.x-std::copysign(std::min(.001,std::abs(offset.x)),offset.x);
    player.m_speed_x=0;
   } else {
    player.y+=offset.y;
    if(offset.y<0) { player.m_grounded=1; player.m_jumps=0; }
    if(offset.y*player.m_speed_y<0) player.m_speed_y=0;
   }
   break;
  }
  case EntityType::portal: {
   in_portal=true;
   const auto& destination=game.m_things.get(entity.m_portal_destination_idx);
   if(!player.m_inside_of_portal && destination.m_kind==EntityType::portal && destination.m_active && destination.m_idx!=idx && ent_valid(game.m_things.get(destination.parent_idx))) {
    player.x=destination.x; player.y=destination.y;
    player.m_moving_platform_idx=0; player.m_grounded=0; player.m_climbing=0;
    player.new_parent_after_transaction=destination.parent_idx;
    player.m_inside_of_portal=1;
    game.queue_sound("warp","warpzone_temp.pcm");
    return; // Remaining contacts belong to the source screen.
   }
   break;
  }
  case EntityType::neon_boss:
   if(!player.m_dead && !entity.m_dead) {
    change_animation_state(entity,"dead"); entity.m_dead=true;
    entity.m_trigger_events.push_back(TriggerActionType::death);
   }
   break;
  case EntityType::boss_projectile:case EntityType::laser_projectile:kill_player(game,player);break;
  default:break;
  }
 }
 player.m_inside_of_portal=in_portal?1:0;
 player.m_moving_platform_idx=riding;
 if(!touching_wall) player.m_should_stop_climbing=1;
 const auto extent=world_size(player);
 auto pbox=bounds(player);
 if(pbox.xmin<.001) { player.x+=.001-pbox.xmin; player.m_speed_x=std::max(0.0,player.m_speed_x); }
 pbox=bounds(player);
 if(pbox.xmax>extent.x-.001) { player.x+=extent.x-.001-pbox.xmax; player.m_speed_x=std::min(0.0,player.m_speed_x); }
 pbox=bounds(player);
 if(pbox.ymax>=extent.y-.001) {
  player.y+=extent.y-.001-pbox.ymax; player.m_speed_y=std::min(0.0,player.m_speed_y);
  player.m_grounded=1; player.m_jumps=0;
 }
}
void update_player(Game& game,Thing& player,const CommandState& requested) {
 const CommandState idle{};
 const auto& command=player.m_dead?idle:requested;
 const auto& context=game.m_context;
 const double dt=game.m_delta_time;
 const double scale=player.m_scale/positive(context.m_base_player_scale,.02);
 const double max_speed=positive(context.m_max_player_horizontal_speed,.2);
 const double half_length=positive(context.m_half_jump_length,.18);
 const bool was_grounded=player.m_grounded!=0;
 const auto previous=bounds(player);
 player.m_should_start_climbing=player.m_should_stop_climbing=0;
 if(player.m_climbing) {
  player.m_speed_y=(static_cast<int>(command.down.pressed)-static_cast<int>(command.up.pressed))*context.m_climbing_speed*scale;
  if((command.left.pressed && player.m_facing==1)||(command.right.pressed && player.m_facing==0)) player.m_should_stop_climbing=1;
 }
 if(was_grounded || player.m_climbing) player.m_coyote_frame=0;
 else if(player.m_coyote_frame<std::numeric_limits<int>::max()) ++player.m_coyote_frame;
 if(player.m_coyote_frame>context.m_coyote_frame_amount) player.m_was_climbing=0;
 if(command.jump.positive_edge && (player.m_jumps==1 || (player.m_jumps==0 && player.m_coyote_frame<=context.m_coyote_frame_amount))) {
  const double norm=scale*2*context.m_jump_max_height*max_speed/half_length;
  Vec2 direction{0,-1};
  if(player.m_climbing || player.m_was_climbing) {
   const bool facing=player.m_was_climbing?!player.m_facing:player.m_facing;
   const double angle=pi/2+(facing?1:-1)*pi/positive(context.m_climbing_jump_division,6);
   direction={std::cos(angle)*context.m_climbing_jump_boost,-std::sin(angle)*context.m_climbing_jump_boost};
  }
  const auto inherited=platform_speed(game,player);
  player.m_speed_x+=inherited.x+direction.x*norm; player.m_speed_y=inherited.y+direction.y*norm;
  ++player.m_jumps; player.m_grounded=0; player.m_moving_platform_idx=0;
  player.m_should_stop_climbing=1; player.m_climbing=0;
 }
 const bool grounded=player.m_grounded!=0;
 const double input=(command.right.pressed?1.0:0.0)-(command.left.pressed?1.0:0.0);
 double movement=0;
 if(grounded) {
  const double abs_speed=player.m_speed_x*input<0?0:std::abs(player.m_speed_x);
  const double bonus=context.m_ground_accel_max_ratio;
  const double ratio=std::clamp(bonus-(bonus-1)*abs_speed/positive(max_speed*scale,max_speed),0.0,std::max(1.0,bonus));
  movement=input*context.m_ground_friction*max_speed*scale*ratio;
 } else if(!player.m_climbing) movement=input*context.m_air_friction*max_speed*context.m_air_accel*scale;
 double gravity=player.m_climbing?0:scale*2*context.m_jump_max_height*max_speed*max_speed/(half_length*half_length);
 if((!grounded && player.m_speed_y>0)||!command.jump.pressed) gravity*=2;
 const auto friction=[&](double velocity) {
  double coefficient=grounded?context.m_ground_friction:context.m_air_friction;
  if(grounded && input==0) coefficient*=1+context.m_ground_friction_max_ratio*std::abs(velocity)/max_speed;
  return -coefficient*velocity;
 };
 const auto inherited=platform_speed(game,player);
 // Velocity Verlet, evaluating friction at the half step as in the original.
 player.x+=(player.m_speed_x+inherited.x)*dt+.5*player.m_accel_x*dt*dt;
 player.y+=(player.m_speed_y+inherited.y)*dt+.5*player.m_accel_y*dt*dt;
 const double vx_half=player.m_speed_x+.5*(movement+player.m_accel_x+friction(player.m_speed_x))*dt;
 const double vy_half=player.m_speed_y+.5*player.m_accel_y*dt;
 const double ax=movement+friction(vx_half);
 double vx=vx_half+.5*(player.m_accel_x+ax)*dt;
 if(input==0 && vx*player.m_speed_x<0) vx=0;
 player.m_speed_x=std::isfinite(vx)?vx:0;
 player.m_speed_y=vy_half+.5*(player.m_accel_y+gravity)*dt;
 player.m_accel_x=ax; player.m_accel_y=gravity;
 player.m_grounded=0;
 player_contacts(game,player,previous,was_grounded,command);
 if(player.m_dead) { advance_animation(game,player); return; }
 if(player.m_should_start_climbing && !command.jump.pressed && !player.m_grounded) {
  player.m_climbing=1; player.m_jumps=0; player.m_accel_y=0;
 } else if(player.m_should_stop_climbing || player.m_grounded) {
  if(player.m_climbing) player.m_was_climbing=1;
  player.m_climbing=0;
 }
 std::string state=player.m_grounded?(input!=0?"walk":"idle"):player.m_climbing?"climb":player.m_jumps==2?"double_jump":"jump";
 if(player.m_animation_state!=state) change_animation_state(player,state);
 else if(state!="climb" || command.up.pressed || command.down.pressed) advance_animation(game,player,state=="climb"?.5:1);
 if(player.m_speed_x>.01) player.m_facing=1;
 else if(player.m_speed_x<-.01) player.m_facing=0;
}
Thing_idx boss_projectile(Game& game,Thing_idx parent,Vec2 pos,Vec2 velocity,bool split) {
 const auto idx=make_entity(game,EntityType::boss_projectile,parent,pos.x,pos.y);
 if(!idx) return 0;
 auto& proj=game.m_things.get(idx);
 proj.m_speed_x=velocity.x; proj.m_speed_y=velocity.y; proj.m_should_rebound=split;
 for(std::size_t i=0;i<proj.m_trail.size();i+=2) { proj.m_trail[i]=pos.x; proj.m_trail[i+1]=pos.y; }
 return idx;
}
void update_boss(Game& game,Thing& boss) {
 advance_animation(game,boss);
 if(boss.m_animation_state=="dead" && !boss.m_dead) {
  boss.m_dead=true; boss.m_trigger_events.push_back(TriggerActionType::death);
 }
 if(boss.m_dead) return;
 const double ticks=game.m_delta_time/positive(boss.m_tick_duration,.01);
 const double duration=std::max(1.0,get_animation_state_duration(game,boss,"tir")*game.m_delta_time/positive(boss.m_tick_duration,.01));
 boss.m_attack_tick+=ticks;
 if(boss.m_attack_tick>=std::max(0.0,boss.m_next_attack-duration) && boss.m_animation_state!="tir") change_animation_state(boss,"tir");
 if(!boss.m_shot_fired && boss.m_attack_tick>=std::max(0.0,boss.m_next_attack-duration/2)) {
  boss.m_shot_fired=true;
  const double angle=std::uniform_real_distribution<double>(-pi,0)(game.m_random);
  const double speed=game.m_context.m_boss_proj_speed;
  if(boss_projectile(game,boss.parent_idx,{boss.x+boss.xmax*boss.m_scale/2,boss.y},{std::cos(angle)*speed,std::sin(angle)*speed},true)) {
   game.queue_sound("spawn_proj","boss_particle.pcm"); boss.m_trigger_events.push_back(TriggerActionType::shot);
  }
 }
 if(boss.m_attack_tick>=boss.m_next_attack) {
  boss.m_attack_tick=0; boss.m_shot_fired=false; change_animation_state(boss,"idle");
  const double delay=std::normal_distribution<double>(boss.m_boss_avg_attack_delay,positive(boss.m_boss_attack_variance,epsilon))(game.m_random);
  boss.m_next_attack=std::max(.05,delay)/positive(boss.m_tick_duration,.01);
 }
}
void update_boss_projectile(Game& game,Thing& proj) {
 const double dt=game.m_delta_time;
 proj.m_trail_index=wrap(proj.m_trail_index,10);
 proj.m_trail[static_cast<std::size_t>(proj.m_trail_index)*2]=proj.x;
 proj.m_trail[static_cast<std::size_t>(proj.m_trail_index)*2+1]=proj.y;
 proj.m_trail_index=(proj.m_trail_index+1)%10;
 proj.m_speed_y+=game.m_context.m_boss_proj_gravity*dt;
 const Vec2 previous{proj.x,proj.y};
 proj.x+=proj.m_speed_x*dt; proj.y+=proj.m_speed_y*dt;
 const auto extent=world_size(proj);
 bool collision=false,horizontal=false;
 if(proj.y>=extent.y) { proj.y=extent.y-.001; collision=horizontal=true; }
 else if(proj.x<=0 || proj.x>=extent.x) { proj.x=std::clamp(proj.x,.001,std::max(.001,extent.x-.001)); collision=true; }
 if(!collision) for(const auto idx:game.m_things.children(proj.parent_idx,true)) {
  const auto& thing=game.m_things.get(idx);
  if(!solid(thing.m_kind)||!thing.m_collision) continue;
  const auto b=bounds(thing);
  // Swept point vs rectangle prevents skipping thin platforms at long frames.
  const double dx=proj.x-previous.x,dy=proj.y-previous.y;
  double enter=0,leave=1; bool axis_y=false,hit=true;
  for(int axis=0;axis<2;++axis) {
   const double start=axis?previous.y:previous.x,d=axis?dy:dx;
   const double low=axis?b.ymin:b.xmin,high=axis?b.ymax:b.xmax;
   if(std::abs(d)<epsilon) { if(start<low || start>high) hit=false; continue; }
   double a=(low-start)/d,c=(high-start)/d;
   if(a>c) std::swap(a,c);
   if(a>enter) { enter=a; axis_y=axis!=0; }
   leave=std::min(leave,c);
   if(enter>leave) hit=false;
  }
  if(hit && enter<=1 && leave>=0) {
   collision=true; horizontal=axis_y;
   proj.x=previous.x+dx*enter; proj.y=previous.y+dy*enter;
   break;
  }
 }
 if(!collision) return;
 if(proj.m_should_rebound) {
  const double vx=horizontal?proj.m_speed_x:-proj.m_speed_x,vy=horizontal?-proj.m_speed_y:proj.m_speed_y;
  const double angle=std::atan2(vy,vx),speed=std::hypot(vx,vy)*game.m_context.m_boss_proj_spring;
  const int count=static_cast<int>(std::clamp(game.m_context.m_boss_proj_split_count,0.0,static_cast<double>(MAX_THINGS)));
  const Vec2 pos{proj.x+(horizontal?0:std::copysign(.0001,vx)),proj.y+(horizontal?std::copysign(.0001,vy):0)};
  for(int i=0;i<count;++i) {
   const double direction=angle+std::normal_distribution<double>(0,pi/16)(game.m_random);
   if(!boss_projectile(game,proj.parent_idx,pos,{std::cos(direction)*speed,std::sin(direction)*speed},false)) break;
  }
  game.queue_sound("split_proj","particle_sol.pcm");
 }
 remove_hierarchy(game.m_things,proj.m_idx);
}
void update_laser(Game& game,Thing& laser) {
 laser.m_attack_tick+=game.m_delta_time;
 if(laser.m_attack_tick<std::max(0.0,laser.m_laser_delay_attack)) return;
 laser.m_attack_tick=0; laser.m_laser_delta=game.m_delta_time;
 const auto idx=make_entity(game,EntityType::laser_projectile,laser.parent_idx,laser.x+.06*std::cos(laser.m_angle),laser.y+.06*std::sin(laser.m_angle));
 if(!idx) return;
 auto& proj=game.m_things.get(idx);
 proj.xmax=proj.ymax=std::max(.0001,laser.m_laser_proj_size/4);
 proj.m_angle=laser.m_angle; proj.m_laser_proj_speed=laser.m_laser_proj_speed;
 proj.m_laser_proj_fold=laser.m_laser_proj_fold; proj.m_laser_proj_size=laser.m_laser_proj_size;
 proj.m_laser_proj_start_pos={proj.x,proj.y}; proj.m_laser_proj_scale_y=laser.m_laser_proj_scale_y;
 proj.m_laser_ondulating=laser.m_laser_ondulating; proj.m_laser_ondulating_color=laser.m_laser_ondulating_color;
 laser.m_trigger_events.push_back(TriggerActionType::shot);
}
void update_laser_projectile(Game& game,Thing& proj) {
 proj.x+=std::cos(proj.m_angle)*proj.m_laser_proj_speed*game.m_delta_time;
 proj.y+=std::sin(proj.m_angle)*proj.m_laser_proj_speed*game.m_delta_time;
 const auto extent=world_size(proj);
 const double margin=std::max(.001,proj.m_laser_proj_size);
 if(proj.x>extent.x+margin || proj.x<-margin || proj.y>extent.y+margin || proj.y<-margin) remove_hierarchy(game.m_things,proj.m_idx);
}
// Return true when restarting invalidates every reference into the pool.
bool fire_trigger(Game& game,const Trigger& trigger) {
 switch(trigger.m_trigger_type) {
 case TriggerType::activate:case TriggerType::deactivate:
 case TriggerType::add_to_count:case TriggerType::remove_from_count:
  for(auto idx:trigger.m_trigger_target) {
   auto& target=game.m_things.get(idx); if(!ent_valid(target)) continue;
   switch(trigger.m_trigger_type) {
   case TriggerType::activate:target.m_active=true; target.m_trigger_events.push_back(TriggerActionType::activated);break;
   case TriggerType::deactivate:target.m_active=false; target.m_trigger_events.push_back(TriggerActionType::deactivated);break;
   case TriggerType::add_to_count:target.m_count+=1;break;
   case TriggerType::remove_from_count:target.m_count-=1;break;
   default:break;
   }
  }
  break;
 case TriggerType::win:game.stop_sound(); game.queue_sound("win","end_win.pcm"); switch_game_to_scene(game,SceneType::game_win);break;
 case TriggerType::lose:switch_game_to_scene(game,SceneType::game_over);break;
 case TriggerType::start_game:game.start_game(true);return true;
 case TriggerType::no_trigger:break;
 }
 return false;
}
bool update_triggers(Game& game,Thing& thing,const CommandState& command) {
 // Consume a snapshot: events emitted onto this entity while executing actions
 // survive until next update rather than being accidentally cleared.
 auto events=std::move(thing.m_trigger_events); thing.m_trigger_events.clear();
 const double count=thing.m_count;
 bool consumed_count=false;
 for(auto& trigger:thing.m_triggers) {
  bool event=std::find(events.begin(),events.end(),trigger.m_trigger_action)!=events.end();
  if(thing.m_active && trigger.m_trigger_action==TriggerActionType::on_start && command.jump.positive_edge) event=true;
  if(thing.m_active && trigger.m_trigger_action==TriggerActionType::on_count && count>=trigger.m_trigger_count_target) { event=true; consumed_count=true; }
  if(event && !trigger.m_triggered) {
   const double probability=std::clamp(trigger.m_trigger_percentage,0.0,1.0);
   if(probability>=1 || std::uniform_real_distribution<double>(0,1)(game.m_random)<probability) {
    trigger.m_triggered=true; trigger.m_trigger_elapsed=0;
   }
  }
  if(!trigger.m_triggered) continue;
  trigger.m_trigger_elapsed+=game.m_delta_time;
  if(trigger.m_trigger_elapsed+epsilon>=std::max(0.0,trigger.m_trigger_delay)) {
   trigger.m_triggered=false; trigger.m_trigger_elapsed=0;
   const Trigger action=trigger;
   if(fire_trigger(game,action)) return true;
  }
 }
 if(consumed_count) thing.m_count-=count;
 return false;
}
} // namespace

Thing_idx make_entity(Game& game,EntityType kind,Thing_idx parent,double x,double y) {
 if(kind==EntityType::nil) return 0;
 if(parent && !ent_valid(game.m_things.get(parent))) { game.send_error("Cannot attach entity to an invalid parent"); return 0; }
 const auto idx=game.m_things.add(kind);
 if(!idx) { game.send_error("Entity pool exhausted"); return 0; }
 auto& thing=game.m_things.get(idx);
 thing.x=x; thing.y=y; thing.m_scale=default_scale(game,kind);
 if(parent && !add_child(game.m_things,parent,idx)) { game.m_things.remove(idx); return 0; }
 switch(kind) {
 case EntityType::player:
  thing.xmax=thing.ymax=1.5;
  animation(thing,"idle","chienpo_idle.ild",10);
  animation(thing,"walk","chienpo_walk-4f.ild",4);
  animation(thing,"double_jump","chienpo_doublejump.ild",4);
  animation(thing,"jump","chienpo_jump.ild",10);
  animation(thing,"climb","chienpo_climb_2f.ild",4);
  animation(thing,"dead","chienpo_dead-b.ild",10);
  break;
 case EntityType::neon_boss:
  thing.xmax=thing.ymax=1.5;
  animation(thing,"idle","boss-puck_01_idle.ild",7);
  animation(thing,"tir","boss-puck_02_action-tir.ild",4);
  animation(thing,"dead","boss-puck_03_win.ild",10,false);
  animation(thing,"dead","boss-puck_04_win_idle.ild",10,false);
  thing.m_boss_avg_attack_delay=game.m_context.m_boss_avg_attack_delay;
  thing.m_boss_attack_variance=game.m_context.m_boss_attack_variance;
  thing.m_tick_duration=.01;
  thing.m_next_attack=std::max(.05,std::normal_distribution<double>(thing.m_boss_avg_attack_delay,positive(thing.m_boss_attack_variance,epsilon))(game.m_random))*100;
  break;
 case EntityType::portal:
  thing.xmax=thing.ymax=.2; thing.m_sprite_offset_x=.03;
  animation(thing,"idle","warpzone_spiral.ild",5);
  break;
 case EntityType::platform:case EntityType::move_platform:case EntityType::masking_box:
  thing.xmax=.2; thing.ymax=.01;
  if(kind==EntityType::masking_box) thing.m_show=false;
  break;
 case EntityType::laser:
  thing.m_laser_delay_attack=game.m_context.m_laser_delay_attack;
  thing.m_laser_delay_alarm=.5;
  break;
 case EntityType::boss_projectile:thing.xmax=thing.ymax=.01;break;
 case EntityType::laser_projectile:thing.xmax=thing.ymax=.0075;break;
 case EntityType::decor:thing.xmax=thing.ymax=1;thing.m_collision=false;break;
 case EntityType::trigger:case EntityType::spawner:thing.m_collision=false;break;
 default:break;
 }
 if(!thing.m_animation_path_map.empty()) { change_animation_state(thing,"idle"); load_animation_list(game,thing.m_animation_path_map); }
 return idx;
}
Thing_idx spawn_player(Game& game,Thing_idx scene,double x,double y) { return make_entity(game,EntityType::player,scene,x,y); }

void update_gameplay(Game& game,Thing& scene,const CommandState& command) {
 const auto scene_idx=scene.m_idx;
 const auto entities=game.m_things.children(scene_idx);
 bool has_start_trigger=false;
 for(auto idx:entities) for(const auto& trigger:game.m_things.get(idx).m_triggers)
  if(trigger.m_trigger_action==TriggerActionType::on_start) has_start_trigger=true;
 if((scene.m_scene_type==SceneType::game_win || scene.m_scene_type==SceneType::game_over) && command.jump.positive_edge && !has_start_trigger) { game.start_game(true); return; }
 for(auto idx:entities) {
  auto& thing=game.m_things.get(idx);
  if(thing.m_active && thing.m_kind==EntityType::move_platform) move_platform(game,thing);
 }
 for(auto idx:entities) {
  auto& thing=game.m_things.get(idx);
  if(!ent_valid(thing) || !thing.m_active || thing.parent_idx!=scene_idx || thing.new_parent_after_transaction) continue;
  switch(thing.m_kind) {
  case EntityType::player:update_player(game,thing,command);break;
  case EntityType::neon_boss:update_boss(game,thing);break;
  case EntityType::boss_projectile:update_boss_projectile(game,thing);break;
  case EntityType::laser:update_laser(game,thing);break;
  case EntityType::laser_projectile:update_laser_projectile(game,thing);break;
  case EntityType::decor:case EntityType::portal:advance_animation(game,thing);break;
  default:break;
  }
 }
 // Inactive targets must still be able to respond to deactivated events and
 // finish already queued delays, otherwise activation chains can deadlock.
 for(auto idx:entities) {
  auto& thing=game.m_things.get(idx);
  if(!ent_valid(thing) || thing.parent_idx!=scene_idx) continue;
  if(update_triggers(game,thing,command)) return;
 }
}

void update_editor(Game& game,Thing& scene,const CommandState& command) {
 auto entities=game.m_things.children(scene.m_idx);
 for(auto idx:entities) {
  auto& thing=game.m_things.get(idx);
  if(thing.m_animation_playing || (thing.m_kind==EntityType::decor && thing.m_playing)) advance_animation(game,thing);
  if(scene.m_debug_moving_platform && thing.m_active && thing.m_kind==EntityType::move_platform) move_platform(game,thing);
 }
 auto selected=std::find(entities.begin(),entities.end(),game.m_entity_selected_index);
 if(command.next_entity.positive_edge || command.prev_entity.positive_edge) {
  if(entities.empty()) game.m_entity_selected_index=0;
  else {
   int pos=selected==entities.end()?(command.next_entity.positive_edge?-1:0):static_cast<int>(selected-entities.begin());
   pos=wrap(pos+(command.next_entity.positive_edge?1:-1),static_cast<int>(entities.size()));
   game.m_entity_selected_index=entities[static_cast<std::size_t>(pos)];
  }
  return;
 }
 const std::pair<bool,EntityType> additions[]{
  {command.add_platform.positive_edge,EntityType::platform},{command.add_moving_platform.positive_edge,EntityType::move_platform},
  {command.add_masking_box.positive_edge,EntityType::masking_box},{command.add_neon_boss.positive_edge,EntityType::neon_boss},
  {command.add_portal.positive_edge,EntityType::portal},{command.add_decor.positive_edge,EntityType::decor},
  {command.add_laser.positive_edge,EntityType::laser},{command.add_trigger_thing.positive_edge,EntityType::trigger}};
 for(auto [pressed,kind]:additions) if(pressed) {
  const auto& device=scene.parent();
  const auto idx=make_entity(game,kind,scene.m_idx,.4/positive(device.m_base_scale_x,1),.49/positive(device.m_base_scale_y,1));
  if(idx) game.m_entity_selected_index=idx;
 }
 auto& thing=game.m_things.get(game.m_entity_selected_index);
 if(!ent_valid(thing) || thing.parent_idx!=scene.m_idx) return;
 const bool moving=thing.m_kind==EntityType::move_platform;
 const bool box=moving || thing.m_kind==EntityType::platform || thing.m_kind==EntityType::masking_box;
 if(command.delete_key.positive_edge) {
  if(moving && !command.shift.pressed && !thing.m_path_point.empty()) {
   thing.m_path_point.pop_back(); thing.m_path_point_size=static_cast<int>(thing.m_path_point.size());
   thing.m_current_node=wrap(thing.m_current_node,thing.m_path_point_size);
  } else {
   const auto next=thing.next_child_idx==thing.m_idx?0:thing.next_child_idx;
   remove_hierarchy(game.m_things,thing.m_idx); game.m_entity_selected_index=next; return;
  }
 }
 if(box && (command.next.positive_edge || command.prev.positive_edge)) thing.m_selection_sub_index=thing.m_selection_sub_index?0:1;
 if(moving) {
  if(scene.m_debug_moving_platform) return;
  if(command.loop_platform.positive_edge) thing.m_loop=!thing.m_loop;
  if(command.confirm.positive_edge) {
   if(thing.m_path_point.size()<64) thing.m_path_point.push_back({thing.x,thing.y});
   else game.send_error("A moving-platform path is limited to 64 points");
  }
  if(command.cancel.positive_edge && !thing.m_path_point.empty()) { thing.x=thing.m_path_point.back().x; thing.y=thing.m_path_point.back().y; }
  thing.m_path_point_size=static_cast<int>(thing.m_path_point.size());
 }
 const double speed=game.m_context.m_cursor_speed*game.m_delta_time*(command.shift.pressed?.05:1);
 const double dx=speed*(static_cast<int>(command.right.pressed)-static_cast<int>(command.left.pressed));
 const double dy=speed*(static_cast<int>(command.down.pressed)-static_cast<int>(command.up.pressed));
 if(box && thing.m_selection_sub_index) {
  thing.xmax=std::max(thing.xmin+.001,thing.xmax+dx);
  if(thing.m_kind!=EntityType::platform) thing.ymax=std::max(thing.ymin+.001,thing.ymax+dy);
 } else { thing.x+=dx; thing.y+=dy; }
 if(command.scale_up.pressed) thing.m_scale+=speed;
 if(command.scale_down.pressed) thing.m_scale=std::max(.001,thing.m_scale-speed);
 if(command.reset_scale.positive_edge) thing.m_scale=default_scale(game,thing.m_kind);
 if(thing.m_kind==EntityType::laser) {
  thing.m_angle+=(static_cast<int>(command.next.pressed)-static_cast<int>(command.prev.pressed))*speed*pi;
 }
 if(thing.m_kind==EntityType::portal && (command.next.positive_edge || command.prev.positive_edge)) {
  std::vector<Thing_idx> portals;
  for(const auto& portal:game.m_things) if(portal.m_kind==EntityType::portal && portal.m_idx!=thing.m_idx) portals.push_back(portal.m_idx);
  if(!portals.empty()) {
   const auto it=std::find(portals.begin(),portals.end(),thing.m_portal_destination_idx);
   const int pos=it==portals.end()?(command.next.positive_edge?-1:0):static_cast<int>(it-portals.begin());
   thing.m_portal_destination_idx=portals[static_cast<std::size_t>(wrap(pos+(command.next.positive_edge?1:-1),static_cast<int>(portals.size())))];
  }
 }
}
} // namespace chienpo

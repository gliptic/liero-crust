#pragma once
#include <tl/cstdint.h>
#include <tl/vector.hpp>
#include <tl/approxmath/am.hpp>
#include "../lierosim/object_list.hpp"

namespace test_ships {

	struct Tank {
		tl::VectorF2 pos;
		f32 vel;
		u32 hp;
		u32 cooldown;
		f32 rot;
		f32 aim;
	};

	struct Bullet {
		tl::VectorF2 pos;
		tl::VectorF2 vel;
		u32 owner, life;
	};

	static f32 const rot_speed = 0.05;
	static f32 const aim_speed = 0.04;
	static f32 const shoot_dist = 35;
	static f32 const hit_dist = 30;
	static f32 const tank_w = 31.0;
	static f32 const tank_h = 17.0;

	inline tl::VectorF2 dir(f32 angle) {
		return tl::VectorF2(am_sinf(tl::pi / 2.0 - angle), am_sinf(angle));
	}

	struct State {
		u32 frame;
		Tank tanks[2];
		liero::FixedObjectList<Bullet, 50> bullets;

		typedef u32 Controls;

		static u32 const c_left = 1 << 0;
		static u32 const c_right = 1 << 1;
		static u32 const c_forward = 1 << 2;
		static u32 const c_back = 1 << 3;
		static u32 const c_aim_left = 1 << 4;
		static u32 const c_aim_right = 1 << 5;
		static u32 const c_fire = 1 << 6;

		struct TransientState {
			Controls tank_controls[2];
		};

		State() {
			frame = 0;
		}

		void copy_from(State const& other) {
			this->frame = other.frame;
			this->bullets = other.bullets;
			memcpy(&this->tanks, &other.tanks, sizeof(Tank) * 2);
		}

		void processTank(State& state, Controls& controls, Tank& tank, u32 id) {
			if ((controls & (c_left | c_right)) == c_left) {
				tank.rot -= rot_speed;
			}
			else if ((controls & (c_left | c_right)) == c_right) {
				tank.rot += rot_speed;
			}

			if ((controls & (c_aim_left | c_aim_right)) == c_aim_left) {
				tank.aim -= aim_speed;
			}
			else if ((controls & (c_aim_left | c_aim_right)) == c_aim_right) {
				tank.aim += aim_speed;
			}

			if ((controls & (c_forward | c_back)) == c_forward) {
				tank.vel += 0.1;
			}
			else if ((controls & (c_forward | c_back)) == c_back) {
				tank.vel -= 0.1;
			}

			auto friction = 0.99;
			tank.vel = tank.vel * friction;

			auto heading = dir(tank.rot);

			tank.pos += heading * tank.vel;

			tank.pos.x = tl::max(tl::min(tank.pos.x, 1024.0f), .0f);
			tank.pos.y = tl::max(tl::min(tank.pos.y, 768.0f), .0f);

			if (tank.cooldown == 0 && controls & c_fire) {
				auto aiming = dir(tank.aim);
				tank.cooldown = 10;
				auto* bullet = state.bullets.new_object_reuse();
				bullet->pos = tank.pos + aiming * shoot_dist;
				bullet->vel = aiming * 6.0 + heading * tank.vel * 0.3;
				bullet->owner = id;
				bullet->life = 100;
			}
			else if (tank.cooldown > 0) {
				--tank.cooldown;
			}
		}

		bool coldetTank(tl::VectorF2 bullet_pos, Tank& tank) {
			auto rel = tl::unrotate(bullet_pos - tank.pos, dir(tank.rot));
			if (rel.x >= -tank_w && rel.x <= tank_w
		     && rel.y >= -tank_h && rel.y <= tank_h
		     && tank.hp > 0) {
				tank.hp -= 1;
				return true;
			}

			return false;
		}

		void process(TransientState& t_state) {
			auto r = this->bullets.all();

			u32 swap = frame & 1;
			++frame;

			for (Bullet* b; (b = r.next()) != 0; ) {
				b->pos += b->vel;
				b->life -= 1;

				if ((b->owner != swap) && coldetTank(b->pos, tanks[swap])
					|| (b->owner != (swap ^ 1)) && coldetTank(b->pos, tanks[swap ^ 1])
					|| b->life == 0) {

					this->bullets.free(r);
				}
			}

			processTank(*this, t_state.tank_controls[swap], tanks[swap], swap);
			processTank(*this, t_state.tank_controls[swap ^ 1], tanks[swap ^ 1], swap ^ 1);
		}
	};
}
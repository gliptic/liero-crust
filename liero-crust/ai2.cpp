#include "ai2.hpp"
#include <tl/vector.hpp>

namespace liero {
namespace ai {

inline Scalar ang_mask(Scalar x) {
	auto mask = (1 << (16 + 7)) - 1;
	return Scalar::from_raw(x.raw() & mask);
}

inline Scalar ang_mask_signed(Scalar x) {
	return Scalar::from_raw((x.raw() << 9) >> 9);
}

inline Scalar ang_diff(Scalar a, Scalar b) {
	return ang_mask_signed(a - b);
}

Node* select(NodeChildren& node) {
	u32 tried = u32(node.children.size());

	++node.n;
	if (tried * tried < node.n) {
		// Try new
		return 0;
	} else {
		// Pick existing
		f64 e = std::log(f64(node.n));

		f64 max = -1.0;
		usize max_i = 0;
		for (usize i = 0; i < node.children.size(); ++i) {
			auto& ch = *node.children[i].node;
			auto b = ch.b(e);
			if (b > max) {
				max = b;
				max_i = i;
			}
		}

		//expand(*node.children[max_i].node);
		return node.children[max_i].node.get();
	}
}

void dummy_play_sound(liero::ModRef&, i16, TransientState&) {
	// Do nothing
}

f64 compute_reward(State& state, u32 worm_index) {
	u32 other_worm_index = worm_index ^ 1;
	auto& worm = state.worms.of_index(worm_index);
	auto& other_worm = state.worms.of_index(other_worm_index);
	
	//return 1.0 / std::log(2.72 + tl::length(other_worm.pos.cast<f64>() - worm.pos.cast<f64>()));
	//return tl::min((100 - other_worm.health) / 100.0, 1.0);
	return (0.5 + worm.health) / f64(1.0 + worm.health + other_worm.health);
	//return worm.health / 100.0;
}

WormInput generate_single(Ai2& ai, State& state, Worm& worm, Worm& target) {

	//if (ai.rand.get_u32_fast_(3) == 0) {
	if (false) { // TEMP

		//u8 act = ai.rand.get_u32_fast_(6) << 4;
		u8 act = 0; // TEMP
		u8 move = ai.rand.get_u32_fast_(4) << 2;
		u8 aim = ai.rand.get_u32_fast_(4);

		return WormInput(act, move, aim);
	} else {
		auto cur_aim = worm.abs_aiming_angle();

		auto dir = target.pos.cast<f64>() - worm.pos.cast<f64>();

		f64 radians = atan2(dir.y, dir.x);
		auto ang_to_target = ang_mask(Scalar::from_raw(i32(radians * (65536.0 * 128.0 / tl::pi2))));

		auto tolerance = Scalar(4);

		auto aim_diff = ang_diff(ang_to_target, cur_aim);

		bool fire = aim_diff >= -tolerance
			&& aim_diff <= tolerance /*
									 && obstacles(game, &worm, target) < 4*/;

		bool move_right = worm.pos.x < target.pos.x;

		bool aim_up = move_right ^ (aim_diff > Scalar());

		if (ai.rand.next() < 0xffffffff / 120) {
			return WormInput::change(WormInput::Move::Left, aim_up ? WormInput::Aim::Up : WormInput::Aim::Down);
		} else {
			return WormInput::combo(false, fire,
				move_right ? WormInput::Move::Right : WormInput::Move::Left,
				aim_up ? WormInput::Aim::Up : WormInput::Aim::Down);
		}
	}
}

void generate(Ai2& ai, State& state, TransientState& transient_state, u32 worm_index, Node& dest_node) {
	for (u32 i = 0; i < 30; ++i) {
		Worm& target = state.worms.of_index(worm_index ^ 1);
		Worm& worm = state.worms.of_index(worm_index);

		auto own_inp = generate_single(ai, state, worm, target);
		auto other_inp = generate_single(ai, state, target, worm);
		auto inp = WormInputs(own_inp, other_inp);
		dest_node.inputs.push_back(inp);

		transient_state.init(state.worms.size(), dummy_play_sound, 0);
		transient_state.graphics = false;
		transient_state.respawn = false;
		transient_state.worm_state[worm_index].input = inp.own;
		transient_state.worm_state[worm_index ^ 1].input = inp.other;
		state.update(transient_state);
	}
}

void Ai2::do_ai(State& state, Worm& worm, u32 worm_index, WormTransientState& transient_state) {
	tl::Vec<Node*> selected;

	for (u32 r = 0; r < 16; ++r) {

		selected.clear();

		Node* node = this->root.get();

		usize last_known_state_idx = 0;

		while (true) {
			selected.push_back(node);

			if (node->tail_state) {
				last_known_state_idx = selected.size();
			}

			Node* ch = select(*node);

			if (!ch) {
				
				// * Find the closest known state, apply intervening actions
				// * Evaluate reward
			
				if (last_known_state_idx == 0) {
					this->spare_state.copy_from(state, false);
				} else {
					this->spare_state.copy_from(*selected[last_known_state_idx - 1]->tail_state, false);
				}

				for (usize i = last_known_state_idx; i < selected.size(); ++i) {
					auto& prev_node = *selected[i];
					auto& n = prev_node.inputs;
					for (usize ii = prev_node.first; ii < n.size(); ++ii) {
						auto& inp = n[ii];
						this->spare_transient_state.init(state.worms.size(), dummy_play_sound, 0);
						this->spare_transient_state.graphics = false;
						this->spare_transient_state.respawn = false;
						this->spare_transient_state.worm_state[worm_index].input = inp.own;
						this->spare_transient_state.worm_state[worm_index ^ 1].input = inp.other;
						this->spare_state.update(this->spare_transient_state);
					}
				}

				ChildRef new_child(new Node());
				new_child.node->n = 1; // TEMP. Some other solution?
				selected.push_back(new_child.node.get());

				generate(*this, this->spare_state, this->spare_transient_state, worm_index, *new_child.node);

				node->children.push_back(move(new_child));
				f64 reward = compute_reward(this->spare_state, worm_index);

				for (auto& s : selected) {
					s->stats.add(reward, s->n);
				}

				break;
			} else {
				node = ch;
			}
		}
	}

	{
		f64 max = -1.0;
		WormInput best;

		for (auto& ch : this->root->children) {
			if (ch.node->stats.mean > max) {
				max = ch.node->stats.mean;
				best = ch.node->inputs[ch.node->first].own;
			}
		}

		transient_state.input = best;
	}
}

void cut_front(Node& root, WormInputs correct_inputs) {
	usize i = 0, max = root.children.size();
	for (; i < max; ) {
		auto& ch = *root.children[i].node;
		auto& hd = ch.inputs[ch.first];
		if (hd.own != correct_inputs.own)
			++ch.deviation;
#if 0
		if (hd.other != correct_inputs.other)
			++ch.deviation;
#endif

		if (ch.deviation < 8) {
			++ch.first;
			ch.deviation *= 2;
			if (ch.first == ch.inputs.size()) {
				// Expand ch.node children into root
				auto children = move(ch.children);
				// TODO: Reuse tail_states in ch subtree
				root.children.remove_swap(i);
				--max;

				for (auto& c : children) {
					root.children.push_back(move(c));
				}
				
			} else {
				++i;
			}
		} else {
			// TODO: Reuse tail_states in ch subtree
			root.children.remove_swap(i);
			--max;
		}
	}
}

void Ai2::done_frame(u32 worm_index, TransientState& transient_state) {
	cut_front(*this->root,
		WormInputs(
			transient_state.worm_state[worm_index].input,
			transient_state.worm_state[worm_index ^ 1].input));
}

}
}
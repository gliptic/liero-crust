#include <tl/rand.hpp>
#include <stdio.h>
#include <tl/vec.hpp>
#include <tl/bits.h>
#include <tl/std.h>

template<typename F, typename T>
inline void print_timer(char const* name, T limits[32], F f) {
	i32 sum = 0;

	u64 time = tl::timer([&] {
#ifdef NDEBUG
		for (int i = 0; i < 220000000; ++i) {
#else
		for (int i = 0; i < 12; ++i) {
#endif
			i32 v = f(limits[i & 31]);
			sum += v;
		}
	});

	printf("%s: %f ms, %d\n", name, time / 10000.0, sum);
}

u32 binom(tl::LcgPair& r, u32 n, double p) {
	u32 x = 0;
	
	for (u32 i = 0; i < n; ++i) {
		x += (r.next() / (double)0x100000000ull) < p;
	}

	return x;
}

struct BinomAlias {
	tl::Vec<double> pr;
	tl::Vec<double> u;
	tl::Vec<u32> k;

	u32 n() {
		return pr.size() - 1;
	}

	// n = 1
	BinomAlias(double p1) {
		pr.push_back(1.0 - p1);
		pr.push_back(p1);
	}

	u32 sample(tl::LcgPair& r) {
		auto bucket = r.get_u32_fast_(pr.size());
		auto coin = r.next() / (double)0x100000000ull;
		auto lim = u[bucket];
		if (coin < lim) {
			return bucket;
		} else {
			return k[bucket];
		}
	}

	// n = a.n() + b.n()
	BinomAlias(BinomAlias& a, BinomAlias& b) {
		auto a_n = a.n();
		auto b_n = b.n();
		auto new_n = a_n + b_n;
		for (u32 i = 0; i <= new_n; ++i) {

			u32 min_a = i > b_n ? i - b_n : 0;

			double sum = 0.0;
			for (u32 j = min_a; i >= j && j <= a_n; ++j) {
				sum += a.pr[j] * b.pr[i - j];
			}

			this->pr.push_back(sum);
		}

		tl::Vec<u32> overfull, underfull;

		for (u32 i = 0; i <= new_n; ++i) {
			double p = new_n * pr[i];
			u.push_back(p);
			k.push_back(i);

			if (p > 1.0) {
				overfull.push_back(i);
			} else if (p < 1.0) {
				underfull.push_back(i);
			}
		}

		while (!overfull.empty() || !underfull.empty()) {
			if (underfull.empty()) {
				u[overfull.back()] = 1.0;
				overfull.pop_back();

			} else if (overfull.empty()) {
				u[underfull.back()] = 1.0;
				underfull.pop_back();

			} else {

				auto over = overfull.back();
				overfull.pop_back();

				auto under = underfull.back();
				underfull.pop_back();


				double p = u[over] + u[under] - 1.0;
				u[over] = p;
				k[under] = over;
				//u[under] = 1.0;

				if (p > 1.0) {
					overfull.push_back(over);
				} else if (p < 1.0) {
					underfull.push_back(over);
				}
			}
		}
	}
};

struct BinomAliasI {
	tl::Vec<u64> pr;
	tl::Vec<u64> u;
	tl::Vec<u32> k;
	i32 lg2;

	u32 n() const {
		return pr.size() - 1;
	}

	u32 sample(tl::LcgPair& r) {
		auto entropy = r.next();
		auto coin = r.next() & 0x7fffffff; // entropy& (0x7fffffff >> this->lg2);
		//auto coin = entropy & (0x7fffffff >> this->lg2);
		auto bucket = entropy >> (32 - this->lg2);
		auto lim = u[bucket];
		if ((u64)coin < lim) {
			return bucket;
		} else {
			return k[bucket];
		}
	}

	void build_tables() {
		tl::Vec<u32> overfull, underfull;

		auto nplus1 = i32(1 << lg2);
		auto sum_p = u64(0x80000000) << this->lg2;
		auto p1 = sum_p >> this->lg2;

		for (u32 i = 0; i < nplus1; ++i) {
			auto p = pr[i];
			u.push_back(p);
			k.push_back(i);

			if (p > p1) {
				overfull.push_back(i);
			} else if (p < p1) {
				underfull.push_back(i);
			}
		}

		while (!overfull.empty()) {

			printf("%d %d\n", (u32)overfull.size(), (u32)underfull.size());


			auto over = overfull.back();
			overfull.pop_back();

			auto under = underfull.back();
			underfull.pop_back();

			auto p = u[over] - (p1 - u[under]);
			u[over] = p;
			k[under] = over;

			printf("removing %f from %d, leaving %f (%x)\n", (p1 - u[under]) / (double)sum_p, over, p / (double)sum_p, p);

			if (p > p1) {
				overfull.push_back(over);
			} else if (p < p1) {
				underfull.push_back(over);
			}
		}

		printf("table:\n");
		for (u32 i = 0; i < nplus1; ++i) {
			double total = p1 / (double)sum_p;
			if (k[i] == i) {
				printf("%f -> %d\n", total, i);
			} else {
				double p = u[i] / (double)sum_p;
				printf("%f (%d) -> %d, %f -> %d\n", p, u[i], i, total - p, k[i]);
			}
		}

		printf("%d %d\n", (u32)overfull.size(), (u32)underfull.size());
	}

	BinomAliasI(u64 p1) {
		this->lg2 = 1;
		pr.push_back(0x100000000 - p1);
		pr.push_back(p1);
	}

	BinomAliasI(BinomAliasI const& a, BinomAliasI const& b) {
		auto a_n = a.n();
		auto b_n = b.n();
		auto new_n = a_n + b_n;
		u64 total = 0;
		for (u32 i = 0; i <= new_n; ++i) {

			u32 min_a = i > b_n ? i - b_n : 0;

			u64 sum = 0;
			for (u32 j = min_a; i >= j && j <= a_n; ++j) {
				sum += (a.pr[j] * b.pr[i - j] >> 32);
			}

			this->pr.push_back(sum);
			total += sum;
		}

		u64 at = 0;
		for (u32 i = 0; i <= new_n; ++i) {
			u64 next_at = total * i / new_n;
			u64 error = next_at - at;
			this->pr[i] += error;
			at = next_at;
		}

		this->build_tables();
	}

	BinomAliasI(BinomAlias const& other) {

		tl::Vec<u32> overfull, underfull;

		//auto nplus1 = ;

		auto lg2 = tl_log2(other.pr.size() - 1) + 1;
		auto nplus1 = i32(1 << lg2);
		auto sum_p = u64(0x80000000) << lg2;
		this->lg2 = lg2;
		
		u64 prev = 0;
		double prev_d = 0.0;

		for (u32 i = 0; i < nplus1; ++i) {
			double prob = (i < other.pr.size() ? other.pr[i] : 0.0);
			double next_d = prev_d + prob;
			printf("[%d] %.17g\n", i, prob);

			auto next = i >= other.pr.size() - 1
				? sum_p
				: u64(sum_p * next_d);
			auto p = next - prev;

			pr.push_back(p);
			
			prev = next;
			prev_d = next_d;
		}

		this->build_tables();

		/*
		for (u32 i = 0; i < nplus1; ++i) {
			auto p = pr[i];
			u.push_back(p);
			k.push_back(i);

			if (p > p1) {
				overfull.push_back(i);
			} else if (p < p1) {
				underfull.push_back(i);
			}
		}

		while (!overfull.empty()) {

			printf("%d %d\n", (u32)overfull.size(), (u32)underfull.size());

			
			auto over = overfull.back();
			overfull.pop_back();

			auto under = underfull.back();
			underfull.pop_back();

			auto p = u[over] - (p1 - u[under]);
			u[over] = p;
			k[under] = over;

			printf("removing %f from %d, leaving %f (%x)\n", (p1 - u[under]) / (double)sum_p, over, p / (double)sum_p, p);

			if (p > p1) {
				overfull.push_back(over);
			} else if (p < p1) {
				underfull.push_back(over);
			}
		}

		printf("table:\n");
		for (u32 i = 0; i < nplus1; ++i) {
			double total = p1 / (double)sum_p;
			if (k[i] == i) {
				printf("%f -> %d\n", total, i);
			} else {
				double p = u[i] / (double)sum_p;
				printf("%f (%d) -> %d, %f -> %d\n", p, u[i], i, total - p, k[i]);
			}
		}

		printf("%d %d\n", (u32)overfull.size(), (u32)underfull.size());
		*/
	}
};

u32 samples_fib(u32 num) {
	u32 count = 0;
#define TRY_SUB(n) if (num >= n) { \
	num -= n; \
	++count; \
	}

	if (false) {
		//TRY_SUB(377);
		TRY_SUB(233);
		//TRY_SUB(144);
		TRY_SUB(89);
		//TRY_SUB(55);
		TRY_SUB(34);
		//TRY_SUB(21);
		TRY_SUB(13);
		//TRY_SUB(8);
		TRY_SUB(5);
		//TRY_SUB(3);
		TRY_SUB(2);
		TRY_SUB(1);
	} else if (false) {
		TRY_SUB(243);
		TRY_SUB(81);
		TRY_SUB(27);
		TRY_SUB(9);
		TRY_SUB(3);
		TRY_SUB(1);
	} else {
		TRY_SUB(255);
		TRY_SUB(255);
		TRY_SUB(127);
		TRY_SUB(127);
		TRY_SUB(63);
		TRY_SUB(63);
		TRY_SUB(31);
		TRY_SUB(31); // 15+15+1
		TRY_SUB(15);
		TRY_SUB(15); // 7+7+1
		TRY_SUB(7);
		TRY_SUB(7); // 3+3+1
		TRY_SUB(3);
		TRY_SUB(3); // 1+1+1
		TRY_SUB(1);
		TRY_SUB(1);
	}

	assert(num == 0);

	return count;
}

u32 samples_bin(u32 num) {
	u32 count = 0;

	while (num) {
		num = num & (num - 1);
		++count;
	}

	assert(num == 0);

	return count;
}

void test_rand() {
#if 0
	tl::LcgPair r;

	i32 limits[32];
	f64 flimits[32];

	for (int i = 0; i < 32; ++i) {
		limits[i] = 100 + r.get_i32_fast_(50000);
		flimits[i] = limits[i] / 4294967296.0;
	}

	print_timer("mul shift", limits, [&](i32 lim) { return r.get_i32_fast_(lim); });
	print_timer("float mul", flimits, [&](f64 lim) { return r.get_i32(lim); });
#else
	tl::LcgPair r;
	double test[33] = {};
	double test2[33] = {};

	BinomAlias b1(0.15);
	BinomAlias b2(b1, b1);
	BinomAlias b4(b2, b2);
	BinomAlias b8(b4, b4);
	BinomAlias b16(b8, b8);
	BinomAlias b32(b16, b16);

	BinomAliasI b16i(b16);
	BinomAliasI b32i(b32);

	BinomAlias b3(b1, b2);
	BinomAlias b5(b2, b3);
	//BinomAlias b8(b3, b5);
	BinomAlias b13(b5, b8);
	BinomAlias b21(b8, b13);
	BinomAlias b34(b13, b21);
	BinomAlias b55(b21, b34);
	BinomAlias b89(b34, b55);
	BinomAlias b144(b55, b89);

	BinomAlias b6(b3, b3);
	BinomAlias b7(b6, b1);

	BinomAlias b14(b7, b7);
	BinomAlias b15(b14, b1);

	BinomAlias b30(b15, b15);
	BinomAlias b31(b15, b1);


	for (u32 i = 0; i < 1000000; ++i) {
		//test[binom(r, 16, 0.15)] += 1.0 / 100000.0;
		//test[b16.sample(r)] += 1.0 / 1000000.0;
		test[b32i.sample(r)] += 1.0 / 1000000.0;
		test2[b32i.sample(r)] += 1.0 / 1000000.0;
	}

	for (u32 i = 0; i < 33; ++i) {
		printf("%f -- %f %f\n", b32.pr[i] - test[i], test[i], b32.pr[i]);
	}

	u32 sum_sf = 0, sum_sb = 0;

	for (u32 i = 200; i < 400; ++i) {
		u32 sf = samples_fib(i);
		u32 sb = samples_bin(i);

		sum_sf += sf;
		sum_sb += sb;
	}

	printf("%f %f\n", sum_sf / 200.0, sum_sb / 200.0);
#endif
}


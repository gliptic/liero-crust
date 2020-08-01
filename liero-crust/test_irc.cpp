#include "tl/io.hpp"
//#include "tl/windows/miniwindows.h"
#include "tl/io/async.hpp"
#include "tl/io/stream.hpp"
#include "tl/socket.hpp"
#include "tl/string.hpp"
#include <cmath>

struct VectorValue {
	static usize const Size = 384;

	float v[Size];

	static VectorValue zero() {
		VectorValue v;
		for (usize i = 0; i < Size; ++i) {
			v.v[i] = 0;
		}
		return v;
	}

	VectorValue& operator+=(VectorValue const& other) {
		for (usize i = 0; i < Size; ++i) {
			this->v[i] += other.v[i];
		}

		return *this;
	}

	VectorValue& operator-=(VectorValue const& other) {
		for (usize i = 0; i < Size; ++i) {
			this->v[i] -= other.v[i];
		}

		return *this;
	}

	float cossim(VectorValue const& other) const {
		float sq0 = 0.f, sq1 = 0.f, m = 0.f;

		for (usize i = 0; i < Size; ++i) {
			sq0 += this->v[i] * this->v[i];
			sq1 += other.v[i] * other.v[i];
			m += this->v[i] * other.v[i];
		}

		return m / (std::sqrt((double)sq0) * std::sqrt((double)sq1));
	}
};

struct Vector : VectorValue {
	tl::String word;
};

struct IrcServer {
	tl::Iocp iocp;
	tl::Socket sock;

	tl::Vec<Vector> wordvecs;

	static usize const no_word = usize(-1);

	usize find_word(tl::StringSlice str) {
		for (usize i = 0; i < wordvecs.size(); ++i) {
			if (wordvecs[i].word.slice_const() == str) {
				return i;
			}
		}

		return no_word;
	}

	IrcServer()
	 : iocp(tl::Iocp::create()), sock(tl::Socket::tcp()) {
	}

	void skip_ws(usize& i, usize size, tl::StringSlice const& msg) {
		for (; i < size && msg[i] == ' '; ) {
			++i;
		}
	}

	void handle_msg(tl::StringSlice msg) {
		usize i = 0;
		usize start = 0;
		usize size = msg.size();
		tl::StringSlice prefix;
		tl::Vec<tl::StringSlice> param;

		if (i < size && msg[i] == ':') {
			++i;
			start = i;
			for (; i < size && msg[i] != ' '; ) {
				++i;
			}

			prefix = tl::StringSlice(msg.begin() + start, msg.begin() + i);
			skip_ws(i, size, msg);
		}
		
		start = i;
		while (i <= size) {
			if (i == size || msg[i] == ' ') {
				param.push_back(tl::StringSlice(msg.begin() + start, msg.begin() + i));
				++i;
				skip_ws(i, size, msg);
				start = i;
			} else if (msg[i] == ':') {
				param.push_back(tl::StringSlice(msg.begin() + i + 1, msg.end()));
				break;
			} else {
				++i;
			}
		}

		if (param.size() < 1) {
			return;
		}

		tl::StringSlice const& cmd = param[0];
		if (cmd == "PING"_S && param.size() >= 2) {
			iocp.send(this->sock, iocp.new_send_op(tl::Vec<u8>::concat("PONG :"_S, param[1], "\r\n"_S)));
		} else if (cmd == "001"_S) {
			iocp.send(this->sock, iocp.new_send_op(tl::Vec<u8>::concat(
				"JOIN #liero\r\n"_S
				/*, "JOIN #liero\r\n"_S*/
			)));
		} else if (cmd == "PRIVMSG"_S && param.size() >= 3 && !prefix.empty()) {
			
			auto chan = param[1];
			auto msg = param[2];
			if (msg.size() > 1 && msg[0] == '%') {
				msg.unsafe_cut_front(1);

#if 0
				usize start = 0, size = msg.size();
				VectorValue val = VectorValue::zero();
				bool sign = true;
				bool valid = true;

				for (usize i = 0; i <= size; ++i) {
					if (i == size || msg[i] == '+' || msg[i] == '-') {
						auto w = find_word(tl::StringSlice(msg.begin() + start, msg.begin() + i));

						if (w == no_word) {
							iocp.send(this->sock, iocp.new_send_op(tl::Vec<u8>::concat("PRIVMSG "_S, chan, " :Dunno that word. Plz use lowercase because I suck\r\n"_S)));
							valid = false;
							break;
						}

						if (sign) {
							val += wordvecs[w];
						} else {
							val -= wordvecs[w];
						}

						if (i < size) {
							if (msg[i] == '+') {
								sign = true;
							} else {
								sign = false;
							}
						}

						start = i + 1;
					}
				}

				if (valid) {
					tl::Vec<u8> out_msg;
					out_msg.append("PRIVMSG "_S, chan, " :Closest words: "_S);

					most_similar_to(val, out_msg);
					out_msg.append("\r\n"_S);

					iocp.send(this->sock, iocp.new_send_op(move(out_msg)));
				}
#else
				tl::Vec<u8> out_msg;
				out_msg.append("PRIVMSG "_S, chan, " :"_S);

				
				for (auto word : msg.split(u8(' '))) {
					auto w = find_word(word);
					if (w != no_word) {
						most_similar_to(wordvecs[w], out_msg, 1);
					}
				}
				out_msg.append("\r\n"_S);
				iocp.send(this->sock, iocp.new_send_op(move(out_msg)));
#endif

				
			}
		}
	}

	struct WordCand {
		usize word;
		float score;
	};

	void most_similar_to(VectorValue const& wvec, tl::String& out, int only_index = -1) {
		static usize const WordCount = 10;
		WordCand words[WordCount];
		usize word_count = 0;
		//auto const& wvec = wordvecs[w];

		for (usize i = 0; i < wordvecs.size(); ++i) {
			auto score = wvec.cossim(wordvecs[i]);

			usize j = 0;
			for (j = 0; j < word_count; ++j) {
				if (words[j].score < score) {
					for (usize k = WordCount - 1; k >= j + 1; --k) {
						words[k] = words[k - 1];
					}
					words[j].word = i;
					words[j].score = score;
					break;
				}
			}

			if (j == word_count && word_count < WordCount) {
				words[word_count].word = i;
				words[word_count].score = score;
			}

			if (word_count < WordCount) {
				++word_count;
			}
		}

		if (only_index >= 0) {
			if ((usize)only_index < word_count) {
				out.append(wordvecs[words[(usize)only_index].word].word.slice_const());
				out.push_back(u8(' '));
			}
		} else {
			for (usize i = 0; i < word_count; ++i) {
				if (i != 0) {
					out.append(", "_S);
				}
				out.append(wordvecs[words[i].word].word.slice_const());

				//printf("%s %f\n", (char const *)c_str(wordvecs[words[i].word].word.slice_const()), words[i].score);
			}
		}
	}

	void test_irc() {
		tl::InternetAddr addr = tl::InternetAddr::from_name("se.quakenet.org", 6667);

		
		{
			//auto src = tl::Source::from_file("D:\\cpp\\word2bits\\1bit_200d_vectors");
			//auto src = tl::Source::from_file("D:\\cpp\\word2bits\\liero_vectors_bit.txt");
			auto src = tl::Source::from_file("D:\\cpp\\word2bits\\liero_vectors.txt");
			//auto src = tl::Source::from_file("D:\\cpp\\word2bits\\vectors\\vectors.txt");
		
			auto w = src.read_all();
			//printf("%d\n", src.size());
			usize start = 0;
			usize vecpos = 0;
			
			Vector v;
			usize i = 0;
			for (; i < w.size(); ++i) {
				u8 c = w[i];
				if (c == '\r' || c == '\n') {
					break;
				}
			}

			for (; i < w.size(); ++i) {
				u8 c = w[i];
				if (c == ' ') {
					tl::StringSlice col(w.begin() + start, w.begin() + i);

					if (vecpos == 0) {
						v.word = move(tl::String(col));
					} else {
						float comp = atof(c_str(col));
						v.v[vecpos - 1] = comp;
					}

					++vecpos;
					start = i + 1;
				} else if (c == '\r' || c == '\n') {
					if (vecpos != 0) {
						wordvecs.push_back(move(v));
					}
					vecpos = 0;
					start = i + 1;
				}
			}
		}

		if (false) {
			auto w = find_word("oob"_S);

			static usize const WordCount = 10;
			WordCand words[WordCount];
			usize word_count = 0;
			auto const& wvec = wordvecs[w];

			for (usize i = 0; i < wordvecs.size(); ++i) {
				auto score = wvec.cossim(wordvecs[i]);

				usize j = 0;
				for (j = 0; j < word_count; ++j) {
					if (words[j].score < score) {
						for (usize k = WordCount - 1; k >= j + 1; --k) {
							words[k] = words[k - 1];
						}
						words[j].word = i;
						words[j].score = score;
						break;
					}
				}

				if (j == word_count && word_count < WordCount) {
					words[word_count].word = i;
					words[word_count].score = score;
				}

				if (word_count < WordCount) {
					++word_count;
				}
			}

			for (usize i = 0; i < WordCount; ++i) {
				printf("%s %f\n", (char const *)c_str(wordvecs[words[i].word].word.slice_const()), words[i].score);
			}

			return;
		}

		bool r = sock.connect(addr);

		iocp.begin_recv(sock.to_handle());

		auto initmsg = tl::Vec<u8>::concat("NICK CppOrSth\r\n"_S, "USER CppOrSth 0 * :CppOrSth\r\n"_S);

		r = iocp.send(this->sock, iocp.new_send_op(move(initmsg)));

		tl::Vec<tl::IocpEvent> events;
		tl::Vec<u8> line_buf;

		while (true) {
			if (iocp.wait(events, 1000)) {
				for (auto& ev : events) {
					if (ev.is_read_()) {
						tl::IocpRecvFromOp& req = *(tl::IocpRecvFromOp *)ev.op.get();
						auto piece = req.slice_const();

						tl::sprint(piece);

						usize b = 0, end = piece.size();
						for (usize i = 0; i < end + 1; ++i) {
							bool end_of_msg = i < end && (piece[i] == '\n' || piece[i] == '\r');
							if (end_of_msg || i == end) {
								line_buf.append(tl::VecSlice<u8 const>(piece.begin() + b, piece.begin() + i));
								b = i + 1;

								if (end_of_msg && !line_buf.empty()) {
									handle_msg(line_buf.slice_const());
									line_buf.clear();
								}
							}
						}
					}
				}

				events.clear();
			}
		}
		sock.close();
	}
};

void test_irc() {
	IrcServer serv;
	serv.test_irc();
}
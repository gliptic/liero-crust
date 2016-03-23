#include <tl/cstdint.h>

typedef struct RegFile {
	u32 i[8];
	double f[8];
} RegFile;

typedef struct CPU {
	// Volatile
	u32 ip;
	i32 cycle_budget;
	RegFile regs;

	u8 const* rom;
	u8* ram;

	u32 rom_mask;
	u32 ram_mask;
} CPU;

#define OP_LABEL (1 << 31)

#define OPC_MOVE (0)
#define OPC_ADDW (1)

//#define OP_ADDR_

#define OPR_READWORD(opr) (((opr) < 8) ? regs.i[opr] : *(u32 const*)(ram + ((opr) & ram_wordmask)))
#define OPR_WRITEWORD(opr, v) (*(((opr) < 8) ? &regs.i[opr] : (u32*)(ram + ((opr) & ram_wordmask))) = (v))

void emulate(CPU* cpu) {
	i32 cycle_budget = cpu->cycle_budget;

	u32 rom_mask = cpu->rom_mask;
	u32 ram_mask = cpu->ram_mask;
	u32 ram_wordmask = ram_mask & ~(3);
	u32 ram_fpumask = ram_mask & ~(7);
	u8 const* rom = cpu->rom;
	u8* ram = cpu->ram;
	u32 ip = cpu->ip;

	RegFile regs = cpu->regs;

	while (cycle_budget > 0) {
		for (;;) {
			u32 a, b, c;
			u32 op = rom[ip & rom_mask];
			u32 opc = op & 0xff;

			switch (opc) {
			case OPC_ADDW:
				c = OPR_READWORD((op >> 24) & 0xff);
			case OPC_MOVE:
				b = OPR_READWORD((op >> 16) & 0xff);

				switch (opc) {
					case OPC_ADDW: {
						a = b + c;
						break;
					}
					case OPC_MOVE: {
						a = b;
						break;
					}
				}

				OPR_WRITEWORD((op >> 8) & 0xff, a);
			}

			cycle_budget -= 1;
		}
	}
}
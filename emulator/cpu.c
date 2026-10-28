/*
 * FileStick emulator - FemtoRV32 "Gracilis" core (rtl/femtorv32_gracilis.v)
 *
 * This follows the RTL rather than the RISC-V spec where the two differ,
 * since the point is to run software the way the real hardware would:
 *
 *  - 24 bit PC and address bus. AUIPC, JAL/JALR link values and the
 *    mepc/sepc/mtvec/stvec CSRs are all 24 bits wide.
 *  - Two register banks, selected by CSR 0x5C0. CSRs 0xDC0-0xDFF read any
 *    of the 64 registers.
 *  - Interrupts: mstatus bit 3 enables, mcause is a lock that is set when an
 *    interrupt is taken and cleared by mret. The instruction executing when
 *    the interrupt is taken completes, mepc is the address of the next one.
 *    A trap takes priority over an interrupt on the same instruction; the
 *    interrupt is then taken at the first instruction of the trap handler.
 *    An interrupt isn't taken on a load or store that causes a privilege
 *    violation either, so the violation trap is taken on the next one.
 *  - Traps (ecall, ebreak, illegal opcode, privilege violation) go to stvec
 *    with sepc = next instruction and the cause in scause.
 *  - Only the RV32IMC opcodes are decoded; anything else (including FENCE)
 *    is an illegal instruction trap. Illegal instructions still write 0 to
 *    their rd field, like the hardware does. Compressed opcodes the
 *    decompressor doesn't know become 0x00000000, which is "lb x0,0(x0)".
 *  - Memory protection: loads and stores below 0x10000 need S or M mode. The
 *    offending access is still carried out and the trap is taken when the
 *    following instruction executes (the violation flag is registered).
 *    S mode is set on reset and on traps, cleared by sret or any CSR
 *    operation on 0x5C1. M mode is set on interrupt, cleared by mret.
 *  - MULHSU treats rs1 as unsigned and rs2 as signed (the RTL's sign logic).
 *
 * Timing is modelled from the state machine: FETCH_INSTR, WAIT_INSTR,
 * EXECUTE then WAIT_ALU_OR_MEM for loads, stores and divides. The single
 * word fetch cache lets the core skip FETCH_INSTR when the next instruction
 * is in the word it last fetched (e.g. the second of two compressed
 * instructions).
 */
#include <inttypes.h>
#include <string.h>

#include "emu.h"

bool cpu_trace;

static struct {
   uint32_t regs[64];      // two banks of 32
   uint32_t pc;
   bool     bank;          // CSR 0x5C0

   bool     mstatus;       // interrupt enable (bit 3 of mstatus)
   bool     mcause;        // interrupt in progress
   uint32_t mepc, mtvec, mscratch;
   uint32_t sepc, stvec, sscratch, scause, stval;

   bool     s_mode, m_mode;
   bool     priv_violation;

   // fetch cache and the state machine's decision about the next fetch
   uint32_t cached_word;   // address >> 2 of the cached word
   bool     next_skip;     // next instruction is in the cache: skip FETCH
   bool     next_second_half; // next is an unaligned long in the cache
} cpu;

// Compressed instructions expanded to their 32 bit form.
static uint32_t decomp_table[65536];

#define OP_LOAD      0x00
#define OP_ALUIMM    0x04
#define OP_AUIPC     0x05
#define OP_STORE     0x08
#define OP_ALUREG    0x0C
#define OP_LUI       0x0D
#define OP_BRANCH    0x18
#define OP_JALR      0x19
#define OP_JAL       0x1B
#define OP_SYSTEM    0x1C

#define CSR_MSTATUS  0x300
#define CSR_MTVEC    0x305
#define CSR_MSCRATCH 0x340
#define CSR_MEPC     0x341
#define CSR_MCAUSE   0x342
#define CSR_CYCLE    0xC00
#define CSR_CYCLEH   0xC80
#define CSR_STVEC    0x105
#define CSR_SSCRATCH 0x140
#define CSR_SEPC     0x141
#define CSR_SCAUSE   0x142
#define CSR_STVAL    0x143
#define CSR_REGBANK  0x5C0
#define CSR_PRIV     0x5C1

#define BIT(x, n)    (((x) >> (n)) & 1)
#define BITS(x, hi, lo) (((x) >> (lo)) & ((1u << ((hi) - (lo) + 1)) - 1))

static inline int32_t sext(uint32_t v, int bits)
{
   return (int32_t)(v << (32 - bits)) >> (32 - bits);
}

//--------------------------------------------------------------------------
// Instruction encoders, used to build the decompressor table.
static uint32_t enc_i(uint32_t imm, unsigned rs1, unsigned f3, unsigned rd, unsigned op)
{
   return ((imm & 0xFFF) << 20) | (rs1 << 15) | (f3 << 12) | (rd << 7) | op;
}

static uint32_t enc_s(uint32_t imm, unsigned rs2, unsigned rs1, unsigned f3, unsigned op)
{
   return (BITS(imm, 11, 5) << 25) | (rs2 << 20) | (rs1 << 15) | (f3 << 12) |
          (BITS(imm, 4, 0) << 7) | op;
}

static uint32_t enc_b(uint32_t imm, unsigned rs2, unsigned rs1, unsigned f3)
{
   return (BIT(imm, 12) << 31) | (BITS(imm, 10, 5) << 25) | (rs2 << 20) |
          (rs1 << 15) | (f3 << 12) | (BITS(imm, 4, 1) << 8) |
          (BIT(imm, 11) << 7) | 0x63;
}

static uint32_t enc_j(uint32_t imm, unsigned rd)
{
   return (BIT(imm, 20) << 31) | (BITS(imm, 10, 1) << 21) | (BIT(imm, 11) << 20) |
          (BITS(imm, 19, 12) << 12) | (rd << 7) | 0x6F;
}

static uint32_t enc_r(unsigned f7, unsigned rs2, unsigned rs1, unsigned f3, unsigned rd)
{
   return (f7 << 25) | (rs2 << 20) | (rs1 << 15) | (f3 << 12) | (rd << 7) | 0x33;
}

//--------------------------------------------------------------------------
// The decompressor module. The patterns are tested in the same order as the
// casez statement in the RTL; the first match wins.
static uint32_t decompress(uint32_t c)
{
   unsigned rcl = 8 + BITS(c, 4, 2);     // register compressed low
   unsigned rch = 8 + BITS(c, 9, 7);     // register compressed high
   unsigned rwl = BITS(c, 6, 2);         // register wide low
   unsigned rwh = BITS(c, 11, 7);        // register wide high
   unsigned shamt = BITS(c, 6, 2);
   uint32_t c12 = BIT(c, 12);

   uint32_t addi4spn = (BITS(c, 10, 7) << 6) | (BITS(c, 12, 11) << 4) |
                       (BIT(c, 5) << 3) | (BIT(c, 6) << 2);
   uint32_t lwsw     = (BIT(c, 5) << 6) | (BITS(c, 12, 10) << 3) | (BIT(c, 6) << 2);
   uint32_t lwsp     = (BITS(c, 3, 2) << 6) | (c12 << 5) | (BITS(c, 6, 4) << 2);
   uint32_t swsp     = (BITS(c, 8, 7) << 6) | (BITS(c, 12, 9) << 2);
   uint32_t addi16sp = sext((c12 << 9) | (BITS(c, 4, 3) << 7) | (BIT(c, 5) << 6) |
                            (BIT(c, 2) << 5) | (BIT(c, 6) << 4), 10);
   uint32_t addimm   = sext((c12 << 5) | BITS(c, 6, 2), 6);
   uint32_t bimm     = sext((c12 << 8) | (BITS(c, 6, 5) << 6) | (BIT(c, 2) << 5) |
                            (BITS(c, 11, 10) << 3) | (BITS(c, 4, 3) << 1), 9);
   uint32_t jalimm   = sext((c12 << 11) | (BIT(c, 8) << 10) | (BITS(c, 10, 9) << 8) |
                            (BIT(c, 6) << 7) | (BIT(c, 7) << 6) | (BIT(c, 2) << 5) |
                            (BIT(c, 11) << 4) | (BITS(c, 5, 3) << 1), 12);
   uint32_t luiimm   = (uint32_t)sext((c12 << 5) | BITS(c, 6, 2), 6) << 12;

#define M(mask, val) ((c & (mask)) == (val))
   if(M(0xFFFF, 0x0000)) return 0;                                       // c.illegal
   if(M(0xE003, 0x0000)) return enc_i(addi4spn, 2, 0, rcl, 0x13);        // c.addi4spn
   if(M(0xE003, 0x4000)) return enc_i(lwsw, rch, 2, rcl, 0x03);          // c.lw
   if(M(0xE003, 0xC000)) return enc_s(lwsw, rcl, rch, 2, 0x23);          // c.sw
   if(M(0xE003, 0x0001)) return enc_i(addimm, rwh, 0, rwh, 0x13);        // c.addi
   if(M(0xE003, 0x2001)) return enc_j(jalimm, 1);                        // c.jal
   if(M(0xE003, 0x4001)) return enc_i(addimm, 0, 0, rwh, 0x13);          // c.li
   if(M(0xEF83, 0x6101)) return enc_i(addi16sp, rwh, 0, rwh, 0x13);      // c.addi16sp
   if(M(0xE003, 0x6001)) return (luiimm & 0xFFFFF000) | (rwh << 7) | 0x37; // c.lui
   if(M(0xEC03, 0x8001)) return enc_i(shamt, rch, 5, rch, 0x13);         // c.srli
   if(M(0xEC03, 0x8401)) return enc_i(0x400 | shamt, rch, 5, rch, 0x13); // c.srai
   if(M(0xEC03, 0x8801)) return enc_i(addimm, rch, 7, rch, 0x13);        // c.andi
   if(M(0xFC63, 0x8C01)) return enc_r(0x20, rcl, rch, 0, rch);           // c.sub
   if(M(0xFC63, 0x8C21)) return enc_r(0, rcl, rch, 4, rch);              // c.xor
   if(M(0xFC63, 0x8C41)) return enc_r(0, rcl, rch, 6, rch);              // c.or
   if(M(0xFC63, 0x8C61)) return enc_r(0, rcl, rch, 7, rch);              // c.and
   if(M(0xE003, 0xA001)) return enc_j(jalimm, 0);                        // c.j
   if(M(0xE003, 0xC001)) return enc_b(bimm, 0, rch, 0);                  // c.beqz
   if(M(0xE003, 0xE001)) return enc_b(bimm, 0, rch, 1);                  // c.bnez
   if(M(0xE003, 0x0002)) return enc_i(shamt, rwh, 1, rwh, 0x13);         // c.slli
   if(M(0xE003, 0x4002)) return enc_i(lwsp, 2, 2, rwh, 0x03);            // c.lwsp
   if(M(0xF07F, 0x8002)) return enc_i(0, rwh, 0, 0, 0x67);               // c.jr
   if(M(0xF003, 0x8002)) return enc_r(0, rwl, 0, 0, rwh);                // c.mv
   if(M(0xFFFF, 0x9002)) return 0x00100073;                              // c.ebreak
   if(M(0xF07F, 0x9002)) return enc_i(0, rwh, 0, 1, 0x67);               // c.jalr
   if(M(0xF003, 0x9002)) return enc_r(0, rwl, rwh, 0, rwh);              // c.add
   if(M(0xE003, 0xC002)) return enc_s(swsp, rwl, 2, 2, 0x23);            // c.swsp
#undef M
   return 0;                                                             // unknown
}

//--------------------------------------------------------------------------
void cpu_reset(void)
{
   memset(&cpu, 0, sizeof(cpu));
   cpu.pc = RESET_ADDR;
   cpu.s_mode = true;      // the RTL starts in S mode
   cpu.cached_word = 0xFFFFFFFF;

   for(uint32_t i = 0; i < 65536; i++)
      decomp_table[i] = ((i & 3) == 3) ? 0 : decompress(i);
}

//--------------------------------------------------------------------------
static uint32_t csr_read(unsigned csr)
{
   if((csr & 0xFC0) == 0xDC0)
      return cpu.regs[csr & 63];

   switch(csr) {
      case CSR_MSTATUS:    return cpu.mstatus << 3;
      case CSR_MSCRATCH:   return cpu.mscratch;
      case CSR_MTVEC:      return cpu.mtvec;
      case CSR_MEPC:       return cpu.mepc;
      case CSR_MCAUSE:     return (uint32_t)cpu.mcause << 31;
      case CSR_CYCLE:      return (uint32_t)emu_now;
      case CSR_CYCLEH:     return (uint32_t)(emu_now >> 32);
      case CSR_STVEC:      return cpu.stvec;
      case CSR_SSCRATCH:   return cpu.sscratch;
      case CSR_SEPC:       return cpu.sepc;
      case CSR_SCAUSE:     return cpu.scause;
      case CSR_STVAL:      return cpu.stval;
      case CSR_REGBANK:    return cpu.bank;
   }
   return 0;
}

//--------------------------------------------------------------------------
// Execute one instruction, including the cycles it takes.
static void cpu_step(void)
{
   uint32_t pc = cpu.pc;
   uint32_t lo = bus_fetch16(pc);
   bool is_long = (lo & 3) == 3;
   unsigned cost;

   // FETCH_INSTR / WAIT_INSTR cycles
   if(cpu.next_second_half) {
      cost = 2;
      cpu.cached_word = ((pc + 4) & ADDR_MASK) >> 2;
   } else if(cpu.next_skip) {
      cost = 1;
   } else if((pc & 2) && is_long) {
      cost = 4;               // fetch both halves
      cpu.cached_word = ((pc + 4) & ADDR_MASK) >> 2;
   } else {
      cost = 2;
      cpu.cached_word = pc >> 2;
   }

   uint32_t instr, pcinc;
   if(is_long) {
      instr = lo | ((uint32_t)bus_fetch16(pc + 2) << 16);
      pcinc = (pc + 4) & ADDR_MASK;
   } else {
      instr = decomp_table[lo];
      pcinc = (pc + 2) & ADDR_MASK;
   }

   emu_now = cycles + cost;
   if(emu_now >= next_event)
      devices_update(emu_now);

   // the interrupt is sampled before this instruction's own bus access
   bool interrupt = irq_line && cpu.mstatus && !cpu.mcause;
   bool priv = cpu.s_mode || cpu.m_mode;
   bool old_violation = cpu.priv_violation;

   unsigned base = cpu.bank ? 32 : 0;
   unsigned rd = BITS(instr, 11, 7);
   unsigned f3 = BITS(instr, 14, 12);
   uint32_t rs1 = cpu.regs[base + BITS(instr, 19, 15)];
   uint32_t rs2 = cpu.regs[base + BITS(instr, 24, 20)];
   uint32_t iimm = sext(instr >> 20, 12);

   bool writeback = true;
   uint32_t wbdata = 0;
   uint32_t pc_new = pcinc;
   unsigned wait = 0;           // WAIT_ALU_OR_MEM cycles

   bool illegal = false, ecall = false, ebreak = false;
   bool mret = false, sret = false, csr_s_clear = false;
   bool new_violation = false;
   int new_bank = -1;

   switch(BITS(instr, 6, 2)) {
      case OP_LOAD: {
         uint32_t addr = (rs1 + iimm) & ADDR_MASK;
         if(!priv && addr < 0x10000) {
            cpu.priv_violation = new_violation = true;
            cpu.stval = addr;
         }
         bus_wait = 1;
         uint32_t w = bus_read(addr & ~3u);
         wait = bus_wait;
         uint32_t hw = (addr & 2) ? w >> 16 : w & 0xFFFF;
         uint32_t by = (addr & 1) ? hw >> 8 : hw & 0xFF;
         switch(f3 & 3) {
            case 0:  wbdata = (f3 & 4) ? by : (uint32_t)sext(by, 8); break;
            case 1:  wbdata = (f3 & 4) ? hw : (uint32_t)sext(hw, 16); break;
            default: wbdata = w;
         }
         break;
      }

      case OP_STORE: {
         uint32_t simm = sext((BITS(instr, 31, 25) << 5) | BITS(instr, 11, 7), 12);
         uint32_t addr = (rs1 + simm) & ADDR_MASK;
         if(!priv && addr < 0x10000) {
            cpu.priv_violation = new_violation = true;
            cpu.stval = addr;
         }

         // byte lane steering as done by the core
         uint32_t b0 = rs2 & 0xFF;
         uint32_t b1 = (addr & 1) ? rs2 & 0xFF : BITS(rs2, 15, 8);
         uint32_t b2 = (addr & 2) ? rs2 & 0xFF : BITS(rs2, 23, 16);
         uint32_t b3 = (addr & 1) ? rs2 & 0xFF :
                       (addr & 2) ? BITS(rs2, 15, 8) : rs2 >> 24;
         uint32_t wdata = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
         unsigned wmask;
         switch(f3 & 3) {
            case 0:  wmask = 1u << (addr & 3); break;
            case 1:  wmask = (addr & 2) ? 0xC : 0x3; break;
            default: wmask = 0xF;
         }

         bus_wait = 1;
         bus_write(addr & ~3u, wdata, wmask);
         wait = bus_wait;
         writeback = false;
         break;
      }

      case OP_ALUIMM:
      case OP_ALUREG: {
         bool isreg = BIT(instr, 5);
         uint32_t a = rs1;
         uint32_t b = isreg ? rs2 : iimm;

         if(isreg && BIT(instr, 25)) {
            // M extension
            if(f3 < 4) {
               bool sign1 = (f3 == 1) && (a >> 31);
               bool sign2 = (f3 == 1 || f3 == 2) && (b >> 31);
               int64_t sa = sign1 ? (int64_t)(int32_t)a : (int64_t)a;
               int64_t sb = sign2 ? (int64_t)(int32_t)b : (int64_t)b;
               uint64_t prod = (uint64_t)(sa * sb);
               wbdata = (f3 == 0) ? (uint32_t)prod : (uint32_t)(prod >> 32);
            } else {
               bool is_unsigned = BIT(instr, 12);
               bool is_rem = BIT(instr, 13);
               bool neg = !is_unsigned && (is_rem ? (a >> 31) :
                          ((a >> 31) != (b >> 31)) && b != 0);
               uint32_t dividend = (!is_unsigned && (a >> 31)) ? -a : a;
               uint32_t divisor  = (!is_unsigned && (b >> 31)) ? -b : b;
               uint32_t q = divisor ? dividend / divisor : 0xFFFFFFFF;
               uint32_t r = divisor ? dividend % divisor : dividend;
               wbdata = is_rem ? r : q;
               if(neg) wbdata = -wbdata;
               wait = 33;
            }
         } else {
            unsigned sh = b & 31;
            switch(f3) {
               case 0:
                  wbdata = (BIT(instr, 30) && isreg) ? a - b : a + b;
                  break;
               case 1:
                  wbdata = a << sh;
                  if(BIT(instr, 30) && (a >> 31) && sh)
                     wbdata |= (1u << sh) - 1;
                  break;
               case 2: wbdata = (int32_t)a < (int32_t)b; break;
               case 3: wbdata = a < b; break;
               case 4: wbdata = a ^ b; break;
               case 5:
                  wbdata = BIT(instr, 30) ? (uint32_t)((int32_t)a >> sh) : a >> sh;
                  break;
               case 6: wbdata = a | b; break;
               case 7: wbdata = a & b; break;
            }
         }
         break;
      }

      case OP_AUIPC:
         wbdata = (pc + (instr & 0xFFFFF000)) & ADDR_MASK;
         break;

      case OP_LUI:
         wbdata = instr & 0xFFFFF000;
         break;

      case OP_BRANCH: {
         bool take;
         switch(f3) {
            case 0:  take = rs1 == rs2; break;
            case 1:  take = rs1 != rs2; break;
            case 4:  take = (int32_t)rs1 < (int32_t)rs2; break;
            case 5:  take = (int32_t)rs1 >= (int32_t)rs2; break;
            case 6:  take = rs1 < rs2; break;
            case 7:  take = rs1 >= rs2; break;
            default: take = false;
         }
         if(take) {
            uint32_t bimm = sext((BIT(instr, 31) << 12) | (BIT(instr, 7) << 11) |
                                 (BITS(instr, 30, 25) << 5) | (BITS(instr, 11, 8) << 1), 13);
            pc_new = (pc + bimm) & ADDR_MASK;
         }
         writeback = false;
         break;
      }

      case OP_JAL: {
         uint32_t jimm = sext((BIT(instr, 31) << 20) | (BITS(instr, 19, 12) << 12) |
                              (BIT(instr, 20) << 11) | (BITS(instr, 30, 21) << 1), 21);
         wbdata = pcinc;
         pc_new = (pc + jimm) & ADDR_MASK;
         break;
      }

      case OP_JALR:
         wbdata = pcinc;
         pc_new = (rs1 + iimm) & ADDR_MASK & ~1u;
         break;

      case OP_SYSTEM: {
         unsigned csr = instr >> 20;
         wbdata = csr_read(csr);

         if(f3 == 0) {
            ecall  = csr == 0x000;
            ebreak = csr == 0x001;
            sret   = csr == 0x102;
            mret   = csr == 0x302;
            if(mret) pc_new = cpu.mepc;
            else if(sret) pc_new = cpu.sepc;
         } else {
            uint32_t mod = (f3 & 4) ? BITS(instr, 19, 15) : rs1;
            uint32_t val = (f3 & 3) == 2 ? wbdata | mod :
                           (f3 & 3) == 3 ? wbdata & ~mod : mod;
            switch(csr) {
               case CSR_MSCRATCH:   cpu.mscratch = val; break;
               case CSR_MSTATUS:    cpu.mstatus = BIT(val, 3); break;
               case CSR_MTVEC:      cpu.mtvec = val & ADDR_MASK; break;
               case CSR_STVEC:      cpu.stvec = val & ADDR_MASK; break;
               case CSR_SSCRATCH:   cpu.sscratch = val; break;
               case CSR_REGBANK:    new_bank = val & 1; break;
               case CSR_PRIV:       csr_s_clear = true; break;
            }
         }
         break;
      }

      default:
         illegal = true;
   }

   if(writeback && rd != 0)
      cpu.regs[base + rd] = wbdata;

   if(cpu_trace) {
      if(writeback && rd != 0)
         emu_log("%10" PRIu64 " %06x: %08x%s  x%-2u = %08x", emu_now, pc, instr,
                 is_long ? "    " : " (c)", rd, wbdata);
      else
         emu_log("%10" PRIu64 " %06x: %08x%s", emu_now, pc, instr,
                 is_long ? "    " : " (c)");
   }
   if(new_bank >= 0)
      cpu.bank = new_bank;

   bool trap = ecall || ebreak || illegal || old_violation;
   // A trap wins over an interrupt, which stays pending (the interrupt line
   // is held until acknowledged) and is taken at the trap handler's first
   // instruction. An interrupt is also held off for an access that causes a
   // privilege violation, so the violation trap is taken first.
   if(trap || new_violation)
      interrupt = false;

   if(trap) {
      cpu.pc = cpu.stvec;
      cpu.sepc = pc_new;
      cpu.scause = ecall ? 8 : ebreak ? 3 : old_violation ? 5 : 2;
   } else if(interrupt) {
      cpu.pc = cpu.mtvec;
      cpu.mepc = pc_new;
      cpu.mcause = true;
   } else {
      cpu.pc = pc_new;
      if(mret) cpu.mcause = false;
      if(sret) cpu.scause = 0;
   }

   if(sret || csr_s_clear)
      cpu.s_mode = false;
   else if(trap)
      cpu.s_mode = true;

   if(mret)
      cpu.m_mode = false;
   else if(interrupt)
      cpu.m_mode = true;

   if(cpu.s_mode || cpu.m_mode)
      cpu.priv_violation = false;

   // decide whether the next fetch can come from the cache
   if(interrupt || trap) {
      cpu.next_skip = cpu.next_second_half = false;
   } else {
      bool hit = cpu.cached_word == (pc_new >> 2);
      bool ul = hit && (pc_new & 2) && (bus_fetch16(pc_new) & 3) == 3;
      cpu.next_skip = hit && !ul;
      cpu.next_second_half = hit && ul;
   }

   cycles += cost + 1 + wait;
}

//--------------------------------------------------------------------------
void cpu_run(uint64_t until)
{
   while(cycles < until)
      cpu_step();
}

//--------------------------------------------------------------------------
void cpu_dump(void)
{
   static const char *names[32] = {
      "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
      "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
      "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
      "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
   };

   emu_log("pc=%06x bank=%d mode=%s%s mstatus=%d mcause=%d cycles=%" PRIu64,
           cpu.pc, cpu.bank, cpu.s_mode ? "S" : "", cpu.m_mode ? "M" : "",
           cpu.mstatus, cpu.mcause, cycles);
   emu_log("mepc=%06x mtvec=%06x sepc=%06x stvec=%06x scause=%x stval=%06x",
           cpu.mepc, cpu.mtvec, cpu.sepc, cpu.stvec, cpu.scause, cpu.stval);
   for(int b = 0; b < 2; b++) {
      emu_log("bank %d:", b);
      for(int i = 0; i < 32; i += 4)
         emu_log("  %-4s %08x  %-4s %08x  %-4s %08x  %-4s %08x",
                 names[i], cpu.regs[b * 32 + i],
                 names[i + 1], cpu.regs[b * 32 + i + 1],
                 names[i + 2], cpu.regs[b * 32 + i + 2],
                 names[i + 3], cpu.regs[b * 32 + i + 3]);
   }
}

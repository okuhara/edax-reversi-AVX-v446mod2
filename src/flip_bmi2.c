/**
 * @file flip_bmi2.c
 *
 * This module deals with flipping discs.
 *
 * A function is provided for each square of the board. These functions are
 * gathered into an array of functions, so that a fast access to each function
 * is allowed. The generic form of the function take as input the player and
 * the opponent bitboards and return the flipped squares into a bitboard.
 *
 * Given the following notation:
 *  - x = square where we play,
 *  - P = player's disc pattern,
 *  - O = opponent's disc pattern,
 * the basic principle is to read into an array the result of a move. Doing
 * this is easier for a single line ; so we can use arrays of the form:
 *  - ARRAY[x][8-bits disc pattern].
 * The problem is thus to convert any line of a 64-bits disc pattern into an
 * 8-bits disc pattern. A fast way to do this is to select the right line,
 * with a bit-mask, to gather the masked-bits into a continuous set by the 
 * BMI2 PEXT instruction.
 * Once we get our 8-bits disc patterns,a first array (OUTFLANK) is used to
 * get the player's discs that surround the opponent discs:
 *  - outflank = OUTFLANK[x][O] & P
 * The result is then used as an index to access a second array giving the
 * flipped discs according to the surrounding player's discs:
 *  - flipped = FLIPPED[x][outflank].
 * Finally, BMI2 PDEP instruction transform the 8-bits disc pattern back into a
 * 64-bits disc pattern, and the flipped squares for each line are gathered and
 * returned to generate moves.
 *
 * @date 1998 - 2014
 * @author Toshihiko Okuhara
 * @version 4.4
 */

#include <x86intrin.h>

/* bit masks for diagonal/vertical/all lines */
static const unsigned long long mask_x[66][4] = {
	{ 0x0000000000000001ULL, 0x8040201008040201ULL, 0x0101010101010101ULL, 0x81412111090503ffULL },
	{ 0x0000000000000102ULL, 0x0080402010080402ULL, 0x0202020202020202ULL, 0x02824222120a07ffULL },
	{ 0x0000000000010204ULL, 0x0000804020100804ULL, 0x0404040404040404ULL, 0x0404844424150effULL },
	{ 0x0000000001020408ULL, 0x0000008040201008ULL, 0x0808080808080808ULL, 0x08080888492a1cffULL },
	{ 0x0000000102040810ULL, 0x0000000080402010ULL, 0x1010101010101010ULL, 0x10101011925438ffULL },
	{ 0x0000010204081020ULL, 0x0000000000804020ULL, 0x2020202020202020ULL, 0x2020212224a870ffULL },
	{ 0x0001020408102040ULL, 0x0000000000008040ULL, 0x4040404040404040ULL, 0x404142444850e0ffULL },
	{ 0x0102040810204080ULL, 0x0000000000000080ULL, 0x8080808080808080ULL, 0x8182848890a0c0ffULL },
	{ 0x0000000000000102ULL, 0x4020100804020104ULL, 0x0101010101010101ULL, 0x412111090503ff03ULL },
	{ 0x0000000000010204ULL, 0x8040201008040201ULL, 0x0202020202020202ULL, 0x824222120a07ff07ULL },
	{ 0x0000000001020408ULL, 0x0080402010080402ULL, 0x0404040404040404ULL, 0x04844424150eff0eULL },
	{ 0x0000000102040810ULL, 0x0000804020100804ULL, 0x0808080808080808ULL, 0x080888492a1cff1cULL },
	{ 0x0000010204081020ULL, 0x0000008040201008ULL, 0x1010101010101010ULL, 0x101011925438ff38ULL },
	{ 0x0001020408102040ULL, 0x0000000080402010ULL, 0x2020202020202020ULL, 0x20212224a870ff70ULL },
	{ 0x0102040810204080ULL, 0x0000000000804020ULL, 0x4040404040404040ULL, 0x4142444850e0ffe0ULL },
	{ 0x0204081020408001ULL, 0x0000000000008040ULL, 0x8080808080808080ULL, 0x82848890a0c0ffc0ULL },
	{ 0x0000000000010204ULL, 0x201008040201000aULL, 0x0101010101010101ULL, 0x2111090503ff0305ULL },
	{ 0x0000000001020408ULL, 0x4020100804020101ULL, 0x0202020202020202ULL, 0x4222120a07ff070aULL },
	{ 0x0000000102040810ULL, 0x8040201008040201ULL, 0x0404040404040404ULL, 0x844424150eff0e15ULL },
	{ 0x0000010204081020ULL, 0x0080402010080402ULL, 0x0808080808080808ULL, 0x0888492a1cff1c2aULL },
	{ 0x0001020408102040ULL, 0x0000804020100804ULL, 0x1010101010101010ULL, 0x1011925438ff3854ULL },
	{ 0x0102040810204080ULL, 0x0000008040201008ULL, 0x2020202020202020ULL, 0x212224a870ff70a8ULL },
	{ 0x0204081020408001ULL, 0x0000000080402010ULL, 0x4040404040404040ULL, 0x42444850e0ffe050ULL },
	{ 0x0408102040800003ULL, 0x0000000000804020ULL, 0x8080808080808080ULL, 0x848890a0c0ffc0a0ULL },
	{ 0x0000000001020408ULL, 0x1008040201000016ULL, 0x0101010101010101ULL, 0x11090503ff030509ULL },
	{ 0x0000000102040810ULL, 0x2010080402010005ULL, 0x0202020202020202ULL, 0x22120a07ff070a12ULL },
	{ 0x0000010204081020ULL, 0x4020100804020101ULL, 0x0404040404040404ULL, 0x4424150eff0e1524ULL },
	{ 0x0001020408102040ULL, 0x8040201008040201ULL, 0x0808080808080808ULL, 0x88492a1cff1c2a49ULL },
	{ 0x0102040810204080ULL, 0x0080402010080402ULL, 0x1010101010101010ULL, 0x11925438ff385492ULL },
	{ 0x0204081020408001ULL, 0x0000804020100804ULL, 0x2020202020202020ULL, 0x2224a870ff70a824ULL },
	{ 0x0408102040800003ULL, 0x0000008040201008ULL, 0x4040404040404040ULL, 0x444850e0ffe05048ULL },
	{ 0x0810204080000007ULL, 0x0000000080402010ULL, 0x8080808080808080ULL, 0x8890a0c0ffc0a090ULL },
	{ 0x0000000102040810ULL, 0x080402010000002eULL, 0x0101010101010101ULL, 0x090503ff03050911ULL },
	{ 0x0000010204081020ULL, 0x100804020100000dULL, 0x0202020202020202ULL, 0x120a07ff070a1222ULL },
	{ 0x0001020408102040ULL, 0x2010080402010003ULL, 0x0404040404040404ULL, 0x24150eff0e152444ULL },
	{ 0x0102040810204080ULL, 0x4020100804020101ULL, 0x0808080808080808ULL, 0x492a1cff1c2a4988ULL },
	{ 0x0204081020408002ULL, 0x8040201008040201ULL, 0x1010101010101010ULL, 0x925438ff38549211ULL },
	{ 0x0408102040800005ULL, 0x0080402010080402ULL, 0x2020202020202020ULL, 0x24a870ff70a82422ULL },
	{ 0x081020408000000bULL, 0x0000804020100804ULL, 0x4040404040404040ULL, 0x4850e0ffe0504844ULL },
	{ 0x1020408000000017ULL, 0x0000008040201008ULL, 0x8080808080808080ULL, 0x90a0c0ffc0a09088ULL },
	{ 0x0000010204081020ULL, 0x040201000000005eULL, 0x0101010101010101ULL, 0x0503ff0305091121ULL },
	{ 0x0001020408102040ULL, 0x080402010000001dULL, 0x0202020202020202ULL, 0x0a07ff070a122242ULL },
	{ 0x0102040810204080ULL, 0x100804020100000bULL, 0x0404040404040404ULL, 0x150eff0e15244484ULL },
	{ 0x0204081020408001ULL, 0x2010080402010003ULL, 0x0808080808080808ULL, 0x2a1cff1c2a498808ULL },
	{ 0x0408102040800003ULL, 0x4020100804020101ULL, 0x1010101010101010ULL, 0x5438ff3854921110ULL },
	{ 0x081020408000000eULL, 0x8040201008040201ULL, 0x2020202020202020ULL, 0xa870ff70a8242221ULL },
	{ 0x102040800000001dULL, 0x0080402010080402ULL, 0x4040404040404040ULL, 0x50e0ffe050484442ULL },
	{ 0x204080000000003bULL, 0x0000804020100804ULL, 0x8080808080808080ULL, 0xa0c0ffc0a0908884ULL },
	{ 0x0001020408102040ULL, 0x02010000000000beULL, 0x0101010101010101ULL, 0x03ff030509112141ULL },
	{ 0x0102040810204080ULL, 0x040201000000003dULL, 0x0202020202020202ULL, 0x07ff070a12224282ULL },
	{ 0x0204081020408001ULL, 0x080402010000001bULL, 0x0404040404040404ULL, 0x0eff0e1524448404ULL },
	{ 0x0408102040800003ULL, 0x1008040201000007ULL, 0x0808080808080808ULL, 0x1cff1c2a49880808ULL },
	{ 0x0810204080000007ULL, 0x2010080402010003ULL, 0x1010101010101010ULL, 0x38ff385492111010ULL },
	{ 0x102040800000000fULL, 0x4020100804020101ULL, 0x2020202020202020ULL, 0x70ff70a824222120ULL },
	{ 0x204080000000003eULL, 0x8040201008040201ULL, 0x4040404040404040ULL, 0xe0ffe05048444241ULL },
	{ 0x408000000000007dULL, 0x0080402010080402ULL, 0x8080808080808080ULL, 0xc0ffc0a090888482ULL },
	{ 0x0102040810204080ULL, 0x010000000000027eULL, 0x0101010101010101ULL, 0xff03050911214181ULL },
	{ 0x0204081020408001ULL, 0x020100000000007dULL, 0x0202020202020202ULL, 0xff070a1222428202ULL },
	{ 0x0408102040800003ULL, 0x040201000000003bULL, 0x0404040404040404ULL, 0xff0e152444840404ULL },
	{ 0x0810204080000007ULL, 0x0804020100000017ULL, 0x0808080808080808ULL, 0xff1c2a4988080808ULL },
	{ 0x102040800000000fULL, 0x1008040201000007ULL, 0x1010101010101010ULL, 0xff38549211101010ULL },
	{ 0x204080000000001fULL, 0x2010080402010003ULL, 0x2020202020202020ULL, 0xff70a82422212020ULL },
	{ 0x408000000000003fULL, 0x4020100804020101ULL, 0x4040404040404040ULL, 0xffe0504844424140ULL },
	{ 0x800000000000017eULL, 0x8040201008040201ULL, 0x8080808080808080ULL, 0xffc0a09088848281ULL },
	{ 0, 0, 0, 0 },	// pass
	{ 0, 0, 0, 0 }
};

/** outflank array */
const unsigned char OUTFLANK[8][64] = {
	{
		0x00, 0x04, 0x00, 0x08, 0x00, 0x04, 0x00, 0x10, 0x00, 0x04, 0x00, 0x08, 0x00, 0x04, 0x00, 0x20,
		0x00, 0x04, 0x00, 0x08, 0x00, 0x04, 0x00, 0x10, 0x00, 0x04, 0x00, 0x08, 0x00, 0x04, 0x00, 0x40,
		0x00, 0x04, 0x00, 0x08, 0x00, 0x04, 0x00, 0x10, 0x00, 0x04, 0x00, 0x08, 0x00, 0x04, 0x00, 0x20,
		0x00, 0x04, 0x00, 0x08, 0x00, 0x04, 0x00, 0x10, 0x00, 0x04, 0x00, 0x08, 0x00, 0x04, 0x00, 0x80,
	},
	{
		0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x20, 0x00,
		0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x40, 0x00,
		0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x20, 0x00,
		0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x80, 0x00,
	},
	{
		0x00, 0x01, 0x00, 0x00, 0x10, 0x11, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x20, 0x21, 0x00, 0x00,
		0x00, 0x01, 0x00, 0x00, 0x10, 0x11, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x40, 0x41, 0x00, 0x00,
		0x00, 0x01, 0x00, 0x00, 0x10, 0x11, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x20, 0x21, 0x00, 0x00,
		0x00, 0x01, 0x00, 0x00, 0x10, 0x11, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x80, 0x81, 0x00, 0x00,
	},
	{
		0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x20, 0x20, 0x22, 0x21, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x40, 0x40, 0x42, 0x41, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x20, 0x20, 0x22, 0x21, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x82, 0x81, 0x00, 0x00, 0x00, 0x00,
	},
	{
		0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x40, 0x40, 0x40, 0x40, 0x44, 0x44, 0x42, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x80, 0x80, 0x80, 0x80, 0x84, 0x84, 0x82, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	},
	{
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x04, 0x04, 0x02, 0x01,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x88, 0x88, 0x88, 0x88, 0x84, 0x84, 0x82, 0x81,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	},
	{
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x08, 0x08, 0x08, 0x08, 0x04, 0x04, 0x02, 0x01,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	},
	{
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
		0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x08, 0x08, 0x08, 0x08, 0x04, 0x04, 0x02, 0x01,
	},
};

/** flip array */
const unsigned char FLIPPED[8][144] = {
	{
		0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	},
	{
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x7c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	},
	{
		0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x08, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x18, 0x1a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x38, 0x3a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x78, 0x7a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	},
	{
		0x00, 0x06, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x10, 0x16, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x30, 0x36, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x70, 0x76, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	},
	{
		0x00, 0x0e, 0x0c, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x20, 0x2e, 0x2c, 0x00, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x60, 0x6e, 0x6c, 0x00, 0x68, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	},
	{
		0x00, 0x1e, 0x1c, 0x00, 0x18, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x40, 0x5e, 0x5c, 0x00, 0x58, 0x00, 0x00, 0x00, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	},
	{
		0x00, 0x3e, 0x3c, 0x00, 0x38, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	},
	{
		0x00, 0x7e, 0x7c, 0x00, 0x78, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	},
};

unsigned long long flip(int pos, unsigned long long P, unsigned long long O)
{
	int	index;
	unsigned long long	flipped, mask;
	int	x = pos & 7;
	int	y = pos & 0x38;

	P &= mask_x[pos][3];	// mask out unrelated bits to make dummy 0 bits for outside

	index = OUTFLANK[x][_bextr_u32((O >> y), 1, 6)] & (P >> y);
	flipped = ((unsigned long long) FLIPPED[x][index]) << y;

	y >>= 3;
	mask = mask_x[pos][0];
	index = OUTFLANK[y][_bextr_u32(_pext_u64(O, mask), 1, 6)] & _pext_u64(P, mask);
	flipped |= _pdep_u64(FLIPPED[y][index], mask);

	mask = mask_x[pos][1];
	index = OUTFLANK[y][_bextr_u32(_pext_u64(O, mask), 1, 6)] & _pext_u64(P, mask);
	flipped |= _pdep_u64(FLIPPED[y][index], mask);

	mask = mask_x[pos][2];
	index = OUTFLANK[y][_bextr_u32(_pext_u64(O, mask), 1, 6)] & _pext_u64(P, mask);
	flipped |= _pdep_u64(FLIPPED[y][index], mask);

	return flipped;
}

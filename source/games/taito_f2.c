/* Current taito f2 driver - work in progress */

/* Most of this emulation is done by the external video chips :
   tc100scn for the background layers (and fg0)
   tc280grd/TC0430GRW for the tile with zoom/rotation

   In this file all I handle is the priorities, what mame calls the TC0360PRI.
   Congratulations to their team for their findings about this chip, no wonder
   Antiriad didn't guess it was a priority chip.

   Notice : the taito f2 is clearly not a standardized hardware, there are differences
   almost between every game, in the memory map, sometimes even in the video chips used.
   It's said on the web that dead connection and metal black use taito f2 hardware, but
   they don't use the tC100scn, they use a much more powerfull chip. So I will not emulate
   them here.

   Growl seems to have a bug, because I could reproduce it in mame : when you select your
   player, sometimes the background music does not start. It seems to depend on the time
   you spend before pressing the button. When you put 2 coins instead of 1, you have one
   more sound effect, and more chances that the music starts ! This bug made me quite
   crazy but it seems to be a game bug and not a raine bug.
   Also it tries to access what we call a tc220ioc for its inputs when it has none.
   Crappy code, really.
   There might be some sounds issues anyway, maybe the ym2610 does not start some
   samples sometimes but I am not totally sure...

   Here is the description of this chip from the info in mame.
   For the other 2 chips, look in tc100scn.c, tc005rot.c, and tc200obj.c for the sprites.

TC0360PRI
---------
Priority manager
A higher priority value means higher priority. 0 could mean disable but
I'm not sure. If two inputs have the same priority value, I think the first
one has priority, but I'm not sure of that either.
It seems the chip accepts three inputs from three different sources, and
each one of them can declare to have four different priority levels.

000 unknown. Could it be related to highlight/shadow effects in qcrayon2
    and gunfront?
001 in games with a roz layer, this is the roz palette bank (bottom 6 bits
    affect roz color, top 2 bits affect priority)
002 unknown
003 unknown

004 ----xxxx \       priority level 0 (unused? usually 0, pulirula sets it to F in some places)
    xxxx---- | Input priority level 1 (usually FG0)
005 ----xxxx |   #1  priority level 2 (usually BG0)
    xxxx---- /       priority level 3 (usually BG1)

006 ----xxxx \       priority level 0 (usually sprites with top color bits 00)
    xxxx---- | Input priority level 1 (usually sprites with top color bits 01)
007 ----xxxx |   #2  priority level 2 (usually sprites with top color bits 10)
    xxxx---- /       priority level 3 (usually sprites with top color bits 11)

008 ----xxxx \       priority level 0 (e.g. roz layer if top bits of register 001 are 00)
    xxxx---- | Input priority level 1 (e.g. roz layer if top bits of register 001 are 01)
009 ----xxxx |   #3  priority level 2 (e.g. roz layer if top bits of register 001 are 10)
    xxxx---- /       priority level 3 (e.g. roz layer if top bits of register 001 are 11)

00a unused
00b unused
00c unused
00d unused
00e unused
00f unused

Metalb uses in the PRI chip
---------------------------

+4	xxxx0000   BG1
	0000xxxx   BG0
+6	xxxx0000   BG3
	0000xxxx   BG2

About the clones :
I removed dinorexu and dinorexj, because they are just taito clones, which means only
1 region byte changes between the roms. The coinage changes too, but I really don't care
about the coinage.

Games known to use line scroll : don do kod, dino rex, thunder fox (every stage ?)
SSI can't use line scroll because it only uses sprites.
The others might use it at one place or another, but it's not easy to find !

*/

#include "gameinc.h"
#include "tc005rot.h"
#include "tc100scn.h"
#include "tc200obj.h"
#include "tc220ioc.h"
#include "sasound.h"		// sample support routines
#include "taitosnd.h"
#include "timer.h"
#include "blit.h"
#include "adpcm.h"
#include "tc110pcr.h"
#include "2203intf.h"

static UINT8 *TC0360PRI_regs;

static struct DIR_INFO don_doko_don_dirs[] =
{
   { "don_doko_don", },
   { "dondokod", },
   { "dondokdj", },
   { NULL, },
};

static struct DIR_INFO mega_blast_dirs[] =
{
   { "mega_blast", },
   { "megab", },
   { "megabl", },
   { NULL, },
};

static struct DIR_INFO liquid_kids_dirs[] =
{
   { "liquid_kids", },
   { "liquidk", },
   { NULL, },
};

/* Well this clone really looks like the original, but the roms are definetely different
   (see the hack in load_ssi : clearly different for the 2 roms. So well, let's keep it...
*/
static struct DIR_INFO majestic_twelve_dirs[] =
{
   { "majestic_twelve", },
   { "mj12", },
   { "majest12", },
   { ROMOF("ssi"), },
   { CLONEOF("ssi"), },
   { NULL, },
};

static struct DIR_INFO super_space_invaders_91_dirs[] =
{
   { "super_space_invaders_91", },
   { "ssi", },
   { "space_invaders_91", },
   { NULL, },
};

static struct DIR_INFO driftout_dirs[] =
{
   { "drift_out", },
   { "driftout", },
   { NULL, },
};

static struct DIR_INFO drive_out_dirs[] =
{
   { "drive_out", },
   { "driveout", },
   { ROMOF("driftout"), },
   { CLONEOF("driftout"), },
   { NULL, },
};

static struct DIR_INFO thunder_fox_dirs[] =
{
   { "thunder_fox", },
   { "thundfox", },
   { "thndfoxj", },
   { NULL, },
};

static struct DIR_INFO dino_rex_dirs[] =
{
   { "dino_rex", },
   { "dinorex", },
   { NULL, },
};

static struct DIR_INFO f2demo_dirs[] =
{
   { "f2demo", },
   { NULL, },
};

static struct DIR_INFO mahjong_quest_dirs[] =
{
   { "mahjong_quest", },
   { "mjnquest", },
   { NULL, },
};

static struct DIR_INFO solitary_fighter_dirs[] =
{
   { "solitary_fighter", },
   { "solfigtr", },
   { NULL, },
};

static struct DIR_INFO final_blow_dirs[] =
{
   { "final_blow", },
   { "finalb", },
   { "finalbl", },
   { NULL, },
};

static struct DIR_INFO growl_dirs[] =
{
  { "growl" },
  { NULL }
};

static struct DIR_INFO gun_frontier_dirs[] =
{
   { "gun_frontier", },
   { "gunfront", },
   { NULL, },
};

static struct DIR_INFO cameltry_dirs[] =
{
  { "cameltry" },
  { NULL }
};

static struct DIR_INFO camltrua_dirs[] =
{
  { "camltrua" },
  { ROMOF("cameltry") },
  { CLONEOF("cameltry"), },
  { NULL }
};

static struct ROM_INFO cameltry_roms[] =
{
  { "c38-11", 0x20000, 0xbe172da0, REGION_ROM1, 0x00000, LOAD_8_16 },
  { "c38-14", 0x20000, 0xffa430de, REGION_ROM1, 0x00001, LOAD_8_16 },
	/* empty! */
  { "c38-01.bin", 0x80000, 0xc170ff36, REGION_GFX2, 0x00000, LOAD_NORMAL },
  { "c38-02.bin", 0x20000, 0x1a11714b, REGION_GFX3, 0x00000, LOAD_NORMAL },
  { "c38-08.bin", 0x10000, 0x7ff78873, REGION_ROM2, 0, LOAD_NORMAL },
	/* no Delta-T samples */
  { "c38-03.bin", 0x020000, 0x59fa59a7, REGION_SMP1, 0x000000, LOAD_NORMAL },
  { NULL, 0, 0, 0, 0, 0 }
};

static struct ROM_INFO camltrua_roms[] =
{
	/* empty! */
  { "c38-us.15", 0x10000, 0x0e60faac, REGION_ROM2, 0x00000, LOAD_NORMAL },
	/* no Delta-T samples */
  { NULL, 0, 0, 0, 0, 0 }
};

static struct ROM_INFO gunfront_roms[] =
{
  { "c71-09.42", 0x20000, 0x10a544a2, REGION_ROM1, 0x00000, LOAD_8_16 },
  { "c71-08.41", 0x20000, 0xc17dc0a0, REGION_ROM1, 0x00001, LOAD_8_16 },
  { "c71-10.40", 0x20000, 0xf39c0a06, REGION_ROM1, 0x40000, LOAD_8_16 },
  { "c71-14.39", 0x20000, 0x312da036, REGION_ROM1, 0x40001, LOAD_8_16 },
  { "c71-16.38", 0x20000, 0x1bbcc2d4, REGION_ROM1, 0x80000, LOAD_8_16 },
  { "c71-15.37", 0x20000, 0xdf3e00bb, REGION_ROM1, 0x80001, LOAD_8_16 },
  { "c71-02.59", 0x100000, 0x2a600c92, REGION_GFX1, 0x000000, LOAD_NORMAL },
  { "c71-03.19", 0x100000, 0x9133c605, REGION_GFX2, 0x000000, LOAD_NORMAL },
  { "c71-12.49", 0x10000, 0x0038c7f8, REGION_ROM2, 0, LOAD_NORMAL },
	/* no Delta-T samples */
/* Pals c71-16.28  c71-07.27 */
  { "c71-01.29", 0x100000, 0x0e73105a, REGION_SMP1, 0x000000, LOAD_NORMAL },
  { NULL, 0, 0, 0, 0, 0 }
};

static struct ROM_INFO growl_roms[] =
{
  { "c74-10.59", 0x40000, 0xca81a20b, REGION_ROM1, 0x00000, LOAD_8_16 },
  { "c74-08.61", 0x40000, 0xaa35dd9e, REGION_ROM1, 0x00001, LOAD_8_16 },
  { "c74-11.58", 0x40000, 0xee3bd6d5, REGION_ROM1, 0x80000, LOAD_8_16 },
  { "c74-14.60", 0x40000, 0xb6c24ec7, REGION_ROM1, 0x80001, LOAD_8_16 },
  { "c74-01.34", 0x100000, 0x3434ce80, REGION_GFX1, 0x000000, LOAD_NORMAL },
  { "c74-03.12", 0x100000, 0x1a0d8951, REGION_GFX2, 0x000000, LOAD_NORMAL },
  { "c74-02.11", 0x100000, 0x15a21506, REGION_GFX2, 0x100000, LOAD_NORMAL },
  { "c74-12.62", 0x10000, 0xbb6ed668, REGION_ROM2, 0, LOAD_NORMAL },
  { "c74-04.28", 0x100000, 0x2d97edf2, REGION_SMP1, 0x000000, LOAD_NORMAL },
/*Pals c74-06.48  c74-07.47 */
  { "c74-05.29", 0x080000, 0xe29c0828, REGION_SMP2, 0x000000, LOAD_NORMAL },
  { NULL, 0, 0, 0, 0, 0 }
};

static struct ROM_INFO finalb_roms[] =
{
  { "b82_09.10", 0x20000, 0x632f1ecd, REGION_ROM1, 0x00000, LOAD_8_16 },
  { "b82_17.11", 0x20000, 0xe91b2ec9, REGION_ROM1, 0x00001, LOAD_8_16 },
  { "b82-06.19", 0x20000, 0xfc450a25, REGION_GFX1, 0x00000, LOAD_8_16 },
  { "b82-07.18", 0x20000, 0xec3df577, REGION_GFX1, 0x00001, LOAD_8_16 },
  { "b82-04.4", 0x80000, 0x6346f98e, REGION_GFX2, 0x000000, LOAD_8_16 },
	/* Note: this is intentional to load at 0x180000, not at 0x100000
	   because finalb_driver_init will move some bits around before data
	   will be 'gfxdecoded'. The whole thing is because this data is 2bits-
	   while above is 4bits-packed format, for a total of 6 bits per pixel. */
  { "b82-03.5", 0x80000, 0xdaa11561, REGION_GFX2, 0x000001, LOAD_8_16 },
  { "b82-05.3", 0x80000, 0xaa90b93a, REGION_GFX2, 0x180000, LOAD_NORMAL },
  { "b82_10.16", 0x10000, 0xa38aaaed, REGION_ROM2, 0, LOAD_NORMAL },
  { "b82-02.1", 0x80000, 0x5dd06bdd, REGION_SMP1, 0x00000, LOAD_NORMAL },
  { "b82-01.2", 0x80000, 0xf0eb6846, REGION_SMP2, 0x00000, LOAD_NORMAL },
  { NULL, 0, 0, 0, 0, 0 }
};

static struct ROM_INFO solfigtr_roms[] =
{
  { "c91-05.59", 0x40000, 0xc1260e7c, REGION_ROM1, 0x00000, LOAD_8_16 },
  { "c91-09.61", 0x40000, 0xd82b5266, REGION_ROM1, 0x00001, LOAD_8_16 },
  { "c91-03.34", 0x100000, 0x8965da12, REGION_GFX1, 0x000000, LOAD_NORMAL },
  { "c91-01.12", 0x100000, 0x0f3f4e00, REGION_GFX2, 0x000000, LOAD_NORMAL },
  { "c91-02.11", 0x100000, 0xe14ab98e, REGION_GFX2, 0x100000, LOAD_NORMAL },
  { "c91-07.62", 0x10000, 0xe471a05a, REGION_ROM2, 0, LOAD_NORMAL },
	/* no Delta-T samples */
/*Pals c74-06.48 */
  { "c91-04.28", 0x80000, 0x390b1065, REGION_SMP1, 0x00000, LOAD_NORMAL },
  { NULL, 0, 0, 0, 0, 0 }
};

static struct ROM_INFO mjnquest_roms[] =
{
  { "c77-09", 0x020000, 0x0a005d01, REGION_ROM1, 0x000000, LOAD_8_16 },
  { "c77-08", 0x020000, 0x4244f775, REGION_ROM1, 0x000001, LOAD_8_16 },
  { "c77-04", 0x080000, 0xc2e7e038, REGION_ROM1, 0x080000, LOAD_SWAP_16 },
  { "c77-01", 0x100000, 0x5ba51205, REGION_GFX1, 0x000000, LOAD_NORMAL },
  { "c77-02", 0x100000, 0x6a6f3040, REGION_GFX1, 0x100000, LOAD_NORMAL },
  { "c77-05", 0x80000, 0xc5a54678, REGION_GFX2, 0x00000, LOAD_NORMAL },
  { "c77-10", 0x10000, 0xf16b2c1e, REGION_ROM2, 0, LOAD_NORMAL },
	/* no Delta-T samples */
  { "c77-03", 0x080000, 0x312f17b1, REGION_SMP1, 0x000000, LOAD_NORMAL },
  { NULL, 0, 0, 0, 0, 0 }
};

static struct ROM_INFO f2demo_roms[] =
{
   {  "lk_obj0.bin", 0x00080000, 0x1bb8aa37, REGION_GFX2, 0, LOAD_NORMAL },
   {  "lk_obj1.bin", 0x00080000, 0x75660aac, REGION_GFX2, 0x80000, LOAD_NORMAL },
   {   "lk_scr.bin", 0x00080000, 0xb178fb05, REGION_GFX1, 0, LOAD_NORMAL },
   {     "lq09.bin", 0x00020000, 0x809a968b, REGION_ROM1, 0, LOAD_8_16, },
   {     "lq11.bin", 0x00020000, 0x7ba3a5cb, REGION_ROM1, 1, LOAD_8_16 },
   {     "lq10.bin", 0x00020000, 0x7ee8cdcd, REGION_ROM1, 0x40000, LOAD_8_16 },
   {     "lq12.bin", 0x00020000, 0x7ee8cdcd, REGION_ROM1, 0x40001, LOAD_8_16 },
   {           NULL,          0,          0, 0, 0, 0, },
};

static struct ROM_INFO dinorex_roms[] =
{
  { "d39_14.9", 0x080000, 0xe6aafdac, REGION_ROM1, 	0x000000, LOAD_8_16 },
  { "d39_16.8", 0x080000, 0xcedc8537, REGION_ROM1, 	0x000001, LOAD_8_16 },
  { "d39-04.6", 0x100000, 0x3800506d, REGION_ROM1, 	0x100000, LOAD_SWAP_16 },
  { "d39-05.7", 0x100000, 0xe2ec3b5d, REGION_ROM1, 	0x200000, LOAD_SWAP_16 },
  { "d39-06.2", 0x100000, 0x52f62835, REGION_GFX1, 	0x000000, LOAD_NORMAL },
  { "d39-01.29", 0x200000, 0xd10e9c7d, REGION_GFX2, 	0x000000, LOAD_NORMAL },
  { "d39-02.28", 0x200000, 0x6c304403, REGION_GFX2, 	0x200000, LOAD_NORMAL },
  { "d39-03.27", 0x200000, 0xfc9cdab4, REGION_GFX2, 	0x400000, LOAD_NORMAL },
  { "d39_12.5", 0x10000, 0x8292c7c1, REGION_ROM2, 0, LOAD_NORMAL },
  { "d39-07.10", 0x100000, 0x28262816, REGION_SMP1, 	0x000000, LOAD_NORMAL },
  { "d39-08.4", 0x080000, 0x377b8b7b, REGION_SMP2, 	0x000000, LOAD_NORMAL },
  { NULL, 0, 0, 0, 0, 0 }
};

static struct ROM_INFO thundfox_roms[] =
{
  { "c28-13-1.51", 0x20000, 0xacb07013, REGION_ROM1, 0x00000, LOAD_8_16 },
  { "c28-16-1.40", 0x20000, 0x1e43d55b, REGION_ROM1, 0x00001, LOAD_8_16 },
  { "c28-08.50", 0x20000, 0x38e038f1, REGION_ROM1, 0x40000, LOAD_8_16 },
  { "c28-07.39", 0x20000, 0x24419abb, REGION_ROM1, 0x40001, LOAD_8_16 },
  { "c28-02.61", 0x80000, 0x6230a09d, REGION_GFX1, 0x000000, LOAD_NORMAL },
  { "c28-03.29", 0x80000, 0x51bdc7af, REGION_GFX2, 0x00000, LOAD_8_16 },
  { "c28-04.28", 0x80000, 0xba7ed535, REGION_GFX2, 0x00001, LOAD_8_16 },
  { "c28-01.63", 0x80000, 0x44552b25, REGION_GFX3, 0x000000, LOAD_NORMAL },
  { "c28-14.3", 0x10000, 0x45ef3616, REGION_ROM2, 0, LOAD_NORMAL },
  { "c28-06.41", 0x80000, 0xdb6983db, REGION_SMP1, 0x00000, LOAD_NORMAL },
/* Pals: c28-09.25  c28-10.26  c28-11.35  b89-01.19  b89-03.37  b89-04.33 */
  { "c28-05.42", 0x80000, 0xd3b238fa, REGION_SMP2, 0x00000, LOAD_NORMAL },
  { NULL, 0, 0, 0, 0, 0 }
};

static struct ROM_INFO driveout_roms[] =
{
  { "driveout.003", 0x80000, 0xdc431e4e, REGION_ROM1, 0x00000, LOAD_8_16 },
  { "driveout.002", 0x80000, 0x6f9063f4, REGION_ROM1, 0x00001, LOAD_8_16 },
	/* empty */
  { "driveout.084", 0x40000, 0x530ac420, REGION_GFX2, 0x00000, LOAD_8_16 },
  { "driveout.081", 0x40000, 0x0e9a3e9e, REGION_GFX2, 0x00001, LOAD_8_16 },
  { "driveout.020", 0x8000, 0x99aaeb2e, REGION_ROM2, 0x0000, LOAD_NORMAL },
  { "driveout.028", 0x80000, 0xcbde0b66, REGION_SMP1, 0, LOAD_NORMAL },
  { "driveout.029", 0x20000, 0x0aba2026, REGION_SMP1, 0x20000, LOAD_NORMAL },
  { "driveout.029", 0x20000, 0x0aba2026, REGION_SMP1, 0x60000, LOAD_NORMAL },
  { "driveout.029", 0x20000, 0x0aba2026, REGION_SMP1, 0xa0000, LOAD_NORMAL },
  { "driveout.029", 0x20000, 0x0aba2026, REGION_SMP1, 0xe0000, LOAD_NORMAL },
  { NULL, 0, 0, 0, 0, 0 }
};

static struct ROM_INFO driftout_roms[] =
{
  { "do_46.rom", 0x80000, 0xf960363e, REGION_ROM1, 0x00000, LOAD_8_16 },
  { "do_45.rom", 0x80000, 0xe3fe66b9, REGION_ROM1, 0x00001, LOAD_8_16 },
	/* empty */
  { "do_obj.rom", 0x80000, 0x5491f1c4, REGION_GFX2, 0x00000, LOAD_NORMAL },
  { "do_piv.rom", 0x80000, 0xc4f012f7, REGION_GFX3, 0x00000, LOAD_NORMAL },
  { "do_50.rom", 0x10000, 0xffe10124, REGION_ROM2, 0, LOAD_NORMAL },
	/* no Delta-T samples */
  { "do_snd.rom", 0x80000, 0xf2deb82b, REGION_SMP1, 0x00000, LOAD_NORMAL },
  { NULL, 0, 0, 0, 0, 0 }
};

static struct ROM_INFO ssi_roms[] =
{
   {  "c64-01.1", 0x00100000, 0xa1b4f486, REGION_GFX2, 0x000000, LOAD_NORMAL, },
   {  "c64-02.2", 0x00020000, 0x3cb0b907, REGION_SMP1, 0x000000, LOAD_NORMAL, },
   { "c64_15-1.bin", 0x00040000, 0xce9308a6, REGION_ROM1, 0x000000, LOAD_8_16,   },
   { "c64_16-1.bin", 0x00040000, 0x470a483a, REGION_ROM1, 0x000001, LOAD_8_16,   },
   {   "c64_09.13", 0x00010000, 0x88d7f65c, REGION_ROM2, 0x000000, LOAD_NORMAL, },
   {           NULL,          0,          0, 0,           0,        0,           },
};

static struct ROM_INFO mj12_roms[] =
{
   {   "c64_07.10", 0x00020000, 0xf29ed5c9, REGION_ROM1, 0x000000, LOAD_8_16,   },
   {   "c64_08.11", 0x00020000, 0xddfd33d5, REGION_ROM1, 0x000001, LOAD_8_16,   },
   {   "c64_06.4", 0x00020000, 0x18dc71ac, REGION_ROM1, 0x040000, LOAD_8_16,   },
   {   "c64_05.5", 0x00020000, 0xb61866c0, REGION_ROM1, 0x040001, LOAD_8_16,   },
   {           NULL,          0,          0, 0,           0,        0,           },
};

static struct ROM_INFO dondokod_roms[] =
{
  { "b95-12.bin", 0x20000, 0xd0fce87a, REGION_ROM1, 0x00000, LOAD_8_16 },
  { "b95-11-1.bin", 0x20000, 0xdad40cd3, REGION_ROM1, 0x00001, LOAD_8_16 },
  { "b95-10.bin", 0x20000, 0xa46e1f0b, REGION_ROM1, 0x40000, LOAD_8_16 },
  { "b95-wrld.7", 0x20000, 0x6e4e1351, REGION_ROM1, 0x40001, LOAD_8_16 },
  { "b95-02.bin", 0x80000, 0x67b4e979, REGION_GFX1, 0x00000, LOAD_NORMAL },
  { "b95-01.bin", 0x80000, 0x51c176ce, REGION_GFX2, 0x00000, LOAD_NORMAL },
  { "b95-03.bin", 0x80000, 0x543aa0d1, REGION_GFX3, 0x00000, LOAD_NORMAL },
  { "b95-08.bin", 0x10000, 0xb5aa49e1, REGION_ROM2, 0, LOAD_NORMAL },
	/* no Delta-T samples */
  { "b95-04.bin", 0x80000, 0xac4c1716, REGION_SMP1, 0x00000, LOAD_NORMAL },
  { NULL, 0, 0, 0, 0, 0 }
};

static struct ROM_INFO megab_roms[] =
{
  { "c11-07.55", 0x20000, 0x11d228b6, REGION_ROM1, 0x00000, LOAD_8_16 },
  { "c11-08.39", 0x20000, 0xa79d4dca, REGION_ROM1, 0x00001, LOAD_8_16 },
  { "c11-06.54", 0x20000, 0x7c249894, REGION_ROM1, 0x40000, LOAD_8_16 },
  { "c11-11.38", 0x20000, 0x263ecbf9, REGION_ROM1, 0x40001, LOAD_8_16 },
  { "c11-05.58", 0x80000, 0x733e6d8e, REGION_GFX1, 0x00000, LOAD_NORMAL },
  { "c11-03.32", 0x80000, 0x46718c7a, REGION_GFX2, 0x00000, LOAD_8_16 },
  { "c11-04.31", 0x80000, 0x663f33cc, REGION_GFX2, 0x00001, LOAD_8_16 },
  { "c11-12.3", 0x10000, 0xb11094f1, REGION_ROM2, 0, LOAD_NORMAL },
  { "c11-01.29", 0x80000, 0xfd1ea532, REGION_SMP1, 0x00000, LOAD_NORMAL },
/*Pals  b89-01.8  b89-02.28  b89-04.27  c11-13.13  c11-14.23 */
  { "c11-02.30", 0x80000, 0x451cc187, REGION_SMP2, 0x00000, LOAD_NORMAL },
  { NULL, 0, 0, 0, 0, 0 }
};

static struct ROM_INFO liquidk_roms[] =
{
  { "c49_09.12", 0x20000, 0x6ae09eb9, REGION_ROM1, 0x00000, LOAD_8_16 },
  { "c49_11.14", 0x20000, 0x42d2be6e, REGION_ROM1, 0x00001, LOAD_8_16 },
  { "c49_10.13", 0x20000, 0x50bef2e0, REGION_ROM1, 0x40000, LOAD_8_16 },
  { "c49_12.15", 0x20000, 0xcb16bad5, REGION_ROM1, 0x40001, LOAD_8_16 },
  { "lk_scr.bin", 0x80000, 0xc3364f9b, REGION_GFX1, 0x00000, LOAD_NORMAL },
  { "lk_obj0.bin", 0x80000, 0x67cc3163, REGION_GFX2, 0x00000, LOAD_NORMAL },
  { "lk_obj1.bin", 0x80000, 0xd2400710, REGION_GFX2, 0x80000, LOAD_NORMAL },
  { "c49_08.9", 0x10000, 0x413c310c, REGION_ROM2, 0, LOAD_NORMAL },
	/* no Delta-T samples */
  { "lk_snd.bin", 0x80000, 0x474d45a4, REGION_SMP1, 0x00000, LOAD_NORMAL },
  { NULL, 0, 0, 0, 0, 0 }
};

static struct DIR_INFO pulirula_dirs[] =
{
   { "pulirula", },
   { NULL, },
};

static struct ROM_INFO pulirula_roms[] =
{
  { "c98-12.rom", 0x40000, 0x816d6cde, REGION_ROM1, 0x00000, LOAD_8_16 },
  { "c98-16.rom", 0x40000, 0x59df5c77, REGION_ROM1, 0x00001, LOAD_8_16 },
  { "c98-06.rom", 0x20000, 0x64a71b45, REGION_ROM1, 0x80000, LOAD_8_16 },
  { "c98-07.rom", 0x20000, 0x90195bc0, REGION_ROM1, 0x80001, LOAD_8_16 },
  { "c98-04.rom", 0x100000, 0x0e1fe3b2, REGION_GFX1, 0x000000, LOAD_NORMAL },
  { "c98-02.rom", 0x100000, 0x4a2ad2b3, REGION_GFX2, 0x000000, LOAD_NORMAL },
  { "c98-03.rom", 0x100000, 0x589a678f, REGION_GFX2, 0x100000, LOAD_NORMAL },
  { "c98-05.rom", 0x080000, 0x9ddd9c39, REGION_GFX3, 0x000000, LOAD_NORMAL },
  { "c98-14.rom", 0x20000, 0xa858e17c, REGION_ROM2, 0, LOAD_NORMAL },
	/* no Delta-T samples */
  { "c98-01.rom", 0x100000, 0x197f66f5, REGION_SMP1, 0x000000, LOAD_NORMAL },
  { NULL, 0, 0, 0, 0, 0 }
};

/*
static struct ROM_INFO koshien_roms[] =
{
  { "c81-11.bin", 0x020000, 0xb44ea8c9, REGION_ROM1, 0x000000, LOAD_8_16 },
  { "c81-10.bin", 0x020000, 0x8f98c40a, REGION_ROM1, 0x000001, LOAD_8_16 },
  { "c81-04.bin", 0x080000, 0x1592b460, REGION_ROM1, 0x080000, LOAD_SWAP_16 },
  { "c81-03.bin", 0x100000, 0x29bbf492, REGION_GFX1, 0x000000, LOAD_NORMAL },
  { "c81-01.bin", 0x100000, 0x64b15d2a, REGION_GFX2, 0x000000, LOAD_NORMAL },
  { "c81-02.bin", 0x100000, 0x962461e8, REGION_GFX2, 0x100000, LOAD_NORMAL },
  { "c81-12.bin", 0x10000, 0x6e8625b6, REGION_ROM2, 0, LOAD_NORMAL },
  { "c81-05.bin", 0x80000, 0x9c3d71be, REGION_SMP1, 0x00000, LOAD_NORMAL },
  { "c81-06.bin", 0x80000, 0x927833b4, REGION_SMP2, 0x00000, LOAD_NORMAL },
  { NULL, 0, 0, 0, 0, 0 }
};
*/

static struct INPUT_INFO don_doko_don_inputs[] =
{
   { KB_DEF_COIN1,        MSG_COIN1,               0x03210E, 0x04, BIT_ACTIVE_0 },
   { KB_DEF_COIN2,        MSG_COIN2,               0x03210E, 0x08, BIT_ACTIVE_0 },
   { KB_DEF_TILT,         MSG_TILT,                0x03210E, 0x01, BIT_ACTIVE_0 },
   { KB_DEF_SERVICE,      MSG_SERVICE,             0x03210E, 0x02, BIT_ACTIVE_0 },

   { KB_DEF_P1_START,     MSG_P1_START,            0x032104, 0x80, BIT_ACTIVE_0 },
   { KB_DEF_P1_UP,        MSG_P1_UP,               0x032104, 0x01, BIT_ACTIVE_0 },
   { KB_DEF_P1_DOWN,      MSG_P1_DOWN,             0x032104, 0x02, BIT_ACTIVE_0 },
   { KB_DEF_P1_LEFT,      MSG_P1_LEFT,             0x032104, 0x04, BIT_ACTIVE_0 },
   { KB_DEF_P1_RIGHT,     MSG_P1_RIGHT,            0x032104, 0x08, BIT_ACTIVE_0 },
   { KB_DEF_P1_B1,        MSG_P1_B1,               0x032104, 0x10, BIT_ACTIVE_0 },
   { KB_DEF_P1_B2,        MSG_P1_B2,               0x032104, 0x20, BIT_ACTIVE_0 },

   { KB_DEF_P2_START,     MSG_P2_START,            0x032106, 0x80, BIT_ACTIVE_0 },
   { KB_DEF_P2_UP,        MSG_P2_UP,               0x032106, 0x01, BIT_ACTIVE_0 },
   { KB_DEF_P2_DOWN,      MSG_P2_DOWN,             0x032106, 0x02, BIT_ACTIVE_0 },
   { KB_DEF_P2_LEFT,      MSG_P2_LEFT,             0x032106, 0x04, BIT_ACTIVE_0 },
   { KB_DEF_P2_RIGHT,     MSG_P2_RIGHT,            0x032106, 0x08, BIT_ACTIVE_0 },
   { KB_DEF_P2_B1,        MSG_P2_B1,               0x032106, 0x10, BIT_ACTIVE_0 },
   { KB_DEF_P2_B2,        MSG_P2_B2,               0x032106, 0x20, BIT_ACTIVE_0 },

   { 0,                   NULL,                    0,        0,    0            },
};

// 3 buttons
static struct INPUT_INFO thunder_fox_inputs[] =
{
   { KB_DEF_COIN1,        MSG_COIN1,               0x02E20E, 0x04, BIT_ACTIVE_0 },
   { KB_DEF_COIN2,        MSG_COIN2,               0x02E20E, 0x08, BIT_ACTIVE_0 },
   { KB_DEF_TILT,         MSG_TILT,                0x02E20E, 0x01, BIT_ACTIVE_0 },
   { KB_DEF_SERVICE,      MSG_SERVICE,             0x02E20E, 0x02, BIT_ACTIVE_0 },

   { KB_DEF_P1_START,     MSG_P1_START,            0x02E204, 0x80, BIT_ACTIVE_0 },
   { KB_DEF_P1_UP,        MSG_P1_UP,               0x02E204, 0x01, BIT_ACTIVE_0 },
   { KB_DEF_P1_DOWN,      MSG_P1_DOWN,             0x02E204, 0x02, BIT_ACTIVE_0 },
   { KB_DEF_P1_LEFT,      MSG_P1_LEFT,             0x02E204, 0x04, BIT_ACTIVE_0 },
   { KB_DEF_P1_RIGHT,     MSG_P1_RIGHT,            0x02E204, 0x08, BIT_ACTIVE_0 },
   { KB_DEF_P1_B1,        MSG_P1_B1,               0x02E204, 0x10, BIT_ACTIVE_0 },
   { KB_DEF_P1_B2,        MSG_P1_B2,               0x02E204, 0x20, BIT_ACTIVE_0 },
   { KB_DEF_P1_B3,        MSG_P1_B3,               0x02E204, 0x40, BIT_ACTIVE_0 },

   { KB_DEF_P2_START,     MSG_P2_START,            0x02E206, 0x80, BIT_ACTIVE_0 },
   { KB_DEF_P2_UP,        MSG_P2_UP,               0x02E206, 0x01, BIT_ACTIVE_0 },
   { KB_DEF_P2_DOWN,      MSG_P2_DOWN,             0x02E206, 0x02, BIT_ACTIVE_0 },
   { KB_DEF_P2_LEFT,      MSG_P2_LEFT,             0x02E206, 0x04, BIT_ACTIVE_0 },
   { KB_DEF_P2_RIGHT,     MSG_P2_RIGHT,            0x02E206, 0x08, BIT_ACTIVE_0 },
   { KB_DEF_P2_B1,        MSG_P2_B1,               0x02E206, 0x10, BIT_ACTIVE_0 },
   { KB_DEF_P2_B2,        MSG_P2_B2,               0x02E206, 0x20, BIT_ACTIVE_0 },
   { KB_DEF_P2_B3,        MSG_P2_B3,               0x02E206, 0x40, BIT_ACTIVE_0 },

   { 0,                   NULL,                    0,        0,    0            },
};

// finalb inputs can't have the address of thunder fox, it would be a waste of ram !
static struct INPUT_INFO final_blow_inputs[] =
{
   { KB_DEF_COIN1,        MSG_COIN1,               0x03C00E, 0x04, BIT_ACTIVE_0 },
   { KB_DEF_COIN2,        MSG_COIN2,               0x03C00E, 0x08, BIT_ACTIVE_0 },
   { KB_DEF_TILT,         MSG_TILT,                0x03C00E, 0x01, BIT_ACTIVE_0 },
   { KB_DEF_SERVICE,      MSG_SERVICE,             0x03C00E, 0x02, BIT_ACTIVE_0 },

   { KB_DEF_P1_START,     MSG_P1_START,            0x03C004, 0x80, BIT_ACTIVE_0 },
   { KB_DEF_P1_UP,        MSG_P1_UP,               0x03C004, 0x01, BIT_ACTIVE_0 },
   { KB_DEF_P1_DOWN,      MSG_P1_DOWN,             0x03C004, 0x02, BIT_ACTIVE_0 },
   { KB_DEF_P1_LEFT,      MSG_P1_LEFT,             0x03C004, 0x04, BIT_ACTIVE_0 },
   { KB_DEF_P1_RIGHT,     MSG_P1_RIGHT,            0x03C004, 0x08, BIT_ACTIVE_0 },
   { KB_DEF_P1_B1,        MSG_P1_B1,               0x03C004, 0x10, BIT_ACTIVE_0 },
   { KB_DEF_P1_B2,        MSG_P1_B2,               0x03C004, 0x20, BIT_ACTIVE_0 },
   { KB_DEF_P1_B3,        MSG_P1_B3,               0x03C004, 0x40, BIT_ACTIVE_0 },

   { KB_DEF_P2_START,     MSG_P2_START,            0x03C006, 0x80, BIT_ACTIVE_0 },
   { KB_DEF_P2_UP,        MSG_P2_UP,               0x03C006, 0x01, BIT_ACTIVE_0 },
   { KB_DEF_P2_DOWN,      MSG_P2_DOWN,             0x03C006, 0x02, BIT_ACTIVE_0 },
   { KB_DEF_P2_LEFT,      MSG_P2_LEFT,             0x03C006, 0x04, BIT_ACTIVE_0 },
   { KB_DEF_P2_RIGHT,     MSG_P2_RIGHT,            0x03C006, 0x08, BIT_ACTIVE_0 },
   { KB_DEF_P2_B1,        MSG_P2_B1,               0x03C006, 0x10, BIT_ACTIVE_0 },
   { KB_DEF_P2_B2,        MSG_P2_B2,               0x03C006, 0x20, BIT_ACTIVE_0 },
   { KB_DEF_P2_B3,        MSG_P2_B3,               0x03C006, 0x40, BIT_ACTIVE_0 },

   { 0,                   NULL,                    0,        0,    0            },
};

// 4 players
static struct INPUT_INFO growl_inputs[] =
{
   { KB_DEF_COIN1,        MSG_COIN1,               0x032384, 0x04, BIT_ACTIVE_0 },
   { KB_DEF_COIN2,        MSG_COIN2,               0x032384, 0x08, BIT_ACTIVE_0 },
   { KB_DEF_TILT,         MSG_TILT,                0x032384, 0x01, BIT_ACTIVE_0 },
   { KB_DEF_SERVICE,      MSG_SERVICE,             0x032384, 0x02, BIT_ACTIVE_0 },

   { KB_DEF_P1_START,     MSG_P1_START,            0x032380, 0x80, BIT_ACTIVE_0 },
   { KB_DEF_P1_UP,        MSG_P1_UP,               0x032380, 0x01, BIT_ACTIVE_0 },
   { KB_DEF_P1_DOWN,      MSG_P1_DOWN,             0x032380, 0x02, BIT_ACTIVE_0 },
   { KB_DEF_P1_LEFT,      MSG_P1_LEFT,             0x032380, 0x04, BIT_ACTIVE_0 },
   { KB_DEF_P1_RIGHT,     MSG_P1_RIGHT,            0x032380, 0x08, BIT_ACTIVE_0 },
   { KB_DEF_P1_B1,        MSG_P1_B1,               0x032380, 0x10, BIT_ACTIVE_0 },
   { KB_DEF_P1_B2,        MSG_P1_B2,               0x032380, 0x20, BIT_ACTIVE_0 },
   { KB_DEF_P1_B3,        MSG_P1_B3,               0x032380, 0x40, BIT_ACTIVE_0 },

   { KB_DEF_P2_START,     MSG_P2_START,            0x032382, 0x80, BIT_ACTIVE_0 },
   { KB_DEF_P2_UP,        MSG_P2_UP,               0x032382, 0x01, BIT_ACTIVE_0 },
   { KB_DEF_P2_DOWN,      MSG_P2_DOWN,             0x032382, 0x02, BIT_ACTIVE_0 },
   { KB_DEF_P2_LEFT,      MSG_P2_LEFT,             0x032382, 0x04, BIT_ACTIVE_0 },
   { KB_DEF_P2_RIGHT,     MSG_P2_RIGHT,            0x032382, 0x08, BIT_ACTIVE_0 },
   { KB_DEF_P2_B1,        MSG_P2_B1,               0x032382, 0x10, BIT_ACTIVE_0 },
   { KB_DEF_P2_B2,        MSG_P2_B2,               0x032382, 0x20, BIT_ACTIVE_0 },
   { KB_DEF_P2_B3,        MSG_P2_B3,               0x032382, 0x40, BIT_ACTIVE_0 },

  { KB_DEF_P3_UP, MSG_P3_UP, 0x04, 0x01, BIT_ACTIVE_0 },
  { KB_DEF_P3_DOWN, MSG_P3_DOWN, 0x04, 0x02, BIT_ACTIVE_0 },
  { KB_DEF_P3_LEFT, MSG_P3_LEFT, 0x04, 0x04, BIT_ACTIVE_0 },
  { KB_DEF_P3_RIGHT, MSG_P3_RIGHT, 0x04, 0x08, BIT_ACTIVE_0 },
  { KB_DEF_P3_B1, MSG_P3_B1, 0x04, 0x10, BIT_ACTIVE_0 },
  { KB_DEF_P3_B2, MSG_P3_B2, 0x04, 0x20, BIT_ACTIVE_0 },
  { KB_DEF_P3_B3, MSG_P3_B3, 0x04, 0x40, BIT_ACTIVE_0 },
  { KB_DEF_P3_START, MSG_P3_START, 0x04, 0x80, BIT_ACTIVE_0 },
  { KB_DEF_P4_UP, MSG_P4_UP, 0x05, 0x01, BIT_ACTIVE_0 },
  { KB_DEF_P4_DOWN, MSG_P4_DOWN, 0x05, 0x02, BIT_ACTIVE_0 },
  { KB_DEF_P4_LEFT, MSG_P4_LEFT, 0x05, 0x04, BIT_ACTIVE_0 },
  { KB_DEF_P4_RIGHT, MSG_P4_RIGHT, 0x05, 0x08, BIT_ACTIVE_0 },
  { KB_DEF_P4_B1, MSG_P4_B1, 0x05, 0x10, BIT_ACTIVE_0 },
  { KB_DEF_P4_B2, MSG_P4_B2, 0x05, 0x20, BIT_ACTIVE_0 },
  { KB_DEF_P4_B3, MSG_P4_B3, 0x05, 0x40, BIT_ACTIVE_0 },
  { KB_DEF_P4_START, MSG_P4_START, 0x05, 0x80, BIT_ACTIVE_0 },

  { KB_DEF_COIN3, MSG_COIN3, 0x06, 0x01, BIT_ACTIVE_0 },
  { KB_DEF_COIN4, MSG_COIN4, 0x06, 0x02, BIT_ACTIVE_0 },
  { KB_DEF_SERVICE,      MSG_SERVICE,             6, 0x04, BIT_ACTIVE_0 },
   { 0,                   NULL,                    0,        0,    0            },
};

// mahjong quest inputs (generated by conv_inputs.pl from the mame source...)
static struct INPUT_INFO mjnquest_inputs[] =
{
  { KB_DEF_P1_A, MSG_P1_A, 0x00, 0x01, BIT_ACTIVE_0 },
  { KB_DEF_P1_E, MSG_P1_E, 0x00, 0x02, BIT_ACTIVE_0 },
  { KB_DEF_P1_I, MSG_P1_I, 0x00, 0x04, BIT_ACTIVE_0 },
  { KB_DEF_P1_M, MSG_P1_M, 0x00, 0x08, BIT_ACTIVE_0 },
  { KB_DEF_P1_KAN, MSG_P1_KAN, 0x00, 0x10, BIT_ACTIVE_0 },
  { KB_DEF_P1_START, MSG_P1_START, 0x00, 0x20, BIT_ACTIVE_0 },

  { KB_DEF_P1_B, MSG_P1_B, 0x02, 0x01, BIT_ACTIVE_0 },
  { KB_DEF_P1_F, MSG_P1_F, 0x02, 0x02, BIT_ACTIVE_0 },
  { KB_DEF_P1_J, MSG_P1_J, 0x02, 0x04, BIT_ACTIVE_0 },
  { KB_DEF_P1_N, MSG_P1_N, 0x02, 0x08, BIT_ACTIVE_0 },
  { KB_DEF_P1_REACH, MSG_P1_REACH, 0x02, 0x10, BIT_ACTIVE_0 },

  { KB_DEF_P1_C, MSG_P1_C, 0x04, 0x01, BIT_ACTIVE_0 },
  { KB_DEF_P1_G, MSG_P1_G, 0x04, 0x02, BIT_ACTIVE_0 },
  { KB_DEF_P1_K, MSG_P1_K, 0x04, 0x04, BIT_ACTIVE_0 },
  { KB_DEF_P1_CHI, MSG_P1_CHI, 0x04, 0x08, BIT_ACTIVE_0 },
  { KB_DEF_P1_RON, MSG_P1_RON, 0x04, 0x10, BIT_ACTIVE_0 },

  { KB_DEF_P1_D, MSG_P1_D, 0x06, 0x01, BIT_ACTIVE_0 },
  { KB_DEF_P1_H, MSG_P1_H, 0x06, 0x02, BIT_ACTIVE_0 },
  { KB_DEF_P1_L, MSG_P1_L, 0x06, 0x04, BIT_ACTIVE_0 },
  { KB_DEF_P1_PON, MSG_P1_PON, 0x06, 0x08, BIT_ACTIVE_0 },

  { KB_DEF_TILT, MSG_TILT, 0x0a, 0x01, BIT_ACTIVE_0 },
  { KB_DEF_COIN2, MSG_COIN2, 0x0a, 0x02, BIT_ACTIVE_0 },

  { KB_DEF_SERVICE, MSG_SERVICE, 0x0c, 0x01, BIT_ACTIVE_0 },
  { KB_DEF_COIN1, MSG_COIN1, 0x0c, 0x02, BIT_ACTIVE_0 },
   { 0, NULL, 0, 0, 0 },
};

// And finally cameltry : pad control
static struct INPUT_INFO camel_try_inputs[] =
{
   { KB_DEF_TILT,         MSG_TILT,                0x03800E, 0x01, BIT_ACTIVE_0 },
   { KB_DEF_SERVICE,      MSG_SERVICE,             0x03800E, 0x02, BIT_ACTIVE_0 },
   { KB_DEF_COIN1,        MSG_COIN1,               0x03800E, 0x04, BIT_ACTIVE_0 },
   { KB_DEF_COIN2,        MSG_COIN2,               0x03800E, 0x08, BIT_ACTIVE_0 },

   { KB_DEF_P1_START,     MSG_P1_START,            0x038004, 0x80, BIT_ACTIVE_0 },
   { KB_DEF_P1_LEFT,      MSG_P1_LEFT,             0x038020, 0xFF, BIT_ACTIVE_1 },
   { KB_DEF_P1_RIGHT,     MSG_P1_RIGHT,            0x038021, 0xFF, BIT_ACTIVE_1 },
   { KB_DEF_P1_B1,        MSG_P1_B1,               0x038004, 0x10, BIT_ACTIVE_0 },

   { KB_DEF_P2_START,     MSG_P2_START,            0x038006, 0x80, BIT_ACTIVE_0 },
   { KB_DEF_P2_LEFT,      MSG_P2_LEFT,             0x038030, 0xFF, BIT_ACTIVE_1 },
   { KB_DEF_P2_RIGHT,     MSG_P2_RIGHT,            0x038031, 0xFF, BIT_ACTIVE_1 },
   { KB_DEF_P2_B1,        MSG_P2_B1,               0x038006, 0x10, BIT_ACTIVE_0 },

   { 0,                   NULL,                    0,        0,    0            },
};

#define TAITO_COINAGE_WORLD_8 \
  { MSG_COIN1, 0x30, 4 },\
  { MSG_4COIN_1PLAY, 0x00, 0x00 },\
  { MSG_3COIN_1PLAY, 0x10, 0x00 },\
  { MSG_2COIN_1PLAY, 0x20, 0x00 },\
  { MSG_1COIN_1PLAY, 0x30, 0x00 },\
  { MSG_COIN2, 0xc0, 4 },\
  { MSG_1COIN_1PLAY, 0xc0, 0x00 },\
  { MSG_1COIN_3PLAY, 0x80, 0x00 },\
  { MSG_1COIN_4PLAY, 0x40, 0x00 },\
  { MSG_1COIN_6PLAY, 0x00, 0x00 },\

#define TAITO_DIFFICULTY_8 \
  { MSG_DIFFICULTY, 0x03, 4 },\
  { "Easy" , 0x02, 0x00 },\
  { "Medium" , 0x03, 0x00 },\
  { "Hard" , 0x01, 0x00 },\
  { "Hardest" , 0x00, 0x00 },\

static struct DSW_DATA dsw_data_camel_try_0[] =
{
   { "Game Style",            0x01, 0x02 },
   { "Table",                 0x01, 0x00 },
   { "Upright",               0x00, 0x00 },
   { MSG_SCREEN,              0x02, 0x02 },
   { MSG_NORMAL,              0x02, 0x00 },
   { MSG_INVERT,              0x00, 0x00 },
   { MSG_TEST_MODE,           0x04, 0x02 },
   { MSG_OFF,                 0x04, 0x00 },
   { MSG_ON,                  0x00, 0x00 },
   { MSG_DEMO_SOUND,          0x08, 0x02 },
   { MSG_ON,                  0x08, 0x00 },
   { MSG_OFF,                 0x00, 0x00 },
   { MSG_COIN1,               0x30, 0x04 },
   { MSG_1COIN_1PLAY,         0x30, 0x00 },
   { MSG_1COIN_2PLAY,         0x20, 0x00 },
   { MSG_2COIN_1PLAY,         0x10, 0x00 },
   { MSG_2COIN_3PLAY,         0x00, 0x00 },
   { MSG_COIN2,               0xC0, 0x04 },
   { MSG_1COIN_1PLAY,         0xC0, 0x00 },
   { MSG_1COIN_2PLAY,         0x80, 0x00 },
   { MSG_2COIN_1PLAY,         0x40, 0x00 },
   { MSG_2COIN_3PLAY,         0x00, 0x00 },
   { NULL,                    0,    0,   },
};

static struct DSW_DATA dsw_data_camel_try_1[] =
{
   { MSG_DIFFICULTY,          0x03, 0x04 },
   { MSG_NORMAL,              0x03, 0x00 },
   { MSG_EASY,                0x02, 0x00 },
   { MSG_HARD,                0x01, 0x00 },
   { MSG_HARDEST,             0x00, 0x00 },
   { "Start Time",            0x0C, 0x04 },
   { "50 sec",                0x0C, 0x00 },
   { "60 sec",                0x08, 0x00 },
   { "40 sec",                0x04, 0x00 },
   { "35 sec",                0x00, 0x00 },
   { "Continue Time Add",     0x30, 0x04 },
   { "30 sec",                0x30, 0x00 },
   { "40 sec",                0x20, 0x00 },
   { "25 sec",                0x10, 0x00 },
   { "20 sec",                0x00, 0x00 },
   { "Continue Play",         0x40, 0x02 },
   { MSG_ON,                  0x40, 0x00 },
   { MSG_OFF,                 0x00, 0x00 },
   { "2 Player Mode",         0x80, 0x02 },
   { "Single",                0x80, 0x00 },
   { "Together",              0x00, 0x00 },
   { NULL,                    0,    0,   },
};

static struct DSW_INFO camel_try_dsw[] =
{
   { 0x038000, 0xFF, dsw_data_camel_try_0 },
   { 0x038002, 0xFF, dsw_data_camel_try_1 },
   { 0,        0,    NULL,      },
};

static struct DSW_DATA dsw_data_final_blow_0[] =
{
   { MSG_SCREEN,              0x02, 0x02 },
   { MSG_NORMAL,              0x02, 0x00 },
   { MSG_INVERT,              0x00, 0x00 },
   { MSG_TEST_MODE,           0x04, 0x02 },
   { MSG_OFF,                 0x04, 0x00 },
   { MSG_ON,                  0x00, 0x00 },
   { MSG_DEMO_SOUND,          0x08, 0x02 },
   { MSG_ON,                  0x08, 0x00 },
   { MSG_OFF,                 0x00, 0x00 },
   { MSG_COIN1,               0x30, 0x04 },
   { MSG_1COIN_1PLAY,         0x30, 0x00 },
   { MSG_1COIN_2PLAY,         0x20, 0x00 },
   { MSG_2COIN_1PLAY,         0x10, 0x00 },
   { MSG_2COIN_3PLAY,         0x00, 0x00 },
   { MSG_COIN2,               0xC0, 0x04 },
   { MSG_1COIN_1PLAY,         0xC0, 0x00 },
   { MSG_1COIN_2PLAY,         0x80, 0x00 },
   { MSG_2COIN_1PLAY,         0x40, 0x00 },
   { MSG_2COIN_3PLAY,         0x00, 0x00 },
   { NULL,                    0,    0,   },
};

static struct DSW_DATA dsw_data_final_blow_1[] =
{
   { MSG_DIFFICULTY,          0x03, 0x04 },
   { MSG_NORMAL,              0x03, 0x00 },
   { MSG_EASY,                0x02, 0x00 },
   { MSG_HARD,                0x01, 0x00 },
   { MSG_HARDEST,             0x00, 0x00 },
   { NULL,                    0,    0,   },
};

static struct DSW_INFO final_blow_dsw[] =
{
   { 0x3c000, 0xFF, dsw_data_final_blow_0 },
   { 0x3c002, 0xFF, dsw_data_final_blow_1 },
   { 0,        0,    NULL,      },
};

static struct DSW_DATA dsw_data_dondokod_0[] =
{
  { MSG_UNKNOWN, 0x01, 2 },
  { MSG_OFF, 0x01, 0x00 },
  { MSG_ON, 0x00, 0x00 },
  { MSG_SCREEN, 0x02, 2 },
  { MSG_OFF, 0x02, 0x00 },
  { MSG_ON, 0x00, 0x00 },
  { MSG_SERVICE, 0x04,2 },
  { MSG_ON, 0,0 },
  { MSG_OFF, 0x04,0 },
	TAITO_COINAGE_WORLD_8
  { MSG_DEMO_SOUND, 0x08, 2 },
  { MSG_OFF, 0x00, 0x00 },
  { MSG_ON, 0x08, 0x00 },
  { NULL, 0, 0}
};

static struct DSW_DATA dsw_data_dondokod_1[] =
{
	TAITO_DIFFICULTY_8
  { MSG_EXTRA_LIFE, 0x0c, 4 },
  { "10k and 100k" , 0x0c, 0x00 },
  { "10k and 150k" , 0x08, 0x00 },
  { "10k and 250k" , 0x04, 0x00 },
  { "10k and 350k" , 0x00, 0x00 },
  { MSG_LIVES, 0x30, 4 },
  { "2" , 0x20, 0x00 },
  { "3" , 0x30, 0x00 },
  { "4" , 0x00, 0x00 },
  { "5" , 0x10, 0x00 },
  { MSG_UNKNOWN, 0x40, 2 },
  { MSG_OFF, 0x40, 0x00 },
  { MSG_ON, 0x00, 0x00 },
  { MSG_UNKNOWN, 0x80, 2 },
  { MSG_OFF, 0x80, 0x00 },
  { MSG_ON, 0x00, 0x00 },
  { NULL, 0, 0}
};

static struct DSW_INFO don_doko_don_dsw[] =
{
   { 0x032100, 0xFF, dsw_data_dondokod_0 },
   { 0x032102, 0xFF, dsw_data_dondokod_1 },
   { 0,        0,    NULL,      },
};

static struct DSW_DATA dsw_data_gunfront_1[] =
{
   { MSG_DIFFICULTY,          0x03, 0x04 },
   { MSG_NORMAL,              0x03, 0x00 },
   { MSG_EASY,                0x02, 0x00 },
   { MSG_HARD,                0x01, 0x00 },
   { MSG_HARDEST,             0x00, 0x00 },
   { MSG_DSWB_BIT3,           0x04, 0x02 },
   { MSG_OFF,                 0x04, 0x00 },
   { MSG_ON,                  0x00, 0x00 },
   { MSG_DSWB_BIT4,           0x08, 0x02 },
   { MSG_ON,                  0x08, 0x00 },
   { MSG_OFF,                 0x00, 0x00 },
   { "Lives",                 0x30, 0x04 },
   { "3",                     0x30, 0x00 },
   { "1",                     0x20, 0x00 },
   { "2",                     0x10, 0x00 },
   { "5",                     0x00, 0x00 },
   { "Continue Mode",         0x40, 0x02 },
   { MSG_OFF,                 0x40, 0x00 },
   { MSG_ON,                  0x00, 0x00 },
   { "Simultaneous Play",     0x80, 0x02 },
   { MSG_OFF,                 0x80, 0x00 },
   { MSG_ON,                  0x00, 0x00 },
   { NULL,                    0,    0,   },
};

static struct DSW_INFO gunfront_dsw[] =
{
  { 0x32100, 0xff, dsw_data_dondokod_0 },
  { 0x32102, 0xff, dsw_data_gunfront_1 },
  { 0, 0, NULL }
};

static struct DSW_DATA dsw_data_megab_0[] =
{
   { "Cabinet",               0x01, 0x02 },
   { "Table",                 0x01, 0x00 },
   { "Upright",               0x00, 0x00 },
   { MSG_SCREEN,              0x02, 0x02 },
   { MSG_NORMAL,              0x02, 0x00 },
   { MSG_INVERT,              0x00, 0x00 },
   { MSG_TEST_MODE,           0x04, 0x02 },
   { MSG_OFF,                 0x04, 0x00 },
   { MSG_ON,                  0x00, 0x00 },
   { MSG_DEMO_SOUND,          0x08, 0x02 },
   { MSG_ON,                  0x08, 0x00 },
   { MSG_OFF,                 0x00, 0x00 },
   { MSG_COIN1,               0x30, 0x04 },
   { MSG_1COIN_1PLAY,         0x30, 0x00 },
   { MSG_2COIN_1PLAY,         0x20, 0x00 },
   { MSG_3COIN_1PLAY,         0x10, 0x00 },
   { MSG_4COIN_1PLAY,         0x00, 0x00 },
   { MSG_COIN2,               0xC0, 0x04 },
   { MSG_1COIN_2PLAY,         0xC0, 0x00 },
   { MSG_1COIN_3PLAY,         0x80, 0x00 },
   { MSG_1COIN_4PLAY,         0x40, 0x00 },
   { MSG_1COIN_6PLAY,         0x00, 0x00 },
   { NULL,                    0,    0,   },
};

static struct DSW_DATA dsw_data_mega_blast_1[] =
{
   { MSG_DIFFICULTY,          0x03, 0x04 },
   { MSG_NORMAL,              0x03, 0x00 },
   { MSG_EASY,                0x02, 0x00 },
   { MSG_HARD,                0x01, 0x00 },
   { MSG_HARDEST,             0x00, 0x00 },
   { "Bonus / K=10,000",      0x0c, 0x04 },
   { "10k, 110K, 210K, 310K", 0x0c, 0x00 },
   { "20k, 220K, 420K, 620K", 0x08, 0x00 },
   { "15K, 145K, 365K, 515K", 0x04, 0x00 },
   { "No Bonus Lives",        0x00, 0x00 },
   { "Lives",                 0x30, 0x04 },
   { "3",                     0x30, 0x00 },
   { "4",                     0x20, 0x00 },
   { "1",                     0x10, 0x00 },
   { "2",                     0x00, 0x00 },
   { "Control Panel",         0x40, 0x02 },
   { "Double",                0x40, 0x00 },
   { "Single",                0x00, 0x00 },
   { NULL,                    0,    0,   },
};

static struct DSW_INFO mega_blast_dsw[] =
{
   { 0x03C000, 0xFF, dsw_data_megab_0 },
   { 0x03C002, 0xFF, dsw_data_mega_blast_1 },
   { 0,        0,    NULL,      },
};

static struct DSW_DATA dsw_data_liquidk_1[] =
{
	TAITO_DIFFICULTY_8
  { MSG_EXTRA_LIFE, 0x0c, 4 },
  { "30k and 100k" , 0x0c, 0x00 },
  { "30k and 150k" , 0x08, 0x00 },
  { "50k and 250k" , 0x04, 0x00 },
  { "50k and 350k" , 0x00, 0x00 },
  { MSG_LIVES, 0x30, 4 },
  { "2" , 0x20, 0x00 },
  { "3" , 0x30, 0x00 },
  { "4" , 0x00, 0x00 },
  { "5" , 0x10, 0x00 },
  { "Allow Continue", 0x40, 2 },
  { MSG_OFF, 0x00, 0x00 },
  { MSG_ON, 0x40, 0x00 },
  { "Upright Controls", 0x80, 2 },
  { "Single" , 0x80, 0x00 },
  { "Dual" , 0x00, 0x00 },
  { NULL, 0, 0}
};

static struct DSW_INFO liquid_kids_dsw[] =
{
   { 0x032100, 0xFF, dsw_data_megab_0 },
   { 0x032102, 0xFF, dsw_data_liquidk_1 },
   { 0,        0,    NULL,      },
};

static struct DSW_DATA dsw_data_ssi_1[] =
{
	TAITO_DIFFICULTY_8
  { "Shields", 0x0c, 4 },
  { "None", 0x00, 0x00 },
  { "1", 0x0c, 0x00 },
  { "2", 0x04, 0x00 },
  { "3", 0x08, 0x00 },
  { MSG_LIVES, 0x10, 2 },
  { "2", 0x00, 0x00 },
  { "3", 0x10, 0x00 },
  { "2 Players Mode", 0xa0, 4 },
  { "Simultaneous", 0xa0, 0x00 },
  { "AlternateSingle", 0x80, 0x00 },
  { "AlternateDual", 0x00, 0x00 },
  { "Not Allowed", 0x20, 0x00 },
  { "Allow Continue", 0x40, 2 },
  { MSG_OFF, 0x00, 0x00 },
  { MSG_ON, 0x40, 0x00 },
  { NULL, 0, 0}
};

static struct DSW_INFO ssi_dsw[] =
{
  { 0x032100, 0xfe, dsw_data_megab_0 },
  { 0x032102, 0xff, dsw_data_ssi_1 },
  { 0, 0, NULL }
};

static struct DSW_DATA dsw_data_drift_out_0[] =
{
   { MSG_SCREEN,              0x02, 0x02 },
   { MSG_NORMAL,              0x02, 0x00 },
   { MSG_INVERT,              0x00, 0x00 },
   { MSG_TEST_MODE,           0x04, 0x02 },
   { MSG_OFF,                 0x04, 0x00 },
   { MSG_ON,                  0x00, 0x00 },
   { MSG_DEMO_SOUND,          0x08, 0x02 },
   { MSG_ON,                  0x08, 0x00 },
   { MSG_OFF,                 0x00, 0x00 },
   { MSG_COIN1,               0x30, 0x04 },
   { MSG_1COIN_1PLAY,         0x30, 0x00 },
   { MSG_2COIN_1PLAY,         0x20, 0x00 },
   { MSG_1COIN_2PLAY,         0x10, 0x00 },
   { MSG_2COIN_3PLAY,         0x00, 0x00 },
   { MSG_COIN2,               0xC0, 0x04 },
   { MSG_1COIN_1PLAY,         0xC0, 0x00 },
   { MSG_2COIN_1PLAY,         0x80, 0x00 },
   { MSG_1COIN_2PLAY,         0x40, 0x00 },
   { MSG_2COIN_3PLAY,         0x00, 0x00 },
   { NULL,                    0,    0,   },
};

static struct DSW_DATA dsw_data_drift_out_1[] =
{
   { MSG_DIFFICULTY,          0x03, 0x04 },
   { MSG_NORMAL,              0x03, 0x00 },
   { MSG_EASY,                0x02, 0x00 },
   { MSG_HARD,                0x01, 0x00 },
   { MSG_HARDEST,             0x00, 0x00 },
   { "Control",               0x0C, 0x04 },
   { "Lever",                 0x0C, 0x00 },
   { "Paddle A",              0x08, 0x00 },
   { "Lever",                 0x04, 0x00 },
   { "Paddle B",              0x00, 0x00 },
   { NULL,                    0,    0,   },
};

static struct DSW_INFO drift_out_dsw[] =
{
   { 0x032100, 0xFF, dsw_data_drift_out_0 },
   { 0x032102, 0xFF, dsw_data_drift_out_1 },
   { 0,        0,    NULL,      },
};

static struct DSW_DATA dsw_data_thunder_fox_0[] =
{
   { MSG_DSWA_BIT1,           0x01, 0x02 },
   { MSG_OFF,                 0x01, 0x00 },
   { MSG_ON,                  0x00, 0x00 },
   { MSG_SCREEN,              0x02, 0x02 },
   { MSG_NORMAL,              0x02, 0x00 },
   { MSG_INVERT,              0x00, 0x00 },
   { MSG_TEST_MODE,           0x04, 0x02 },
   { MSG_OFF,                 0x04, 0x00 },
   { MSG_ON,                  0x00, 0x00 },
   { MSG_DEMO_SOUND,          0x08, 0x02 },
   { MSG_ON,                  0x08, 0x00 },
   { MSG_OFF,                 0x00, 0x00 },
   { MSG_COIN1,               0x30, 0x04 },
   { MSG_1COIN_1PLAY,         0x30, 0x00 },
   { MSG_2COIN_1PLAY,         0x20, 0x00 },
   { MSG_3COIN_1PLAY,         0x10, 0x00 },
   { MSG_4COIN_1PLAY,         0x00, 0x00 },
   { MSG_COIN2,               0xC0, 0x04 },
   { MSG_1COIN_2PLAY,         0xC0, 0x00 },
   { MSG_1COIN_3PLAY,         0x80, 0x00 },
   { MSG_1COIN_4PLAY,         0x40, 0x00 },
   { MSG_1COIN_6PLAY,         0x00, 0x00 },
   { NULL,                    0,    0,   },
};

static struct DSW_DATA dsw_data_thunder_fox_1[] =
{
   { MSG_DIFFICULTY,          0x03, 0x04 },
   { MSG_NORMAL,              0x03, 0x00 },
   { MSG_EASY,                0x02, 0x00 },
   { MSG_HARD,                0x01, 0x00 },
   { MSG_HARDEST,             0x00, 0x00 },
   { "Time Limit",            0x04, 0x02 },
   { MSG_ON,                  0x04, 0x00 },
   { MSG_OFF,                 0x00, 0x00 },
   { "Lives",                 0x30, 0x04 },
   { "3",                     0x30, 0x00 },
   { "2",                     0x20, 0x00 },
   { "4",                     0x10, 0x00 },
   { "5",                     0x00, 0x00 },
   { "Continue Play",         0x40, 0x02 },
   { MSG_ON,                  0x40, 0x00 },
   { MSG_OFF,                 0x00, 0x00 },
   { "Controls",              0x80, 0x02 },
   { "Dual",                  0x80, 0x00 },
   { "Single",                0x00, 0x00 },
   { NULL,                    0,    0,   },
};

static struct DSW_INFO thunder_fox_dsw[] =
{
   { 0x02e200, 0xFF, dsw_data_thunder_fox_0 },
   { 0x02e202, 0xFF, dsw_data_thunder_fox_1 },
   { 0,        0,    NULL,      },
};

static struct DSW_DATA dsw_data_growl_1[] =
{
   { MSG_DIFFICULTY,          0x03, 0x04 },
   { MSG_NORMAL,              0x03, 0x00 },
   { MSG_EASY,                0x02, 0x00 },
   { MSG_HARD,                0x01, 0x00 },
   { MSG_HARDEST,             0x00, 0x00 },
   { "Game Type",             0x30, 0x04 },
   { "1 Credit/2P",           0x30, 0x00 },
   { "4 Credits/4P",          0x20, 0x00 },
   { "1 Credit/4P",           0x10, 0x00 },
   { "2 Credits/4P",          0x00, 0x00 },
   { "Final Stage Cont",      0x40, 0x02 },
   { MSG_OFF,                 0x40, 0x00 },
   { MSG_ON,                  0x00, 0x00 },
   { NULL,                    0,    0,   },
};

static struct DSW_INFO growl_dsw[] =
{
  { 0x32100, 0xff, dsw_data_thunder_fox_0 },
  { 0x32102, 0xff, dsw_data_growl_1 },
  { 0, 0, NULL }
};

static struct DSW_DATA dsw_data_dino_rex_1[] =
{
   { MSG_DIFFICULTY,          0x03, 0x04 },
   { MSG_NORMAL,              0x03, 0x00 },
   { MSG_EASY,                0x02, 0x00 },
   { MSG_HARD,                0x01, 0x00 },
   { MSG_HARDEST,             0x00, 0x00 },
   { "Damage",                0x0C, 0x04 },
   { MSG_NORMAL,              0x0C, 0x00 },
   { "Small",                 0x08, 0x00 },
   { "Big",                   0x04, 0x00 },
   { "Biggest",               0x00, 0x00 },
   { "Timer",                 0x10, 0x02 },
   { MSG_NORMAL,              0x10, 0x00 },
   { "Fast",                  0x00, 0x00 },
   { "Match Type",            0x20, 0x02 },
   { "Best of 3",             0x20, 0x00 },
   { "Single",                0x00, 0x00 },
   { "2 Player Mode",         0x40, 0x02 },
   { "Upright",               0x40, 0x00 },
   { "Cocktail",              0x00, 0x00 },
   { "Upright Controls",      0x80, 0x02 },
   { "Dual",                  0x80, 0x00 },
   { "Single",                0x00, 0x00 },
   { NULL,                    0,    0,   },
};

static struct DSW_INFO dino_rex_dsw[] =
{
   { 0x032100, 0xFF, dsw_data_thunder_fox_0 },
   { 0x032102, 0xFF, dsw_data_dino_rex_1 },
   { 0,        0,    NULL,      },
};

static struct DSW_DATA dsw_data_solfigtr_1[] =
{
	TAITO_DIFFICULTY_8
  { MSG_UNKNOWN, 0x04, 2 },
  { MSG_OFF, 0x04, 0x00 },
  { MSG_ON, 0x00, 0x00 },
  { MSG_UNKNOWN, 0x08, 2 },
  { MSG_OFF, 0x08, 0x00 },
  { MSG_ON, 0x00, 0x00 },
  { MSG_UNKNOWN, 0x10, 2 },
  { MSG_OFF, 0x10, 0x00 },
  { MSG_ON, 0x00, 0x00 },
  { MSG_UNKNOWN, 0x20, 2 },
  { MSG_OFF, 0x20, 0x00 },
  { MSG_ON, 0x00, 0x00 },
  { MSG_UNKNOWN, 0x40, 2 },
  { MSG_OFF, 0x40, 0x00 },
  { MSG_ON, 0x00, 0x00 },
  { NULL, 0, 0}
};

static struct DSW_INFO solfigtr_dsw[] =
{
  { 0x32100, 0xff, dsw_data_dondokod_0 },
  { 0x32102, 0xff, dsw_data_solfigtr_1 },
  { 0, 0, NULL }
};

#define TAITO_COINAGE_JAPAN_NEW_8 \
  { MSG_COIN1, 0x30, 4 },\
  { MSG_3COIN_1PLAY, 0x00, 0x00 },\
  { MSG_2COIN_1PLAY, 0x10, 0x00 },\
  { MSG_1COIN_1PLAY, 0x30, 0x00 },\
  { MSG_1COIN_1PLAY, 0x20, 0x00 },\
  { MSG_COIN2, 0xc0, 4 },\
  { MSG_3COIN_1PLAY, 0x00, 0x00 },\
  { MSG_2COIN_1PLAY, 0x40, 0x00 },\
  { MSG_1COIN_1PLAY, 0xc0, 0x00 },\
  { MSG_1COIN_1PLAY, 0x80, 0x00 },\

static struct DSW_DATA dsw_data_pulirulj_0[] =
{
  { MSG_UNKNOWN, 0x01, 2 },
  { MSG_OFF, 0x01, 0x00 },
  { MSG_ON, 0x00, 0x00 },
  { MSG_SCREEN, 0x02, 2 },
  { MSG_OFF, 0x02, 0x00 },
  { MSG_ON, 0x00, 0x00 },
  { MSG_SERVICE, 0x04,2 },
  { MSG_ON, 0,0 },
  { MSG_OFF, 0x04,0 },
	TAITO_COINAGE_JAPAN_NEW_8
  { MSG_DEMO_SOUND, 0x08, 2 },
  { MSG_OFF, 0x00, 0x00 },
  { MSG_ON, 0x08, 0x00 },
  { NULL, 0, 0}
};

// mjnquest has some kind of protection to filter inputs/dsw. I use input_buffer for that
static struct DSW_INFO mjnquest_dsw[] =
{
  { 0xe, 0xff, dsw_data_pulirulj_0 },
  { 0x10, 0xff, dsw_data_solfigtr_1 },
  { 0, 0, NULL }
};

static struct ROMSW_DATA romsw_data_taito_jap_us[] =
{
   { "Taito Japan",           0x01 },
   { "Taito America",         0x02 },
   { "World",                 0x03 },
   { NULL,                    0    },
};

static struct ROMSW_DATA romsw_data_majestic_twelve_0[] =
{
   { "Taito Japan (MJ12)",    0x01 },
   { "Taito America (MJ12)",  0x02 },
   { "Taito Japan (SSI)",     0x03 },
   { NULL,                    0    },
};

static struct ROMSW_DATA romsw_data_thunder_fox_0[] =
{
   { "Taito Japan",           0x00 },
   { "Taito America",         0x01 },
   { "Taito",                 0x02 },
   { NULL,                    0    },
};

static struct ROMSW_DATA romsw_data_growl_0[] =
{
   { "Taito Japan (Runark)",    0x01 },
   { "Taito America (Growl)",   0x02 },
   { "Taito Worldwide (Growl)", 0x03 },
   { NULL,                      0    },
};

static struct ROMSW_DATA romsw_data_camel_try_alt_0[] =
{
   { "Taito Japan (Japanese)", 0x00 },
   { "Taito America",          0x01 },
   { "Taito Japan",            0x02 },
/*   { "Taito America (Romstar)",0x03 },
   { "Taito (Phoenix)",        0x04 }, */
   { NULL,                     0    },
};

static struct ROMSW_INFO camel_try_romsw[] =
{
   { 0x03FFFF, 0x01, romsw_data_camel_try_alt_0 },
   { 0,        0,    NULL },
};

static struct ROMSW_INFO growl_romsw[] =
{
   { 0x0FFFFF, 0x03, romsw_data_growl_0 },
   { 0,        0,    NULL },
};

static struct ROMSW_INFO thunder_fox_romsw[] =
{
   { 0x03FFFF, 0x02, romsw_data_thunder_fox_0 },
   { 0,        0,    NULL },
};

static struct ROMSW_INFO don_doko_don_romsw[] =
{
   { 0x077FFF, 0x03, romsw_data_taito_jap_us },
   { 0,        0,    NULL },
};

static struct ROMSW_INFO solfigtr_romsw[] =
{
   { 0x03ffff, 0x03, romsw_data_taito_jap_us },
   { 0,        0,    NULL },
};

static struct ROMSW_INFO mega_blast_romsw[] =
{
   { 0x07FFFF, 0x02, romsw_data_thunder_fox_0 },
   { 0,        0,    NULL },
};

static struct ROMSW_INFO majestic_twelve_romsw[] =
{
   { 0x07FFFF, 0x03, romsw_data_majestic_twelve_0 },
   { 0,        0,    NULL },
};

static struct ROMSW_INFO liquidk_romsw[] =
{
   { 0x07FFFF, 0x03, romsw_data_taito_jap_us },
   { 0,        0,    NULL },
};

static struct GfxLayout tilelayout =
{
	16,16,	/* 16*16 sprites */
	RGN_FRAC(1,1),
	4,	/* 4 bits per pixel */
	{ 0, 1, 2, 3 },
	{ 1*4, 0*4, 3*4, 2*4, 5*4, 4*4, 7*4, 6*4, 9*4, 8*4, 11*4, 10*4, 13*4, 12*4, 15*4, 14*4 },
	{ 0*64, 1*64, 2*64, 3*64, 4*64, 5*64, 6*64, 7*64, 8*64, 9*64, 10*64, 11*64, 12*64, 13*64, 14*64, 15*64 },
	128*8	/* every sprite takes 128 consecutive bytes */
};

static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	RGN_FRAC(1,1),
	4,	/* 4 bits per pixel */
	{ 0, 1, 2, 3 },
	{ 2*4, 3*4, 0*4, 1*4, 6*4, 7*4, 4*4, 5*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8	/* every sprite takes 32 consecutive bytes */
};

static struct GfxLayout pivotlayout =
{
	8,8,	/* 8*8 characters */
	RGN_FRAC(1,1),
	4,	/* 4 bits per pixel */
	{ 0, 1, 2, 3 },
	{ 0*4, 1*4, 2*4, 3*4, 4*4, 5*4, 6*4, 7*4 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8	/* every sprite takes 32 consecutive bytes */
};

static struct GfxLayout finalb_tilelayout =
{
	16,16,	/* 16*16 sprites */
	RGN_FRAC(1,2),
	6,	/* 6 bits per pixel */
	{ RGN_FRAC(1,2)+0, RGN_FRAC(1,2)+1, 0, 1, 2, 3 },
	{ 3*4, 2*4, 1*4, 0*4, 7*4, 6*4, 5*4, 4*4,
			11*4, 10*4, 9*4, 8*4, 15*4, 14*4, 13*4, 12*4 },
	{ 0*64, 1*64, 2*64, 3*64, 4*64, 5*64, 6*64, 7*64,
			8*64, 9*64, 10*64, 11*64, 12*64, 13*64, 14*64, 15*64 },
	128*8	/* every sprite takes 128 consecutive bytes */
};

static struct GFX_LIST pivot_gfxdecodeinfo[] =
{
	{ REGION_GFX1, &charlayout }, // 256 color banks
	{ REGION_GFX2, &tilelayout }, // 256 color banks
	{ REGION_GFX3, &pivotlayout }, // 256 color banks
	{ 0, NULL } /* end of array */
};

static struct GFX_LIST thundfox_decodeinfo[] =
{
	{ REGION_GFX1, &charlayout }, // tc100scn #0
	{ REGION_GFX2, &tilelayout }, // 256 color banks
	{ REGION_GFX3, &charlayout }, // tc100scn #1
	{ 0, NULL } /* end of array */
};

// REGION_GFX3 is empty
static struct GFX_LIST finalb_gfxdecodeinfo[] =
{
	{ REGION_GFX1, &charlayout }, // 256 color banks
	{ REGION_GFX2, &finalb_tilelayout },
	{ 0, NULL } /* end of array */
};

static struct GFX_LIST mjnquest_gfxdecodeinfo[] =
{
	{ REGION_GFX1, &charlayout }, // 256 color banks
	{ REGION_GFX2, &tilelayout },
	{ 0, NULL } /* end of array */
};

static UINT8 *RAM_VIDEO,*RAM_VIDEO2,*GFX_BANK;
static UINT8 *RAM_SCROLL,*RAM_SCROLL2;
static UINT8 *RAM_OBJECT;
static UINT8 *RAM_INPUT;
static UINT8 *RAM_ROTATE;

static struct layer {
  UINT32 pri,num;
} layer[16];

static int layer_cmp(const void *a, const void *b) {
  return ((struct layer *)a)->pri - ((struct layer *)b)->pri;
}

static void load_common() {
  UINT32 size = get_region_size(REGION_CPU1);
   RAMSize=0x50000;

   if(!(RAM=AllocateMem(0x40000+0x14000))) return;

   /*-----------------------*/

   memset(RAM+0x00000,0x00,0x40000);

   // make_solid_mask not called for GFX3.

   ByteSwap(ROM,size);
   AddMemFetch(0x000000, size-1, ROM+0x000000-0x000000);	// 68000 ROM
   AddMemFetch(-1, -1, NULL);

   AddReadBW(0x000000, size-1, NULL, ROM+0x000000);			// 68000 ROM
}

static int init_gfx;

static void setup_gfx() {
   // Init tc0200obj emulation
   // ------------------------

  init_gfx = 0;
   tc0200obj.RAM	= RAM_OBJECT+0x0000;
   tc0200obj.RAM_B	= NULL;
   tc0200obj.bmp_x	= 32;
   tc0200obj.bmp_y	= 32;
   tc0200obj.bmp_w	= 320;
   tc0200obj.bmp_h	= 224;
   tc0200obj.ofs_x	= -3;
   tc0200obj.ofs_y	= 0; // sprite type now

   // Init tc0100scn emulation
   // ------------------------

   tc0100scn[0].layer[0].RAM	=RAM_VIDEO+0x0000;
   tc0100scn[0].layer[0].SCR	=RAM_SCROLL+0;
   tc0100scn[0].layer[0].type	=0;
   tc0100scn[0].layer[0].bmp_x	=32;
   tc0100scn[0].layer[0].bmp_y	=32;
   tc0100scn[0].layer[0].bmp_w	=320;
   tc0100scn[0].layer[0].bmp_h	=224;

   tc0100scn[0].layer[1].RAM	=RAM_VIDEO+0x8000;
   tc0100scn[0].layer[1].SCR	=RAM_SCROLL+2;
   tc0100scn[0].layer[1].type	=0;
   tc0100scn[0].layer[1].bmp_x	=32;
   tc0100scn[0].layer[1].bmp_y	=32;
   tc0100scn[0].layer[1].bmp_w	=320;
   tc0100scn[0].layer[1].bmp_h	=224;

   tc0100scn[0].layer[2].RAM	=RAM_VIDEO+0x4000;
   tc0100scn[0].layer[2].GFX	=GFX_FG0;
   tc0100scn[0].layer[2].SCR	=RAM_SCROLL+4;
   tc0100scn[0].layer[2].type	=3;
   tc0100scn[0].layer[2].bmp_x	=32;
   tc0100scn[0].layer[2].bmp_y	=32;
   tc0100scn[0].layer[2].bmp_w	=320;
   tc0100scn[0].layer[2].bmp_h	=224;

   tc0100scn[0].RAM     = RAM_VIDEO;
   tc0100scn[0].GFX_FG0 = GFX_FG0;

   tc0100scn[0].layer[0].scr_x	=16-3;
   tc0100scn[0].layer[0].scr_y	=8;

   tc0100scn[0].layer[1].scr_x	=13; // -80 for f2demo !
   tc0100scn[0].layer[1].scr_y	=8;

   tc0100scn[0].layer[2].scr_x	=13;
   tc0100scn[0].layer[2].scr_y	=8;

   if (load_region[REGION_GFX3] && tc0005rot.RAM) {
     // to init pixel_bitmap in tc005rot (will be called in finish_setup_gfx again)
     tc0005rot.GFX_ROT = load_region[REGION_GFX3]; // init this to tell loadroms that we
     //  don't want to call make_solid_region on it
     init_tc0005rot();
   }
}

static void finish_setup_gfx() {
  // called when the layouts have been applied
  init_gfx = 1;
   tc0200obj.GFX	= gfx2;
   tc0200obj.MASK	= gfx2_solid;
   tc0100scn[0].layer[0].GFX	=GFX;
   tc0100scn[0].layer[0].MASK	=gfx_solid[0];
   tc0100scn[0].layer[1].GFX	=GFX;
   tc0100scn[0].layer[1].MASK	=gfx_solid[0];
   tc0200obj.tile_mask	= max_sprites[1]-1;
   tc0100scn[0].layer[0].tile_mask=max_sprites[0]-1;
   tc0100scn[0].layer[1].tile_mask=max_sprites[0]-1;
   init_tc0200obj();
   if (gfx3 && tc0005rot.RAM) {
     tc0005rot.GFX_ROT = gfx3;
     if (is_current_game("pulirula") || is_current_game("driftout") ||
       is_current_game("driveout")) 
       init_tc430grw();
     else
       init_tc0005rot();
     {
       int max = 0;
       int size = get_region_size(REGION_GFX3);
       int n;
       for (n=0; n<size; n++) {
	 if (gfx3[n] > max)
	   max = n;
       }
       printf("max gfx3 %x size %x\n",max,size);
     }
   }
}

static void load_common_cameltry() {
  UINT8 *RAM_COLOUR;
   // 68000 Speed Hack

   WriteLong68k(&ROM[0x00BA2],0x13FC0000);	// move.b #$00,$AA0000
   WriteLong68k(&ROM[0x00BA6],0x00AA0000);
   WriteWord68k(&ROM[0x00BAA],0x6100-16);	// bra.s <loop>

   // Set vcu type

   WriteWord68k(&ROM[0x3FF8C],0x0000);

   load_common();

   RAM_OBJECT     = RAM+0x10000;
   RAM_ROTATE     = RAM+0x34000;
   RAM_COLOUR     = RAM+0x36000;
   RAM_VIDEO      = RAM+0x20000;
   RAM_INPUT      = RAM+0x38000;
   RAM_SCROLL     = RAM+0x38200;
   TC0360PRI_regs = RAM+0x38300;
   GFX_FG0        = RAM+0x40000;
   RAMSize = 0x38320;

   set_colour_mapper(&col_map_rrrr_gggg_bbbb_xxxx);
   InitPaletteMap(RAM_COLOUR, 0x100, 0x10, 0x1000);

   // Init tc0220ioc emulation
   // ------------------------

   tc0220ioc.RAM  = RAM_INPUT;
   tc0220ioc.ctrl = 0;		//TC0220_STOPCPU;
   reset_tc0220ioc();

   // Init tc0005rot emulation
   // ------------------------

   tc0005rot.RAM     = RAM_ROTATE;
   tc0005rot.RAM_SCR = RAM+0x30100;
   setup_gfx();

   tc0100scn[0].layer[2].scr_x	=19;

   tc0200obj.ofs_x	= 8;
   init_tc0100scn(0);


   AddRWBW(0x100000, 0x10FFFF, NULL, RAM+0x000000);			// 68000 RAM
   AddRWBW(0x900000, 0x90fFFF, NULL, RAM_OBJECT);			// OBJECT RAM
   AddWriteByte(0x806000, 0x806FFF, tc0100scn_0_gfx_fg0_wb, NULL);	// FG0 GFX RAM
   AddWriteWord(0x806000, 0x806FFF, tc0100scn_0_gfx_fg0_ww, NULL);	// FG0 GFX RAM
   AddRWBW(0x800000, 0x813FFF, NULL, RAM_VIDEO);			// SCREEN RAM
   AddWriteWord(0xA00000, 0xA01FFF, tc0005rot_bg0_ww, NULL);		// SCREEN RAM (ROTATE)
   AddRWBW(0xA00000, 0xA01FFF, NULL, RAM_ROTATE);			// SCREEN RAM (ROTATION)
   AddReadByte(0x320000, 0x320003, tc0140syt_read_main_68k, NULL);	// SOUND COMM
   AddReadBW(0x300000, 0x30001F, NULL, RAM_INPUT);			// INPUT RAM
   AddReadByte(0x000000, 0xFFFFFF, DefBadReadByte, NULL);		// <Bad Reads>
   AddReadByte(-1, -1, NULL, NULL);

   AddReadWord(0x200000, 0x201FFF, NULL, RAM_COLOUR);			// COLOR RAM
   AddWriteWord(0x200000, 0x201FFF, NULL, RAM_COLOUR);			// COLOR RAM
   AddReadWord(0x000000, 0xFFFFFF, DefBadReadWord, NULL);		// <Bad Reads>
   AddReadWord(-1, -1,NULL, NULL);

   AddWriteByte(0x320000, 0x320003, tc0140syt_write_main_68k, NULL);	// SOUND COMM
   AddWriteByte(0x300000, 0x30001F, tc0220ioc_wb, NULL);		// INPUT RAM
   AddWriteByte(0xAA0000, 0xAA0001, Stop68000, NULL);			// Trap Idle 68000
   AddWriteByte(0x000000, 0xFFFFFF, DefBadWriteByte, NULL);		// <Bad Writes>
   AddWriteByte(-1, -1, NULL, NULL);

   AddWriteWord(0xA02000, 0xA0200F, NULL, RAM+0x030100);		// SCROLL RAM (ROTATION)
   AddWriteWord(0x820000, 0x82000F, NULL, RAM_SCROLL);			// SCROLL RAM
   AddWriteWord(0xD00000, 0xD0001F, NULL, TC0360PRI_regs);		// Priorities
   AddWriteWord(0x300000, 0x30001F, tc0220ioc_ww, NULL);		// INPUT RAM
   AddWriteWord(0x000000, 0xFFFFFF, DefBadWriteWord, NULL);		// <Bad Writes>
   AddWriteWord(-1, -1, NULL, NULL);

   AddInitMemory();	// Set Starscream mem pointers...

   GameMouse=1;
}

static void load_cameltry() {
  load_common_cameltry();
  AddTaitoYM2610(0x01E6, 0x0185, 0x10000);
}

static struct YM2203interface ym2203_interface =
{
	1,		/* 1 chip ??? */
	3000000,	/* 3 MHz ??? (tempo much too fast @4) */
	{ YM2203_VOL(60,20) },
	{ 0 },	/* portA read */
	{ 0 },
	{ NULL }, // camltrua_porta_w },	/* portA write - not implemented */
	{ 0 },	/* portB write */
	{ z80_irq_handler }
};

static struct SOUND_INFO camltrua_sound[] =
{
   { SOUND_YM2203,   &ym2203_interface,	},
   { 0, 	    NULL,		},
};

static void load_camltrua() {
  /* Alt version with YM2203 sound missing ADPCM chip? Also sound tempo
     may be fractionally too slow. */
  /* This alternate version seems quite experimental, I take its specificities from mame */
  UINT8 *z80_ram;
  load_common_cameltry();

  z80_ram = RAM + RAMSize;
  AddZ80AROMBase(Z80ROM, 0x0038, 0x0066);

  AddZ80ARead(0x0000, 0x7FFF, NULL,	Z80ROM);   // No bank control found
  AddZ80ARead(0x8000, 0x8fff, NULL, z80_ram);
  AddZ80ARead(0x9000, 0x9000, YM2203_status_port_0_r, NULL);
  AddZ80ARead(0xa001, 0xa001, tc0140syt_read_sub_z80, NULL);

  AddZ80AWrite(0x8000,0x8fff, NULL, z80_ram);
  AddZ80AWrite(0x9000,0x9000, YM2203_control_port_0_w, NULL);
  AddZ80AWrite(0x9001,0x9001, YM2203_write_port_0_w, NULL);
  AddZ80AWrite(0xa000,0xa001, tc0140syt_write_sub_z80, NULL);

  AddZ80ARead(0, 0xffff, DefBadReadZ80, NULL);
  AddZ80ARead (    -1,     -1, NULL,			NULL);

  AddZ80AWrite    (0x0000, 0xFFFF, DefBadWriteZ80,		NULL);
  AddZ80AWrite(    -1,     -1, NULL,			NULL);

  AddZ80AReadPort(0x00, 0xFF, DefBadReadZ80,		NULL);
  AddZ80AReadPort(  -1,   -1, NULL,			NULL);

  // AddZ80AWritePort(0xAA, 0xAA, StopZ80Mode2,		NULL);
  AddZ80AWritePort(0x00, 0xFF, DefBadWriteZ80,		NULL);
  AddZ80AWritePort(  -1,   -1, NULL,			NULL);

  AddZ80AInit();
}

static void load_gunfront() {
   // speed hack

   WriteWord68k(&ROM[0x151DE],0x4EF9);			//	jmp	$0000C0
   WriteLong68k(&ROM[0x151E0],0x000000C0);

   WriteLong68k(&ROM[0x000C0],0x13FC0000);		//	move.b	#$00,$AA0000
   WriteLong68k(&ROM[0x000C4],0x00AA0000);

   WriteWord68k(&ROM[0x000C8],0x082D);			//	btst	#2,-32762(a5)
   WriteLong68k(&ROM[0x000CA],0x00028006);

   WriteWord68k(&ROM[0x000CE],0x6700+(0x100-0x10)); 	//	beq.s	<loop>

   WriteWord68k(&ROM[0x000D0],0x082D);			//	btst	#5,-1223(a5)
   WriteLong68k(&ROM[0x000D2],0x0005FB39);

   WriteWord68k(&ROM[0x000D6],0x6600+(0x100-0x18)); 	//	bne.s	<loop>

   WriteWord68k(&ROM[0x000D8],0x4EF9);			//	jmp	$0151EE
   WriteLong68k(&ROM[0x000DA],0x000151EE);

   // scroll hack

   WriteLong68k(&ROM[0x10DFC],0x4EB80100);		//	jsr	$100.w

   WriteWord68k(&ROM[0x00100],0x4EB9);			//	jsr	$1101C
   WriteLong68k(&ROM[0x00102],0x0001101C);

   WriteLong68k(&ROM[0x00106],0x13FC0000);		//	move.b	#$00,$AA0000
   WriteLong68k(&ROM[0x0010A],0x00AA0000);

   WriteWord68k(&ROM[0x0010E],0x4E75);			//	rts

   load_common();

   AddTaitoYM2610(0x01A4, 0x0150, 0x10000);

   RAM_VIDEO      = RAM+0x10000;
   RAM_SCROLL     = RAM+0x32000;
   RAM_OBJECT     = RAM+0x20000;
   RAM_INPUT      = RAM+0x32100;
   TC0360PRI_regs = RAM+0x32200;
   GFX_FG0        = RAM+0x35000;
   RAMSize = 0x33000;

   set_colour_mapper(&col_map_rrrr_gggg_bbbb_xxxx);
   InitPaletteMap(RAM+0x30000, 0x100, 0x10, 0x1000);

   setup_gfx();

   // Init tc0220ioc emulation
   // ------------------------

   tc0220ioc.RAM  = RAM_INPUT;
   tc0220ioc.ctrl = 0;	//TC0220_STOPCPU;
   reset_tc0220ioc();

   // Init tc0100scn emulation
   // ------------------------
   tc0100scn[0].layer[0].scr_x	=19;
   tc0100scn[0].layer[1].scr_x	=19; // -80 for f2demo !
   tc0100scn[0].layer[2].scr_x	=19;

   tc0200obj.ofs_x	= 8+1;
   init_tc0100scn(0);

   AddRWBW(0x100000, 0x10FFFF, NULL, RAM+0x000000);			// 68000 RAM
   AddWriteByte(0x806000, 0x806FFF, tc0100scn_0_gfx_fg0_wb, NULL);	// FG0 GFX RAM
   AddWriteWord(0x806000, 0x806FFF, tc0100scn_0_gfx_fg0_ww, NULL);	// FG0 GFX RAM
   AddRWBW(0x800000, 0x80FFFF, NULL, RAM_VIDEO);			// SCREEN RAM
   AddRWBW(0x900000, 0x90FFFF, NULL, RAM_OBJECT);			// OBJECT RAM
   AddReadByte(0x300000, 0x30000F, tc0220ioc_rb_bswap, NULL);		// INPUT
   AddReadByte(0x320000, 0x320003, tc0140syt_read_main_68k, NULL);	// SOUND COMM
   AddReadByte(0x000000, 0xFFFFFF, DefBadReadByte, NULL);		// <Bad Reads>
   AddReadByte(-1, -1, NULL, NULL);

   AddReadWord(0x200000, 0x201FFF, NULL, RAM+0x030000);			// COLOR RAM
   AddReadWord(0x300000, 0x30000F, tc0220ioc_rw_bswap, NULL);		// INPUT
   AddReadWord(0x000000, 0xFFFFFF, DefBadReadWord, NULL);		// <Bad Reads>
   AddReadWord(-1, -1, NULL, NULL);

   AddWriteByte(0x320000, 0x320003, tc0140syt_write_main_68k, NULL);	// SOUND COMM
   AddWriteByte(0x300000, 0x30000F, tc0220ioc_wb_bswap, NULL);		// INPUT
   AddWriteByte(0xAA0000, 0xAA0001, Stop68000, NULL);			// Trap Idle 68000
   AddWriteByte(0x000000, 0xFFFFFF, DefBadWriteByte, NULL);		// <Bad Writes>
   AddWriteByte(-1, -1, NULL, NULL);

   AddWriteWord(0x200000, 0x201FFF, NULL, RAM+0x030000);		// COLOR RAM
   AddWriteWord(0x820000, 0x82000F, NULL, RAM_SCROLL);			// SCROLL RAM
   AddWriteWord(0xB00000, 0xB0001F, NULL, TC0360PRI_regs);		// ??? RAM
   AddWriteWord(0x300000, 0x30000F, tc0220ioc_ww_bswap, NULL);		// INPUT
   AddWriteWord(0x000000, 0xFFFFFF, DefBadWriteWord, NULL);		// <Bad Writes>
   AddWriteWord(-1, -1, NULL, NULL);

   AddInitMemory();	// Set Starscream mem pointers...
}

static void load_growl() {
  UINT8 *RAM_COLOUR;

  // Addresses in rom :
  // 1124 : sound mode
  // called from 7542 from f34 (int 5)
   ROM[0x01010]=0x4E;		// SKIP OLD CODE (NO ROOM FOR HACK)
   ROM[0x01011]=0xF9;		// (JMP $72900)
   ROM[0x01012]=0x00;
   ROM[0x01013]=0x07;
   ROM[0x01014]=0x29;
   ROM[0x01015]=0x00;

   ROM[0x72900]=0x4E;		// jsr $BB4
   ROM[0x72901]=0xB9;		// (random number)
   ROM[0x72902]=0x00;
   ROM[0x72903]=0x00;
   ROM[0x72904]=0x0B;
   ROM[0x72905]=0xB4;

   ROM[0x72906]=0x13;		// move.b #$00,$AA0000
   ROM[0x72907]=0xFC;		// (Speed Hack)
   ROM[0x72908]=0x00;
   ROM[0x72909]=0x00;
   ROM[0x7290A]=0x00;
   ROM[0x7290B]=0xAA;
   ROM[0x7290C]=0x00;
   ROM[0x7290D]=0x00;

   ROM[0x7290E]=0x60;		// Loop
   ROM[0x7290F]=0x100-16;

   // Frame Sync Hack
   // ---------------

   WriteLong68k(&ROM[0x75B0],0x13FC0000);
   WriteLong68k(&ROM[0x75B4],0x00AA0000);
   WriteLong68k(&ROM[0x75B8],0x4E714E71);

   // Fix Sprite/Int6 Wait
   // --------------------

   WriteLong68k(&ROM[0x4CEE],0x4E714E71);

   load_common();

   RAM_VIDEO       = RAM+0x10000;
   RAM_OBJECT      = RAM+0x20000;
   RAM_SCROLL      = RAM+0x30000;
   GFX_BANK        = RAM+0x30010;
   TC0360PRI_regs  = RAM+0x30020;
   RAM_INPUT       = RAM+0x32100;
   GFX_FG0         = RAM+0x32200;
   RAM_COLOUR      = RAM+0x36100;

   RAMSize = 0x38100;

   AddTaitoYM2610(0x01A9, 0x0155, 0x10000);
   // AddTaitoYM2610(0, 0, 0x10000);

   set_colour_mapper(&col_map_rrrr_gggg_bbbb_xxxx);
   InitPaletteMap(RAM_COLOUR, 0x100, 0x10, 0x1000);

   setup_gfx();

   tc0100scn[0].layer[0].scr_x	= 0x3+16;
   tc0100scn[0].layer[1].scr_x	= 0x3+16;
   tc0100scn[0].layer[2].scr_x	= 0x3+16;

   init_tc0100scn(0);
   tc0200obj.ofs_x	= 0; // - 0x13;
   tc0200obj.ofs_y	= 0; // - 0x60;


   AddRWBW(0x100000, 0x10FFFF, NULL, RAM+0x000000);			// 68000 RAM
   AddWriteWord(0x806000, 0x806FFF, tc0100scn_0_gfx_fg0_ww, NULL);	// FG0 GFX RAM
   AddWriteByte(0x806000, 0x806FFF, tc0100scn_0_gfx_fg0_wb, NULL);	// FG0 GFX RAM
   AddRWBW(0x800000, 0x80FFFF, NULL, RAM_VIDEO);			// SCREEN RAM
   AddRWBW(0x900000, 0x90FFFF, NULL, RAM_OBJECT);			// OBJECT RAM
   AddReadBW(0x300000, 0x30000F, NULL, RAM_INPUT);			// input (dsw)
   AddReadBW(0x320000, 0x32000F, NULL, RAM+0x32380);			// INPUT
   AddReadBW(0x508000, 0x508001, NULL, &input_buffer[4]); // input player 3
   AddReadBW(0x50c000, 0x50c001, NULL, &input_buffer[6]); // input player 3
   AddReadByte(0x400000, 0x400003, tc0140syt_read_main_68k, NULL);	// SOUND COMM
   AddRWBW(0x820000, 0x82000F, NULL, RAM_SCROLL);		// SCROLL RAM
   AddRWBW(0x200000, 0x201FFF, NULL, RAM_COLOUR);			// COLOR RAM
   AddRWBW(0x500000, 0x50000F, NULL, GFX_BANK);		// OBJECT BANK?
   AddReadByte(0x000000, 0xFFFFFF, DefBadReadByte, NULL);		// <Bad Reads>
   AddReadByte(-1, -1, NULL, NULL);

   AddReadWord(0x000000, 0xFFFFFF, DefBadReadWord, NULL);		// <Bad Reads>
   AddReadWord(-1, -1, NULL, NULL);

   // AddWriteByte(0x300000, 0x30000F, tc0220ioc_wb, NULL);		// tc0220ioc
   AddWriteByte(0x400000, 0x400003, tc0140syt_write_main_68k, NULL);	// SOUND COMM
   AddWriteByte(0xAA0000, 0xAA0001, Stop68000, NULL);			// Trap Idle 68000
   AddWriteByte(0x000000, 0xFFFFFF, DefBadWriteByte, NULL);		// <Bad Writes>
   AddWriteByte(-1, -1, NULL, NULL);

   // AddWriteWord(0x340000, 0x34000F, NULL, RAM+0x032380);		// watchdog
   // AddWriteWord(0x380000, 0x38000F, NULL, RAM+0x032180);		// ???
   // AddWriteWord(0x600000, 0x60000F, NULL, RAM+0x032280);		// ???
   AddWriteWord(0xB00000, 0xB0001F, NULL, TC0360PRI_regs);		// priorities
   AddWriteWord(0x000000, 0xFFFFFF, DefBadWriteWord, NULL);		// <Bad Writes>
   AddWriteWord(-1, -1, NULL, NULL);

   AddInitMemory();	// Set Starscream mem pointers...
}

static void LoadDonDokoDon(void)
{
   // 68000 Speed Hack
   // ----------------

   WriteWord68k(&ROM[0x005EC4],0x4E71);

   // Fix ROM Checksum
   // ----------------

   WriteWord68k(&ROM[0x00D1A],0x4E75);

   // Fix Long Sound Wait
   // -------------------

   WriteWord68k(&ROM[0x001A6],0x4E71);

  load_common();

   /*-----[Sound Setup]-----*/

   AddTaitoYM2610(0x01E6, 0x0185, 0x10000);

   RAM_VIDEO  = RAM+0x10000;
   RAM_SCROLL = RAM+0x32000;
   RAM_ROTATE = RAM+0x34000;
   RAM_OBJECT = RAM+0x20000;
   RAM_INPUT  = RAM+0x32100;
   GFX_FG0    = RAM+0x3C000;

   set_colour_mapper(&col_map_rrrr_gggg_bbbb_xxxx);
   InitPaletteMap(RAM+0x30000, 0x100, 0x10, 0x1000);

   // Init tc0220ioc emulation
   // ------------------------

   tc0220ioc.RAM  = RAM_INPUT;
   tc0220ioc.ctrl = TC0220_STOPCPU;
   reset_tc0220ioc();

   // Init tc0005rot emulation
   // ------------------------

   tc0005rot.RAM     = RAM_ROTATE;
   tc0005rot.RAM_SCR = RAM+0x36000;
   setup_gfx();
   tc0200obj.RAM_B	= RAM_OBJECT+0x8000;
   // tc0200obj.ofs_x	= -3+7; // ???

   init_tc0100scn(0);


/*
 *  StarScream Stuff follows
 */

   AddRWBW(  0x100000, 0x10FFFF, NULL, RAM+0x000000);			// 68000 RAM
   AddWriteByte(0x806000, 0x806FFF, tc0100scn_0_gfx_fg0_wb, NULL);	// FG0 GFX RAM
   AddWriteWord(0x806000, 0x806FFF, tc0100scn_0_gfx_fg0_ww, NULL);	// FG0 GFX RAM
   AddRWBW(0x800000, 0x80FFFF, NULL, RAM_VIDEO);			// SCREEN RAM
   AddRWBW(0x900000, 0x90FFFF, NULL, RAM_OBJECT);			// OBJECT RAM
   AddReadBW(0x300000, 0x30001F, NULL, RAM_INPUT);			// INPUT
   AddReadByte(0x320000, 0x320003, tc0140syt_read_main_68k, NULL);	// SOUND COMM
   AddReadByte(0x000000, 0xFFFFFF, DefBadReadByte, NULL);		// <Bad Reads>
   AddReadByte(-1, -1, NULL, NULL);

   AddReadWord(0x200000, 0x201FFF, NULL, RAM+0x030000);			// COLOR RAM
   AddReadWord(0x000000, 0xFFFFFF, DefBadReadWord, NULL);		// <Bad Reads>
   AddReadWord(-1, -1,NULL, NULL);

   AddWriteByte(0x300000, 0x30000F, tc0220ioc_wb, NULL);		// INPUT
   AddWriteByte(0x320000, 0x320003, tc0140syt_write_main_68k, NULL);	// SOUND COMM
   AddWriteByte(0xAA0000, 0xAA0001, Stop68000, NULL);			// Trap Idle 68000
   AddWriteBW(0xB00000, 0xB000FF, NULL, RAM+0x032300);		// priority register
   TC0360PRI_regs = RAM+0x032300;
   AddWriteByte(0x000000, 0xFFFFFF, DefBadWriteByte, NULL);		// <Bad Writes>
   AddWriteByte(-1, -1, NULL, NULL);

   AddWriteWord(0x200000, 0x201FFF, NULL, RAM+0x030000);		// COLOR RAM
   AddWriteWord(0xA00000, 0xA01FFF, tc0005rot_bg0_ww, NULL);		// SCREEN RAM (ROTATE)
   AddWriteWord(0x300000, 0x30000F, tc0220ioc_ww, NULL);		// INPUT
   AddWriteWord(0x820000, 0x82000F, NULL, RAM_SCROLL);			// SCROLL RAM
   AddWriteWord(0xA02000, 0xA0200F, NULL, RAM+0x036000);		// SCROLL RAM (ROTATE)
   AddWriteWord(0x000000, 0xFFFFFF, DefBadWriteWord, NULL);		// <Bad Writes>
   AddWriteWord(-1, -1, NULL, NULL);

   AddInitMemory();	// Set Starscream mem pointers...
}

extern UINT16 *f2_sprite_extension,f2_sprites_colors;

static void load_finalb() {
  int i;
  unsigned char data;
  unsigned int offset;
  UINT8 *gfx = load_region[REGION_GFX2];

  offset = 0x100000;
  for (i = 0x180000; i<0x200000; i++)
    {
      int d1,d2,d3,d4;

      /* convert from 2bits into 4bits format */
      data = gfx[i];
      d1 = (data>>0) & 3;
      d2 = (data>>2) & 3;
      d3 = (data>>4) & 3;
      d4 = (data>>6) & 3;

      gfx[offset] = (d3<<2) | (d4<<6);
      offset++;

      gfx[offset] = (d1<<2) | (d2<<6);
      offset++;
    }

   // Fix Int#6
   // ---------

   //WriteLong68k(&ROM[0x00610],0x4E714E71);	//	nop

   // Speed Hack
   // ----------

   WriteLong68k(&ROM[0x00744],0x13FC0000);	//	move.b	#$00,$AA0000
   WriteLong68k(&ROM[0x00748],0x00AA0000);
   WriteWord68k(&ROM[0x0074C],0x6100-16);

   //WriteLong68k(&ROM[0x00618],0x13FC0000);	//	move.b	#$00,$AA0000
   //WriteLong68k(&ROM[0x0061C],0x00AA0000);

   // Fix Colour ram error
   // --------------------

   WriteWord68k(&ROM[0x022E6],0x4E71);
   load_common();

   AddTaitoYM2610(0x033A, 0x02A7, 0x10000);

   RAM_VIDEO  = RAM+0x20000;
   RAM_SCROLL = RAM+0x30000;
   RAM_OBJECT = RAM+0x10000;
   RAM_INPUT  = RAM+0x3c000;
   GFX_FG0    = RAM+0x31000;
   RAMSize = 0x3c010;

   tc0110pcr_init(RAM+0x35000, 0);

   set_colour_mapper(&col_map_xbbb_bbgg_gggr_rrrr);
   InitPaletteMap(RAM+0x35000, 0x100, 0x10, 0x8000);

   // Init tc0220ioc emulation
   // ------------------------

   tc0220ioc.RAM  = RAM_INPUT;
   tc0220ioc.ctrl = 0;		//TC0220_STOPCPU;
   reset_tc0220ioc();

   setup_gfx();

   init_tc0100scn(0);
   f2_sprites_colors = 64;

   AddRWBW(0x100000, 0x10FFFF, NULL, RAM+0x000000);			// 68000 RAM
   AddReadBW(0x300000, 0x30000F, NULL, RAM_INPUT);			// INPUT
   AddReadByte(0x320000, 0x320003, tc0140syt_read_main_68k, NULL);	// SOUND COMM
   AddWriteByte(0x806000, 0x806FFF, tc0100scn_0_gfx_fg0_wb, NULL);	// FG0 GFX RAM
   AddWriteWord(0x806000, 0x806FFF, tc0100scn_0_gfx_fg0_ww, NULL);	// FG0 GFX RAM
   AddRWBW(0x800000, 0x80FFFF, NULL, RAM_VIDEO);			// SCREEN RAM
   AddRWBW(0x900000, 0x90fFFF, NULL, RAM_OBJECT);			// OBJECT RAM
   AddReadByte(0x000000, 0xFFFFFF, DefBadReadByte, NULL);		// <Bad Reads>
   AddReadByte(-1, -1, NULL, NULL);

   AddReadWord(0x200000, 0x200007, tc0110pcr_rw, NULL);			// COLOR RAM
   AddReadWord(0x000000, 0xFFFFFF, DefBadReadWord, NULL);		// <Bad Reads>
   AddReadWord(-1, -1,NULL, NULL);

   AddWriteByte(0x320000, 0x320003, tc0140syt_write_main_68k, NULL);	// SOUND COMM
   AddWriteByte(0x300000, 0x30000F, tc0220ioc_wb, NULL);		// INPUT
   AddWriteByte(0xAA0000, 0xAA0001, Stop68000, NULL);			// Trap Idle 68000
   AddWriteByte(0x000000, 0xFFFFFF, DefBadWriteByte, NULL);		// <Bad Writes>
   AddWriteByte(-1, -1, NULL, NULL);

   AddWriteWord(0x200000, 0x200007, tc0110pcr_ww, NULL);		// COLOR RAM
   AddWriteWord(0x820000, 0x82000F, NULL, RAM_SCROLL);			// SCROLL RAM
   // AddWriteWord(0x360000, 0x36000F, NULL, RAM+0x03C300);		// ??? RAM
   AddWriteWord(0x300000, 0x30000F, tc0220ioc_ww, NULL);		// INPUT
   AddWriteWord(0x000000, 0xFFFFFF, DefBadWriteWord, NULL);		// <Bad Writes>
   AddWriteWord(-1, -1, NULL, NULL);

   AddInitMemory();	// Set Starscream mem pointers...
}

static UINT16 mjnquest_input;

static UINT16  mjnquest_dsw_r (UINT32 offset) {
  switch (offset&0xf)
    {
    case 0x00:
      return (input_buffer[0xa] << 0) + (input_buffer[0xe]<<8); /* DSW A + coin */
    case 0x02:
      return (input_buffer[0xc] << 0) + (input_buffer[0x10]<<8); /* DSW B + coin */
    }
  return 0xff;
}

static UINT16  mjnquest_input_r (UINT32 offset) {
  switch (mjnquest_input)
    {
    case 0x01:
      return input_buffer[0]; /* IN0 */
    case 0x02:
      return input_buffer[2]; /* IN1 */
    case 0x04:
      return input_buffer[4]; /* IN2 */
    case 0x08:
      return input_buffer[6]; /* IN3 */
    case 0x10:
      return input_buffer[8]; /* IN4 */
    }

  return 0xff;
}

static void  mjnquest_inputselect_w (UINT32 offset, UINT16 data) {
  mjnquest_input = (data >> 6);
}

static void load_mjnquest() {
  UINT8 *RAM_COLOUR;
  int i,size = get_region_size(REGION_GFX2);
  UINT8 *gfx = load_region[REGION_GFX2];

  /* the bytes in each longword are in reversed order, put them in the
     order used by the other games. */

  for (i = 0;i < size;i += 2) {
    int t;

    t = gfx[i];
    gfx[i] = (gfx[i+1] >> 4) | (gfx[i+1] << 4);
    gfx[i+1] = (t >> 4) | (t << 4);
  }

   // 68000 Speed Hack
   // ----------------

   WriteLong68k(&ROM[0x004A4],0x13FC0000);	// move.b #$00,$AA0000
   WriteLong68k(&ROM[0x004A8],0x00AA0000);

   load_common();

   AddTaitoYM2610(0x0338, 0x02A5, 0x10000);

   RAM_OBJECT = RAM+0x20000;
   RAM_VIDEO  = RAM+0x30000;
   RAM_SCROLL = RAM+0x4A000;
   RAM_COLOUR = RAM+0x40000;
   RAM_INPUT  = RAM+0x4A100;
   GFX_BANK   = RAM+0x4A200;
   GFX_FG0    = RAM+0x50000;
   RAMSize = 0x54000;

   setup_gfx();

   tc0110pcr_init(RAM_COLOUR, 0);

   set_colour_mapper(&col_map_xbbb_bbgg_gggr_rrrr);
   InitPaletteMap(RAM_COLOUR, 0x100, 0x10, 0x8000);

   // Init tc0220ioc emulation
   // ------------------------

   tc0220ioc.RAM  = RAM_INPUT;
   tc0220ioc.ctrl = 0;		//TC0220_STOPCPU;
   reset_tc0220ioc();

   tc0100scn[0].layer[0].scr_x	=16+1;

   tc0100scn[0].layer[1].scr_x	=16+1;

   tc0100scn[0].layer[2].scr_x	=16+1;
   init_tc0100scn(0);

   tc0200obj.ofs_x	= 0+6;

   AddRWBW(0x110000, 0x12FFFF, NULL, RAM+0x000000);			// 68000 RAM
   AddReadWord(0x200000, 0x200007, tc0110pcr_rw, NULL);			// COLOR RAM
   AddReadBW(0x300000, 0x30000f, mjnquest_dsw_r, NULL);
   AddReadBW(0x310000, 0x310001, mjnquest_input_r, NULL);
   AddReadByte(0x360000, 0x360003, tc0140syt_read_main_68k, NULL);	// SOUND COMM
   AddWriteByte(0x406000, 0x406FFF, tc0100scn_0_gfx_fg0_wb, NULL);	// FG0 GFX RAM
   AddWriteWord(0x406000, 0x406FFF, tc0100scn_0_gfx_fg0_ww, NULL);	// FG0 GFX RAM
   AddRWBW(0x400000, 0x40FFFF, NULL, RAM_VIDEO);			// SCREEN RAM
   AddRWBW(0x420000, 0x42000F, NULL, RAM_SCROLL);			// SCROLL RAM
   AddRWBW(0x500000, 0x50FFFF, NULL, RAM_OBJECT);			// OBJECT RAM
   AddReadByte(0x000000, 0xFFFFFF, DefBadReadByte, NULL);		// <Bad Reads>
   AddReadByte(-1, -1, NULL, NULL);
   AddReadWord(0x000000, 0xFFFFFF, DefBadReadWord, NULL);		// <Bad Reads>
   AddReadWord(-1, -1,NULL, NULL);

   AddWriteWord(0x200000, 0x200007, tc0110pcr_ww, NULL);		// COLOR RAM
   AddWriteWord(0x320000, 0x320001, mjnquest_inputselect_w, NULL);
   AddWriteByte(0x360000, 0x360003, tc0140syt_write_main_68k, NULL);	// SOUND COMM
   AddWriteWord(0x380000, 0x380001, NULL, GFX_BANK);			// BANK SWITCH
   AddWriteByte(0xAA0000, 0xAA0001, Stop68000, NULL);			// Trap Idle 68000
   AddWriteByte(0x000000, 0xFFFFFF, DefBadWriteByte, NULL);		// <Bad Writes>
   AddWriteByte(-1, -1, NULL, NULL);
   AddWriteWord(0x000000, 0xFFFFFF, DefBadWriteWord, NULL);		// <Bad Writes>
   AddWriteWord(-1, -1, NULL, NULL);

   AddInitMemory();	// Set Starscream mem pointers...
}

static void load_dinorex() {
   WriteWord68k(&ROM[0x01084],0x4EF9);		// JMP $7FF00
   WriteLong68k(&ROM[0x01086],0x0007FF00);

   WriteWord68k(&ROM[0x7FF00],0x4EB9);
   WriteLong68k(&ROM[0x7FF02],0x000032FC);
   WriteLong68k(&ROM[0x7FF06],0x13FC0000);	// Stop 68000
   WriteLong68k(&ROM[0x7FF0A],0x00AA0000);
   WriteWord68k(&ROM[0x7FF0E],0x6100-16);	// Loop

  load_common();

   /*-----[Sound Setup]-----*/
   AddTaitoYM2610(0x0211, 0x017A, 0x10000);

   RAM_OBJECT = RAM+0x10000;
   f2_sprite_extension = (UINT16*)(RAM+0x20000);
   RAM_SCROLL = RAM+0x33100;
   RAM_INPUT  = RAM+0x32100;
   TC0360PRI_regs = RAM+0x34000;
   RAM_VIDEO  = RAM+0x40000;
   GFX_FG0    = RAM+0x50000;
   memset(GFX_FG0,0,0x4000);
   RAMSize = 0x54000;

   // don't know if this color mapper is correct or not... anyway...
   set_colour_mapper(&col_Map_15bit_RRRRGGGGBBBBRGBx);
   InitPaletteMap(RAM+0x30000, 0x100, 0x10, 0x8000);

   setup_gfx();

   // Init tc0220ioc emulation
   // ------------------------

   tc0220ioc.RAM  = RAM_INPUT;
   tc0220ioc.ctrl = 0;		//TC0220_STOPCPU;
   reset_tc0220ioc();

   tc0200obj.ofs_x	= 9;
   tc0200obj.ofs_y	= 3; // sprite type now

   tc0100scn[0].layer[0].scr_x	=19;
   tc0100scn[0].layer[1].scr_x	=19;
   tc0100scn[0].layer[2].scr_x	=19;

   init_tc0100scn(0);

   AddRWBW(0x600000, 0x60FFFF, NULL, RAM+0x000000);			// 68000 RAM
   AddRWBW(0x800000, 0x80FFFF, NULL, RAM_OBJECT);
   AddRWBW(0x400000, 0x400FFF, NULL, (UINT8*)f2_sprite_extension);
   // Notice : you can't position GFX_FG0 = RAM_VIDEO + 0x6000
   // because these 2 functions create a 0x4000 bytes long buffer from the
   // 0x1000 bytes of data (4x the size)
   AddWriteByte(0x906000, 0x906FFF, tc0100scn_0_gfx_fg0_wb, NULL);	// FG0 GFX RAM
   AddWriteWord(0x906000, 0x906FFF, tc0100scn_0_gfx_fg0_ww, NULL);	// FG0 GFX RAM
   AddRWBW(0x900000, 0x90fFFF, NULL, RAM_VIDEO);			// SCREEN RAM
   AddRWBW(0x500000, 0x501FFF, NULL, RAM+0x030000);			// COLOR RAM
   AddReadBW(0x300000, 0x30001F, NULL, RAM_INPUT);			// INPUT
   AddReadByte(0xA00000, 0xA00003, tc0140syt_read_main_68k, NULL);	// SOUND COMM
   AddRWBW(0x700000, 0x7000FF, NULL, TC0360PRI_regs);

   AddReadByte(0x000000, 0xFFFFFF, DefBadReadByte, NULL);		// <Bad Reads>
   AddReadByte(-1, -1, NULL, NULL);

   AddReadWord(0x000000, 0xFFFFFF, DefBadReadWord, NULL);		// <Bad Reads>
   AddReadWord(-1, -1,NULL, NULL);

   AddWriteByte(0xA00000, 0xA00003, tc0140syt_write_main_68k, NULL);	// SOUND COMM
   AddWriteByte(0x300000, 0x30001F, tc0220ioc_wb, NULL);		// INPUT
   AddWriteByte(0xAA0000, 0xAA0001, Stop68000, NULL);			// Trap Idle 68000

   AddWriteByte(0x000000, 0xFFFFFF, DefBadWriteByte, NULL);		// <Bad Writes>
   AddWriteByte(-1, -1, NULL, NULL);

   AddWriteWord(0x920000, 0x92000F, NULL, RAM_SCROLL);			// SCROLL RAM
   AddWriteWord(0x300000, 0x30001F, tc0220ioc_ww, NULL);		// INPUT
   AddWriteWord(0x000000, 0xFFFFFF, DefBadWriteWord, NULL);		// <Bad Writes>
   AddWriteWord(-1, -1, NULL, NULL);

   AddInitMemory();	// Set Starscream mem pointers...
}

static void solitary_fighter_ioc_0_wb(UINT32 offset, UINT8 data)
{
   switch(offset & 6){
      case 0:
         tc0220ioc_wb(0, data);
      break;
      case 2:
         tc0220ioc_wb(2, data);
      break;
      case 4:
         tc0220ioc_wb(8, data);
      break;
      default:
      break;
   }
}

static void solitary_fighter_ioc_0_ww(UINT32 offset, UINT16 data)
{
   solitary_fighter_ioc_0_wb(offset, (UINT8) (data & 0xFF));
}

static UINT8 solitary_fighter_ioc_0_rb(UINT32 offset)
{
   switch(offset & 6){
      case 0:
         return tc0220ioc_rb(0);
      break;
      case 2:
         return tc0220ioc_rb(2);
      break;
      case 4:
         return tc0220ioc_rb(8);
      break;
      default:
         return 0xFF;
      break;
   }
}

static UINT16 solitary_fighter_ioc_0_rw(UINT32 offset)
{
   return solitary_fighter_ioc_0_rb(offset);
}

static void solitary_fighter_ioc_1_wb(UINT32 offset, UINT8 data)
{
   switch(offset & 6){
      case 0:
         tc0220ioc_wb(4, data);
      break;
      case 2:
         tc0220ioc_wb(6, data);
      break;
      case 4:
         tc0220ioc_wb(14, data);
      break;
      default:
      break;
   }
}

static void solitary_fighter_ioc_1_ww(UINT32 offset, UINT16 data)
{
   solitary_fighter_ioc_1_wb(offset, (UINT8) (data & 0xFF));
}

static UINT8 solitary_fighter_ioc_1_rb(UINT32 offset)
{
   switch(offset & 6){
      case 0:
         return tc0220ioc_rb(4);
      break;
      case 2:
         return tc0220ioc_rb(6);
      break;
      case 4:
         return tc0220ioc_rb(14);
      break;
      default:
         return 0xFF;
      break;
   }
}

static UINT16 solitary_fighter_ioc_1_rw(UINT32 offset)
{
   return solitary_fighter_ioc_1_rb(offset);

}

static void load_solfigtr() {
  UINT8 *RAM_COLOUR;
   // SBCD flag bug - game relies on undefined flags (not supported in starscream)
  // Effect : big slow down when one of the player is loosing
  WriteWord68k(&ROM[0x09B9A],0x4E71);

   // 68000 Speed Hack

   WriteLong68k(&ROM[0x0058E],0x4EF800C0);

   WriteLong68k(&ROM[0x000C0],0x46FC2000);

   WriteLong68k(&ROM[0x000C4],0x13FC0000);	// move.b #$00,$AA0000
   WriteLong68k(&ROM[0x000C8],0x00AA0000);
   WriteWord68k(&ROM[0x000CC],0x6100-10);

   load_common();

   AddTaitoYM2610(0x01A9, 0x0155, 0x10000);

   RAM_OBJECT = RAM+0x10000;
   RAM_COLOUR = RAM+0x20000;
   RAM_SCROLL = RAM+0x22000;
   GFX_BANK   = RAM+0x22200;
   GFX_FG0    = RAM+0x23000;
   RAM_INPUT  = RAM+0x2e200;
   RAM_VIDEO  = RAM+0x30000;

   setup_gfx();

   RAMSize=0x40000;

   set_colour_mapper(&col_map_rrrr_gggg_bbbb_xxxx);
   InitPaletteMap(RAM_COLOUR, 0x100, 0x10, 0x1000);

   tc0220ioc.RAM  = RAM_INPUT;
   tc0220ioc.ctrl = 0;		//TC0220_STOPCPU;
   reset_tc0220ioc();

   init_tc0100scn(0);

   AddRWBW(0x100000, 0x103FFF, NULL, RAM+0x000000);			// 68000 RAM
   AddRWBW(0x900000, 0x90FFFF, NULL, RAM_OBJECT);			// OBJECT RAM
   AddWriteWord(0x806000, 0x806FFF, tc0100scn_0_gfx_fg0_ww, NULL);	// FG0 GFX RAM
   AddWriteByte(0x806000, 0x806FFF, tc0100scn_0_gfx_fg0_wb, NULL);	// FG0 GFX RAM
   AddRWBW(0x800000, 0x80FFFF, NULL, RAM_VIDEO);			// SCREEN RAM
   AddRWBW(0x200000, 0x201FFF, NULL, RAM_COLOUR);			// COLOR RAM
   AddReadByte(0x300000, 0x300007, solitary_fighter_ioc_0_rb, NULL);	// INPUT
   AddReadByte(0x320000, 0x320007, solitary_fighter_ioc_1_rb, NULL);	// INPUT
   AddReadByte(0x400000, 0x400003, tc0140syt_read_main_68k, NULL);	// SOUND COMM
   AddReadByte(0x000000, 0xFFFFFF, DefBadReadByte, NULL);		// <Bad Reads>
   AddReadByte(-1, -1, NULL, NULL);

   AddReadWord(0x300000, 0x300007, solitary_fighter_ioc_0_rw, NULL);	// INPUT
   AddReadWord(0x320000, 0x320007, solitary_fighter_ioc_1_rw, NULL);	// INPUT
   AddReadWord(0x000000, 0xFFFFFF, DefBadReadWord, NULL);		// <Bad Reads>
   AddReadWord(-1, -1,NULL, NULL);

   AddWriteBW(0x820000, 0x82000F, NULL, RAM_SCROLL);			// SCROLL RAM
   AddWriteByte(0x400000, 0x400003, tc0140syt_write_main_68k, NULL);	// SOUND COMM
   AddWriteByte(0x300000, 0x300007, solitary_fighter_ioc_0_wb, NULL);	// INPUT
   AddWriteByte(0x320000, 0x320007, solitary_fighter_ioc_1_wb, NULL);	// INPUT
   AddWriteByte(0xAA0000, 0xAA0001, Stop68000, NULL);			// Trap Idle 68000
   AddWriteByte(0x000000, 0xFFFFFF, DefBadWriteByte, NULL);		// <Bad Writes>
   AddWriteByte(-1, -1, NULL, NULL);

   AddWriteWord(0x500000, 0x50001F, NULL, GFX_BANK);			// BANK
   AddWriteWord(0xB00000, 0xB0001F, NULL, GFX_BANK+0x20);		// priorities
   TC0360PRI_regs = GFX_BANK + 0x20;
   AddWriteWord(0x300000, 0x300007, solitary_fighter_ioc_0_ww, NULL);	// INPUT
   AddWriteWord(0x320000, 0x320007, solitary_fighter_ioc_1_ww, NULL);	// INPUT
   AddWriteWord(0x000000, 0xFFFFFF, DefBadWriteWord, NULL);		// <Bad Writes>
   AddWriteWord(-1, -1, NULL, NULL);

   AddInitMemory();	// Set Starscream mem pointers...
}

static int init_gfx3;

static void load_thundfox(void)
{
   // 68000 Speed Hack
   // ----------------

   WriteLong68k(&ROM[0x0A0C],0x13FC0000);	// move.b #$00,$AA0000
   WriteLong68k(&ROM[0x0A10],0x00AA0000);
   WriteWord68k(&ROM[0x0A14],0x6100-16);	// bra.s <loop>

   WriteLong68k(&ROM[0x07B0],0x13FC0000);	// move.b #$00,$AA0000
   WriteLong68k(&ROM[0x07B4],0x00AA0000);

  load_common();

   /*-----[Sound Setup]-----*/
   AddTaitoYM2610(0x023A, 0x01BA, 0x10000);

   RAM_INPUT  = RAM+0x2e200;

   RAM_VIDEO  = RAM+0x04000;
   RAM_SCROLL = RAM+0x2E000;

   RAM_VIDEO2 = RAM+0x14000;
   RAM_SCROLL2= RAM+0x2E100;
   TC0360PRI_regs = RAM+0x2e200;

   RAM_OBJECT = RAM+0x24000;

   GFX_FG0    = RAM+0x30000;
   GFX_FG1    = RAM+0x34000;
   RAMSize=0x38000;

   set_colour_mapper(&col_map_rrrr_gggg_bbbb_xxxx);
   InitPaletteMap(RAM+0x2C000, 0x100, 0x10, 0x1000);

   setup_gfx();

   // Init tc0220ioc emulation
   // ------------------------

   tc0220ioc.RAM  = RAM_INPUT;
   tc0220ioc.ctrl = 0;		//TC0220_STOPCPU;
   reset_tc0220ioc();

   set_colour_mapper(&col_map_rrrr_gggg_bbbb_xxxx);
   InitPaletteMap(RAM+0x2C000, 0x100, 0x10, 0x1000);

   init_tc0100scn(0);
   memcpy(&tc0100scn[1],&tc0100scn[0],sizeof(struct TC0100SCN));

   init_gfx3 = 0;

   tc0100scn[1].layer[0].RAM	=RAM_VIDEO2 +0x0000;
   tc0100scn[1].layer[0].SCR	=RAM_SCROLL2 +0;
   tc0100scn[1].layer[0].scr_x	=15; // to be checked
   tc0100scn[1].layer[0].scr_y	=15;

   tc0100scn[1].layer[1].RAM	=RAM_VIDEO2 +0x8000;
   tc0100scn[1].layer[1].SCR	=RAM_SCROLL2 +2;
   tc0100scn[1].layer[1].scr_x	=15;
   tc0100scn[1].layer[1].scr_y	=15;

   tc0100scn[1].layer[2].RAM	=RAM_VIDEO2 +0x4000;
   tc0100scn[1].layer[2].GFX	=GFX_FG1;
   tc0100scn[1].layer[2].SCR	=RAM_SCROLL2 +4;

   tc0100scn[1].RAM     = RAM_VIDEO2;
   tc0100scn[1].GFX_FG0 = GFX_FG1;

   init_tc0100scn(1);

   AddRWBW(0x300000, 0x303FFF, NULL, RAM+0x000000);			// 68000 RAM
   AddWriteByte(0x406000, 0x406FFF, tc0100scn_0_gfx_fg0_wb, NULL);	// FG0 GFX RAM
   AddWriteWord(0x406000, 0x406FFF, tc0100scn_0_gfx_fg0_ww, NULL);	// FG0 GFX RAM
   AddRWBW(0x400000, 0x40FFFF, NULL, RAM_VIDEO);			// SCREEN0 RAM
   AddWriteByte(0x506000, 0x506FFF, tc0100scn_1_gfx_fg0_wb, NULL);	// FG1 GFX RAM
   AddWriteWord(0x506000, 0x506FFF, tc0100scn_1_gfx_fg0_ww, NULL);	// FG1 GFX RAM
   AddRWBW(0x500000, 0x50FFFF, NULL, RAM_VIDEO2);			// SCREEN1 RAM
   AddRWBW(0x600000, 0x607FFF, NULL, RAM_OBJECT);			// OBJECT RAM
   AddReadBW(0x200000, 0x20000F, NULL, RAM_INPUT);			// INPUT
   AddReadByte(0x220000, 0x220003, tc0140syt_read_main_68k, NULL);	// SOUND COMM

   AddRWBW(0x800000,0x80001f,NULL,TC0360PRI_regs);

   AddReadByte(0x000000, 0xFFFFFF, DefBadReadByte, NULL);		// <Bad Reads>
   AddReadByte(-1, -1, NULL, NULL);

   AddReadWord(0x100000, 0x101FFF, NULL, RAM+0x02C000);			// COLOR RAM
   AddReadWord(0x000000, 0xFFFFFF, DefBadReadWord, NULL);		// <Bad Reads>
   AddReadWord(-1, -1,NULL, NULL);

   AddWriteByte(0x220000, 0x220003, tc0140syt_write_main_68k, NULL);	// SOUND COMM
   AddWriteByte(0x200000, 0x20000F, tc0220ioc_wb, NULL);		// INPUT RAM
   AddWriteByte(0xAA0000, 0xAA0001, Stop68000, NULL);			// Trap Idle 68000
   AddWriteByte(-1, -1, NULL, NULL);

   AddWriteWord(0x100000, 0x101FFF, NULL, RAM+0x02C000);		// COLOR RAM
   AddWriteWord(0x420000, 0x42000F, NULL, RAM_SCROLL);			// SCROLL0 RAM
   AddWriteWord(0x520000, 0x52000F, NULL, RAM_SCROLL2);			// SCROLL1 RAM
   AddWriteWord(0x200000, 0x20000F, tc0220ioc_ww, NULL);		// INPUT RAM
   AddWriteWord(-1, -1, NULL, NULL);

   AddInitMemory();	// Set Starscream mem pointers...
}

static struct DSW_DATA dsw_data_pulirula_0[] =
{
   { MSG_DSWA_BIT1,           0x01, 0x02 },
   { MSG_OFF,                 0x01, 0x00 },
   { MSG_ON,                  0x00, 0x00 },
   { MSG_SCREEN,              0x02, 0x02 },
   { MSG_NORMAL,              0x02, 0x00 },
   { MSG_INVERT,              0x00, 0x00 },
   { MSG_TEST_MODE,           0x04, 0x02 },
   { MSG_OFF,                 0x04, 0x00 },
   { MSG_ON,                  0x00, 0x00 },
   { MSG_DEMO_SOUND,          0x08, 0x02 },
   { MSG_ON,                  0x08, 0x00 },
   { MSG_OFF,                 0x00, 0x00 },
   { MSG_COIN1,               0x30, 0x04 },
   { MSG_1COIN_1PLAY,         0x30, 0x00 },
   { MSG_2COIN_1PLAY,         0x20, 0x00 },
   { MSG_3COIN_1PLAY,         0x10, 0x00 },
   { MSG_4COIN_1PLAY,         0x00, 0x00 },
   { MSG_COIN2,               0xC0, 0x04 },
   { MSG_1COIN_2PLAY,         0xC0, 0x00 },
   { MSG_1COIN_3PLAY,         0x80, 0x00 },
   { MSG_2COIN_1PLAY,         0x40, 0x00 },
   { MSG_2COIN_3PLAY,         0x00, 0x00 },
   { NULL,                    0,    0,   },
};

static struct DSW_DATA dsw_data_pulirula_1[] =
{
   { MSG_DIFFICULTY,          0x03, 0x04 },
   { MSG_NORMAL,              0x03, 0x00 },
   { MSG_EASY,                0x02, 0x00 },
   { MSG_HARD,                0x01, 0x00 },
   { MSG_HARDEST,             0x00, 0x00 },
   { "Magic",                 0x0C, 0x04 },
   { "3",                     0x0C, 0x00 },
   { "4",                     0x08, 0x00 },
   { "5",                     0x04, 0x00 },
// { "5",                     0x00, 0x00 },
   { MSG_LIVES,               0x30, 0x04 },
   { "3",                     0x30, 0x00 },
   { "2",                     0x20, 0x00 },
   { "4",                     0x10, 0x00 },
   { "5",                     0x00, 0x00 },
   { MSG_DSWB_BIT7,           0x40, 0x02 },
   { MSG_OFF,                 0x40, 0x00 },
   { MSG_ON,                  0x00, 0x00 },
   { "Upright Controls",      0x80, 0x02 },
   { "Dual",                  0x80, 0x00 },
   { "Single",                0x00, 0x00 },
   { NULL,                    0,    0,   },
};

static struct DSW_INFO pulirula_dsw[] =
{
   { 0x03c000, 0xFF, dsw_data_pulirula_0 },
   { 0x03c002, 0xFF, dsw_data_pulirula_1 },
   { 0,        0,    NULL,      },
};

static void pri_swap_bytes(UINT32 offset, UINT16 data) {
  offset &= 0x1f;
  
  if (TC0360PRI_regs[offset] != data) {
    TC0360PRI_regs[(offset)] = data;
  }
}

static void pri_swap_word(UINT32 offset, UINT16 data) {
  offset &= 0x1f;
  if (ReadWord68k(&TC0360PRI_regs[offset]) != data) {
    WriteWord68k(&TC0360PRI_regs[(offset )], data);
  }
}

static void load_pulirula(void)
{
  UINT8 *RAM_COLOUR;
   // 68000 Speed Hack
   // ----------------

   WriteWord68k(&ROM[0x084C],0x4EF9);		// jmp $300
   WriteLong68k(&ROM[0x084E],0x00000300);
   WriteLong68k(&ROM[0x00300],0x526DABF2);	// jsr <random gen>
   WriteLong68k(&ROM[0x00304],0x13FC0000);	// move.b #$00,$AA0000
   WriteLong68k(&ROM[0x00308],0x00AA0000);
   WriteWord68k(&ROM[0x0030C],0x6100-14);	// bra.s <loop>

  load_common();

   /*-----[Sound Setup]-----*/
   AddTaitoYM2610(0x01A4, 0x0150, 0x20000);

   RAM_OBJECT = RAM+0x10000;
   RAM_VIDEO  = RAM+0x20000;
   RAM_COLOUR = RAM+0x30000;
   RAM_SCROLL = RAM+0x32000;
   TC0360PRI_regs = RAM_SCROLL+0x10;
   RAM_ROTATE = RAM+0x34000;
   f2_sprite_extension = (UINT16*)(RAM+0x37000);
   RAM_INPUT  = RAM+0x3c000;
   GFX_FG0    = RAM+0x40000;
   RAMSize = 0x40000;

   // set_colour_mapper(&col_map_rrrr_gggg_bbbb_xxxx);
   set_colour_mapper(&col_map_xrrr_rrgg_gggb_bbbb);
   InitPaletteMap(RAM_COLOUR, 0x100, 0x10, 0x8000);

   tc0005rot.RAM     = RAM_ROTATE;
   tc0005rot.RAM_SCR = RAM_ROTATE+0x2000;
   setup_gfx();

   // Init tc0220ioc emulation
   // ------------------------

   tc0220ioc.RAM  = RAM_INPUT;
   tc0220ioc.ctrl = 0;		//TC0220_STOPCPU;
   reset_tc0220ioc();

   init_gfx3 = 0;
   tc0100scn[0].layer[0].scr_x	=16+1;

   tc0100scn[0].layer[1].scr_x	=16+1;

   tc0100scn[0].layer[2].scr_x	=16+1;
   f2_sprites_colors = 64;

   init_tc0100scn(0);
   tc0200obj.RAM_B	= RAM_OBJECT+0x8000;
   tc0200obj.ofs_x	= 6;
   tc0200obj.ofs_y	= 2; // sprite type : pulirula

   AddRWBW(0x300000, 0x30FFFF, NULL, RAM+0x000000);			// 68000 RAM
   AddWriteByte(0x806000, 0x806FFF, tc0100scn_0_gfx_fg0_wb, NULL);	// FG0 GFX RAM
   AddWriteWord(0x806000, 0x806FFF, tc0100scn_0_gfx_fg0_ww, NULL);	// FG0 GFX RAM
   AddRWBW(0x800000, 0x80FFFF, NULL, RAM_VIDEO);			// SCREEN0 RAM
   AddRWBW(0x900000, 0x90FFFF, NULL, RAM_OBJECT);			// OBJECT RAM
   AddWriteWord(0x600000, 0x603FFF, NULL, (UINT8*)f2_sprite_extension);		// OBJECT EXTRA RAM
   AddWriteWord(0x400000, 0x401fff, tc0005rot_bg0_ww, NULL);		// SCREEN RAM (ROTATE)
   AddRWBW(0x400000, 0x40200F, NULL, RAM_ROTATE);			// SCREEN RAM (ROTATION) + scroll registers
   AddReadBW(0xB00000, 0xB0000F, NULL, RAM_INPUT);			// INPUT
   AddRWBW(0x700000, 0x701FFF, NULL, RAM_COLOUR);
   AddReadByte(0x200000, 0x200003, tc0140syt_read_main_68k, NULL);	// SOUND COMM

   AddWriteByte(0xa00000,0xa0001f,pri_swap_bytes, NULL);
   // AddWriteByte(0xa00000,0xa0001f,NULL, TC0360PRI_regs);
   AddWriteWord(0xa00000,0xa0001f,pri_swap_word, NULL);
   // AddWriteWord(0xa00000,0xa0001f,NULL, TC0360PRI_regs);

   AddReadByte(0x000000, 0xFFFFFF, DefBadReadByte, NULL);		// <Bad Reads>
   AddReadByte(-1, -1, NULL, NULL);

   AddReadWord(0x000000, 0xFFFFFF, DefBadReadWord, NULL);		// <Bad Reads>
   AddReadWord(-1, -1,NULL, NULL);

   AddWriteByte(0x200000, 0x200003, tc0140syt_write_main_68k, NULL);	// SOUND COMM
   AddWriteByte(0xB00000, 0xB0000F, tc0220ioc_wb, NULL);		// INPUT
   AddWriteByte(0xAA0000, 0xAA0001, Stop68000, NULL);			// Trap Idle 68000
   AddWriteByte(-1, -1, NULL, NULL);

   AddWriteWord(0x820000, 0x82000F, NULL, RAM_SCROLL);			// SCROLL0 RAM
   AddWriteWord(0xB00000, 0xB0000F, tc0220ioc_ww, NULL);		// INPUT
   AddWriteWord(-1, -1, NULL, NULL);

   AddInitMemory();	// Set Starscream mem pointers...
}

/* specific sound roms for driveout... */

static struct OKIM6295interface okim6295_interface =
{
  1,
  { 8000 },			/* Hz ?? */
  { REGION_SOUND1 },	/* memory region */
  { 255 }			/* volume ?? */
};

static SOUND_INFO driveout_sound[] =
{
   { SOUND_M6295,   &okim6295_interface,	},
   { 0, 	    NULL,		},
};

static int driveout_sound_latch = 0;

static UINT8 driveout_sound_command_r(UINT32 offset)
{
  //cpu_set_irq_line(1,0,CLEAR_LINE);
  /*	logerror("sound IRQ OFF (sound command=%02x)\n",driveout_sound_latch); */
  return driveout_sound_latch;
}

static int oki_bank = 0;

static void reset_driveout_sound_region(void)
{
  OKIM6295_set_bank_base(0, ALL_VOICES, oki_bank*0x40000);
}

static void oki_bank_w(UINT32 offset, UINT8 data)
{
  if ((data&4) && (oki_bank!=(data&3)) )
    {
      oki_bank = (data&3);
    }
  reset_driveout_sound_region();
}

static void driveout_sound_command_wb(UINT32 offset, UINT16 data)
{
  static int nibble = 0;
  offset &=3;

  if (offset==0)
    {
      nibble = data & 1;
    }
  else
    {
      if (nibble==0)
	{
	  driveout_sound_latch = (data & 0x0f) | (driveout_sound_latch & 0xf0);
	}
      else
	{
	  driveout_sound_latch = ((data<<4) & 0xf0) | (driveout_sound_latch & 0x0f);
	  cpu_interrupt(CPU_Z80_0, 0x38);
	}
    }
}

static void load_common_driftout() {
  load_common();

  /*-----[Sound Setup]-----*/

  RAM_OBJECT = RAM+0x33000;
  RAM_VIDEO  = RAM+0x16000-0x4000;
  RAM_SCROLL = RAM+0x20020;
  RAM_ROTATE = RAM+0x10000;
  GFX_FG0    = RAM+0x21000;

  RAM_INPUT  = RAM+0x32100;

  set_colour_mapper(&col_map_xrrr_rrgg_gggb_bbbb);
  InitPaletteMap(RAM+0x14000, 0x100, 0x10, 0x8000);
  // Init tc0005rot emulation
  // ------------------------

  tc0005rot.RAM     = RAM_ROTATE;
  tc0005rot.RAM_SCR = RAM_ROTATE+0x2000;
  setup_gfx();

  // tc0200obj.ofs_x	= 8;

  tc0220ioc.RAM  = RAM_INPUT;
  tc0220ioc.ctrl = 0;		//TC0220_STOPCPU;
  reset_tc0220ioc();

  tc0200obj.ofs_x	= -3+6; // ???
  tc0100scn[0].layer[0].scr_x	=19;
  tc0100scn[0].layer[1].scr_x	=19; // -80 for f2demo !
  tc0100scn[0].layer[2].scr_x	=19;
  init_tc0100scn(0);


  /*
   *  StarScream Stuff follows
   */

  AddRWBW(0x300000, 0x30FFFF, NULL, RAM+0x000000);			// 68000 RAM
  AddReadByte(0xB00000, 0xB0001F, NULL, RAM_INPUT);			// INPUT RAM
  AddRWBW(0x900000, 0x90fFFF, NULL, RAM_OBJECT);			// OBJECT RAM
  AddRWBW(0x700000, 0x701FFF, NULL, RAM+0x014000);		// COLOR RAM

  AddReadWord(0x400000, 0x40200F, NULL, RAM_ROTATE);			// SCREEN/SCROLL (ROT)
  AddReadWord(0xB00000, 0xB0001F, NULL, RAM_INPUT);			// INPUT RAM

  AddWriteByte(0xB00000, 0xB0001F, tc0220ioc_wb, NULL);		// INPUT RAM
  AddWriteByte(0xAA0000, 0xAA0001, Stop68000, NULL);			// Trap Idle 68000

  AddWriteWord(0x400000, 0x401FFF, tc0005rot_bg0_ww, NULL);		// SCREEN RAM (ROTATE)
  AddWriteWord(0x402000, 0x40200F, NULL, RAM+0x012000);		// SCROLL RAM (ROTATE)
  AddWriteWord(0x804000, 0x805FFF, NULL, RAM+0x016000);		// SCREEN RAM
  AddWriteWord(0x806000, 0x806FFF, tc0100scn_0_gfx_fg0_ww, NULL);	// FG0 GFX RAM
  AddWriteWord(0x820000, 0x82000F, NULL, RAM+0x020020);		// SCROLL RAM
  AddWriteBW(0xA00000, 0xA0001F, NULL, RAM+0x020030);		// Priorities
  TC0360PRI_regs = RAM+0x020030;
  AddWriteWord(0xB00000, 0xB0001F, tc0220ioc_ww, NULL);		// INPUT RAM
}

static void load_driftout() {
  // 68000 Speed Hack
  // ----------------
  WriteWord68k(&ROM[0x0724],0x13FC);	//	move.b	#$00,$AA0000
  WriteWord68k(&ROM[0x0726],0x0000);	//	move.b	#$00,$AA0000
  WriteWord68k(&ROM[0x0728],0x00AA);	//
  WriteWord68k(&ROM[0x072a],0x0000);	//	move.b	#$00,$AA0000
  WriteWord68k(&ROM[0x072C],0x6100-14);	//	bra.s	<loop>

  load_common_driftout();

  AddTaitoYM2610(0x023A, 0x01BA, 0x10000);

  AddReadByte(0x200000, 0x200003, tc0140syt_read_main_68k, NULL);	// SOUND COMM
  AddWriteByte(0x200000, 0x200003, tc0140syt_write_main_68k, NULL);	// SOUND COMM

  AddReadByte(0x000000, 0xFFFFFF, DefBadReadByte, NULL);		// <Bad Reads>
  AddReadByte(-1, -1, NULL, NULL);
  AddReadWord(0x000000, 0xFFFFFF, DefBadReadWord, NULL);		// <Bad Reads>
  AddReadWord(-1, -1,NULL, NULL);
  AddWriteByte(0x000000, 0xFFFFFF, DefBadWriteByte, NULL);		// <Bad Writes>
  AddWriteByte(-1, -1, NULL, NULL);
  AddWriteWord(0x000000, 0xFFFFFF, DefBadWriteWord, NULL);		// <Bad Writes>
  AddWriteWord(-1, -1, NULL, NULL);

  AddInitMemory();	// Set Starscream mem pointers...
}

static void load_driveout() {
  UINT8 *z80_ram;
  WriteWord68k(&ROM[0x0700],0x13FC);	//	move.b	#$00,$AA0000
  WriteWord68k(&ROM[0x0702],0x0000);	//	move.b	#$00,$AA0000
  WriteWord68k(&ROM[0x0704],0x00AA);	//
  WriteWord68k(&ROM[0x0706],0x0000);	//	move.b	#$00,$AA0000
  WriteWord68k(&ROM[0x0708],0x6100-14);	//	bra.s	<loop>

  load_common_driftout();

  z80_ram = RAM + 0x32120;

  AddZ80AROMBase(Z80ROM, 0x0038, 0x0066);

  AddZ80AReadByte(0x0000, 0x7FFF, NULL,	Z80ROM);			// BANK ROM
  AddZ80AReadByte(0x8000, 0x87FF, NULL, z80_ram);	// Z80 RAM
  AddZ80AReadByte(0x9800, 0x9801, OKIM6295_status_0_r, NULL);
  AddZ80AReadByte(0xa000, 0xa000, driveout_sound_command_r, NULL);
  AddZ80AReadByte(0x0000, 0xFFFF, DefBadReadZ80,		NULL);
  AddZ80AReadByte(    -1,     -1, NULL, 			NULL);

  AddZ80AWriteByte(0x8000, 0x87ff, NULL, z80_ram);	// Z80 RAM
  AddZ80AWrite(0x9000,0x9000,oki_bank_w, NULL);
  AddZ80AWrite(0x9800,0x9800,OKIM6295_data_0_w,NULL);
  AddZ80AWriteByte(0x0000, 0xFFFF, DefBadWriteZ80,		NULL);
  AddZ80AWriteByte(    -1,     -1, NULL,			NULL);

  AddZ80AReadPort(0x00, 0xFF, DefBadReadZ80,		NULL);
  AddZ80AReadPort(  -1,   -1, NULL,			NULL);

  // AddZ80AWritePort(0xAA, 0xAA, StopZ80Mode2,		NULL);
  AddZ80AWritePort(0x00, 0xFF, DefBadWriteZ80,		NULL);
  AddZ80AWritePort(  -1,   -1, NULL,			NULL);

  AddZ80AInit();

  // AddReadByte(0x200000, 0x200003, tc0140syt_read_main_68k, NULL);	// SOUND COMM
  AddWriteByte(0x200000, 0x200003, driveout_sound_command_wb, NULL);	// SOUND COMM

  AddReadByte(0x000000, 0xFFFFFF, DefBadReadByte, NULL);		// <Bad Reads>
  AddReadByte(-1, -1, NULL, NULL);
  // AddReadWord(0x000000, 0xFFFFFF, DefBadReadWord, NULL);		// <Bad Reads>
  AddReadWord(-1, -1,NULL, NULL);
  AddWriteByte(0x000000, 0xFFFFFF, DefBadWriteByte, NULL);		// <Bad Writes>
  AddWriteByte(-1, -1, NULL, NULL);
  // AddWriteWord(0x000000, 0xFFFFFF, DefBadWriteWord, NULL);		// <Bad Writes>
  AddWriteWord(-1, -1, NULL, NULL);

  AddInitMemory();	// Set Starscream mem pointers...
}

static void load_ssi() {
  WriteWord68k(&ROM[0x01FB4],0x4EF9);
  WriteLong68k(&ROM[0x01FB6],0x000000C0);

  WriteWord68k(&ROM[0x000C0],0x4EB9);		//	jsr	<???>
  if(is_current_game("ssi"))
    WriteLong68k(&ROM[0x000C2],0x0000932A);
  else
    WriteLong68k(&ROM[0x000C2],0x0000929A);

  WriteWord68k(&ROM[0x000C6],0x4EB9);		//	jsr	<random gen>
  WriteLong68k(&ROM[0x000C8],0x00001BE6);

  WriteLong68k(&ROM[0x000CC],0x13FC0000);	//	move.b	#$00,$AA0000
  WriteLong68k(&ROM[0x000D0],0x00AA0000);

  WriteWord68k(&ROM[0x000D4],0x6100-0x16);	//	bra.s	<loop>

  if(is_current_game("ssi"))
    {
      WriteLong68k(&ROM[0x09CFC],0x00000100);

      WriteWord68k(&ROM[0x00100],0x4EB9);
      WriteLong68k(&ROM[0x00102],0x0000B3E6);
      WriteLong68k(&ROM[0x00106],0x13FC0000);	//	move.b	#$00,$AA0000
      WriteLong68k(&ROM[0x0010A],0x00AA0000);
      WriteWord68k(&ROM[0x0010E],0x4E75);
    }
  else
    {
      WriteLong68k(&ROM[0x09C6C],0x00000100);

      WriteWord68k(&ROM[0x00100],0x4EB9);
      WriteLong68k(&ROM[0x00102],0x0000B41C);
      WriteLong68k(&ROM[0x00106],0x13FC0000);	//	move.b	#$00,$AA0000
      WriteLong68k(&ROM[0x0010A],0x00AA0000);
      WriteWord68k(&ROM[0x0010E],0x4E75);
    }

  load_common();

  /*-----[Sound Setup]-----*/

  AddTaitoYM2610(0x0217, 0x018F, 0x10000);

  RAM_OBJECT = RAM+0x10000;
  RAM_INPUT  = RAM+0x32100;
  GFX_FG0    = RAM+0x3C000;

  set_colour_mapper(&col_map_rrrr_gggg_bbbb_xxxx);
  InitPaletteMap(RAM+0x20000, 0x100, 0x10, 0x1000);

  setup_gfx();
  tc0220ioc.RAM  = RAM_INPUT;
  tc0220ioc.ctrl = 0;		//TC0220_STOPCPU;
  reset_tc0220ioc();

  tc0200obj.ofs_x	= 10;

  /*
   *  StarScream Stuff follows
   */

  AddReadByte(0x200000, 0x20FFFF, NULL, RAM+0x000000);			// 68000 RAM
  AddReadByte(0x100000, 0x10000F, NULL, RAM_INPUT);			// INPUT
  AddReadByte(0x400000, 0x400003, tc0140syt_read_main_68k, NULL);	// SOUND COMM
  AddReadByte(0x800000, 0x80FFFF, NULL, RAM_OBJECT);			// SPRITE
  AddReadByte(0x000000, 0xFFFFFF, DefBadReadByte, NULL);		// <Bad Reads>
  AddReadByte(-1, -1, NULL, NULL);

  AddReadWord(0x200000, 0x20FFFF, NULL, RAM+0x000000);			// 68000 RAM
  AddReadWord(0x800000, 0x80FFFF, NULL, RAM_OBJECT);			// SPRITE
  AddReadWord(0x300000, 0x301FFF, NULL, RAM+0x020000);			// COLOR RAM
  AddReadWord(0x100000, 0x10000F, NULL, RAM_INPUT);			// INPUT
  AddReadWord(0x000000, 0xFFFFFF, DefBadReadWord, NULL);		// <Bad Reads>
  AddReadWord(-1, -1,NULL, NULL);

  AddWriteByte(0x200000, 0x20FFFF, NULL, RAM+0x000000);		// 68000 RAM
  AddWriteByte(0x800000, 0x80FFFF, NULL, RAM_OBJECT);			// SPRITE
  AddWriteByte(0x400000, 0x400003, tc0140syt_write_main_68k, NULL);	// SOUND COMM
  AddWriteByte(0x500000, 0x5000FF, NULL, RAM+0x022200);		// ???
  AddWriteByte(0x100000, 0x10000F, tc0220ioc_wb, NULL);		// INPUT
  AddWriteByte(0xAA0000, 0xAA0001, Stop68000, NULL);			// Trap Idle 68000
  AddWriteByte(0x000000, 0xFFFFFF, DefBadWriteByte, NULL);		// <Bad Writes>
  AddWriteByte(-1, -1, NULL, NULL);

  AddWriteWord(0x200000, 0x20FFFF, NULL, RAM+0x000000);		// 68000 RAM
  AddWriteWord(0x800000, 0x80FFFF, NULL, RAM_OBJECT);			// SPRITE
  AddWriteWord(0x300000, 0x301FFF, NULL, RAM+0x020000);		// COLOR RAM
  AddWriteWord(0x500000, 0x5000FF, NULL, RAM+0x022200);		// ???
  AddWriteWord(0x100000, 0x10000F, tc0220ioc_ww, NULL);		// INPUT
  AddWriteWord(0x000000, 0xFFFFFF, DefBadWriteWord, NULL);		// <Bad Writes>
  AddWriteWord(-1, -1, NULL, NULL);

  AddInitMemory();	// Set Starscream mem pointers...
}

static void LoadMegaBlast(void)
{
  ROM[0x00628]=0x4E;
  ROM[0x00629]=0x71;
  ROM[0x0062A]=0x4E;
  ROM[0x0062B]=0x71;

  ROM[0x0770]=0x13;		// move.b #$00,$AA0000
  ROM[0x0771]=0xFC;		// Speed Hack
  ROM[0x0772]=0x00;
  ROM[0x0773]=0x00;
  ROM[0x0774]=0x00;
  ROM[0x0775]=0xAA;
  ROM[0x0776]=0x00;
  ROM[0x0777]=0x00;

  ROM[0x0778]=0x60;
  ROM[0x0779]=0x100-(6+10);

  load_common();

  AddTaitoYM2610(0x023A, 0x01BA, 0x10000);

  /*-----------------------*/

  RAM_VIDEO  = RAM+0x18000;
  RAM_SCROLL = RAM+0x3C100;
  RAM_OBJECT = RAM+0x10000;
  RAM_INPUT  = RAM+0x3C000;
  GFX_FG0    = RAM+0x3c200;

  set_colour_mapper(&col_map_rrrr_gggg_bbbb_xxxx);
  InitPaletteMap(RAM+0x3A000, 0x100, 0x10, 0x1000);

  setup_gfx();

  // Init tc0220ioc emulation
  // ------------------------

  tc0100scn[0].layer[0].scr_x	=19;
  tc0100scn[0].layer[1].scr_x	=19; // -80 for f2demo !
  tc0100scn[0].layer[2].scr_x	=19;
  tc0200obj.ofs_x	= -3+6; // ???

  tc0220ioc.RAM  = RAM_INPUT;
  tc0220ioc.ctrl = 0;		//TC0220_STOPCPU;
  reset_tc0220ioc();

  // Init tc0100scn emulation
  // ------------------------

  init_tc0100scn(0);
  tc0100scn_0_copy_gfx_fg0(ROM+0x10FC2, 0x1000);

  /*
   *  StarScream Stuff follows
   */

  AddRWBW(0x200000, 0x20FFFF, NULL, RAM+0x000000);			// 68000 RAM
  AddRWBW(0x600000, 0x61FFFF, NULL, RAM_VIDEO);			// SCREEN RAM

  AddReadBW(0x120000, 0x12000F, NULL, RAM_INPUT);			// INPUT
  AddReadByte(0x100000, 0x100003, tc0140syt_read_main_68k, NULL);	// SOUND COMM
  AddRWBW(0x180000, 0x180FFF, NULL, RAM+0x038000);			// C-CHIP (HACKED?)
  AddReadByte(0x000000, 0xFFFFFF, DefBadReadByte, NULL);		// <Bad Reads>
  AddReadByte(-1, -1, NULL, NULL);

  AddReadWord(0x800000, 0x807FFF, NULL, RAM_OBJECT);			// SPRITE RAM
  AddReadWord(0x300000, 0x301FFF, NULL, RAM+0x03A000);			// COLOR RAM
  AddReadWord(0x000000, 0xFFFFFF, DefBadReadWord, NULL);		// <Bad Reads>
  AddReadWord(-1, -1,NULL, NULL);

  AddWriteByte(0x800000, 0x807FFF, NULL, RAM_OBJECT);			// SPRITE RAM
  AddWriteByte(0x400000, 0x4000FF, NULL, RAM+0x039000);		// ???
  AddWriteByte(0x100000, 0x100003, tc0140syt_write_main_68k, NULL);	// SOUND COMM
  AddWriteByte(0x120000, 0x12000F, tc0220ioc_wb, NULL);		// INPUT
  AddWriteByte(0xAA0000, 0xAA0001, Stop68000, NULL);			// Trap Idle 68000
  AddWriteByte(0x000000, 0xFFFFFF, DefBadWriteByte, NULL);		// <Bad Writes>
  AddWriteByte(-1, -1, NULL, NULL);

  AddWriteWord(0x200000, 0x20FFFF, NULL, RAM+0x000000);		// 68000 RAM
  AddWriteWord(0x800000, 0x807FFF, NULL, RAM_OBJECT);			// SPRITE RAM
  AddWriteWord(0x300000, 0x301FFF, NULL, RAM+0x03A000);		// COLOR RAM
  AddWriteWord(0x620000, 0x6200FF, NULL, RAM+0x03C100);		// SCROLL RAM
  AddWriteWord(0x400000, 0x4000FF, NULL, RAM+0x039000);		// ???
  AddWriteWord(0x120000, 0x12000F, tc0220ioc_ww, NULL);		// INPUT
  AddWriteWord(0x000000, 0xFFFFFF, DefBadWriteWord, NULL);		// <Bad Writes>
  AddWriteWord(-1, -1, NULL, NULL);

  AddInitMemory();	// Set Starscream mem pointers...
  TC0360PRI_regs = RAM+0x039000;
}

static void LoadLiquidKids(void)
{
   // 68000 Speed Hack
   // ----------------

   WriteLong68k(&ROM[0x89F6],0x4E714E71);
   WriteLong68k(&ROM[0x89FA],0x4E714E71);

   // Fix ROM Checksum
   // ----------------

   WriteWord68k(&ROM[0x1DAE],0x4E75);

   // Fix Long Sound Wait
   // -------------------

   WriteWord68k(&ROM[0x017A],0x4E71);

   load_common();

   if (load_region[REGION_CPU2]) {
     setup_z80_frame(CPU_Z80_0,CPU_FRAME_MHz(4.2,60));
     AddTaitoYM2610(0x01DD, 0x0189, 0x10000);
   }

   /*-----------------------*/

   RAM_VIDEO  = RAM+0x10000;
   RAM_SCROLL = RAM+0x32000;
   RAM_OBJECT = RAM+0x20000;
   RAM_INPUT  = RAM+0x32100;
   GFX_FG0    = RAM+0x34000;

   setup_gfx();
   set_colour_mapper(&col_map_rrrr_gggg_bbbb_xxxx);
   InitPaletteMap(RAM+0x30000, 0x100, 0x10, 0x1000);

   // Init tc0220ioc emulation
   // ------------------------

   tc0220ioc.RAM  = RAM_INPUT;
   tc0220ioc.ctrl = TC0220_STOPCPU;
   reset_tc0220ioc();

   // Init tc0100scn emulation
   // ------------------------

   // tc0100scn[0].layer[0].scr_x	=20;

   // tc0100scn[0].layer[1].scr_x	=20; // -80 for f2demo !

   // tc0100scn[0].layer[2].scr_x	=20;
   // tc0100scn[0].layer[2].scr_y	=24;
   init_tc0100scn(0);

   // Init tc0200obj emulation
   // ------------------------

   tc0200obj.RAM_B	= RAM_OBJECT+0x8000;
   tc0200obj.ofs_x	= -3+7; // ???

/*
 *  StarScream Stuff follows
 */

   AddReadByte(0x100000, 0x10FFFF, NULL, RAM+0x000000);			// 68000 RAM
   AddReadByte(0x800000, 0x80FFFF, NULL, RAM_VIDEO);			// SCREEN RAM
   AddReadByte(0x900000, 0x90FFFF, NULL, RAM_OBJECT);			// OBJECT RAM
   AddReadByte(0x300000, 0x30001F, NULL, RAM_INPUT);			// INPUT
   AddReadByte(0x320000, 0x320003, tc0140syt_read_main_68k, NULL);	// SOUND COMM
   AddReadByte(0x400000, 0x40000F, NULL, RAM+0x032200);			// ???
   AddReadByte(0x000000, 0xFFFFFF, DefBadReadByte, NULL);		// <Bad Reads>
   AddReadByte(-1, -1, NULL, NULL);

   AddReadWord(0x100000, 0x10FFFF, NULL, RAM+0x000000);			// 68000 RAM
   AddReadWord(0x800000, 0x80FFFF, NULL, RAM_VIDEO);			// SCREEN RAM
   AddReadWord(0x900000, 0x90FFFF, NULL, RAM_OBJECT);			// OBJECT RAM
   AddReadWord(0x200000, 0x201FFF, NULL, RAM+0x030000);			// COLOR RAM
   AddReadWord(0x300000, 0x30001F, NULL, RAM_INPUT);			// INPUT
   AddReadWord(0x000000, 0xFFFFFF, DefBadReadWord, NULL);		// <Bad Reads>
   AddReadWord(-1, -1,NULL, NULL);

   AddWriteByte(0x100000, 0x10FFFF, NULL, RAM+0x000000);		// 68000 RAM
   AddWriteByte(0x806000, 0x806FFF, tc0100scn_0_gfx_fg0_wb, NULL);	// FG0 GFX RAM
   AddWriteByte(0x800000, 0x80FFFF, NULL, RAM_VIDEO);			// SCREEN RAM
   AddWriteByte(0x900000, 0x90FFFF, NULL, RAM_OBJECT);			// OBJECT RAM
   AddWriteByte(0x300000, 0x30001F, tc0220ioc_wb, NULL);		// INPUT
   AddWriteByte(0x400000, 0x40000F, NULL, RAM+0x032200);		// ???
   AddWriteByte(0x320000, 0x320003, tc0140syt_write_main_68k, NULL);	// SOUND COMM
   AddWriteByte(0xAA0000, 0xAA0001, Stop68000, NULL);			// Trap Idle 68000
   AddWriteBW(0xB00000, 0xB000FF, NULL, RAM+0x032300);		// ???
   AddWriteByte(0x000000, 0xFFFFFF, DefBadWriteByte, NULL);		// <Bad Writes>
   AddWriteByte(-1, -1, NULL, NULL);

   AddWriteWord(0x100000, 0x10FFFF, NULL, RAM+0x000000);		// 68000 RAM
   AddWriteWord(0x806000, 0x806FFF, tc0100scn_0_gfx_fg0_ww, NULL);	// FG0 GFX RAM
   AddWriteWord(0x800000, 0x80FFFF, NULL, RAM_VIDEO);			// SCREEN RAM
   AddWriteWord(0x900000, 0x90FFFF, NULL, RAM_OBJECT);			// OBJECT RAM
   AddWriteWord(0x200000, 0x201FFF, NULL, RAM+0x030000);		// COLOR RAM
   AddWriteWord(0x300000, 0x30001F, tc0220ioc_ww, NULL);		// INPUT
   AddWriteWord(0x820000, 0x82000F, NULL, RAM_SCROLL);			// SCROLL RAM
   AddWriteWord(0x380000, 0x38000F, NULL, RAM+0x032180);		// ???
   AddWriteWord(0x600000, 0x60000F, NULL, RAM+0x032280);		// ???
   // AddWriteWord(0x000000, 0xFFFFFF, BadWriteWord, NULL);		// <Bad Writes>
   AddWriteWord(-1, -1, NULL, NULL);

   AddInitMemory();	// Set Starscream mem pointers...
   TC0360PRI_regs = RAM+0x032300;
}

void clear_taito_f2(void)
{
   RemoveTaitoYM2610();
}

static int last_scr,last_scr2;

void ExecuteDonDokoDonFrame(void)
{
   tc0005rot_set_bitmap();

   last_scr=ReadLong(&RAM_OBJECT[0x24]); // Keep Sprites and Scrolling in sync (sprites are 1 frame behind)

   //print_ingame(60,"%04x",ReadWord(&RAM_OBJECT[0x0A]));

   cpu_execute_cycles(CPU_68K_0, CPU_FRAME_MHz(12,60));	// M68000 12MHz (60fps)
   cpu_interrupt(CPU_68K_0, 5);
   cpu_execute_cycles(CPU_68K_0, CPU_FRAME_MHz(12,60));	// M68000 12MHz (60fps)
   cpu_interrupt(CPU_68K_0, 6);

   Taito2610_Frame();			// Z80 and YM2610

   tc0005rot_unset_bitmap();
}

static void ExecuteDriftOutFrame(void)
{
   tc0005rot_set_bitmap();

   cpu_execute_cycles(CPU_68K_0, CPU_FRAME_MHz(12,60));	// M68000 12MHz (60fps)
   cpu_interrupt(CPU_68K_0, 6);
   cpu_execute_cycles(CPU_68K_0,500);
   cpu_interrupt(CPU_68K_0, 5);

   Taito2610_Frame();			// Z80 and YM2610

   tc0005rot_unset_bitmap();
}

static void ExecuteDriveOutFrame(void)
{
  // rotation frame, no sprites delay
   tc0005rot_set_bitmap();

   cpu_execute_cycles(CPU_68K_0, CPU_FRAME_MHz(12,60));	// M68000 12MHz (60fps)
   cpu_interrupt(CPU_68K_0, 5);
   cpu_execute_cycles(CPU_68K_0, 500);	// M68000 12MHz (60fps)
   cpu_interrupt(CPU_68K_0, 6);

   cpu_execute_cycles(CPU_Z80_0, (4000000/60));			// Sound Z80
   tc0005rot_unset_bitmap();
}

static void ExecuteLiquidKidsFrame(void)
{
   last_scr=ReadLong(&RAM_OBJECT[0x24]); // Keep Sprites and Scrolling in sync (sprites are 1 frame behind)

   cpu_execute_cycles(CPU_68K_0, CPU_FRAME_MHz(12,60));	// M68000 12MHz (60fps)
   cpu_interrupt(CPU_68K_0, 5);
   cpu_interrupt(CPU_68K_0, 6);
   cpu_execute_cycles(CPU_68K_0, CPU_FRAME_MHz(12,60));	// M68000 12MHz (60fps)
   Taito2610_Frame();			// Z80 and YM2610
}

static void execute_demo_frame(void)
{
  // the demo appears too much on the right, but it's based on mame source...
  // It's funny there is such a difference, because we are very similar for the games.
  UINT16 *scroll = (UINT16*)RAM_SCROLL+1;
   last_scr=ReadLong(&RAM_OBJECT[0x24]); // Keep Sprites and Scrolling in sync (sprites are 1 frame behind)

   cpu_execute_cycles(CPU_68K_0, CPU_FRAME_MHz(12,60));	// M68000 12MHz (60fps)
   cpu_interrupt(CPU_68K_0, 5);
   cpu_execute_cycles(CPU_68K_0, CPU_FRAME_MHz(12,60));	// M68000 12MHz (60fps)
   cpu_interrupt(CPU_68K_0, 6);

   if ((RAM[0x32104] & 8) == 0)
     scroll[1]--;
   else if ((RAM[0x32104] & 4) == 0)
     scroll[1]++;
   else if ((RAM[0x32104] & 1) == 0) // up
     scroll[4]++;
   else if ((RAM[0x32104] & 2) == 0) // up
     scroll[4]--;
}

static void ExecuteMegaBlastFrame(void)
{
  RAM[0x38802]=0x01; // c chip for megab

   cpu_execute_cycles(CPU_68K_0, CPU_FRAME_MHz(12,60));	// M68000 12MHz (60fps)
   cpu_interrupt(CPU_68K_0, 5);
   cpu_execute_cycles(CPU_68K_0, 500);	// M68000 12MHz (60fps)
   cpu_interrupt(CPU_68K_0, 6);

   Taito2610_Frame();			// Z80 and YM2610
}

static void execute_super_space_invaders_frame(void)
{
   cpu_execute_cycles(CPU_68K_0, CPU_FRAME_MHz(12,60));	// M68000 12MHz (60fps)
   cpu_interrupt(CPU_68K_0, 5);
   cpu_execute_cycles(CPU_68K_0, CPU_FRAME_MHz(6,60));	// Overflow for sprite sync

   Taito2610_Frame();			// Z80 and YM2610
}

static void execute_f2_simple(void)
{
  // no delay, no c chip, no rotating tile !
   cpu_execute_cycles(CPU_68K_0, CPU_FRAME_MHz(12,60));	// M68000 12MHz (60fps)
   cpu_interrupt(CPU_68K_0, 5);
   cpu_execute_cycles(CPU_68K_0, 500);	// M68000 12MHz (60fps)
   cpu_interrupt(CPU_68K_0, 6);

   Taito2610_Frame();			// Z80 and YM2610
}

static void execute_f2_spritebank(void)
{
  // no delay, no c chip, no rotating tile, but sprite banks...
   cpu_execute_cycles(CPU_68K_0, CPU_FRAME_MHz(12,60));	// M68000 12MHz (60fps)
   cpu_interrupt(CPU_68K_0, 5);
   cpu_execute_cycles(CPU_68K_0, 500);	// M68000 12MHz (60fps)
   cpu_interrupt(CPU_68K_0, 6);

   Taito2610_Frame();			// Z80 and YM2610
   make_object_bank(GFX_BANK);
}

static void execute_f2_spritebank_b(void)
{
  // no delay, no c chip, no rotating tile, but sprite banks...
   cpu_execute_cycles(CPU_68K_0, CPU_FRAME_MHz(12,60));	// M68000 12MHz (60fps)
   cpu_interrupt(CPU_68K_0, 5);
   cpu_execute_cycles(CPU_68K_0, CPU_FRAME_MHz(12,60));	// M68000 12MHz (60fps)
   cpu_interrupt(CPU_68K_0, 6);

   Taito2610_Frame();			// Z80 and YM2610
   make_object_bank(GFX_BANK);
}

static void ExecuteGunFrontFrame(void)
{
  // 68k overclocked because of speed hacks, 2 execute_cycles / frame for sprite sync
   cpu_execute_cycles(CPU_68K_0, CPU_FRAME_MHz(20,60) - s68000context.odometer);
   cpu_interrupt(CPU_68K_0, 6);
   cpu_interrupt(CPU_68K_0, 5);
   s68000context.odometer = 0;
   cpu_execute_cycles(CPU_68K_0, CPU_FRAME_MHz(20,60));

   Taito2610_Frame();			// Z80 and YM2610
}

void execute_camel_try_frame(void)
{
   static int p1_paddle_x,p1_paddle_y;
   static int p2x,p2y;
   int px,py;

   p1_paddle_x=p1_paddle_x/2;
   p1_paddle_y=p1_paddle_y/2;
   p2x=p2x/2;
   p2y=p2y/2;

   /*------[Mouse Hack]-------*/

   GetMouseMickeys(&px,&py);

   p1_paddle_x+=px/4;
   p1_paddle_y+=py/4;

   if(*MouseB&1){RAM_INPUT[0x04]&=0x10^255;}

   if((RAM_INPUT[0x20]!=0)&&(p1_paddle_x> -0x40)) p1_paddle_x-=0x08;
   if((RAM_INPUT[0x21]!=0)&&(p1_paddle_x<  0x3F)) p1_paddle_x+=0x08;

   if((RAM_INPUT[0x30]!=0)&&(p2x> -0x40)) p2x-=0x08;
   if((RAM_INPUT[0x31]!=0)&&(p2x<  0x3F)) p2x+=0x08;

   WriteWord(&RAM_INPUT[0x18],p1_paddle_x);
   WriteWord(&RAM_INPUT[0x1C],p2x);

   tc0005rot_set_bitmap();

   cpu_execute_cycles(CPU_68K_0, CPU_FRAME_MHz(12,60));	// M68000 12MHz (60fps)
   cpu_interrupt(CPU_68K_0, 6);
   cpu_interrupt(CPU_68K_0, 5);

   execute_z80_audio_frame();

   tc0005rot_unset_bitmap();
}

#define TC0360PRI_regs_lsb(x) TC0360PRI_regs[(x)*2]
#define TC0360PRI_regs_msb(x) TC0360PRI_regs[(x)*2+1]

static void draw_f2_pri_rot_delay(void)
{
  int ta, num,pri_mask=0;
  UINT8 rozpri;
   ClearPaletteMap();

   if (!init_gfx)
     finish_setup_gfx();

   for (ta=0; ta<8; ta++)
     layer[ta].num = ta;

   layer[0].pri = TC0360PRI_regs_lsb(5) & 0x0f; // tc100scn BG0 (or flipped)
   layer[1].pri = TC0360PRI_regs_lsb(5) >> 4; // tc100scn BG1 (or flipped)
   layer[2].pri = TC0360PRI_regs_lsb(4) >> 4; // tc100scn FG0
   rozpri = (TC0360PRI_regs_lsb(1) & 0xc0) >> 6;
   rozpri = (TC0360PRI_regs_lsb(8 + rozpri/2) >> 4*(rozpri & 1)) & 0x0f;
   layer[3].pri = rozpri;
   layer[4].pri = TC0360PRI_regs_lsb(6) & 0x0f; // sprites colors 00
   layer[5].pri = TC0360PRI_regs_lsb(6) >> 4;   // sprites colors 01
   layer[6].pri = TC0360PRI_regs_lsb(7) & 0x0f; // sprites colors 10
   layer[7].pri = TC0360PRI_regs_lsb(7) >> 4;   // sprites colors 11
   qsort(&layer,8,sizeof(struct layer),layer_cmp);

   // Init tc0100scn emulation
   // ------------------------

   tc0100scn[0].ctrl = ReadWord(RAM_SCROLL+12);

   /* Notice : below there is a hack to try to do without a priority bitmap
    * but it doesn't work well for pulirula.
    * But adding a priority bitmap would require to use it for all the games
    * using tc005rot, tc200obj or tc100scn, which is a really big mess.
    * So for now I prefer to leave it as it is... ! */

   if(RefreshBuffers){
      tc0005rot_refresh_buffer();
   }

   last_scr2=ReadLong(&RAM_OBJECT[0x24]);	// [Store]
   WriteLong(&RAM_OBJECT[0x24],last_scr);	// Delay Scrolling 1 frame
   for (ta=0; ta<8; ta++) {
     num = layer[ta].num;
     if (ta == 0 && num > 3)
       clear_game_screen(0);
     switch(num) {
     case 0:
     case 1:
     case 2:
       if (pri_mask) {
	 render_tc0200obj_mapped(pri_mask);
	 pri_mask = 0;
       }
       render_tc0100scn_layer_mapped(0,num,ta>0);
       break;
     case 3:
       tc0005rot_draw_rot((ReadWord(&TC0360PRI_regs[2])&0x3F)<<2,ta>0);
       break;
     default:
       pri_mask |= (1<<(num & 3));
     }
   }
   if (pri_mask) {
     render_tc0200obj_mapped(pri_mask);
   } 
   WriteLong(&RAM_OBJECT[0x24],last_scr2);	// [Restore]
}

// Compared to dondokod it does not have the 1 frame delay for the sprites
// and it does not have bg0/bg1.
static void draw_cameltry(void)
{
  int ta, num,pri_mask=0;
  UINT8 rozpri;
   ClearPaletteMap();

   if (!init_gfx)
     finish_setup_gfx();

   for (ta=0; ta<6; ta++)
     layer[ta].num = ta;

   rozpri = (TC0360PRI_regs_lsb(1) & 0xc0) >> 6;
   rozpri = (TC0360PRI_regs_lsb(8 + rozpri/2) >> 4*(rozpri & 1)) & 0x0f;
   layer[0].pri = TC0360PRI_regs_lsb(6) & 0x0f; // sprites colors 00
   layer[1].pri = TC0360PRI_regs_lsb(6) >> 4;   // sprites colors 01
   layer[2].pri = TC0360PRI_regs_lsb(7) & 0x0f; // sprites colors 10
   layer[3].pri = TC0360PRI_regs_lsb(7) >> 4;   // sprites colors 11
   layer[4].pri = rozpri;
   layer[5].pri = TC0360PRI_regs_lsb(4) >> 4; // tc100scn FG0
   qsort(&layer,6,sizeof(struct layer),layer_cmp);

   // Init tc0100scn emulation
   // ------------------------

   tc0100scn[0].ctrl = ReadWord(RAM_SCROLL+12);

   if(RefreshBuffers){
      tc0005rot_refresh_buffer();
   }
   if (layer[0].num != 4) {
     UINT8 *map;
     MAP_PALETTE_MAPPED_NEW(
            0,
              16,
              map
            );
     clear_game_screen(ReadLong(&map[0]));
   }

   for (ta=0; ta<6; ta++) {
     num = layer[ta].num;
     switch(num) {
     case 5:
       if (pri_mask) {
	 render_tc0200obj_mapped(pri_mask);
	 pri_mask = 0;
       }
       render_tc0100scn_layer_mapped(0,2,ta>0);
       break;
     case 4:
       if (pri_mask) {
	 render_tc0200obj_mapped(pri_mask);
	 pri_mask = 0;
       }
       tc0005rot_draw_rot((ReadWord(&TC0360PRI_regs[2])&0x3F)<<2,ta>0);
       break;
     default:
       pri_mask |= (1<<(num & 3));
     }
   }
   if (pri_mask) {
     render_tc0200obj_mapped(pri_mask);
   }
}

static void draw_f2_pri_rot(void)
{
  int ta, num,pri_mask=0;
  UINT8 rozpri,*map;
   ClearPaletteMap();

   if (!init_gfx)
     finish_setup_gfx();

   for (ta=0; ta<6; ta++)
     layer[ta].num = ta;

   layer[5].pri = TC0360PRI_regs_msb(4) >> 4; // tc100scn FG0
   rozpri = (TC0360PRI_regs_msb(1) & 0xc0) >> 6;
   rozpri = (TC0360PRI_regs_msb(8 + rozpri/2) >> 4*(rozpri & 1)) & 0x0f;
   layer[4].pri = rozpri;
   layer[0].pri = TC0360PRI_regs_msb(6) & 0x0f; // sprites colors 00
   layer[1].pri = TC0360PRI_regs_msb(6) >> 4;   // sprites colors 01
   layer[2].pri = TC0360PRI_regs_msb(7) & 0x0f; // sprites colors 10
   layer[3].pri = TC0360PRI_regs_msb(7) >> 4;   // sprites colors 11
   qsort(&layer,6,sizeof(struct layer),layer_cmp);

   // Init tc0100scn emulation
   // ------------------------

   tc0100scn[0].ctrl = ReadWord(RAM_SCROLL+12);

   if(RefreshBuffers){
      tc0005rot_refresh_buffer();
   }

   // in fact the priorities in drift out never place the rotating layer as the first
   // layer (at least nowhere in the attract mode nor in the 1st race). So no need to
   // bother : just clear the screen then draw a transparant rotating layer.
   if (layer[0].num < 4) {
     MAP_PALETTE_MAPPED_NEW(
            0,
              16,
              map
            );
     clear_game_screen(ReadLong(&map[0]));
   }
   for (ta=0; ta<6; ta++) {
     num = layer[ta].num;
     switch(num) {
     case 0:
     case 1:
     case 2:
     case 3:
       pri_mask |= (1<<(num));
       break;
     case 4:
       if (pri_mask) {
	 render_tc0200obj_mapped(pri_mask);
	 pri_mask = 0;
       }
       tc0005rot_draw_rot_r270((ReadWord(&TC0360PRI_regs[2])&0x3F)<<2,ta>0);
       break;
     case 5:
       if (pri_mask) {
	 render_tc0200obj_mapped(pri_mask);
	 pri_mask = 0;
       }
       render_tc0100scn_layer_mapped(0,2,ta>0);
       break;
     }
   }
   if (pri_mask) {
     render_tc0200obj_mapped(pri_mask);
   }
}

// same thing without the rotated layer (TC280GRD)
static void draw_f2_pri(void)
{
  int ta, num,pri_mask=0;
  UINT8 *map;
   ClearPaletteMap();

   if (!init_gfx)
     finish_setup_gfx();

   for (ta=0; ta<8; ta++)
     layer[ta].num = ta;

   layer[0].pri = TC0360PRI_regs_lsb(5) & 0x0f; // tc100scn BG0 (or flipped)
   layer[1].pri = TC0360PRI_regs_lsb(5) >> 4; // tc100scn BG1 (or flipped)
   layer[2].pri = TC0360PRI_regs_lsb(4) >> 4; // tc100scn FG0
   // sprites must start at 4, so layer 3 is empty
   layer[4].pri = TC0360PRI_regs_lsb(6) & 0x0f; // sprites colors 00
   layer[5].pri = TC0360PRI_regs_lsb(6) >> 4;   // sprites colors 01
   layer[6].pri = TC0360PRI_regs_lsb(7) & 0x0f; // sprites colors 10
   layer[7].pri = TC0360PRI_regs_lsb(7) >> 4;   // sprites colors 11
   qsort(&layer,8,sizeof(struct layer),layer_cmp);

   // Init tc0100scn emulation
   // ------------------------

   tc0100scn[0].ctrl = ReadWord(RAM_SCROLL+12);

   if (layer[0].num >= 4) {
     MAP_PALETTE_MAPPED_NEW(
            0,
              16,
              map
            );
     clear_game_screen(ReadLong(&map[0]));
   }
   for (ta=0; ta<8; ta++) {
     num = layer[ta].num;
     switch(num) {
     case 0:
     case 1:
     case 2:
       if (pri_mask) {
	 render_tc0200obj_mapped(pri_mask);
	 pri_mask = 0;
       }
       render_tc0100scn_layer_mapped(0,num,ta>0);
       break;
     case 3:
       // tc0005rot_draw_rot((ReadWord(&RAM[0x32302])&0x3F)<<2);
       break;
     default:
       pri_mask |= (1<<(num & 3));
     }
   }
   if (pri_mask) {
     render_tc0200obj_mapped(pri_mask);
   }
}

// And a mix between the 2
static void draw_f2_pri_delay(void)
{
  int ta, num,pri_mask=0;
  UINT8 *map;
   ClearPaletteMap();

   if (!init_gfx)
     finish_setup_gfx();

   for (ta=0; ta<8; ta++)
     layer[ta].num = ta;

   layer[0].pri = TC0360PRI_regs_lsb(5) & 0x0f; // tc100scn BG0 (or flipped)
   layer[1].pri = TC0360PRI_regs_lsb(5) >> 4; // tc100scn BG1 (or flipped)
   layer[2].pri = TC0360PRI_regs_lsb(4) >> 4; // tc100scn FG0
   // no layer 3
   layer[4].pri = TC0360PRI_regs_lsb(6) & 0x0f; // sprites colors 00
   layer[5].pri = TC0360PRI_regs_lsb(6) >> 4;   // sprites colors 01
   layer[6].pri = TC0360PRI_regs_lsb(7) & 0x0f; // sprites colors 10
   layer[7].pri = TC0360PRI_regs_lsb(7) >> 4;   // sprites colors 11
   qsort(&layer,8,sizeof(struct layer),layer_cmp);

   // Init tc0100scn emulation
   // ------------------------

   tc0100scn[0].ctrl = ReadWord(RAM_SCROLL+12);

   last_scr2=ReadLong(&RAM_OBJECT[0x24]);	// [Store]
   WriteLong(&RAM_OBJECT[0x24],last_scr);	// Delay Scrolling 1 frame
   if (layer[0].num > 1) {
     MAP_PALETTE_MAPPED_NEW(
            0,
              16,
              map
            );
     clear_game_screen(ReadLong(&map[0]));
   }
   for (ta=0; ta<8; ta++) {
     num = layer[ta].num;
     // printf("%d:%d ",num,layer[ta].pri);
     switch(num) {
     case 0:
     case 1:
     case 2:
       if (pri_mask) {
	 render_tc0200obj_mapped(pri_mask);
	 pri_mask = 0;
       }
       render_tc0100scn_layer_mapped(0,num,ta>0);
       break;
     case 3:
       break;
     default:
       pri_mask |= (1<<(num & 3));
       break;
     }
   }
   if (pri_mask) {
     render_tc0200obj_mapped(pri_mask);
   }
   WriteLong(&RAM_OBJECT[0x24],last_scr2);	// [Restore]
}

static void draw_thundfox(void)
{
  UINT8 *map;
  /* Clearly the priorities chip is not used for this game :
      - if you believe theory, a bg layer should go on top of sprites, which creates
     a big black thunder bolt during attract mode
      - Also fg priority often becomes bad, and text disappears behind sprites
    The priorities found by hand by Antiriad seem to work much better ! :-) */

   ClearPaletteMap();
   if (!init_gfx3) {
     tc0100scn[1].layer[0].GFX	=gfx3;
     tc0100scn[1].layer[0].MASK	=gfx3_solid;
     tc0100scn[1].layer[1].GFX	=gfx3;
     tc0100scn[1].layer[1].MASK	=gfx3_solid;
     tc0100scn[1].layer[0].tile_mask=
     tc0100scn[1].layer[1].tile_mask=
       max_sprites[2]-1;
     init_gfx3 = 1;

     finish_setup_gfx();
   }
   // it would be draw_no_pri if bg0 was really solid, but in attract mode
   // you get a black area if you draw bg0 as solid.
   // And you have the init of the 2 tc100scn also.
   MAP_PALETTE_MAPPED_NEW(
            0,
              16,
              map
            );
   clear_game_screen(ReadLong(&map[0]));

   // Init tc0100scn emulation
   // ------------------------

   tc0100scn[0].ctrl = ReadWord(RAM_SCROLL + 12);
   tc0100scn[1].ctrl = ReadWord(RAM_SCROLL2 + 12);

   // BG0 A
   // -----

   render_tc0100scn_layer_mapped(1,0,1);

   // BG0 B
   // -----

   render_tc0100scn_layer_mapped(0,0,1);

   // BG1 A
   // -----

   render_tc0100scn_layer_mapped(1,1,1);

   // BG1 B
   // -----

   render_tc0100scn_layer_mapped(0,1,1);

   // OBJECT
   // ------

   render_tc0200obj_mapped(255);

   // FG0 A
   // -----

   render_tc0100scn_layer_mapped(1,2,1);

   // FG0 B
   // -----

   render_tc0100scn_layer_mapped(0,2,1);
}

static void draw_ssi(void)
{
  UINT8 *map;
   ClearPaletteMap();

   if (!init_gfx)
     finish_setup_gfx();

     MAP_PALETTE_MAPPED_NEW(
            0,
              16,
              map
            );
     clear_game_screen(ReadLong(&map[0]));

   // only sprites, and apparently there are no priorities to handle
   // (because the sprites are probably grouped since there are big sprites)
   render_tc0200obj_mapped(255);
}

static void draw_no_pri() {
  ClearPaletteMap();
   if (!init_gfx)
     finish_setup_gfx();

   // Init tc0100scn emulation
   // ------------------------

   tc0100scn[0].ctrl = ReadWord(RAM_SCROLL+12);

   // BG0
   // ---

   render_tc0100scn_layer_mapped(0,0,0);

   // BG1
   // ---

   render_tc0100scn_layer_mapped(0,1,1);

   // OBJECT
   // ------

   render_tc0200obj_mapped(255);

   // FG0
   // ---

   render_tc0100scn_layer_mapped(0,2,1);
}

// and mahjong quest : no priorities at all ?!!
void draw_mjnquest(void)
{
   if(ReadWord(&GFX_BANK[0])==0){
      tc0100scn[0].layer[0].GFX  = GFX;
      tc0100scn[0].layer[0].MASK = gfx_solid[0];
      tc0100scn[0].layer[1].GFX  = GFX;
      tc0100scn[0].layer[1].MASK = gfx_solid[0];
   }
   else{
      tc0100scn[0].layer[0].GFX  = GFX + (0x8000 * 0x40);
      tc0100scn[0].layer[0].MASK = gfx_solid[0] + (0x8000);
      tc0100scn[0].layer[1].GFX  = GFX + (0x8000 * 0x40);
      tc0100scn[0].layer[1].MASK = gfx_solid[0] + (0x8000);
   }

   draw_no_pri();
}

static struct VIDEO_INFO f2_180_pri_rot =
{
   draw_f2_pri_rot_delay,
   320,
   224,
   32,
   VIDEO_ROTATE_180 | VIDEO_ROTATABLE,
   pivot_gfxdecodeinfo
};

static struct VIDEO_INFO pulirula_video =
{
   draw_f2_pri_rot_delay,
   320,
   224,
   32,
   VIDEO_ROTATE_NORMAL | VIDEO_ROTATABLE,
   pivot_gfxdecodeinfo
};

static struct VIDEO_INFO cameltry_video =
{
   draw_cameltry,
   320,
   224,
   32,
   VIDEO_ROTATE_NORMAL | VIDEO_ROTATABLE,
   pivot_gfxdecodeinfo
};

static struct VIDEO_INFO f2_180_pri_delay =
{
   draw_f2_pri_delay,
   320,
   224,
   32,
   VIDEO_ROTATE_180 | VIDEO_ROTATABLE,
   pivot_gfxdecodeinfo
};

static struct VIDEO_INFO f2_pri =
{
   draw_f2_pri,
   320,
   224,
   32,
   VIDEO_ROTATE_NORMAL | VIDEO_ROTATABLE,
   pivot_gfxdecodeinfo
};

static struct VIDEO_INFO mjnquest =
{
   draw_mjnquest,
   320,
   224,
   32,
   VIDEO_ROTATE_NORMAL | VIDEO_ROTATABLE,
   mjnquest_gfxdecodeinfo
};

static struct VIDEO_INFO thundfox =
{
   draw_thundfox,
   320,
   224,
   32,
   VIDEO_ROTATE_NORMAL | VIDEO_ROTATABLE,
   thundfox_decodeinfo
};

static struct VIDEO_INFO finalb =
{
   draw_no_pri,
   320,
   224,
   32,
   VIDEO_ROTATE_NORMAL | VIDEO_ROTATABLE,
   finalb_gfxdecodeinfo
};

static struct VIDEO_INFO majestic_twelve_video =
{
  // this one is particular : only sprites !
  draw_ssi,
   320,
   224,
   32,
   VIDEO_ROTATE_270 |
   VIDEO_ROTATABLE,
   pivot_gfxdecodeinfo
};

static struct VIDEO_INFO drift_out_video =
{
   draw_f2_pri_rot,
   320,
   224,
   32,
   VIDEO_ROTATE_270 |VIDEO_ROTATABLE,
   pivot_gfxdecodeinfo
};

static struct VIDEO_INFO gunfront =
{
   draw_f2_pri,
   320,
   224,
   32,
   VIDEO_ROTATE_270 | VIDEO_ROTATABLE,
   pivot_gfxdecodeinfo
};

GAME( don_doko_don ,
   don_doko_don_dirs,
   dondokod_roms,
   don_doko_don_inputs,
   don_doko_don_dsw,
   don_doko_don_romsw,

   LoadDonDokoDon,
   clear_taito_f2,
   &f2_180_pri_rot,
   ExecuteDonDokoDonFrame,
   "dondokdj",
   "Don Doko Don",
   "ドンドコドン",
   COMPANY_ID_TAITO,
   "B95",
   1989,
   taito_ym2610_sound,
   GAME_PLATFORM
);

// The inputs do not have the same adreses as dondokod. Maybe we should use input_buffer
// instead of a direct mapping...
static struct INPUT_INFO mega_blast_inputs[] =
{
   { KB_DEF_COIN1,        MSG_COIN1,               0x03C00E, 0x04, BIT_ACTIVE_0 },
   { KB_DEF_COIN2,        MSG_COIN2,               0x03C00E, 0x08, BIT_ACTIVE_0 },
   { KB_DEF_TILT,         MSG_TILT,                0x03C00E, 0x01, BIT_ACTIVE_0 },
   { KB_DEF_SERVICE,      MSG_SERVICE,             0x03C00E, 0x02, BIT_ACTIVE_0 },

   { KB_DEF_P1_START,     MSG_P1_START,            0x03C00E, 0x10, BIT_ACTIVE_0 },
   { KB_DEF_P1_UP,        MSG_P1_UP,               0x03C004, 0x01, BIT_ACTIVE_0 },
   { KB_DEF_P1_DOWN,      MSG_P1_DOWN,             0x03C004, 0x02, BIT_ACTIVE_0 },
   { KB_DEF_P1_LEFT,      MSG_P1_LEFT,             0x03C004, 0x04, BIT_ACTIVE_0 },
   { KB_DEF_P1_RIGHT,     MSG_P1_RIGHT,            0x03C004, 0x08, BIT_ACTIVE_0 },
   { KB_DEF_P1_B1,        MSG_P1_B1,               0x03C004, 0x10, BIT_ACTIVE_0 },
   { KB_DEF_P1_B2,        MSG_P1_B2,               0x03C004, 0x20, BIT_ACTIVE_0 },

   { KB_DEF_P2_START,     MSG_P2_START,            0x03C00E, 0x20, BIT_ACTIVE_0 },
   { KB_DEF_P2_UP,        MSG_P2_UP,               0x03C006, 0x01, BIT_ACTIVE_0 },
   { KB_DEF_P2_DOWN,      MSG_P2_DOWN,             0x03C006, 0x02, BIT_ACTIVE_0 },
   { KB_DEF_P2_LEFT,      MSG_P2_LEFT,             0x03C006, 0x04, BIT_ACTIVE_0 },
   { KB_DEF_P2_RIGHT,     MSG_P2_RIGHT,            0x03C006, 0x08, BIT_ACTIVE_0 },
   { KB_DEF_P2_B1,        MSG_P2_B1,               0x03C006, 0x10, BIT_ACTIVE_0 },
   { KB_DEF_P2_B2,        MSG_P2_B2,               0x03C006, 0x20, BIT_ACTIVE_0 },

   { 0,                   NULL,                    0,        0,    0            },
};

GAME( mega_blast ,
   mega_blast_dirs,
   megab_roms,
   mega_blast_inputs,
   mega_blast_dsw,
   mega_blast_romsw,

   LoadMegaBlast,
   clear_taito_f2,
   &f2_pri,
   ExecuteMegaBlastFrame,
   "megab",
   "Mega Blast",
   "メガブラスト",
   COMPANY_ID_TAITO,
   "C11",
   1989,
   taito_ym2610_sound,
   GAME_SHOOT
);

GAME( liquid_kids ,
   liquid_kids_dirs,
   liquidk_roms,
   don_doko_don_inputs,
   liquid_kids_dsw,
   liquidk_romsw,

   LoadLiquidKids,
   clear_taito_f2,
      &f2_180_pri_delay,
   ExecuteLiquidKidsFrame,
   "liquidk",
   "Liquid Kids",
   "ミズバクｵ蝟`険",
   COMPANY_ID_TAITO,
   "C49",
   1990,
   taito_ym2610_sound,
   GAME_PLATFORM
);

GAME( majestic_twelve ,
   majestic_twelve_dirs,
   mj12_roms,
   don_doko_don_inputs,
   ssi_dsw,
   majestic_twelve_romsw,

   load_ssi,
   clear_taito_f2,
   &majestic_twelve_video,
   execute_super_space_invaders_frame,
   "majest12",
   "Majestic Twelve",
   "マジェスティックトウェルブ",
   COMPANY_ID_TAITO,
   "C64",
   1990,
   taito_ym2610_sound,
   GAME_SHOOT
);

GAME( super_space_invaders_91 ,
   super_space_invaders_91_dirs,
   ssi_roms,
   don_doko_don_inputs,
   ssi_dsw,
   majestic_twelve_romsw,

   load_ssi,
   clear_taito_f2,
   &majestic_twelve_video,
   execute_super_space_invaders_frame,
   "ssi",
   "Super Space Invaders 91",
   "スーパースペースインベーダー’９１",
   COMPANY_ID_TAITO,
   "C64",
   1990,
   taito_ym2610_sound,
   GAME_SHOOT
);

GAME( drift_out ,
   driftout_dirs,
   driftout_roms,
   don_doko_don_inputs,
   drift_out_dsw,
   NULL,

   load_driftout,
   clear_taito_f2,
   &drift_out_video,
   ExecuteDriftOutFrame,
   "driftout",
   "Drift Out",
   "ドリフトアウト",
   COMPANY_ID_VISCO,
   NULL,
   1991,
   taito_ym2610_sound,
   GAME_RACE
);

GAME( drive_out ,
   drive_out_dirs,
   driveout_roms,
   don_doko_don_inputs,
   drift_out_dsw,
   NULL,

   load_driveout,
   clear_taito_f2,
   &drift_out_video,
   ExecuteDriveOutFrame,
   "driveout",
   "Drive Out",
   NULL,
   COMPANY_ID_BOOTLEG,
   NULL,
   1991,
   driveout_sound,
   GAME_RACE
);

GAME( dino_rex ,
   dino_rex_dirs,
   dinorex_roms,
   don_doko_don_inputs,
   dino_rex_dsw,
   don_doko_don_romsw,

   load_dinorex,
   clear_taito_f2,
   &f2_pri,
   execute_f2_simple,
   "dinorex",
   "Dino Rex",
   "ダイノレックス",
   COMPANY_ID_TAITO,
   "D39",
   1992,
   taito_ym2610_sound,
   GAME_BEAT
);

GAME( f2demo ,
   f2demo_dirs,
   f2demo_roms,
   don_doko_don_inputs,
   liquid_kids_dsw,
   mega_blast_romsw,

   LoadLiquidKids,
   clear_taito_f2,
      &f2_180_pri_delay,
   execute_demo_frame,
   "f2demo",
   "Demo Taito F2",
      NULL,
   COMPANY_ID_BOOTLEG,
   "C49",
      2000, // ???
   taito_ym2610_sound,
   GAME_MISC
);

GAME( thunder_fox ,
   thunder_fox_dirs,
   thundfox_roms,
   thunder_fox_inputs,
   thunder_fox_dsw,
   thunder_fox_romsw,

   load_thundfox,
   clear_taito_f2,
   &thundfox,
   ExecuteLiquidKidsFrame,
   "thundfox",
   "Thunder Fox",
   "サンダーフォックス",
   COMPANY_ID_TAITO,
   "C28",
   1990,
   taito_ym2610_sound,
   GAME_SHOOT
);

GAME( solitary_fighter ,
   solitary_fighter_dirs,
   solfigtr_roms,
      thunder_fox_inputs,
   solfigtr_dsw,
   solfigtr_romsw,

   load_solfigtr,
   clear_taito_f2,
   &f2_pri,
      execute_f2_spritebank,
   "solfigtr",
   "Solitary Fighter",
   "ダイノレックス",
   COMPANY_ID_TAITO,
   "C91",
   1990,
   taito_ym2610_sound,
   GAME_BEAT
);

GAME( mahjong_quest ,
   mahjong_quest_dirs,
   mjnquest_roms,
   mjnquest_inputs,
   mjnquest_dsw,
   NULL,

   load_mjnquest,
   clear_taito_f2,
   &mjnquest,
   execute_f2_simple,
   "mjnquest",
   "Mahjong Quest",
   NULL,
   COMPANY_ID_TAITO,
   "C77",
   1990,
   taito_ym2610_sound,
   GAME_PUZZLE
);

GAME( final_blow ,
   final_blow_dirs,
   finalb_roms,
   final_blow_inputs,
   final_blow_dsw,
   thunder_fox_romsw,

   load_finalb,
   clear_taito_f2,
   &finalb,
   execute_f2_simple,
   "finalb",
   "Final Blow",
   "ファイナルブロー",
   COMPANY_ID_TAITO,
   "B82",
   1988,
   taito_ym2610_sound,
   GAME_BEAT
);

GAME( growl ,
   growl_dirs,
   growl_roms,
   growl_inputs,
   growl_dsw,
   growl_romsw,

   load_growl,
   clear_taito_f2,
   &f2_pri,
   execute_f2_spritebank_b,
   "growl",
   "Growl",
   "ルナーク",
   COMPANY_ID_TAITO,
   "C74",
   1990,
   taito_ym2610_sound,
   GAME_BEAT
);

GAME( gun_frontier ,
   gun_frontier_dirs,
   gunfront_roms,
   don_doko_don_inputs,
   gunfront_dsw,
   liquidk_romsw,

   load_gunfront,
   clear_taito_f2,
   &gunfront,
   ExecuteGunFrontFrame,
   "gunfront",
   "Gun Frontier",
   "ガンフロンティア",
   COMPANY_ID_TAITO,
   "C71",
   1990,
   taito_ym2610_sound,
   GAME_SHOOT
);

GAME( cameltry ,
   cameltry_dirs,
   cameltry_roms,
   camel_try_inputs,
   camel_try_dsw,
   camel_try_romsw,

   load_cameltry,
   clear_taito_f2,
   &cameltry_video,
   execute_camel_try_frame,
   "cameltry",
   "Camel Try",
   "キャメルトライ",
   COMPANY_ID_TAITO,
   "C38",
   1989,
   taito_ym2610_sound,
   GAME_PUZZLE
);

GAME( camltrua ,
   camltrua_dirs,
   camltrua_roms,
   camel_try_inputs,
   camel_try_dsw,
   camel_try_romsw,

   load_camltrua,
   clear_taito_f2,
   &cameltry_video,
   execute_camel_try_frame,
   "camltrua",
   "Camel Try (Alternate)",
   "キャメルトライ",
   COMPANY_ID_TAITO,
   "C38",
   1989,
   camltrua_sound,
   GAME_PUZZLE
);

GAME( pulirula ,
   pulirula_dirs,
   pulirula_roms,
   final_blow_inputs,
   pulirula_dsw,
   liquidk_romsw,

   load_pulirula,
   clear_taito_f2,
   &pulirula_video,
   ExecuteDriftOutFrame,
   "pulirula",
   "Pulirula",
   "プリルラ",
   COMPANY_ID_TAITO,
   "C98",
   1991,
   taito_ym2610_sound,
   GAME_BEAT
);



#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
/*                                                                            */
/*                               LOAD/SAVE GAME                               */
/*                                                                            */
/******************************************************************************/

#include "deftypes.h"

// Save/Load Slot number (0..9)

extern int SaveSlot, UseCompression;
extern UINT8 *default_eeprom;
extern UINT16 default_eeprom_size;
extern int savegame_version;

// UseCompression : saved games compressed ?

// Attempt to Save Current Game

void GameSave(void);
void GameSaveName(void);
void do_save_state(char *name); // with a given name

// Attempt to Load Current Game

void GameLoad(void);
void GameLoadName(void);
void do_load_state(char *name); // with a given name

// Add some data to save

extern int SaveDataCount;
void AddSaveData(UINT32 id, UINT8 *src, UINT32 size);

// Add a callback routine before savegame/after loadgame

extern UINT32 SaveCallbackCount;
void AddLoadCallback(void *callback);
void AddSaveCallback(void *callback);

// Private (cpu state saving etc)

void AddLoadCallback_Internal(void *callback);
void AddSaveCallback_Internal(void *callback);

// -------------------------------------------------------------

// ---- Save File Formats ------------------------------
// (to share with the demos in demos.c)

#define SAVE_FILE_TYPE_0      ASCII_ID('J','3','d','!')
#define SAVE_FILE_TYPE_1      ASCII_ID('R','N','E','!')
// TYPE_2 : starting with 0.35 : new mz80 3.4
#define SAVE_FILE_TYPE_2      ASCII_ID('R','N','E','1')

// PUBLIC: Use these for save_data id

#define SAVE_USER_0        ASCII_ID('U','S','R',0x00)
#define SAVE_USER_1        ASCII_ID('U','S','R',0x01)
#define SAVE_USER_2        ASCII_ID('U','S','R',0x02)
#define SAVE_USER_3        ASCII_ID('U','S','R',0x03)
#define SAVE_USER_4        ASCII_ID('U','S','R',0x04)
#define SAVE_USER_5        ASCII_ID('U','S','R',0x05)
#define SAVE_USER_6        ASCII_ID('U','S','R',0x06)
#define SAVE_USER_7        ASCII_ID('U','S','R',0x07)
#define SAVE_USER_8        ASCII_ID('U','S','R',0x08)
#define SAVE_USER_9        ASCII_ID('U','S','R',0x09)
#define SAVE_USER_10       ASCII_ID('U','S','R',0x0A)
#define SAVE_USER_11       ASCII_ID('U','S','R',0x0B)
#define SAVE_USER_12       ASCII_ID('U','S','R',0x0C)
#define SAVE_USER_13       ASCII_ID('U','S','R',0x0D)
#define SAVE_USER_14       ASCII_ID('U','S','R',0x0E)
#define SAVE_USER_15       ASCII_ID('U','S','R',0x0F)

// INTERNAL: Do not use these in game drivers

#define SAVE_M6502_0          ASCII_ID('M',0x65,0x02,0x00)
#define SAVE_M6502_1          ASCII_ID('M',0x65,0x02,0x01)
#define SAVE_M6502_2          ASCII_ID('M',0x65,0x02,0x01)

#define SAVE_Z80_0            ASCII_ID('Z','8','0',0x00)
#define SAVE_Z80_1            ASCII_ID('Z','8','0',0x01)
#define SAVE_Z80_2            ASCII_ID('Z','8','0',0x02)
#define SAVE_Z80_3            ASCII_ID('Z','8','0',0x03)

#define SAVE_68K_0            ASCII_ID('6','8','K',0x00)
#define SAVE_68K_1            ASCII_ID('6','8','K',0x01)

#define SAVE_M68020_0         ASCII_ID('0','2','0',0x00)

#define SAVE_MCU_0            ASCII_ID('M','C','U',0x00)

// -------------------------------------------------------------

#define EPR_INVALIDATE_ON_ROM_CHANGE (0x01)

void clear_eeprom_list(void);

void add_eeprom(UINT8 *source, UINT32 size, UINT8 flags);

int load_eeprom(void);

void save_eeprom(void);

void next_save_slot(void);

// -------------------------------------------------------------

// dynamic callbacks

void reset_dyn_callbacks();
void AddSaveDynCallbacks(int id,void (*load)(UINT8 *buff, int len),
  void (*save)(UINT8 **buf, int *len));

#ifdef __cplusplus
}
#endif

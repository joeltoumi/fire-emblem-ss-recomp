#ifndef GUARD_MENU_DEF_H
#define GUARD_MENU_DEF_H

struct MenuDef;
struct SelectTarget;

// The definitions in src/menu_def.c that are CONST_DATA rather than const:
// a modern compiler rejects a const declaration of them.
#ifdef PLATFORM_PC
#define MENU_DEF_CONST
#else
#define MENU_DEF_CONST const
#endif

extern MENU_DEF_CONST struct MenuDef gDebugClearMenuDef;
extern MENU_DEF_CONST struct MenuDef gDebugChargeMenuDef;
extern MENU_DEF_CONST struct MenuDef gDebugContinueMenuDef;
extern MENU_DEF_CONST struct MenuDef gDebugChuudanMenuDef;
extern MENU_DEF_CONST struct MenuDef gDebugMenuDef;
extern MENU_DEF_CONST struct MenuDef gMenuInfo_RepairItems;
extern MENU_DEF_CONST struct MenuDef gStealItemMenuDef;
extern MENU_DEF_CONST struct MenuDef gYesNoSelectionMenuDef;
extern MENU_DEF_CONST struct MenuDef gItemSubMenuDef;
extern MENU_DEF_CONST struct MenuDef gItemMenuDef;
extern MENU_DEF_CONST struct MenuDef gStaffItemSelectMenuDef;
extern MENU_DEF_CONST struct MenuDef gItemSelectMenuDef;
extern MENU_DEF_CONST struct MenuDef gBallistaRangeMenuDef;
extern MENU_DEF_CONST struct MenuDef gWeaponSelectMenuDef;
extern MENU_DEF_CONST struct MenuDef gUnitActionMenuDef;

extern struct SelectInfo CONST_DATA gSelectInfo_OffensiveStaff;
extern struct SelectInfo CONST_DATA gSelectInfo_Barrier;
extern struct SelectInfo CONST_DATA gSelectInfo_Restore;
extern struct SelectInfo CONST_DATA gSelectInfo_Heal;
extern struct SelectInfo CONST_DATA gSelectInfo_Dance;
extern struct SelectInfo CONST_DATA gSelectInfo_PutTrap;
extern struct SelectInfo CONST_DATA gSelectInfo_WarpUnit;
extern struct SelectInfo CONST_DATA gSelectInfo_Steal;
extern struct SelectInfo CONST_DATA gSelectInfo_Summon;
extern struct SelectInfo CONST_DATA gSelectInfo_Pick;
extern struct SelectInfo CONST_DATA gSelectInfo_Support;
extern struct SelectInfo CONST_DATA gSelectInfo_Talk;
extern struct SelectInfo CONST_DATA gSelectInfo_Repair;
extern struct SelectInfo CONST_DATA gSelectInfo_Trade;
extern struct SelectInfo CONST_DATA gSelectInfo_Attack;
extern struct SelectInfo CONST_DATA gSelectInfo_Give;
extern struct SelectInfo CONST_DATA gSelectInfo_Take;
extern struct SelectInfo CONST_DATA gSelectInfo_Drop;
extern struct SelectInfo CONST_DATA gSelectInfo_Rescue;

#endif // GUARD_MENU_DEF_H

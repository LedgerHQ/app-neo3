#pragma once

#if defined(TARGET_STAX) || defined(TARGET_FLEX)
#define ICON_APP_HOME C_icon_neo_n3_64x64
#elif defined(TARGET_APEX_P)
#define ICON_APP_HOME C_icon_neo_n3_48x48
#endif

/**
 * Show main menu (ready screen, version, about, quit).
 */
void ui_menu_main(void);

void ui_menu_settings(bool confirm);

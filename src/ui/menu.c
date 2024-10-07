/*****************************************************************************
 *   Ledger App Boilerplate.
 *   (c) 2020 Ledger SAS.
 *       2021 COZ Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *****************************************************************************/

#include "os.h"
#include "ux.h"
#include "glyphs.h"

#include "globals.h"
#include "shared_context.h"
#include "menu.h"
#include "utils.h"

#ifdef HAVE_NBGL
#include "nbgl_use_case.h"
#endif

#ifdef HAVE_BAGL

static void ui_menu_about();

static void display_settings(const ux_flow_step_t* const start_step);
static void switch_settings_contract_scripts(void);
#if !defined(TARGET_NANOS)
static void switch_settings_display_script(void);
#endif

UX_STEP_NOCB(ux_menu_ready_step, pn, {&C_badge_neo, "Wake up NEO.."});
UX_STEP_NOCB(ux_menu_version_step, bn, {"Version", APPVERSION});
UX_STEP_CB(ux_menu_settings_step, pb, display_settings(NULL), {&C_icon_eye, "Settings"});
UX_STEP_CB(ux_menu_about_step, pb, ui_menu_about(), {&C_icon_certificate, "About"});
UX_STEP_VALID(ux_menu_exit_step, pb, os_sched_exit(-1), {&C_icon_dashboard_x, "Quit"});
// FLOW for the main menu:
// #1 screen: ready
// #2 screen: version of the app
// #3 screen: settings
// #4 screen: about submenu
// #5 screen: quit
UX_FLOW(ux_menu_main_flow,
        &ux_menu_ready_step,
        &ux_menu_version_step,
        &ux_menu_settings_step,
        &ux_menu_about_step,
        &ux_menu_exit_step,
        FLOW_LOOP);

#if defined(TARGET_NANOS)
// clang-format off
UX_STEP_CB(
    ux_settings_contract_scripts,
    bnnn_paging,
    switch_settings_contract_scripts(),
    {
        .title = "Contract data",
        .text = strings.scriptsAllowed
    });

#else
UX_STEP_CB(
    ux_settings_contract_scripts,
    bnnn,
    switch_settings_contract_scripts(),
    {
        "Contract scripts",
        "Allow contract scripts",
        "in transactions",
        strings.scriptsAllowed
    });

UX_STEP_CB(
    ux_settings_display_script,
    bnnn,
    switch_settings_display_script(),
    {
        "Transaction script",
        "Display script hash",
        "in transactions",
        strings.showScriptHash
    });
#endif

UX_STEP_CB(
    ux_settings_back_step,
    pb,
    ui_menu_main(),
    {
      &C_icon_back_x,
      "Back",
    });

// clang-format on
#if defined(TARGET_NANOS)
UX_FLOW(ux_settings_flow, &ux_settings_contract_scripts, &ux_settings_back_step);
#else
UX_FLOW(ux_settings_flow, &ux_settings_contract_scripts, &ux_settings_display_script, &ux_settings_back_step);
#endif

static void display_settings(const ux_flow_step_t* const start_step) {
    strlcpy(strings.scriptsAllowed, (N_storage.scriptsAllowed ? "Allowed" : "NOT Allowed"), 12);
    #if !defined(TARGET_NANOS)
    strlcpy(strings.showScriptHash, (N_storage.showScriptHash ? "Show" : "Hide"), 6);
    #endif
    ux_flow_init(0, ux_settings_flow, start_step);
}

static void switch_settings_contract_scripts() {
    uint8_t value = (N_storage.scriptsAllowed ? 0 : 1);
    nvm_write((void*) &N_storage.scriptsAllowed, (void*) &value, sizeof(uint8_t));
    display_settings(&ux_settings_contract_scripts);
}

#if !defined(TARGET_NANOS)
static void switch_settings_display_script() {
    uint8_t value = (N_storage.showScriptHash ? 0 : 1);
    nvm_write((void*) &N_storage.showScriptHash, (void*) &value, sizeof(uint8_t));
    display_settings(&ux_settings_display_script);
}
#endif

UX_STEP_NOCB(ux_menu_info_step, bn, {"NEO N3 App", "(c) 2021 COZ Inc"});
UX_STEP_CB(ux_menu_back_step, pb, ui_menu_main(), {&C_icon_back, "Back"});

// FLOW for the about submenu:
// #1 screen: app info
// #2 screen: back button to main menu
UX_FLOW(ux_menu_about_flow, &ux_menu_info_step, &ux_menu_back_step, FLOW_LOOP);

static void ui_menu_about() {
    ux_flow_init(0, ux_menu_about_flow, NULL);
}

#else

#define INFO_CONTENT_SIZE 2

static const char* const info_types[INFO_CONTENT_SIZE] = {"Version", DISPLAYABLE_APPNAME};
static const char* const info_contents[INFO_CONTENT_SIZE] = {APPVERSION, "(c) 2021 COZ Inc"};

static const nbgl_contentInfoList_t infoList = {
    .nbInfos = INFO_CONTENT_SIZE,
    .infoTypes = info_types,
    .infoContents = info_contents,
};

static void quit_app_callback(void) {
    os_sched_exit(-1);
}

// Settings

#define SETTING_CONTENTS_NB 2
#define SETTINGS_SWITCHES_NB 2
enum {
    SWITCH_CONTRACT_DATA_SET_TOKEN = FIRST_USER_TOKEN,
    SWITCH_TRANSACTION_DISPLAY_SCRIPT_HASH
};

static nbgl_contentSwitch_t switches[SETTINGS_SWITCHES_NB] = {0};

static void controls_callback(int token, uint8_t index, int page) {

    UNUSED(page);
    UNUSED(index);

    bool new_setting;

    switch (token) {
        case SWITCH_CONTRACT_DATA_SET_TOKEN:
            switches[0].initState = !(switches[0].initState);
            new_setting = (switches[0].initState == ON_STATE);
            nvm_write((void*) &N_storage.scriptsAllowed, &new_setting, 1);
            break;
        case SWITCH_TRANSACTION_DISPLAY_SCRIPT_HASH:
            switches[1].initState = !(switches[0].initState);
            new_setting = (switches[1].initState == ON_STATE);
            nvm_write((void*) &N_storage.showScriptHash, &new_setting, 1);
            break;
        default:
            PRINTF("Should not happen !");
            break;
    }
}

static const nbgl_content_t contents[SETTING_CONTENTS_NB] = {
    {
        .type = SWITCHES_LIST,
        .content.switchesList.nbSwitches = SETTINGS_SWITCHES_NB,
        .content.switchesList.switches = switches,
        .contentActionCallback = controls_callback
    }
};

static const nbgl_genericContents_t settingContents = {
    .callbackCallNeeded = false,
    .contentsList = contents,
    .nbContents = 1
};

void ui_menu_settings(bool confirm) {
    switches[0].text = "Contract scripts";
    switches[0].subText = "Allow contract scripts";
    switches[0].token = SWITCH_CONTRACT_DATA_SET_TOKEN;
    switches[0].tuneId = TUNE_TAP_CASUAL;
    if (N_storage.scriptsAllowed) {
        switches[0].initState = ON_STATE;
    } else {
        switches[0].initState = OFF_STATE;
    }

    switches[1].text = "Transaction script";
    switches[1].subText = "Display SHA256 of transaction script";
    switches[1].token = SWITCH_TRANSACTION_DISPLAY_SCRIPT_HASH;
    switches[1].tuneId = TUNE_TAP_CASUAL;
    if (N_storage.showScriptHash) {
        switches[1].initState = ON_STATE;
    } else {
        switches[1].initState = OFF_STATE;
    }


    nbgl_useCaseHomeAndSettings(DISPLAYABLE_APPNAME, 
                                &C_icon_neo_n3_64x64, 
                                NULL, 
                                (confirm ? 0 : INIT_HOME_PAGE), 
                                &settingContents, 
                                &infoList, 
                                NULL, 
                                quit_app_callback);
}


static void ui_menu_main_nbgl(void) {
    ui_menu_settings(false);
}
#endif

void ui_menu_main() {
#ifdef HAVE_BAGL
    if (G_ux.stack_count == 0) {
        ux_stack_push();
    }
    ux_flow_init(0, ux_menu_main_flow, NULL);
#else
    ui_menu_main_nbgl();
#endif
}

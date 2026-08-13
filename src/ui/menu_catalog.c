#include "menu_catalog.h"

#include "asset_icon_recon.h"
#include "asset_icon_deauth.h"
#include "asset_icon_evil.h"
#include "asset_icon_bt.h"
#include "asset_icon_remote.h"

const menu_item_t MENU_MAIN[] = {
    {"Recon", "Scan / map targets", ASSET_ICON_RECON_DATA, ASSET_ICON_RECON_W, ASSET_ICON_RECON_H},
    {"Deauth Attacks", "Kick clients off AP", ASSET_ICON_DEAUTH_DATA, ASSET_ICON_DEAUTH_W, ASSET_ICON_DEAUTH_H},
    {"Evil Twin", "Rogue AP tools", ASSET_ICON_EVIL_DATA, ASSET_ICON_EVIL_W, ASSET_ICON_EVIL_H},
    {"Bluetooth Attacks", "BLE HID tools", ASSET_ICON_BT_DATA, ASSET_ICON_BT_W, ASSET_ICON_BT_H},
    {"Remote Control", "Remote ops", ASSET_ICON_REMOTE_DATA, ASSET_ICON_REMOTE_W, ASSET_ICON_REMOTE_H},
};
const int MENU_MAIN_COUNT = (int)(sizeof(MENU_MAIN) / sizeof(MENU_MAIN[0]));

static const sub_item_t SUB_RECON[] = {
    {"Probe Scan", "Passive probe request sniff", "—", "idle", "scanning", "n/a", "coming soon", ACT_SOON},
    {"AP Map", "Map nearby access points", "—", "idle", "running", "n/a", "coming soon", ACT_SOON},
    {"Back", "Return to main menu", "—", "—", "—", "—", "click to go back", ACT_BACK},
};

static const sub_item_t SUB_DEAUTH[] = {
    {"Deauth Burst", "Kick clients with deauth frames", "target", "idle", "working", "not armed", "coming soon", ACT_SOON},
    {"Deauth Flood", "Continuous deauth flood", "target", "idle", "flooding", "not armed", "coming soon", ACT_SOON},
    {"Back", "Return to main menu", "—", "—", "—", "—", "click to go back", ACT_BACK},
};

static const sub_item_t SUB_EVIL[] = {
    {"Clone AP", "Stand up a twin of target AP", "twin", "idle", "spoofing", "down", "coming soon", ACT_SOON},
    {"Captive Portal", "Phish via captive portal", "portal", "idle", "live", "down", "coming soon", ACT_SOON},
    {"Back", "Return to main menu", "—", "—", "—", "—", "click to go back", ACT_BACK},
};

static const sub_item_t SUB_BT[] = {
    {"Keyboard", "Act as BT keyboard — go rogue", "Lenovo Keyboard", "idle", "Working",
     "not connected", "press again to stop", ACT_HID_KBD},
    {"AirSpam", "Spam Apple BLE popups", "AirPods", "idle", "spamming", "nearby",
     "press again to stop", ACT_AIRSPAM},
    {"Jammer", "BLE jammer — soon", "—", "soon", "soon", "n/a", "not ready", ACT_SOON},
    {"Back", "Return to main menu", "—", "—", "—", "—", "click to go back", ACT_BACK},
};

static const sub_item_t SUB_REMOTE[] = {
    {"HID Replay", "Replay captured HID input", "device", "idle", "replaying", "n/a", "coming soon", ACT_SOON},
    {"Back", "Return to main menu", "—", "—", "—", "—", "click to go back", ACT_BACK},
};

const category_t CATEGORIES[] = {
    {"recon", SUB_RECON, (int)(sizeof(SUB_RECON) / sizeof(SUB_RECON[0]))},
    {"deauth attacks", SUB_DEAUTH, (int)(sizeof(SUB_DEAUTH) / sizeof(SUB_DEAUTH[0]))},
    {"evil twin", SUB_EVIL, (int)(sizeof(SUB_EVIL) / sizeof(SUB_EVIL[0]))},
    {"bluetooth attacks", SUB_BT, (int)(sizeof(SUB_BT) / sizeof(SUB_BT[0]))},
    {"remote control", SUB_REMOTE, (int)(sizeof(SUB_REMOTE) / sizeof(SUB_REMOTE[0]))},
};

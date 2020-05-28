#ifndef HALMAC_POWER_SEQUENCE_8821C
#define HALMAC_POWER_SEQUENCE_8821C

#include "../../halmac_pwr_seq_cmd.h"

#define HALMAC_8821C_PWR_SEQ_VER  "V13"
extern PHALMAC_WLAN_PWR_CFG halmac_8821c_card_disable_flow[];
extern PHALMAC_WLAN_PWR_CFG halmac_8821c_card_enable_flow[];
extern PHALMAC_WLAN_PWR_CFG halmac_8821c_suspend_flow[];
extern PHALMAC_WLAN_PWR_CFG halmac_8821c_resume_flow[];
extern PHALMAC_WLAN_PWR_CFG halmac_8821c_hwpdn_flow[];
extern PHALMAC_WLAN_PWR_CFG halmac_8821c_enter_lps_flow[];
extern PHALMAC_WLAN_PWR_CFG halmac_8821c_enter_deep_lps_flow[];
extern PHALMAC_WLAN_PWR_CFG halmac_8821c_leave_lps_flow[];

#endif

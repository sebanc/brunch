#ifndef __ATH_EXP_H
#define __ATH_EXP_H
// generate with this command line:
//	sed 's/^EXPORT_SYMBOL\(_GPL\)\?(\([^)]*\)).*/#define \2 __ar10k_\2/;t;d' ../ath/*.c
#define ath_opmode_to_string __ar10k_ath_opmode_to_string
#define ath_hw_setbssidmask __ar10k_ath_hw_setbssidmask
#define ath_hw_cycle_counters_update __ar10k_ath_hw_cycle_counters_update
#define ath_hw_get_listen_time __ar10k_ath_hw_get_listen_time
#define ath_hw_keyreset __ar10k_ath_hw_keyreset
#define ath_key_config __ar10k_ath_key_config
#define ath_key_delete __ar10k_ath_key_delete
#define ath_rxbuf_alloc __ar10k_ath_rxbuf_alloc
#define ath_printk __ar10k_ath_printk
#define ath_is_world_regd __ar10k_ath_is_world_regd
#define ath_is_49ghz_allowed __ar10k_ath_is_49ghz_allowed
#define ath_reg_notifier_apply __ar10k_ath_reg_notifier_apply
#define ath_regd_init __ar10k_ath_regd_init
#define ath_regd_get_band_ctl __ar10k_ath_regd_get_band_ctl

#define dfs_pattern_detector_init __ar10k_dfs_pattern_detector_init
#define ath_is_mybeacon __ar10k_ath_is_mybeacon
#endif

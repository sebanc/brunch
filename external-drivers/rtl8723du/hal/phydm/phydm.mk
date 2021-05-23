EXTRA_CFLAGS += -I$(src)/hal/phydm

_PHYDM_FILES := hal/phydm/phydm_debug.o	\
			hal/phydm/phydm_antdiv.o\
			hal/phydm/phydm_soml.o\
			hal/phydm/phydm_smt_ant.o\
			hal/phydm/phydm_antdect.o\
			hal/phydm/phydm_interface.o\
			hal/phydm/phydm_phystatus.o\
			hal/phydm/phydm_hwconfig.o\
			hal/phydm/phydm.o\
			hal/phydm/phydm_dig.o\
			hal/phydm/phydm_pathdiv.o\
			hal/phydm/phydm_rainfo.o\
			hal/phydm/phydm_dynamictxpower.o\
			hal/phydm/phydm_adaptivity.o\
			hal/phydm/phydm_cfotracking.o\
			hal/phydm/phydm_noisemonitor.o\
			hal/phydm/phydm_acs.o\
			hal/phydm/phydm_dfs.o\
			hal/phydm/phydm_hal_txbf_api.o\
			hal/phydm/phydm_ccx.o\
			hal/phydm/phydm_psd.o\
			hal/phydm/phydm_primary_cca.o\
			hal/phydm/phydm_cck_pd.o\
			hal/phydm/phydm_rssi_monitor.o\
			hal/phydm/phydm_auto_dbg.o\
			hal/phydm/phydm_math_lib.o\
			hal/phydm/phydm_api.o\
			hal/phydm/phydm_pow_train.o\
			hal/phydm/halrf.o\
			hal/phydm/halphyrf_ce.o\
			hal/phydm/halrf_powertracking_ce.o\
			hal/phydm/halrf_powertracking.o\
			hal/phydm/halrf_kfree.o
	
RTL871X = rtl8723d
_PHYDM_FILES += hal/phydm/halhwimg8723d_bb.o\
			hal/phydm/halhwimg8723d_mac.o\
			hal/phydm/halhwimg8723d_rf.o\
			hal/phydm/phydm_regconfig8723d.o\
			hal/phydm/phydm_rtl8723d.o\
			hal/phydm/halrf_8723d.o

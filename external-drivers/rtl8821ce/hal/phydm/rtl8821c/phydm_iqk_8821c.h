#ifndef	__PHYDM_IQK_8821C_H__
#define    __PHYDM_IQK_8821C_H__

#if (RTL8821C_SUPPORT == 1)


/*--------------------------Define Parameters-------------------------------*/
#define		MAC_REG_NUM_8821C 3
#define		BB_REG_NUM_8821C 10
#define		RF_REG_NUM_8821C 6
#define     DPK_BB_REG_NUM_8821C 24
#define     DPK_BACKUP_REG_NUM_8821C 3


#define	LOK_delay_8821C 2
#define	GS_delay_8821C 2
#define	WBIQK_delay_8821C 2

#define TXIQK 0
#define RXIQK 1
#define	SS_8821C 1

/*---------------------------End Define Parameters-------------------------------*/


#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
void
do_iqk_8821c(
	void	*p_dm_void,
	u8		delta_thermal_index,
	u8		thermal_value,
	u8		threshold
);
#else
void
do_iqk_8821c(
	void		*p_dm_void,
	u8		delta_thermal_index,
	u8		thermal_value,
	u8		threshold
);
#endif

void
phy_iq_calibrate_8821c(
	void		*p_dm_void,
	boolean		clear
);
VOID
phy_dp_calibrate_8821c(
	void		*p_dm_void,
	boolean		clear

	);

#else	/* (RTL8821C_SUPPORT == 0)*/

#define phy_iq_calibrate_8821c(_pdm_void, clear)
#define phy_dp_calibrate_8821c(_pDM_VOID, clear)

#endif	/* RTL8821C_SUPPORT */

#endif	/* #ifndef __PHYDM_IQK_8821C_H__*/

/*
 * Rockchip VPU codec driver
 *
 * Copyright (c) 2016 Rockchip Electronics Co., Ltd.
 *	Jung Zhao <jung.zhao@rock-chips.com>
 *	Alpha Lin <Alpha.Lin@rock-chips.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "rockchip_vpu_common.h"

#include <linux/types.h>
#include <linux/sort.h>

#include "rockchip_vpu_hw.h"
#include "rk3399_vdec_regs.h"

/* Size with u32 units. */
#define RKV_CABAC_INIT_BUFFER_SIZE	(3680 + 128)
#define RKV_RPS_SIZE			(128 + 128)
#define RKV_SCALING_LIST_SIZE		(6 * 16 + 6 * 64 + 128)
#define RKV_ERROR_INFO_SIZE		(256 * 144 * 4)

struct hw_sps_pps_packet {
	u8 info[32];
};

/* Data structure describing auxiliary buffer format. */
struct rk3399_vdec_h264d_priv_tbl {
	u8 cabac_table[RKV_CABAC_INIT_BUFFER_SIZE];
	u8 scaling_list[RKV_SCALING_LIST_SIZE];
	u8 rps[RKV_RPS_SIZE];
	struct hw_sps_pps_packet param_set[256];
	u8 err_info[RKV_ERROR_INFO_SIZE];
};

/* Constant CABAC table. */
static const u32 h264_cabac_table[] = {
	0x3602f114, 0xf1144a03, 0x4a033602, 0x68e97fe4, 0x36ff35fa, 0x21173307,
	0x00150217, 0x31000901, 0x390576db, 0x41f54ef3, 0x310c3e01, 0x321149fc,
	0x2b094012, 0x431a001d, 0x68095a10, 0x68ec7fd2, 0x4ef34301, 0x3e0141f5,
	0x5fef56fa, 0x2d093dfa, 0x51fa45fd, 0x370660f5, 0x56fb4307, 0x3a005802,
	0x5ef64cfd, 0x45043605, 0x580051fd, 0x4afb43f9, 0x50fb4afc, 0x3a0148f9,
	0x3f002900, 0x3f003f00, 0x560453f7, 0x48f96100, 0x3e03290d, 0x4efc2d00,
	0x7ee560fd, 0x65e762e4, 0x52e443e9, 0x53f05eec, 0x5beb6eea, 0x5df366ee,
	0x5cf97fe3, 0x60f959fb, 0x2efd6cf3, 0x39ff41ff, 0x4afd5df7, 0x57f85cf7,
	0x36057ee9, 0x3b063c06, 0x30ff4506, 0x45fc4400, 0x55fe58f8, 0x4bff4efa,
	0x36024df9, 0x44fd3205, 0x2a063201, 0x3f0151fc, 0x430046fc, 0x4cfe3902,
	0x4004230b, 0x230b3d01, 0x180c1912, 0x240d1d0d, 0x49f95df6, 0x2e0d49fe,
	0x64f93109, 0x35023509, 0x3dfe3505, 0x38003800, 0x3cfb3ff3, 0x39043eff,
	0x390445fa, 0x3304270e, 0x4003440d, 0x3f093d01, 0x27103207, 0x34042c05,
	0x3cfb300b, 0x3b003bff, 0x2c052116, 0x4eff2b0e, 0x45093c00, 0x28021c0b,
	0x31002c03, 0x2c022e00, 0x2f003302, 0x3e022704, 0x36002e06, 0x3a023603,
	0x33063f04, 0x35073906, 0x37063406, 0x240e2d0b, 0x52ff3508, 0x4efd3707,
	0x1f162e0f, 0x071954ff, 0x031cf91e, 0x0020041c, 0x061eff22, 0x0920061e,
	0x1b1a131f, 0x14251e1a, 0x4611221c, 0x3b054301, 0x1e104309, 0x23122012,
	0x1f181d16, 0x2b122617, 0x3f0b2914, 0x40093b09, 0x59fe5eff, 0x4cfa6cf7,
	0x2d002cfe, 0x40fd3400, 0x46fc3bfe, 0x52f84bfc, 0x4df766ef, 0x2a001803,
	0x37003000, 0x47f93bfa, 0x57f553f4, 0x3a0177e2, 0x24ff1dfd, 0x2b022601,
	0x3a0037fa, 0x4afd4000, 0x46005af6, 0x1f051dfc, 0x3b012a07, 0x48fd3afe,
	0x61f551fd, 0x05083a00, 0x120e0e0a, 0x28021b0d, 0x46fd3a00, 0x55f84ffa,
	0x6af30000, 0x57f66af0, 0x6eee72eb, 0x6eea62f2, 0x67ee6aeb, 0x6ce96beb,
	0x60f670e6, 0x5bfb5ff4, 0x5eea5df7, 0x430956fb, 0x55f650fc, 0x3c0746ff,
	0x3d053a09, 0x320f320c, 0x36113112, 0x2e07290a, 0x310733ff, 0x29093408,
	0x37022f06, 0x2c0a290d, 0x35053206, 0x3f04310d, 0x45fe4006, 0x46063bfe,
	0x1f092c0a, 0x35032b0c, 0x260a220e, 0x280d34fd, 0x2c072011, 0x320d2607,
	0x2b1a390a, 0x0e0b0b0e, 0x0b120b09, 0xfe170915, 0xf120f120, 0xe927eb22,
	0xe129df2a, 0xf426e42e, 0xe82d1d15, 0xe630d335, 0xed2bd541, 0x091ef627,
	0x1b141a12, 0x52f23900, 0x61ed4bfb, 0x001b7ddd, 0xfc1f001c, 0x0822061b,
	0x16180a1e, 0x20161321, 0x29151f1a, 0x2f172c1a, 0x470e4110, 0x3f063c08,
	0x18154111, 0x171a1417, 0x171c201b, 0x2817181c, 0x1d1c2018, 0x39132a17,
	0x3d163516, 0x280c560b, 0x3b0e330b, 0x47f94ffc, 0x46f745fb, 0x44f642f8,
	0x45f449ed, 0x43f146f0, 0x46ed3eec, 0x41ea42f0, 0xfe093fec, 0xf721f71a,
	0xfe29f927, 0x0931032d, 0x3b241b2d, 0x23f942fa, 0x2df82af9, 0x38f430fb,
	0x3efb3cfa, 0x4cf842f8, 0x51fa55fb, 0x51f94df6, 0x49ee50ef, 0x53f64afc,
	0x43f747f7, 0x42f83dff, 0x3b0042f2, 0xf3153b02, 0xf927f221, 0x0233fe2e,
	0x113d063c, 0x3e2a2237, 0x00000000, 0x00000000, 0x3602f114, 0xf1144a03,
	0x4a033602, 0x68e97fe4, 0x36ff35fa, 0x19163307, 0x00100022, 0x290409fe,
	0x410276e3, 0x4ff347fa, 0x32093405, 0x360a46fd, 0x1613221a, 0x02390028,
	0x451a2429, 0x65f17fd3, 0x47fa4cfc, 0x34054ff3, 0x5af34506, 0x2b083400,
	0x52fb45fe, 0x3b0260f6, 0x57fd4b02, 0x380164fd, 0x55fa4afd, 0x51fd3b00,
	0x5ffb56f9, 0x4dff42ff, 0x56fe4601, 0x3d0048fb, 0x3f002900, 0x3f003f00,
	0x560453f7, 0x48f96100, 0x3e03290d, 0x33070f0d, 0x7fd95002, 0x60ef5bee,
	0x62dd51e6, 0x61e966e8, 0x63e877e5, 0x66ee6eeb, 0x50007fdc, 0x5ef959fb,
	0x27005cfc, 0x54f14100, 0x49fe7fdd, 0x5bf768f4, 0x37037fe1, 0x37073807,
	0x35fd3d08, 0x4af94400, 0x67f358f7, 0x59f75bf3, 0x4cf85cf2, 0x6ee957f4,
	0x4ef669e8, 0x63ef70ec, 0x7fba7fb2, 0x7fd27fce, 0x4efb42fc, 0x48f847fc,
	0x37ff3b02, 0x4bfa46f9, 0x77de59f8, 0x14204bfd, 0x7fd4161e, 0x3dfb3600,
	0x3cff3a00, 0x43f83dfd, 0x4af254e7, 0x340541fb, 0x3d003902, 0x46f545f7,
	0x47fc3712, 0x3d073a00, 0x19122909, 0x2b052009, 0x2c002f09, 0x2e023300,
	0x42fc2613, 0x2a0c260f, 0x59002209, 0x1c0a2d04, 0xf5211f0a, 0x0f12d534,
	0xea23001c, 0x0022e726, 0xf420ee27, 0x0000a266, 0xfc21f138, 0xfb250a1d,
	0xf727e333, 0xc645de34, 0xfb2cc143, 0xe3370720, 0x00000120, 0xe721241b,
	0xe424e222, 0xe526e426, 0xf023ee22, 0xf820f222, 0x0023fa25, 0x121c0a1e,
	0x291d191a, 0x48024b00, 0x230e4d08, 0x23111f12, 0x2d111e15, 0x2d122a14,
	0x36101a1b, 0x38104207, 0x430a490b, 0x70e974f6, 0x3df947f1, 0x42fb3500,
	0x50f74df5, 0x57f654f7, 0x65eb7fde, 0x35fb27fd, 0x4bf53df9, 0x5bef4df1,
	0x6fe76be7, 0x4cf57ae4, 0x34f62cf6, 0x3af739f6, 0x45f948f0, 0x4afb45fc,
	0x420256f7, 0x200122f7, 0x34051f0b, 0x43fe37fe, 0x59f84900, 0x04073403,
	0x0811080a, 0x25031310, 0x49fb3dff, 0x4efc46ff, 0x7eeb0000, 0x6eec7ce9,
	0x7ce77ee6, 0x79e569ef, 0x66ef75e5, 0x74e575e6, 0x5ff67adf, 0x5ff864f2,
	0x72e46fef, 0x50fe59fa, 0x55f752fc, 0x48ff51f8, 0x43014005, 0x45003809,
	0x45074501, 0x43fa45f9, 0x40fe4df0, 0x43fa3d02, 0x390240fd, 0x42fd41fd,
	0x33093e00, 0x47fe42ff, 0x46ff4bfe, 0x3c0e48f7, 0x2f002510, 0x250b2312,
	0x290a290c, 0x290c3002, 0x3b00290d, 0x28133203, 0x32124203, 0xfa12fa13,
	0xf41a000e, 0xe721f01f, 0xe425ea21, 0xe22ae227, 0xdc2dd62f, 0xef29de31,
	0xb9450920, 0xc042c13f, 0xd936b64d, 0xf629dd34, 0xff280024, 0x1a1c0e1e,
	0x370c2517, 0xdf25410b, 0xdb28dc27, 0xdf2ee226, 0xe828e22a, 0xf426e331,
	0xfd26f628, 0x141ffb2e, 0x2c191e1d, 0x310b300c, 0x16162d1a, 0x151b1617,
	0x1c1a1421, 0x221b181e, 0x27192a12, 0x460c3212, 0x470e3615, 0x2019530b,
	0x36153115, 0x51fa55fb, 0x51f94df6, 0x49ee50ef, 0x53f64afc, 0x43f747f7,
	0x42f83dff, 0x3b0042f2, 0xf6113b02, 0xf72af320, 0x0035fb31, 0x0a440340,
	0x392f1b42, 0x180047fb, 0x2afe24ff, 0x39f734fe, 0x41fc3ffa, 0x52f943fc,
	0x4cfd51fd, 0x4efa48f9, 0x44f248f4, 0x4cfa46fd, 0x3efb42fb, 0x3dfc3900,
	0x36013cf7, 0xf6113a02, 0xf72af320, 0x0035fb31, 0x0a440340, 0x392f1b42,
	0x00000000, 0x00000000, 0x3602f114, 0xf1144a03, 0x4a033602, 0x68e97fe4,
	0x36ff35fa, 0x101d3307, 0x000e0019, 0x3efd33f6, 0x101a63e5, 0x66e855fc,
	0x39063905, 0x390e49ef, 0x0a142814, 0x0036001d, 0x610c2a25, 0x75ea7fe0,
	0x55fc4afe, 0x390566e8, 0x58f25dfa, 0x37042cfa, 0x67f159f5, 0x391374eb,
	0x54043a14, 0x3f016006, 0x6af355fb, 0x4b063f05, 0x65ff5afd, 0x4ffc3703,
	0x61f44bfe, 0x3c0132f9, 0x3f002900, 0x3f003f00, 0x560453f7, 0x48f96100,
	0x3e03290d, 0x58f72207, 0x7fdc7fec, 0x5ff25bef, 0x56e754e7, 0x5bef59f4,
	0x4cf27fe1, 0x5af367ee, 0x500b7fdb, 0x54024c05, 0x37fa4e05, 0x53f23d04,
	0x4ffb7fdb, 0x5bf568f5, 0x41007fe2, 0x48004ffe, 0x38fa5cfc, 0x47f84403,
	0x56fc62f3, 0x52fb58f4, 0x43fc48fd, 0x59f048f8, 0x3bff45f7, 0x39044205,
	0x47fe47fc, 0x4aff3a02, 0x45ff2cfc, 0x33f93e00, 0x2afa2ffc, 0x35fa29fd,
	0x4ef74c08, 0x340953f5, 0x5afb4300, 0x48f14301, 0x50f84bfb, 0x40eb53eb,
	0x40e71ff3, 0x4b095ee3, 0x4af83f11, 0x1bfe23fb, 0x41035b0d, 0x4d0845f9,
	0x3e0342f6, 0x51ec44fd, 0x07011e00, 0x4aeb17fd, 0x7ce94210, 0xee2c2511,
	0x7feade32, 0x2a002704, 0x1d0b2207, 0x25061f08, 0x28032a07, 0x2b0d2108,
	0x2f04240d, 0x3a023703, 0x2c083c06, 0x2a0e2c0b, 0x38043007, 0x250d3404,
	0x3a133109, 0x2d0c300a, 0x21144500, 0xee233f08, 0xfd1ce721, 0x001b0a18,
	0xd434f222, 0x1113e827, 0x1d24191f, 0x0f222118, 0x4916141e, 0x1f132214,
	0x10132c1b, 0x240f240f, 0x15191c15, 0x0c1f141e, 0x2a18101b, 0x380e5d00,
	0x261a390f, 0x73e87fe8, 0x3ef752ea, 0x3b003500, 0x59f355f2, 0x5cf55ef3,
	0x64eb7fe3, 0x43f439f2, 0x4df647f5, 0x58f055eb, 0x62f168e9, 0x52f67fdb,
	0x3df830f8, 0x46f942f8, 0x4ff64bf2, 0x5cf453f7, 0x4ffc6cee, 0x4bf045ea,
	0x3a013afe, 0x53f74ef3, 0x63f351fc, 0x26fa51f3, 0x3afa3ef3, 0x49f03bfe,
	0x56f34cf6, 0x57f653f7, 0x7fea0000, 0x78e77fe7, 0x72ed7fe5, 0x76e775e9,
	0x71e875e6, 0x78e176e4, 0x5ef67cdb, 0x63f666f1, 0x7fce6af3, 0x39115cfb,
	0x5ef356fb, 0x4dfe5bf4, 0x49ff4700, 0x51f94004, 0x390f4005, 0x44004301,
	0x440143f6, 0x40024d00, 0x4efb4400, 0x3b053707, 0x360e4102, 0x3c052c0f,
	0x4cfe4602, 0x460c56ee, 0x46f44005, 0x3805370b, 0x41024500, 0x36054afa,
	0x4cfa3607, 0x4dfe52f5, 0x2a194dfe, 0xf710f311, 0xeb1bf411, 0xd829e225,
	0xd130d72a, 0xd82ee027, 0xd72ecd34, 0xed2bd934, 0xc93d0b20, 0xce3ed238,
	0xec2dbd51, 0x0f1cfe23, 0x01270122, 0x2614111e, 0x360f2d12, 0xf0244f00,
	0xef25f225, 0x0f220120, 0x19180f1d, 0x101f1622, 0x1c1f1223, 0x1c242921,
	0x3e152f1b, 0x1a131f12, 0x17181824, 0x1e18101b, 0x29161d1f, 0x3c102a16,
	0x3c0e340f, 0x7bf04e03, 0x38163515, 0x21153d19, 0x3d113213, 0x4af84efd,
	0x48f648f7, 0x47f44bee, 0x46fb3ff5, 0x48f24bef, 0x35f843f0, 0x34f73bf2,
	0xfe0944f5, 0xfc1ff61e, 0x0721ff21, 0x17250c1f, 0x4014261f, 0x25f947f7,
	0x31f52cf8, 0x3bf438f6, 0x43f73ff8, 0x4ff644fa, 0x4af84efd, 0x48f648f7,
	0x47f44bee, 0x46fb3ff5, 0x48f24bef, 0x35f843f0, 0x34f73bf2, 0xfe0944f5,
	0xfc1ff61e, 0x0721ff21, 0x17250c1f, 0x4014261f, 0x00000000, 0x00000000,
	0x3602f114, 0xf1144a03, 0x4a033602, 0x68e97fe4, 0x36ff35fa, 0x00003307,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x3f002900, 0x3f003f00, 0x560453f7, 0x48f96100, 0x3e03290d, 0x37010b00,
	0x7fef4500, 0x520066f3, 0x6beb4af9, 0x7fe17fe5, 0x5fee7fe8, 0x72eb7fe5,
	0x7bef7fe2, 0x7af073f4, 0x3ff473f5, 0x54f144fe, 0x46fd68f3, 0x5af65df8,
	0x4aff7fe2, 0x5bf961fa, 0x38fc7fec, 0x4cf952fb, 0x5df97dea, 0x4dfd57f5,
	0x3ffc47fb, 0x54f444fc, 0x41f93ef9, 0x38053d08, 0x400142fe, 0x4efe3d00,
	0x34073201, 0x2c00230a, 0x2d01260b, 0x2c052e00, 0x3301111f, 0x131c3207,
	0x3e0e2110, 0x64f16cf3, 0x5bf365f3, 0x58f65ef4, 0x56f654f0, 0x57f353f9,
	0x46015eed, 0x4afb4800, 0x66f83b12, 0x5f0064f1, 0x48024bfc, 0x47fd4bf5,
	0x45f32e0f, 0x41003e00, 0x48f12515, 0x36103909, 0x480c3e00, 0x090f0018,
	0x120d1908, 0x130d090f, 0x120c250a, 0x21141d06, 0x2d041e0f, 0x3e003a01,
	0x260c3d07, 0x270f2d0b, 0x2c0d2a0b, 0x290c2d10, 0x221e310a, 0x370a2a12,
	0x2e113311, 0xed1a5900, 0xef1aef16, 0xec1ce71e, 0xe525e921, 0xe428e921,
	0xf521ef26, 0xfa29f128, 0x11290126, 0x031bfa1e, 0xf025161a, 0xf826fc23,
	0x0325fd26, 0x002a0526, 0x16271023, 0x251b300e, 0x440c3c15, 0x47fd6102,
	0x32fb2afa, 0x3efe36fd, 0x3f013a00, 0x4aff48fe, 0x43fb5bf7, 0x27fd1bfb,
	0x2e002cfe, 0x44f840f0, 0x4dfa4ef6, 0x5cf456f6, 0x3cf637f1, 0x41fc3efa,
	0x4cf849f4, 0x58f750f9, 0x61f56eef, 0x4ff554ec, 0x4afc49fa, 0x60f356f3,
	0x75ed61f5, 0x21fb4ef8, 0x35fe30fc, 0x47f33efd, 0x56f44ff6, 0x61f25af3,
	0x5dfa0000, 0x4ff854fa, 0x47ff4200, 0x3cfe3e00, 0x4bfb3bfe, 0x3afc3efd,
	0x4fff42f7, 0x44034700, 0x3ef92c0a, 0x280e240f, 0x1d0c1b10, 0x24142c01,
	0x2a052012, 0x3e0a3001, 0x40092e11, 0x61f568f4, 0x58f960f0, 0x55f955f8,
	0x58f355f7, 0x4dfd4204, 0x4cfa4cfd, 0x4cff3a0a, 0x63f953ff, 0x5f025ff2,
	0x4afb4c00, 0x4bf54600, 0x41004401, 0x3e0349f2, 0x44ff3e04, 0x370b4bf3,
	0x460c4005, 0x1306060f, 0x0e0c1007, 0x0b0d0d12, 0x100f0f0d, 0x170d170c,
	0x1a0e140f, 0x28112c0e, 0x11182f11, 0x16191515, 0x1d161b1f, 0x320e2313,
	0x3f07390a, 0x52fc4dfe, 0x45095efd, 0xdd246df4, 0xe620de24, 0xe02ce225,
	0xf122ee22, 0xf921f128, 0x0021fb23, 0x0d210226, 0x3a0d2317, 0x001afd1d,
	0xf91f1e16, 0xfd22f123, 0xff240322, 0x0b200522, 0x0c220523, 0x1d1e0b27,
	0x271d1a22, 0x151f4213, 0x32191f1f, 0x70ec78ef, 0x55f572ee, 0x59f25cf1,
	0x51f147e6, 0x440050f2, 0x38e846f2, 0x32e844e9, 0xf3174af5, 0xf128f31a,
	0x032cf231, 0x222c062d, 0x52133621, 0x17ff4bfd, 0x2b012201, 0x37fe3600,
	0x40013d00, 0x5cf74400, 0x61f36af2, 0x5af45af1, 0x49f658ee, 0x56f24ff7,
	0x46f649f6, 0x42fb45f6, 0x3afb40f7, 0xf6153b02, 0xf81cf518, 0x031dff1c,
	0x1423091d, 0x430e241d
};

static void rk3399_vdec_h264d_assemble_hw_pps(struct rockchip_vpu_ctx *ctx)
{
	const struct v4l2_ctrl_h264_sps *sps = ctx->run.h264d.sps;
	const struct v4l2_ctrl_h264_pps *pps = ctx->run.h264d.pps;
	const struct v4l2_h264_dpb_entry *dpb =
		ctx->run.h264d.decode_param->dpb;
	struct rk3399_vdec_h264d_priv_tbl *priv_tbl =
		ctx->hw.h264d.priv_tbl.cpu;
	u32 scaling_distance;
	dma_addr_t scaling_list_address;
	struct hw_sps_pps_packet *hw_ps;
	u32 i;

	/*
	 * HW read the SPS/PPS informantion from PPS packet index by PPS id.
	 * offset from the base can be calculated by PPS_id * 32 (size per PPS
	 * packet unit). so the driver copy SPS/PPS information to the exact PPS
	 * packet unit for HW accessing.
	 */
	hw_ps = &priv_tbl->param_set[pps->pic_parameter_set_id];
	memset(hw_ps, 0, sizeof(*hw_ps));

#define WRITE_PPS(value, field)	WRITE_HEADER(value, (u32 *)hw_ps, field)
	/* write sps */
	WRITE_PPS(sps->chroma_format_idc, CHROMA_FORMAT_IDC);
	WRITE_PPS(sps->bit_depth_luma_minus8 + 8, BIT_DEPTH_LUMA);
	WRITE_PPS(sps->bit_depth_chroma_minus8 + 8, BIT_DEPTH_CHROMA);
	WRITE_PPS(0, QPPRIME_Y_ZERO_TRANSFORM_BYPASS_FLAG);
	WRITE_PPS(sps->log2_max_frame_num_minus4, LOG2_MAX_FRAME_NUM_MINUS4);
	WRITE_PPS(sps->max_num_ref_frames, MAX_NUM_REF_FRAMES);
	WRITE_PPS(sps->pic_order_cnt_type, PIC_ORDER_CNT_TYPE);
	WRITE_PPS(sps->log2_max_pic_order_cnt_lsb_minus4,
		  LOG2_MAX_PIC_ORDER_CNT_LSB_MINUS4);
	WRITE_PPS((sps->flags &
		   V4L2_H264_SPS_FLAG_DELTA_PIC_ORDER_ALWAYS_ZERO) >> 2,
		  DELTA_PIC_ORDER_ALWAYS_ZERO_FLAG);
	WRITE_PPS(sps->pic_width_in_mbs_minus1 + 1, PIC_WIDTH_IN_MBS);
	WRITE_PPS(sps->pic_height_in_map_units_minus1 + 1, PIC_HEIGHT_IN_MBS);
	WRITE_PPS((sps->flags & V4L2_H264_SPS_FLAG_FRAME_MBS_ONLY) >> 4,
		  FRAME_MBS_ONLY_FLAG);
	WRITE_PPS((sps->flags &
		   V4L2_H264_SPS_FLAG_MB_ADAPTIVE_FRAME_FIELD) >> 5,
		  MB_ADAPTIVE_FRAME_FIELD_FLAG);
	WRITE_PPS((sps->flags & V4L2_H264_SPS_FLAG_DIRECT_8X8_INFERENCE) >> 6,
		  DIRECT_8X8_INFERENCE_FLAG);

	/* write pps */
	WRITE_PPS(pps->flags & V4L2_H264_PPS_FLAG_ENTROPY_CODING_MODE,
		  ENTROPY_CODING_MODE_FLAG);
	WRITE_PPS((pps->flags &
		   V4L2_H264_PPS_FLAG_BOTTOM_FIELD_PIC_ORDER_IN_FRAME_PRESENT)
		   >> 1, BOTTOM_FIELD_PIC_ORDER_IN_FRAME_PRESENT_FLAG);
	WRITE_PPS(pps->num_ref_idx_l0_default_active_minus1,
		  NUM_REF_IDX_L0_DEFAULT_ACTIVE_MINUS1);
	WRITE_PPS(pps->num_ref_idx_l1_default_active_minus1,
		  NUM_REF_IDX_L1_DEFAULT_ACTIVE_MINUS1);
	WRITE_PPS((pps->flags & V4L2_H264_PPS_FLAG_WEIGHTED_PRED) >> 2,
		  WEIGHTED_PRED_FLAG);
	WRITE_PPS(pps->weighted_bipred_idc, WEIGHTED_BIPRED_IDC);
	WRITE_PPS(pps->pic_init_qp_minus26, PIC_INIT_QP_MINUS26);
	WRITE_PPS(pps->pic_init_qs_minus26, PIC_INIT_QS_MINUS26);
	WRITE_PPS(pps->chroma_qp_index_offset, CHROMA_QP_INDEX_OFFSET);
	WRITE_PPS((pps->flags &
		   V4L2_H264_PPS_FLAG_DEBLOCKING_FILTER_CONTROL_PRESENT) >> 3,
		  DEBLOCKING_FILTER_CONTROL_PRESENT_FLAG);
	WRITE_PPS((pps->flags & V4L2_H264_PPS_FLAG_CONSTRAINED_INTRA_PRED) >> 4,
		  CONSTRAINED_INTRA_PRED_FLAG);
	WRITE_PPS((pps->flags &
		   V4L2_H264_PPS_FLAG_REDUNDANT_PIC_CNT_PRESENT) >> 5,
		  REDUNDANT_PIC_CNT_PRESENT);
	WRITE_PPS((pps->flags & V4L2_H264_PPS_FLAG_TRANSFORM_8X8_MODE) >> 6,
		  TRANSFORM_8X8_MODE_FLAG);
	WRITE_PPS(pps->second_chroma_qp_index_offset,
		  SECOND_CHROMA_QP_INDEX_OFFSET);

	/* always use the matrix sent from userspace */
	WRITE_PPS(1, SCALING_LIST_ENABLE_FLAG);

	scaling_distance =
		offsetof(struct rk3399_vdec_h264d_priv_tbl, scaling_list);

	scaling_list_address =
		ctx->hw.h264d.priv_tbl.dma + scaling_distance;
	WRITE_PPS(scaling_list_address, SCALING_LIST_ADDRESS);

	for (i = 0; i < 16; i++)
		write_header((dpb[i].flags &
			      V4L2_H264_DPB_ENTRY_FLAG_LONG_TERM) ?
			     1 : 0, (u32 *)hw_ps, IS_LONG_TERM_OFF(i),
			     IS_LONG_TERM_LEN);
}

static void rk3399_vdec_h264d_assemble_hw_rps(struct rockchip_vpu_ctx *ctx)
{
	const struct v4l2_ctrl_h264_decode_param *dec_param =
		ctx->run.h264d.decode_param;
	const struct v4l2_h264_dpb_entry *dpb = dec_param->dpb;
	struct rk3399_vdec_h264d_priv_tbl *priv_tbl =
		ctx->hw.h264d.priv_tbl.cpu;

	u8 *hw_rps = priv_tbl->rps;
	u32 i, j;
	u16 *p = (u16 *)hw_rps;

	memset(hw_rps, 0, RKV_RPS_SIZE);

	/*
	 * Assign an invalid pic_num if DPB entry at that position is inactive.
	 * If we assign 0 in that position hardware will treat that as a real
	 * reference picture with pic_num 0, triggering output picture
	 * corruption.
	 */
	for (i = 0; i < 16; i++)
		p[i] = (dpb[i].flags & V4L2_H264_DPB_ENTRY_FLAG_ACTIVE) ?
			dpb[i].pic_num : 0xff;

	for (j = 0; j < 3; j++) {
		for (i = 0; i < 32; i++) {
			u8 dpb_valid = 0;
			u8 idx = 0;

			switch (j) {
			case 0:
				idx = dec_param->ref_pic_list_p0[i];
				break;
			case 1:
				idx = dec_param->ref_pic_list_b0[i];
				break;
			case 2:
				idx = dec_param->ref_pic_list_b1[i];
				break;
			}

			if (idx >= ARRAY_SIZE(dec_param->dpb))
				continue;
			dpb_valid = !!(dpb[idx].flags &
				       V4L2_H264_DPB_ENTRY_FLAG_ACTIVE);

			write_header(idx | dpb_valid << 4,
				     (u32 *)hw_rps, DPB_INFO_OFF(i, j),
				     DPB_INFO_LEN);
		}
	}
}

/*
 * NOTE: The values in a scaling list are in zig-zag order, apply inverse
 * scanning process to get the values in matrix order.
 */

static const u32 zig_zag_4x4[16] = {
	0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15
};

static const u32 zig_zag_8x8[64] = {
	0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
	12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};

static void rk3399_vpu_h264d_reorder_scaling_list(struct rockchip_vpu_ctx *ctx)
{
	const struct v4l2_ctrl_h264_scaling_matrix *scaling =
		ctx->run.h264d.scaling_matrix;
	const size_t num_list_4x4 = ARRAY_SIZE(scaling->scaling_list_4x4);
	const size_t list_len_4x4 = ARRAY_SIZE(scaling->scaling_list_4x4[0]);
	const size_t num_list_8x8 = ARRAY_SIZE(scaling->scaling_list_8x8);
	const size_t list_len_8x8 = ARRAY_SIZE(scaling->scaling_list_8x8[0]);
	struct rk3399_vdec_h264d_priv_tbl *tbl = ctx->hw.h264d.priv_tbl.cpu;
	u8 *dst = tbl->scaling_list;
	const u8 *src;
	int i, j;

	BUILD_BUG_ON(ARRAY_SIZE(zig_zag_4x4) != list_len_4x4);
	BUILD_BUG_ON(ARRAY_SIZE(zig_zag_8x8) != list_len_8x8);
	BUILD_BUG_ON(ARRAY_SIZE(tbl->scaling_list) <
		     num_list_4x4 * list_len_4x4 +
		     num_list_8x8 * list_len_8x8);

	src = &scaling->scaling_list_4x4[0][0];
	for (i = 0; i < num_list_4x4; ++i) {
		for (j = 0; j < list_len_4x4; ++j)
			dst[zig_zag_4x4[j]] = src[j];
		src += list_len_4x4;
		dst += list_len_4x4;
	}

	src = &scaling->scaling_list_8x8[0][0];
	for (i = 0; i < num_list_8x8; ++i) {
		for (j = 0; j < list_len_8x8; ++j)
			dst[zig_zag_8x8[j]] = src[j];
		src += list_len_8x8;
		dst += list_len_8x8;
	}
}

int rk3399_vdec_h264d_init(struct rockchip_vpu_ctx *ctx)
{
	struct rockchip_vpu_dev *vpu = ctx->dev;
	struct rk3399_vdec_h264d_priv_tbl *tbl;
	int ret;

	ret = rockchip_vpu_aux_buf_alloc(vpu, &ctx->hw.h264d.priv_tbl,
					 sizeof(struct
						rk3399_vdec_h264d_priv_tbl));
	if (ret) {
		vpu_err("allocate h264 priv_tbl failed\n");
		return ret;
	}

	tbl = ctx->hw.h264d.priv_tbl.cpu;

	/*
	 * Prepare auxiliary buffer.
	 *
	 * TODO: The CABAC table never changes, but maybe it would be better
	 * to have it as a control, which is set by userspace once?
	 */
	memcpy(tbl->cabac_table, h264_cabac_table, sizeof(h264_cabac_table));

	return 0;
}

void rk3399_vdec_h264d_exit(struct rockchip_vpu_ctx *ctx)
{
	rockchip_vpu_aux_buf_free(ctx->dev, &ctx->hw.h264d.priv_tbl);
}

static void rk3399_vdec_h264d_prepare_table(struct rockchip_vpu_ctx *ctx)
{
	rk3399_vpu_h264d_reorder_scaling_list(ctx);
	rk3399_vdec_h264d_assemble_hw_pps(ctx);
	rk3399_vdec_h264d_assemble_hw_rps(ctx);
}

/*
 * dpb poc related registers table
 */
static u32 poc_reg_tbl_top_field[16] = {
	RKVDEC_REG_H264_POC_REFER0(0),
	RKVDEC_REG_H264_POC_REFER0(2),
	RKVDEC_REG_H264_POC_REFER0(4),
	RKVDEC_REG_H264_POC_REFER0(6),
	RKVDEC_REG_H264_POC_REFER0(8),
	RKVDEC_REG_H264_POC_REFER0(10),
	RKVDEC_REG_H264_POC_REFER0(12),
	RKVDEC_REG_H264_POC_REFER0(14),
	RKVDEC_REG_H264_POC_REFER1(1),
	RKVDEC_REG_H264_POC_REFER1(3),
	RKVDEC_REG_H264_POC_REFER1(5),
	RKVDEC_REG_H264_POC_REFER1(7),
	RKVDEC_REG_H264_POC_REFER1(9),
	RKVDEC_REG_H264_POC_REFER1(11),
	RKVDEC_REG_H264_POC_REFER1(13),
	RKVDEC_REG_H264_POC_REFER2(0)
};

static u32 poc_reg_tbl_bottom_field[16] = {
	RKVDEC_REG_H264_POC_REFER0(1),
	RKVDEC_REG_H264_POC_REFER0(3),
	RKVDEC_REG_H264_POC_REFER0(5),
	RKVDEC_REG_H264_POC_REFER0(7),
	RKVDEC_REG_H264_POC_REFER0(9),
	RKVDEC_REG_H264_POC_REFER0(11),
	RKVDEC_REG_H264_POC_REFER0(13),
	RKVDEC_REG_H264_POC_REFER1(0),
	RKVDEC_REG_H264_POC_REFER1(2),
	RKVDEC_REG_H264_POC_REFER1(4),
	RKVDEC_REG_H264_POC_REFER1(6),
	RKVDEC_REG_H264_POC_REFER1(8),
	RKVDEC_REG_H264_POC_REFER1(10),
	RKVDEC_REG_H264_POC_REFER1(12),
	RKVDEC_REG_H264_POC_REFER1(14),
	RKVDEC_REG_H264_POC_REFER2(1)
};

static void rk3399_vdec_h264d_config_registers(struct rockchip_vpu_ctx *ctx)
{
	const struct v4l2_ctrl_h264_decode_param *dec_param =
		ctx->run.h264d.decode_param;
	const struct v4l2_ctrl_h264_sps *sps = ctx->run.h264d.sps;
	const struct v4l2_h264_dpb_entry *dpb = dec_param->dpb;
	struct rockchip_vpu_dev *vpu = ctx->dev;
	dma_addr_t priv_start_addr = ctx->hw.h264d.priv_tbl.dma;
	dma_addr_t rlc_addr;
	dma_addr_t refer_addr;
	u32 rlc_len;
	u32 hor_virstride = 0;
	u32 ver_virstride = 0;
	u32 y_virstride = 0;
	u32 yuv_virstride = 0;
	u32 offset;
	dma_addr_t dst_addr;
	u32 reg, i;

	reg = RKVDEC_MODE(RKVDEC_MODE_H264);
	vdpu_write_relaxed(vpu, reg, RKVDEC_REG_SYSCTRL);

	hor_virstride = (sps->bit_depth_luma_minus8 + 8)
			* ctx->dst_fmt.width / 8;
	ver_virstride = round_up(ctx->dst_fmt.height, 16);
	y_virstride = hor_virstride * ver_virstride;

	if (sps->chroma_format_idc == 0)
		yuv_virstride = y_virstride;
	else if (sps->chroma_format_idc == 1)
		yuv_virstride += y_virstride + y_virstride / 2;
	else if (sps->chroma_format_idc == 2)
		yuv_virstride += 2 * y_virstride;

	reg = RKVDEC_Y_HOR_VIRSTRIDE(hor_virstride / 16) |
			RKVDEC_UV_HOR_VIRSTRIDE(hor_virstride / 16) |
			RKVDEC_SLICE_NUM_HIGHBIT |
			RKVDEC_SLICE_NUM_LOWBITS(0x7ff);
	vdpu_write_relaxed(vpu, reg, RKVDEC_REG_PICPAR);

	/* config rlc base address */
	rlc_addr = vb2_dma_contig_plane_dma_addr(&ctx->run.src->b.vb2_buf, 0);
	vdpu_write_relaxed(vpu, rlc_addr, RKVDEC_REG_STRM_RLC_BASE);

	rlc_len = vb2_plane_size(&ctx->run.src->b.vb2_buf, 0);
	reg = RKVDEC_STRM_LEN(rlc_len);
	vdpu_write_relaxed(vpu, reg, RKVDEC_REG_STRM_LEN);

	/* config cabac table */
	offset = offsetof(struct rk3399_vdec_h264d_priv_tbl, cabac_table);
	vdpu_write_relaxed(vpu, priv_start_addr + offset,
			   RKVDEC_REG_CABACTBL_PROB_BASE);

	/* config output base address */
	dst_addr = vb2_dma_contig_plane_dma_addr(&ctx->run.dst->b.vb2_buf, 0);
	vdpu_write_relaxed(vpu, dst_addr, RKVDEC_REG_DECOUT_BASE);

	reg = RKVDEC_Y_VIRSTRIDE(y_virstride / 16);
	vdpu_write_relaxed(vpu, reg, RKVDEC_REG_Y_VIRSTRIDE);

	reg = RKVDEC_YUV_VIRSTRIDE(yuv_virstride / 16);
	vdpu_write_relaxed(vpu, reg, RKVDEC_REG_YUV_VIRSTRIDE);

	/* config ref pic address & poc */
	for (i = 0; i < 16; i++) {
		struct vb2_buffer *vb_buf;

		/*
		* Set up addresses of DPB buffers.
		*
		* If a DPB entry is unused, address
		* of current destination buffer is used.
		*/
		if (dpb[i].flags & V4L2_H264_DPB_ENTRY_FLAG_ACTIVE
		    && dpb[i].buf_index < ctx->vq_dst.num_buffers)
			vb_buf = ctx->dst_bufs[dpb[i].buf_index];
		else
			vb_buf = &ctx->run.dst->b.vb2_buf;

		refer_addr = vb2_dma_contig_plane_dma_addr(vb_buf, 0)
			| RKVDEC_COLMV_USED_FLAG_REF
			| RKVDEC_TOPFIELD_USED_REF
			| RKVDEC_BOTFIELD_USED_REF;
		vdpu_write_relaxed(vpu, dpb[i].top_field_order_cnt,
				   poc_reg_tbl_top_field[i]);
		vdpu_write_relaxed(vpu, dpb[i].bottom_field_order_cnt,
				   poc_reg_tbl_bottom_field[i]);
		if (i < 15)
			vdpu_write_relaxed(vpu, refer_addr,
					   RKVDEC_REG_H264_BASE_REFER(i));
		else
			vdpu_write_relaxed(vpu, refer_addr,
					   RKVDEC_REG_H264_BASE_REFER15);
	}

	/*
	* Since support frame mode only
	* top_field_order_cnt is the same as bottom_field_order_cnt
	*/
	reg = RKVDEC_CUR_POC(dec_param->top_field_order_cnt);
	vdpu_write_relaxed(vpu, reg, RKVDEC_REG_CUR_POC0);

	reg = RKVDEC_CUR_POC(dec_param->bottom_field_order_cnt);
	vdpu_write_relaxed(vpu, reg, RKVDEC_REG_CUR_POC1);

	/* config hw pps address */
	offset = offsetof(struct rk3399_vdec_h264d_priv_tbl, param_set);
	vdpu_write_relaxed(vpu, priv_start_addr + offset, RKVDEC_REG_PPS_BASE);

	/* config hw rps address */
	offset = offsetof(struct rk3399_vdec_h264d_priv_tbl, rps);
	vdpu_write_relaxed(vpu, priv_start_addr + offset, RKVDEC_REG_RPS_BASE);

	reg = RKVDEC_AXI_DDR_RDATA(0);
	vdpu_write_relaxed(vpu, reg, RKVDEC_REG_AXI_DDR_RDATA);

	reg = RKVDEC_AXI_DDR_WDATA(0);
	vdpu_write_relaxed(vpu, reg, RKVDEC_REG_AXI_DDR_WDATA);

	offset = offsetof(struct rk3399_vdec_h264d_priv_tbl, err_info);
	vdpu_write_relaxed(vpu, priv_start_addr + offset,
			   RKVDEC_REG_H264_ERRINFO_BASE);
}

void rk3399_vdec_h264d_run(struct rockchip_vpu_ctx *ctx)
{
	struct rockchip_vpu_dev *vpu = ctx->dev;

	/* Prepare data in memory. */
	rk3399_vdec_h264d_prepare_table(ctx);

	rockchip_vpu_power_on(vpu);

	/* Configure hardware registers. */
	rk3399_vdec_h264d_config_registers(ctx);

	schedule_delayed_work(&vpu->watchdog_work, msecs_to_jiffies(2000));

	vdpu_write(vpu, 1, RKVDEC_REG_PREF_LUMA_CACHE_COMMAND);
	vdpu_write(vpu, 1, RKVDEC_REG_PREF_CHR_CACHE_COMMAND);

	/* Start decoding! */
	vdpu_write(vpu, RKVDEC_INTERRUPT_DEC_E
			   | RKVDEC_CONFIG_DEC_CLK_GATE_E,
			   RKVDEC_REG_INTERRUPT);
}

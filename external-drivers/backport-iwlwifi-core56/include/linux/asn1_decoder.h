/* Automatically created during backport process */
#ifndef CPTCFG_BPAUTO_ASN1_DECODER
#include_next <linux/asn1_decoder.h>
#else
#undef asn1_ber_decoder
#define asn1_ber_decoder LINUX_BACKPORT(asn1_ber_decoder)
#include <linux/backport-asn1_decoder.h>
#endif /* CPTCFG_BPAUTO_ASN1_DECODER */

ccflags-y += -I$(src) -Wframe-larger-than=1280
ifeq ($(CONFIG_BACKPORT_INTEGRATE),)
obj-m += compat.o
else
obj-y += compat.o
endif
compat-y += main.o

# Kernel backport compatibility code
compat-$(CPTCFG_KERNEL_4_5) += backport-4.5.o
compat-$(CPTCFG_KERNEL_4_6) += backport-4.6.o
compat-$(CPTCFG_KERNEL_4_7) += backport-4.7.o
compat-$(CPTCFG_KERNEL_4_8) += backport-4.8.o
compat-$(CPTCFG_KERNEL_4_10) += backport-4.10.o
compat-$(CPTCFG_KERNEL_4_11) += backport-4.11.o
compat-$(CPTCFG_KERNEL_4_12) += backport-4.12.o
compat-$(CPTCFG_KERNEL_4_18) += backport-4.18.o
compat-$(CPTCFG_KERNEL_5_2) += backport-5.2.o backport-genetlink.o
compat-$(CPTCFG_KERNEL_5_3) += backport-5.3.o
compat-$(CPTCFG_KERNEL_5_5) += backport-5.5.o
compat-$(CPTCFG_KERNEL_5_9) += backport-5.9.o
compat-$(CPTCFG_KERNEL_5_10) += backport-5.10.o
compat-$(CPTCFG_KERNEL_5_11) += backport-5.11.o
compat-$(CPTCFG_KERNEL_5_13) += backport-5.13.o
compat-$(CPTCFG_KERNEL_5_15) += backport-5.15.o

compat-$(CPTCFG_BPAUTO_BUILD_SYSTEM_DATA_VERIFICATION) += verification/verify.o
compat-$(CPTCFG_BPAUTO_BUILD_SYSTEM_DATA_VERIFICATION) += verification/pkcs7.asn1.o
compat-$(CPTCFG_BPAUTO_BUILD_SYSTEM_DATA_VERIFICATION) += verification/pkcs7_verify.o
compat-$(CPTCFG_BPAUTO_BUILD_SYSTEM_DATA_VERIFICATION) += verification/pkcs7_parser.o
compat-$(CPTCFG_BPAUTO_BUILD_SYSTEM_DATA_VERIFICATION) += verification/x509.asn1.o
compat-$(CPTCFG_BPAUTO_BUILD_SYSTEM_DATA_VERIFICATION) += verification/x509_akid.asn1.o
compat-$(CPTCFG_BPAUTO_BUILD_SYSTEM_DATA_VERIFICATION) += verification/x509_cert_parser.o
compat-$(CPTCFG_BPAUTO_BUILD_SYSTEM_DATA_VERIFICATION) += verification/x509_public_key.o
compat-$(CPTCFG_BPAUTO_BUILD_SYSTEM_DATA_VERIFICATION) += verification/pkcs7_trust.o
compat-$(CPTCFG_BPAUTO_BUILD_SYSTEM_DATA_VERIFICATION) += verification/key.o
compat-$(CPTCFG_BPAUTO_BUILD_SYSTEM_DATA_VERIFICATION) += verification/public_key.o
compat-$(CPTCFG_BPAUTO_BUILD_SYSTEM_DATA_VERIFICATION) += verification/rsa.o
compat-$(CPTCFG_BPAUTO_BUILD_SYSTEM_DATA_VERIFICATION) += verification/bignum.o
compat-$(CPTCFG_BPAUTO_BUILD_SYSTEM_DATA_VERIFICATION) += verification/md.o
compat-$(CPTCFG_BPAUTO_BUILD_SYSTEM_DATA_VERIFICATION) += verification/md_wrap.o
compat-$(CPTCFG_BPAUTO_BUILD_SYSTEM_DATA_VERIFICATION) += verification/sha256.o
compat-$(CPTCFG_BPAUTO_BUILD_SYSTEM_DATA_VERIFICATION) += verification/oid.o
compat-$(CPTCFG_BPAUTO_BUILD_SYSTEM_DATA_VERIFICATION) += verification/asn1parse.o
compat-$(CPTCFG_BPAUTO_BUILD_SYSTEM_DATA_VERIFICATION) += verification/rsapubkey.asn1.o

$(obj)/lib-oid_registry.o: $(obj)/oid_registry_data.c

$(obj)/oid_registry_data.c: $(src)/../include/linux/backport-oid_registry.h \
			    $(src)/build_OID_registry
	$(call cmd,build_OID_registry)

quiet_cmd_build_OID_registry = GEN     $@
	cmd_build_OID_registry = perl $(src)/build_OID_registry $< $@
compat-$(CPTCFG_BPAUTO_ASN1_DECODER) += lib-asn1_decoder.o
compat-$(CPTCFG_BPAUTO_PUBLIC_KEY) += 
compat-$(CPTCFG_BPAUTO_REFCOUNT) += lib-refcount.o
compat-$(CPTCFG_BPAUTO_RHASHTABLE) += lib-rhashtable.o
compat-$(CPTCFG_BPAUTO_BUILD_SYSTEM_DATA_VERIFICATION) += lib-oid_registry.o
compat-$(CPTCFG_BPAUTO_BUCKET_LOCKS) += lib-bucket_locks.o
compat-$(CPTCFG_BPAUTO_PKCS7) += 
compat-$(CPTCFG_BPAUTO_BUILD_CRYPTO_LIB_ARC4) += lib-crypto-arc4.o

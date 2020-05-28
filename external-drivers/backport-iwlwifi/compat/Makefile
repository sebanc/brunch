ccflags-y += -I$(src) -Wframe-larger-than=1280
ifeq ($(CONFIG_BACKPORT_INTEGRATE),)
obj-m += compat.o
else
obj-y += compat.o
endif
compat-y += main.o

# Kernel backport compatibility code
compat-$(CPTCFG_KERNEL_3_0) += compat-3.0.o
compat-$(CPTCFG_KERNEL_3_1) += compat-3.1.o
compat-$(CPTCFG_KERNEL_3_2) += backport-3.2.o
compat-$(CPTCFG_KERNEL_3_3) += compat-3.3.o
compat-$(CPTCFG_KERNEL_3_4) += compat-3.4.o
compat-$(CPTCFG_KERNEL_3_5) += compat-3.5.o user_namespace.o
compat-$(CPTCFG_KERNEL_3_6) += compat-3.6.o
compat-$(CPTCFG_KERNEL_3_7) += compat-3.7.o
compat-$(CPTCFG_KERNEL_3_8) += compat-3.8.o
compat-$(CPTCFG_KERNEL_3_9) += compat-3.9.o
compat-$(CPTCFG_KERNEL_3_10) += backport-3.10.o
compat-$(CPTCFG_KERNEL_3_11) += backport-3.11.o
compat-$(CPTCFG_KERNEL_3_12) += backport-3.12.o
compat-$(CPTCFG_KERNEL_3_13) += backport-3.13.o memneq.o
compat-$(CPTCFG_KERNEL_3_14) += backport-3.14.o
compat-$(CPTCFG_KERNEL_3_15) += backport-3.15.o
compat-$(CPTCFG_KERNEL_3_17) += backport-3.17.o
compat-$(CPTCFG_KERNEL_3_18) += backport-3.18.o
compat-$(CPTCFG_KERNEL_3_19) += backport-3.19.o
compat-$(CPTCFG_KERNEL_4_0) += backport-4.0.o
compat-$(CPTCFG_KERNEL_4_1) += backport-4.1.o
compat-$(CPTCFG_KERNEL_4_2) += backport-4.2.o
compat-$(CPTCFG_KERNEL_4_3) += backport-4.3.o
compat-$(CPTCFG_KERNEL_4_4) += backport-4.4.o
compat-$(CPTCFG_KERNEL_4_5) += backport-4.5.o
compat-$(CPTCFG_KERNEL_4_6) += backport-4.6.o
compat-$(CPTCFG_KERNEL_4_7) += backport-4.7.o
compat-$(CPTCFG_KERNEL_4_8) += backport-4.8.o
compat-$(CPTCFG_KERNEL_4_10) += backport-4.10.o
compat-$(CPTCFG_KERNEL_4_18) += backport-4.18.o
compat-$(CPTCFG_KERNEL_5_2) += backport-5.2.o backport-genetlink.o

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
compat-$(CPTCFG_BPAUTO_BUILD_SYSTEM_DATA_VERIFICATION) += lib-oid_registry.o
compat-$(CPTCFG_BPAUTO_REFCOUNT) += lib-refcount.o
compat-$(CPTCFG_BPAUTO_BUILD_WANT_DEV_COREDUMP) += drivers-base-devcoredump.o
compat-$(CPTCFG_BPAUTO_RHASHTABLE) += lib-rhashtable.o
compat-$(CPTCFG_BPAUTO_PUBLIC_KEY) += 
compat-$(CPTCFG_BPAUTO_BUCKET_LOCKS) += lib-bucket_locks.o
compat-$(CPTCFG_BPAUTO_PKCS7) += 
compat-$(CPTCFG_BPAUTO_BUILD_CRYPTO_LIB_ARC4) += lib-crypto-arc4.o

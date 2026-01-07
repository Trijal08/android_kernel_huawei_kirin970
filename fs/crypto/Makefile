obj-$(CONFIG_FS_ENCRYPTION)	+= fscrypto.o

fscrypto-y := crypto.o \
	      fname.o \
	      hkdf.o \
	      hooks.o \
	      keyring.o \
	      keysetup.o \
	      keysetup_v1.o \
	      policy.o

fscrypto-$(CONFIG_BLOCK) += bio.o
fscrypto-$(CONFIG_FS_ENCRYPTION_INLINE_CRYPT) += inline_crypt.o

# Huawei
fscrypto-y += fscrypt_extra_ctrl.o
EXTRA_CFLAGS := -I$(srctree)/lib/libc_sec/securec_v2/include

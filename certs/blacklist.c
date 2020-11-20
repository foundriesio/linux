// SPDX-License-Identifier: GPL-2.0-or-later
/* System hash blacklist.
 *
 * Copyright (C) 2016 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 */

#define pr_fmt(fmt) "blacklist: "fmt
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/key.h>
#include <linux/key-type.h>
#include <linux/sched.h>
#include <linux/ctype.h>
#include <linux/err.h>
#include <linux/seq_file.h>
#include <linux/uidgid.h>
#include <keys/system_keyring.h>
#include "blacklist.h"

/*
 * According to crypto/asymmetric_keys/x509_cert_parser.c:x509_note_pkey_algo(),
 * the size of the currently longest supported hash algorithm is 512 bits,
 * which translates into 128 hex characters.
 */
#define MAX_HASH_LEN	128

static const char tbs_prefix[] = "tbs";
static const char bin_prefix[] = "bin";

static struct key *blacklist_keyring;

/*
 * The description must be a type prefix, a colon and then an even number of
 * hex digits.  The hash is kept in the description.
 */
static int blacklist_vet_description(const char *desc)
{
	int i, prefix_len, tbs_step = 0, bin_step = 0;

	/* The following algorithm only works if prefix lengths match. */
	BUILD_BUG_ON(sizeof(tbs_prefix) != sizeof(bin_prefix));
	prefix_len = sizeof(tbs_prefix) - 1;
	for (i = 0; *desc; desc++, i++) {
		if (*desc == ':') {
			if (tbs_step == prefix_len)
				goto found_colon;
			if (bin_step == prefix_len)
				goto found_colon;
			return -EINVAL;
		}
		if (i >= prefix_len)
			return -EINVAL;
		if (*desc == tbs_prefix[i])
			tbs_step++;
		if (*desc == bin_prefix[i])
			bin_step++;
	}
	return -EINVAL;

found_colon:
	desc++;
	for (i = 0; *desc && i < MAX_HASH_LEN; desc++, i++) {
		if (!isxdigit(*desc) || isupper(*desc))
			return -EINVAL;
	}
	if (*desc)
		/* The hash is greater than MAX_HASH_LEN. */
		return -ENOPKG;

	/* Checks for an even number of hexadecimal characters. */
	if (i == 0 || i & 1)
		return -EINVAL;
	return 0;
}

/*
 * The hash to be blacklisted is expected to be in the description.  There will
 * be no payload.
 */
static int blacklist_preparse(struct key_preparsed_payload *prep)
{
	if (prep->datalen > 0)
		return -EINVAL;
	return 0;
}

static void blacklist_free_preparse(struct key_preparsed_payload *prep)
{
}

static void blacklist_describe(const struct key *key, struct seq_file *m)
{
	seq_puts(m, key->description);
}

static struct key_type key_type_blacklist = {
	.name			= "blacklist",
	.vet_description	= blacklist_vet_description,
	.preparse		= blacklist_preparse,
	.free_preparse		= blacklist_free_preparse,
	.instantiate		= generic_key_instantiate,
	.describe		= blacklist_describe,
};

static char *get_raw_hash(const u8 *hash, size_t hash_len,
		enum blacklist_hash_type hash_type)
{
	size_t type_len;
	const char *type_prefix;
	char *buffer, *p;

	switch (hash_type) {
	case BLACKLIST_HASH_X509_TBS:
		type_len = sizeof(tbs_prefix) - 1;
		type_prefix = tbs_prefix;
		break;
	case BLACKLIST_HASH_BINARY:
		type_len = sizeof(bin_prefix) - 1;
		type_prefix = bin_prefix;
		break;
	default:
		WARN_ON_ONCE(1);
		return ERR_PTR(-EINVAL);
	}
	buffer = kmalloc(type_len + 1 + hash_len * 2 + 1, GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);
	p = memcpy(buffer, type_prefix, type_len);
	p += type_len;
	*p++ = ':';
	bin2hex(p, hash, hash_len);
	p += hash_len * 2;
	*p = '\0';
	return buffer;
}

/**
 * mark_raw_hash_blacklisted - Add a hash to the system blacklist
 * @hash: The hash as a hex string with a type prefix (eg. "tbs:23aa429783")
 */
static int mark_raw_hash_blacklisted(const char *hash)
{
	key_ref_t key;

	key = key_create_or_update(make_key_ref(blacklist_keyring, true),
				   "blacklist",
				   hash,
				   NULL,
				   0,
				   ((KEY_POS_ALL & ~KEY_POS_SETATTR) |
				    KEY_USR_VIEW),
				   KEY_ALLOC_NOT_IN_QUOTA |
				   KEY_ALLOC_BUILT_IN);
	if (IS_ERR(key)) {
		pr_err("Problem blacklisting hash (%ld)\n", PTR_ERR(key));
		return PTR_ERR(key);
	}
	return 0;
}

int mark_hash_blacklisted(const u8 *hash, size_t hash_len,
		enum blacklist_hash_type hash_type)
{
	const char *buffer;

	buffer = get_raw_hash(hash, hash_len, hash_type);
	if (IS_ERR(buffer))
		return PTR_ERR(buffer);
	kfree(buffer);
	return 0;
}
/**
 * is_hash_blacklisted - Determine if a hash is blacklisted
 * @hash: The hash to be checked as a binary blob
 * @hash_len: The length of the binary hash
 * @hash_type: Type of hash
 */
int is_hash_blacklisted(const u8 *hash, size_t hash_len,
		enum blacklist_hash_type hash_type)
{
	key_ref_t kref;
	const char *buffer;
	int ret = 0;

	buffer = get_raw_hash(hash, hash_len, hash_type);
	if (IS_ERR(buffer))
		return PTR_ERR(buffer);
	kref = keyring_search(make_key_ref(blacklist_keyring, true),
			      &key_type_blacklist, buffer, false);
	if (!IS_ERR(kref)) {
		key_ref_put(kref);
		ret = -EKEYREJECTED;
	}

	kfree(buffer);
	return ret;
}
EXPORT_SYMBOL_GPL(is_hash_blacklisted);

int is_binary_blacklisted(const u8 *hash, size_t hash_len)
{
	if (is_hash_blacklisted(hash, hash_len, BLACKLIST_HASH_BINARY) ==
			-EKEYREJECTED)
		return -EPERM;

	return 0;
}
EXPORT_SYMBOL_GPL(is_binary_blacklisted);

/*
 * Initialise the blacklist
 */
static int __init blacklist_init(void)
{
	const char *const *bl;

	if (register_key_type(&key_type_blacklist) < 0)
		panic("Can't allocate system blacklist key type\n");

	blacklist_keyring =
		keyring_alloc(".blacklist",
			      GLOBAL_ROOT_UID, GLOBAL_ROOT_GID, current_cred(),
			      (KEY_POS_ALL & ~KEY_POS_SETATTR) |
			      KEY_USR_VIEW | KEY_USR_READ |
			      KEY_USR_SEARCH,
			      KEY_ALLOC_NOT_IN_QUOTA |
			      KEY_ALLOC_SET_KEEP,
			      NULL, NULL);
	if (IS_ERR(blacklist_keyring))
		panic("Can't allocate system blacklist keyring\n");

	for (bl = blacklist_hashes; *bl; bl++)
		if (mark_raw_hash_blacklisted(*bl) < 0)
			pr_err("- blacklisting failed\n");
	return 0;
}

/*
 * Must be initialised before we try and load the keys into the keyring.
 */
device_initcall(blacklist_init);

/* keys.c - Bluetooth key handling */

/*
 * Copyright (c) 2015 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>
#include <nanokernel.h>
#include <atomic.h>
#include <misc/util.h>

#include <bluetooth/log.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>
#include <bluetooth/hci.h>

#include "hci_core.h"
#include "smp.h"
#include "keys.h"

#if !defined(CONFIG_BLUETOOTH_DEBUG_KEYS)
#undef BT_DBG
#define BT_DBG(fmt, ...)
#endif

static struct bt_keys key_pool[CONFIG_BLUETOOTH_MAX_PAIRED];

#if defined(CONFIG_BLUETOOTH_SMP)
struct bt_keys *bt_keys_get_addr(const bt_addr_le_t *addr)
{
	struct bt_keys *keys;
	int i;

	BT_DBG("%s", bt_addr_le_str(addr));

	for (i = 0; i < ARRAY_SIZE(key_pool); i++) {
		keys = &key_pool[i];

		if (!bt_addr_le_cmp(&keys->addr, addr)) {
			return keys;
		}

		if (!bt_addr_le_cmp(&keys->addr, BT_ADDR_LE_ANY)) {
			bt_addr_le_copy(&keys->addr, addr);
			BT_DBG("created %p for %s", keys, bt_addr_le_str(addr));
			return keys;
		}
	}

	BT_DBG("unable to create keys for %s", bt_addr_le_str(addr));

	return NULL;
}
struct bt_keys *bt_keys_find(int type, const bt_addr_le_t *addr)
{
	int i;

	BT_DBG("type %d %s", type, bt_addr_le_str(addr));

	for (i = 0; i < ARRAY_SIZE(key_pool); i++) {
		if ((key_pool[i].keys & type) &&
		    !bt_addr_le_cmp(&key_pool[i].addr, addr)) {
			return &key_pool[i];
		}
	}

	return NULL;
}

struct bt_keys *bt_keys_get_type(int type, const bt_addr_le_t *addr)
{
	struct bt_keys *keys;

	BT_DBG("type %d %s", type, bt_addr_le_str(addr));

	keys = bt_keys_find(type, addr);
	if (keys) {
		return keys;
	}

	keys = bt_keys_get_addr(addr);
	if (!keys) {
		return NULL;
	}

	bt_keys_add_type(keys, type);

	return keys;
}

struct bt_keys *bt_keys_find_irk(const bt_addr_le_t *addr)
{
	int i;

	BT_DBG("%s", bt_addr_le_str(addr));

	if (!bt_addr_le_is_rpa(addr)) {
		return NULL;
	}

	for (i = 0; i < ARRAY_SIZE(key_pool); i++) {
		if (!(key_pool[i].keys & BT_KEYS_IRK)) {
			continue;
		}

		if (!bt_addr_cmp((bt_addr_t *)addr->val,
				 &key_pool[i].irk.rpa)) {
			BT_DBG("cached RPA %s for %s",
			       bt_addr_str(&key_pool[i].irk.rpa),
			       bt_addr_le_str(&key_pool[i].addr));
			return &key_pool[i];
		}
	}

	for (i = 0; i < ARRAY_SIZE(key_pool); i++) {
		if (!(key_pool[i].keys & BT_KEYS_IRK)) {
			continue;
		}

		if (bt_smp_irk_matches(key_pool[i].irk.val,
				       (bt_addr_t *)addr->val)) {
			BT_DBG("RPA %s matches %s",
			       bt_addr_str(&key_pool[i].irk.rpa),
			       bt_addr_le_str(&key_pool[i].addr));

			bt_addr_copy(&key_pool[i].irk.rpa,
				     (bt_addr_t *)addr->val);

			return &key_pool[i];
		}
	}

	BT_DBG("No IRK for %s", bt_addr_le_str(addr));

	return NULL;
}

struct bt_keys *bt_keys_find_addr(const bt_addr_le_t *addr)
{
	int i;

	BT_DBG("%s", bt_addr_le_str(addr));

	for (i = 0; i < ARRAY_SIZE(key_pool); i++) {
		if (!bt_addr_le_cmp(&key_pool[i].addr, addr)) {
			return &key_pool[i];
		}
	}

	return NULL;
}
#endif /* CONFIG_BLUETOOTH_SMP */

void bt_keys_add_type(struct bt_keys *keys, int type)
{
	keys->keys |= type;
}

void bt_keys_clear(struct bt_keys *keys, int type)
{
	BT_DBG("keys for %s type %d", bt_addr_le_str(&keys->addr), type);

	keys->keys &= ~type;

	if (!keys->keys) {
		memset(keys, 0, sizeof(*keys));
	}
}

#if defined(CONFIG_BLUETOOTH_BREDR)
static struct bt_keys *bt_keys_get_addr_br(const bt_addr_t *addr)
{
	struct bt_keys *keys;
	int i;

	BT_DBG("%s", bt_addr_str(addr));

	for (i = 0; i < ARRAY_SIZE(key_pool); i++) {
		keys = &key_pool[i];

		/*
		 * When both LE and BR/EDR keys are for the same device,
		 * the bt_addr_le_t is the public address, i.e. the same
		 * as the BR/EDR address.
		 */
		if (keys->addr.type == BT_ADDR_LE_PUBLIC &&
		    !bt_addr_cmp((const bt_addr_t *)keys->addr.val, addr)) {
			return keys;
		}

		/*
		 * BT_ADDR_LE_ANY has the same type of as BT_ADDR_LE_PUBLIC
		 * value. No need to make redudant comparision against
		 * BT_ADDR_LE_PUBLIC.
		 */
		if (!bt_addr_le_cmp(&keys->addr, BT_ADDR_LE_ANY)) {
			bt_addr_copy((bt_addr_t *)keys->addr.val, addr);
			BT_DBG("created %p for %s", keys, bt_addr_str(addr));
			return keys;
		}
	}

	BT_DBG("unable to create keys for %s", bt_addr_str(addr));

	return NULL;
}

struct bt_keys *bt_keys_find_link_key(const bt_addr_t *addr)
{
	struct bt_keys *keys;
	int i;

	BT_DBG("%s", bt_addr_str(addr));

	for (i = 0; i < ARRAY_SIZE(key_pool); i++) {
		keys = &key_pool[i];

		/*
		 * When both LE and BR/EDR keys are for the same device,
		 * the bt_addr_le_t is the public address, i.e. the same
		 * as the BR/EDR address.
		 */
		if (keys->addr.type == BT_ADDR_LE_PUBLIC &&
		    (keys->keys & BT_KEYS_LINK_KEY) &&
		    !bt_addr_cmp((const bt_addr_t *)keys->addr.val, addr)) {
			return keys;
		}
	}

	return NULL;
}

struct bt_keys *bt_keys_get_link_key(const bt_addr_t *addr)
{
	struct bt_keys *keys;

	BT_DBG("%s", bt_addr_str(addr));

	keys = bt_keys_find_link_key(addr);
	if (keys) {
		return keys;
	}

	keys = bt_keys_get_addr_br(addr);
	if (!keys) {
		return NULL;
	}

	bt_keys_add_type(keys, BT_KEYS_LINK_KEY);

	return keys;
}
#endif /* CONFIG_BLUETOOTH_BREDR */

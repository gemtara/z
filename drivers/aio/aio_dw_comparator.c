/*
 * Copyright (c) 2015 Intel Corporation.
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

#include <errno.h>
#include <stdio.h>

#include <nanokernel.h>
#include <device.h>
#include <init.h>
#include <aio_comparator.h>

#include "aio_dw_comparator.h"

#define INT_COMPARATORS_MASK	0x7FFFF

static int dw_aio_cmp_config(struct device *dev);

static int dw_aio_cmp_disable(struct device *dev, uint8_t index)
{
	struct dw_aio_cmp_dev_cfg_t *config =
	    (struct dw_aio_cmp_dev_cfg_t *)dev->config->config_info;
	struct dw_aio_cmp_t *regs =
	    (struct dw_aio_cmp_t *)config->base_address;

	if (index >= AIO_DW_CMP_COUNT) {
		return -EINVAL;
	}

	/* Disable interrupt to host */
	SCSS_INTERRUPT->int_comparators_host_mask |= (1 << index);

	/* Disable comparator <index> */
	regs->en &= ~(1 << index);
	/* Disable power in comparator <index> */
	regs->pwr &= ~(1 << index);

	return 0;
}

static int dw_aio_cmp_configure(struct device *dev, uint8_t index,
			 enum aio_cmp_polarity polarity,
			 enum aio_cmp_ref refsel,
			 aio_cmp_cb cb, void *param)
{
	struct dw_aio_cmp_dev_cfg_t *config =
	    (struct dw_aio_cmp_dev_cfg_t *)dev->config->config_info;
	struct dw_aio_cmp_dev_data_t *dev_data =
	    (struct dw_aio_cmp_dev_data_t *)dev->driver_data;
	struct dw_aio_cmp_t *regs =
	    (struct dw_aio_cmp_t *)config->base_address;
	uint32_t reg_val;

	/* index out of range */
	if (index >= AIO_DW_CMP_COUNT) {
		return -EINVAL;
	}

	/* make sure reference makes sense */
	if ((refsel != AIO_CMP_REF_A) && (refsel != AIO_CMP_REF_B))
		return -EINVAL;

	/* make sure polarity makes sense */
	if ((polarity != AIO_CMP_POL_RISE) && (polarity != AIO_CMP_POL_FALL))
		return -EINVAL;

	dev_data->cb[index].cb = cb;
	dev_data->cb[index].param = param;

	/* Disable interrupt to host */
	SCSS_INTERRUPT->int_comparators_host_mask |= (1 << index);

	/* Disable comparator <index> before config */
	regs->en &= ~(1 << index);
	regs->pwr &= ~(1 << index);

	/**
	 * Setup reference voltage source
	 * REF_A: bit ==> 0, REF_B: bit ==> 1
	 */
	reg_val = regs->ref_sel;
	if (refsel == AIO_CMP_REF_A)
		reg_val &= ~(1 << index);
	else
		reg_val |= (1 << index);
	regs->ref_sel = reg_val;

	/**
	 * Setup reference polarity
	 * RISING: bit ==> 0, FALLING: bit ==> 1
	 */
	reg_val = regs->ref_pol;
	if (polarity == AIO_CMP_POL_RISE)
		reg_val &= ~(1 << index);
	else
		reg_val |= (1 << index);
	regs->ref_pol = reg_val;

	/* Enable power of comparator <index> */
	regs->pwr |= (1 << index);

	/* Enable comparator <index> */
	regs->en |= (1 << index);

	/* Enable interrupt to host */
	SCSS_INTERRUPT->int_comparators_host_mask &= ~(1 << index);

	return 0;
}

void dw_aio_cmp_isr(struct device *dev)
{
	struct dw_aio_cmp_dev_cfg_t *config =
	    (struct dw_aio_cmp_dev_cfg_t *)dev->config->config_info;
	struct dw_aio_cmp_dev_data_t *dev_data =
	    (struct dw_aio_cmp_dev_data_t *)dev->driver_data;
	struct dw_aio_cmp_t *regs =
	    (struct dw_aio_cmp_t *)config->base_address;
	int i;
	int reg_stat_clr;

	reg_stat_clr = regs->stat_clr;
	for (i = 0; i < dev_data->num_cmp; i++) {
		if (reg_stat_clr & (1 << i)) {

			dw_aio_cmp_disable(dev, i);

			if (dev_data->cb[i].cb != NULL) {
				dev_data->cb[i].cb(dev_data->cb[i].param);
			}
		}
	}

	/* Clear interrupt status by writing 1s */
	regs->stat_clr = reg_stat_clr;
}

static struct aio_cmp_driver_api dw_aio_cmp_funcs = {
	.disable = dw_aio_cmp_disable,
	.configure = dw_aio_cmp_configure,
};

int dw_aio_cmp_init(struct device *dev)
{
	struct dw_aio_cmp_dev_cfg_t *config =
	    (struct dw_aio_cmp_dev_cfg_t *)dev->config->config_info;
	struct dw_aio_cmp_dev_data_t *dev_data =
	    (struct dw_aio_cmp_dev_data_t *)dev->driver_data;
	struct dw_aio_cmp_t *regs =
	    (struct dw_aio_cmp_t *)config->base_address;
	int i;

	if ((config->base_address == 0) || (config->interrupt_num == 0))
		return -EINVAL;

	if (config->config_func) {
		i =  config->config_func(dev);
		if (i != 0)
			return i;
	}

	dev->driver_api = &dw_aio_cmp_funcs;

	/* Clear host interrupt mask */
	SCSS_INTERRUPT->int_comparators_host_mask |= INT_COMPARATORS_MASK;

	/* Clear comparator interrupt status */
	regs->stat_clr |= INT_COMPARATORS_MASK;

	/* Disable all comparators */
	regs->en &= ~INT_COMPARATORS_MASK;

	/* Power down all comparators */
	regs->pwr &= ~INT_COMPARATORS_MASK;

	/* Clear callback pointers */
	for (i = 0; i < dev_data->num_cmp; i++) {
		dev_data->cb[i].cb = NULL;
		dev_data->cb[i].param = NULL;
	}

	irq_enable(config->interrupt_num);

	return 0;
}

struct dw_aio_cmp_dev_cfg_t dw_aio_cmp_dev_config = {
	.base_address = AIO_DW_COMPARATOR_BASE_ADDR,
	.interrupt_num = INT_AIO_CMP_IRQ,
	.config_func = dw_aio_cmp_config,
};

struct dw_aio_cmp_dev_data_t dw_aio_cmp_dev_data = {
	.num_cmp = AIO_DW_CMP_COUNT,
};

DEVICE_INIT(dw_aio_cmp, CONFIG_AIO_DW_COMPARATOR_DEV_NAME, &dw_aio_cmp_init,
			&dw_aio_cmp_dev_data, &dw_aio_cmp_dev_config,
			SECONDARY, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);

static int dw_aio_cmp_config(struct device *dev)
{
	ARG_UNUSED(dev);

	IRQ_CONNECT(INT_AIO_CMP_IRQ, CONFIG_AIO_DW_COMPARATOR_IRQ_PRI,
		    dw_aio_cmp_isr,
		    DEVICE_GET(dw_aio_cmp), 0);
	return 0;
}

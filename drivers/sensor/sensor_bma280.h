/*
 * Copyright (c) 2016 Intel Corporation
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

#ifndef __SENSOR_BMA280_H__
#define __SENSOR_BMA280_H__

#include <device.h>
#include <misc/util.h>
#include <stdint.h>

#ifndef CONFIG_SENSOR_DEBUG
#define DBG(...) { ; }
#else
#include <misc/printk.h>
#define DBG printk
#endif /* CONFIG_SENSOR_DEBUG */

#if CONFIG_BMA280_I2C_ADDR_0x18
	#define BMA280_I2C_ADDRESS	0x18
#elif CONFIG_BMA280_I2C_ADDR_0x19
	#define BMA280_I2C_ADDRESS	0x19
#endif

#define BMA280_REG_CHIP_ID		0x00
#define BMA280_CHIP_ID			0xFB

#define BMA280_REG_PMU_BW		0x10
#if CONFIG_BMA280_PMU_BW_1
	#define BMA280_PMU_BW		0x08
#elif CONFIG_BMA280_PMU_BW_2
	#define BMA280_PMU_BW		0x09
#elif CONFIG_BMA280_PMU_BW_3
	#define BMA280_PMU_BW		0x0A
#elif CONFIG_BMA280_PMU_BW_4
	#define BMA280_PMU_BW		0x0B
#elif CONFIG_BMA280_PMU_BW_5
	#define BMA280_PMU_BW		0x0C
#elif CONFIG_BMA280_PMU_BW_6
	#define BMA280_PMU_BW		0x0D
#elif CONFIG_BMA280_PMU_BW_7
	#define BMA280_PMU_BW		0x0E
#elif CONFIG_BMA280_PMU_BW_8
	#define BMA280_PMU_BW		0x0F
#endif

/*
 * accel and slope scale measured in nano-m/s^2 instead
 * of m/s^2 to avoid using struct sensor_value for it
 */
#define GRAVITY_CONST			9807
#define BMA280_REG_PMU_RANGE		0x0F
#if CONFIG_BMA280_PMU_RANGE_2G
	#define BMA280_PMU_RANGE	0x03
	#define BMA280_ACCEL_SCALE	(244 * GRAVITY_CONST)
	#define BMA280_SLOPE_TH_SCALE	3910
#elif CONFIG_BMA280_PMU_RANGE_4G
	#define BMA280_PMU_RANGE	0x05
	#define BMA280_ACCEL_SCALE	(488 * GRAVITY_CONST)
	#define BMA280_SLOPE_TH_SCALE	7810
#elif CONFIG_BMA280_PMU_RANGE_8G
	#define BMA280_PMU_RANGE	0x08
	#define BMA280_ACCEL_SCALE	(977 * GRAVITY_CONST)
	#define BMA280_SLOPE_TH_SCALE	15630
#elif CONFIG_BMA280_PMU_RANGE_16G
	#define BMA280_PMU_RANGE	0x0C
	#define BMA280_ACCEL_SCALE	(1953 * GRAVITY_CONST)
	#define BMA280_SLOPE_TH_SCALE	31250
#endif

#define BMA280_REG_TEMP			0x08

#define BMA280_REG_INT_STATUS_0		0x09
#define BMA280_BIT_SLOPE_INT_STATUS	BIT(2)
#define BMA280_REG_INT_STATUS_1		0x0A
#define BMA280_BIT_DATA_INT_STATUS	BIT(7)

#define BMA280_REG_INT_EN_0		0x16
#define BMA280_BIT_SLOPE_EN_X		BIT(0)
#define BMA280_BIT_SLOPE_EN_Y		BIT(1)
#define BMA280_BIT_SLOPE_EN_Z		BIT(2)
#define BMA280_SLOPE_EN_XYZ (BMA280_BIT_SLOPE_EN_X | \
		BMA280_BIT_SLOPE_EN_Y | BMA280_BIT_SLOPE_EN_X)

#define BMA280_REG_INT_EN_1		0x17
#define BMA280_BIT_DATA_EN		BIT(4)

#define BMA280_REG_INT_MAP_0		0x19
#define BMA280_INT_MAP_0_BIT_SLOPE	BIT(2)

#define BMA280_REG_INT_MAP_1		0x1A
#define BMA280_INT_MAP_1_BIT_DATA	BIT(0)

#define BMA280_REG_INT_RST_LATCH	0x21
#define BMA280_INT_MODE_LATCH		0x0F
#define BMA280_BIT_INT_LATCH_RESET	BIT(7)

#define BMA280_REG_INT_5		0x27
#define BMA280_SLOPE_DUR_SHIFT		0
#define BMA280_SLOPE_DUR_MASK		(3 << BMA280_SLOPE_DUR_SHIFT)

#define BMA280_REG_SLOPE_TH		0x28

#define BMA280_REG_ACCEL_X_LSB		0x2
#define BMA280_REG_ACCEL_Y_LSB		0x4
#define BMA280_REG_ACCEL_Z_LSB		0x6
#define BMA280_ACCEL_LSB_BITS		6
#define BMA280_ACCEL_LSB_SHIFT		2
#define BMA280_ACCEL_LSB_MASK		(0x3F << BMA280_ACCEL_LSB_SHIFT)
#define BMA280_REG_ACCEL_X_MSB		0x3
#define BMA280_REG_ACCEL_Y_MSB		0x5
#define BMA280_REG_ACCEL_Z_MSB		0x7

#define BMA280_FIBER_PRIORITY		10
#define BMA280_FIBER_STACKSIZE_UNIT	1024

struct bma280_data {
	struct device *i2c;
	int16_t x_sample;
	int16_t y_sample;
	int16_t z_sample;
	int8_t temp_sample;

#ifdef CONFIG_BMA280_TRIGGER
	struct device *gpio;

	struct sensor_trigger data_ready_trigger;
	sensor_trigger_handler_t data_ready_handler;

	struct sensor_trigger any_motion_trigger;
	sensor_trigger_handler_t any_motion_handler;

#if defined(CONFIG_BMA280_TRIGGER_OWN_FIBER)
	char __stack fiber_stack[CONFIG_BMA280_FIBER_STACK_SIZE];
	struct nano_sem gpio_sem;
#elif defined(CONFIG_BMA280_TRIGGER_GLOBAL_FIBER)
	struct sensor_work work;
#endif

#endif /* CONFIG_BMA280_TRIGGER */
};

#ifdef CONFIG_BMA280_TRIGGER
int bma280_reg_write(struct bma280_data *drv_data,
		     uint8_t reg, uint8_t val);

int bma280_reg_read(struct bma280_data *drv_data,
		    uint8_t reg, uint8_t *val);

int bma280_reg_update(struct bma280_data *drv_data,
		      uint8_t reg, uint8_t mask, uint8_t val);

int bma280_trigger_set(struct device *dev,
		       const struct sensor_trigger *trig,
		       sensor_trigger_handler_t handler);

int bma280_attr_set(struct device *dev,
		    enum sensor_channel chan,
		    enum sensor_attribute attr,
		    const struct sensor_value *val);

int bma280_init_interrupt(struct device *dev);
#endif

#endif /* __SENSOR_BMA280_H__ */

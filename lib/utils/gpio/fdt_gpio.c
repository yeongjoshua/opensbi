/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Anup Patel <anup.patel@wdc.com>
 */

#include <libfdt.h>
#include <sbi/sbi_error.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/gpio/fdt_gpio.h>

/* List of FDT gpio drivers generated at compile time */
extern struct fdt_gpio *fdt_gpio_drivers[];
extern unsigned long fdt_gpio_drivers_size;

static int fdt_gpio_init(const void *fdt, int nodeoff)
{
	int pos, rc;
	struct fdt_gpio *drv;
	const struct fdt_match *match;

	/* Check "gpio-controller" property */
	if (!fdt_getprop(fdt, nodeoff, "gpio-controller", &rc))
		return SBI_EINVAL;

	/* Try all GPIO drivers one-by-one */
	for (pos = 0; pos < fdt_gpio_drivers_size; pos++) {
		drv = fdt_gpio_drivers[pos];

		match = fdt_match_node(fdt, nodeoff, drv->match_table);
		if (match && drv->init) {
			rc = drv->init(fdt, nodeoff, match);
			if (rc == SBI_ENODEV)
				continue;
			if (rc)
				return rc;
			return 0;
		}
	}

	return SBI_ENOSYS;
}

static int fdt_gpio_chip_find(const void *fdt, int nodeoff,
			      struct gpio_chip **out_chip)
{
	int rc;
	struct gpio_chip *chip = gpio_chip_find(nodeoff);

	if (!chip) {
		/* GPIO chip not found so initialize matching driver */
		rc = fdt_gpio_init(fdt, nodeoff);
		if (rc)
			return rc;

		/* Try to find GPIO chip again */
		chip = gpio_chip_find(nodeoff);
		if (!chip)
			return SBI_ENOSYS;
	}

	if (out_chip)
		*out_chip = chip;

	return 0;
}

int fdt_gpio_pin_get(const void *fdt, int nodeoff, int index,
		     struct gpio_pin *out_pin)
{
	int rc;
	struct fdt_gpio *drv;
	struct gpio_chip *chip = NULL;
	struct fdt_phandle_args pargs;

	if (!fdt || (nodeoff < 0) || (index < 0) || !out_pin)
		return SBI_EINVAL;

	pargs.node_offset = pargs.args_count = 0;
	rc = fdt_parse_phandle_with_args(fdt, nodeoff,
					 "gpios", "#gpio-cells",
					 index, &pargs);
	if (rc)
		return rc;

	rc = fdt_gpio_chip_find(fdt, pargs.node_offset, &chip);
	if (rc)
		return rc;

	drv = chip->driver;
	if (!drv || !drv->xlate)
		return SBI_ENOSYS;

	return drv->xlate(chip, &pargs, out_pin);
}

int fdt_gpio_simple_xlate(struct gpio_chip *chip,
			  const struct fdt_phandle_args *pargs,
			  struct gpio_pin *out_pin)
{
	if ((pargs->args_count < 2) || (chip->ngpio <= pargs->args[0]))
		return SBI_EINVAL;

	out_pin->chip = chip;
	out_pin->offset = pargs->args[0];
	out_pin->flags = pargs->args[1];
	return 0;
}

/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/export.h>
#include <mach/rpm.h>

#include "rpm_resources.h"
#include "qdss-priv.h"

enum {
	QDSS_CLK_OFF,
	QDSS_CLK_ON_DBG,
	QDSS_CLK_ON_HSDBG,
};

struct qdss_ctx {
	struct kobject	*modulekobj;
	uint8_t		max_clk;
	uint8_t		clk_count;
	struct mutex	clk_mutex;
};

static struct qdss_ctx qdss;


struct kobject *qdss_get_modulekobj(void)
{
	return qdss.modulekobj;
}

/**
 * qdss_clk_enable - enable qdss clocks
 *
 * Enables qdss clocks via RPM if they aren't already enabled, otherwise
 * increments the reference count.
 *
 * CONTEXT:
 * Might sleep. Uses a mutex lock. Should be called from a non-atomic context.
 *
 * RETURNS:
 * 0 on success, non-zero on failure
 */
int qdss_clk_enable(void)
{
	int ret;
	struct msm_rpm_iv_pair iv;

	mutex_lock(&qdss.clk_mutex);
	if (qdss.clk_count == 0) {
		iv.id = MSM_RPM_ID_QDSS_CLK;
		if (qdss.max_clk)
			iv.value = QDSS_CLK_ON_HSDBG;
		else
			iv.value = QDSS_CLK_ON_DBG;
		ret = msm_rpmrs_set(MSM_RPM_CTX_SET_0, &iv, 1);
		if (WARN(ret, "qdss clks not enabled (%d)\n", ret))
			goto err_clk;
	}
	qdss.clk_count++;
	mutex_unlock(&qdss.clk_mutex);
	return 0;
err_clk:
	mutex_unlock(&qdss.clk_mutex);
	return ret;
}
EXPORT_SYMBOL(qdss_clk_enable);

/**
 * qdss_clk_disable - disable qdss clocks
 *
 * Disables qdss clocks via RPM if the reference count is one, otherwise
 * decrements the reference count.
 *
 * CONTEXT:
 * Might sleep. Uses a mutex lock. Should be called from a non-atomic context.
 */
void qdss_clk_disable(void)
{
	int ret;
	struct msm_rpm_iv_pair iv;

	mutex_lock(&qdss.clk_mutex);
	if (WARN(qdss.clk_count == 0, "qdss clks are unbalanced\n"))
		goto out;
	if (qdss.clk_count == 1) {
		iv.id = MSM_RPM_ID_QDSS_CLK;
		iv.value = QDSS_CLK_OFF;
		ret = msm_rpmrs_set(MSM_RPM_CTX_SET_0, &iv, 1);
		WARN(ret, "qdss clks not disabled (%d)\n", ret);
	}
	qdss.clk_count--;
out:
	mutex_unlock(&qdss.clk_mutex);
}
EXPORT_SYMBOL(qdss_clk_disable);

#define QDSS_ATTR(name)						\
static struct kobj_attribute name##_attr =				\
		__ATTR(name, S_IRUGO | S_IWUSR, name##_show, name##_store)

static ssize_t max_clk_store(struct kobject *kobj,
			struct kobj_attribute *attr,
			const char *buf, size_t n)
{
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	qdss.max_clk = val;
	return n;
}
static ssize_t max_clk_show(struct kobject *kobj,
			struct kobj_attribute *attr,
			char *buf)
{
	unsigned long val = qdss.max_clk;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
QDSS_ATTR(max_clk);

static int __init qdss_sysfs_init(void)
{
	int ret;

	qdss.modulekobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (!qdss.modulekobj) {
		pr_err("failed to find QDSS sysfs module kobject\n");
		ret = -ENOENT;
		goto err;
	}

	ret = sysfs_create_file(qdss.modulekobj, &max_clk_attr.attr);
	if (ret) {
		pr_err("failed to create QDSS sysfs max_clk attribute\n");
		goto err;
	}

	return 0;
err:
	return ret;
}

static void __exit qdss_sysfs_exit(void)
{
	sysfs_remove_file(qdss.modulekobj, &max_clk_attr.attr);
}

static int __init qdss_init(void)
{
	int ret;

	mutex_init(&qdss.clk_mutex);

	ret = qdss_sysfs_init();
	if (ret)
		goto err_sysfs;

	pr_info("QDSS initialized\n");
	return 0;
err_sysfs:
	mutex_destroy(&qdss.clk_mutex);
	pr_err("QDSS init failed\n");
	return ret;
}
module_init(qdss_init);

static void __exit qdss_exit(void)
{
	qdss_sysfs_exit();
	mutex_destroy(&qdss.clk_mutex);
}
module_exit(qdss_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm Debug SubSystem Driver");

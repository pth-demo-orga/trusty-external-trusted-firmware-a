/*
 * Copyright (c) 2015-2019, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <string.h>

#include <libfdt.h>

#include <platform_def.h>

#include <arch_helpers.h>
#include <common/bl_common.h>
#include <common/debug.h>
#include <common/desc_image_load.h>
#include <common/fdt_fixup.h>
#include <lib/optee_utils.h>
#include <lib/utils.h>
#include <plat/common/platform.h>

#include "qemu_private.h"


/* Data structure which holds the extents of the trusted SRAM for BL2 */
static meminfo_t bl2_tzram_layout __aligned(CACHE_WRITEBACK_GRANULE);

void bl2_early_platform_setup2(u_register_t arg0, u_register_t arg1,
			       u_register_t arg2, u_register_t arg3)
{
	meminfo_t *mem_layout = (void *)arg1;

	/* Initialize the console to provide early debug support */
	qemu_console_init();

	/* Setup the BL2 memory layout */
	bl2_tzram_layout = *mem_layout;

	plat_qemu_io_setup();
}

static void security_setup(void)
{
	/*
	 * This is where a TrustZone address space controller and other
	 * security related peripherals, would be configured.
	 */
}

#ifdef SPD_trusty

#define GIC_SPI 0
#define GIC_PPI 1

static int spd_add_dt_node(void *fdt)
{
	int offs, trusty_offs, root_offs;
	int gic, ipi;
	int len;
	const uint32_t *prop;

	if (fdt_path_offset(fdt, "/trusty") >= 0) {
		WARN("Trusty Device Tree node already exists!\n");
		return 0;
	}

	offs = fdt_node_offset_by_compatible(fdt, -1, "arm,cortex-a15-gic");
	if (offs < 0)
		offs = fdt_node_offset_by_compatible(fdt, -1, "arm,gic-v3");

	if (offs < 0)
		return -1;
	gic = fdt_get_phandle(fdt, offs);
	if (!gic) {
		WARN("Failed to get gic phandle\n");
		return -1;
	}
	INFO("Found gic phandle 0x%x\n", gic);

	offs = fdt_path_offset(fdt, "/");
	if (offs < 0)
		return -1;
	root_offs = offs;

	/* CustomIPI node for pre 5.10 linux driver */
	offs = fdt_add_subnode(fdt, offs, "interrupt-controller");
	if (offs < 0)
		return -1;
	ipi = fdt_get_max_phandle(fdt) + 1;
	if (fdt_setprop_u32(fdt, offs, "phandle", 1))
		return -1;
	INFO("Found ipi phandle 0x%x\n", ipi);

	ipi = fdt_get_phandle(fdt, offs);
	if (!ipi) {
		WARN("Failed to get ipi phandle\n");
		return -1;
	}

	if (fdt_appendprop_string(fdt, offs, "compatible", "android,CustomIPI"))
		return -1;
	if (fdt_setprop_u32(fdt, offs, "#interrupt-cells", 1))
		return -1;
	if (fdt_setprop_u32(fdt, offs, "interrupt-controller", 0))
		return -1;

	offs = fdt_add_subnode(fdt, root_offs, "trusty");
	if (offs < 0)
		return -1;
	trusty_offs = offs;

	if (fdt_appendprop_string(fdt, offs, "compatible", "android,trusty-smc-v1"))
		return -1;
	if (fdt_setprop_u32(fdt, offs, "ranges", 0))
		return -1;
	if (fdt_setprop_u32(fdt, offs, "#address-cells", 2))
		return -1;
	if (fdt_setprop_u32(fdt, offs, "#size-cells", 2))
		return -1;

	offs = fdt_add_subnode(fdt, trusty_offs, "irq");
	if (offs < 0)
		return -1;
	if (fdt_appendprop_string(fdt, offs, "compatible", "android,trusty-irq-v1"))
		return -1;
	if (fdt_appendprop_u32(fdt, offs, "interrupt-templates", ipi))
		return -1;
	if (fdt_appendprop_u32(fdt, offs, "interrupt-templates", 0))
		return -1;
	if (fdt_appendprop_u32(fdt, offs, "interrupt-templates", gic))
		return -1;
	if (fdt_appendprop_u32(fdt, offs, "interrupt-templates", 1))
		return -1;
	if (fdt_appendprop_u32(fdt, offs, "interrupt-templates", GIC_PPI))
		return -1;
	if (fdt_appendprop_u32(fdt, offs, "interrupt-templates", 4))
		return -1;
	if (fdt_appendprop_u32(fdt, offs, "interrupt-templates", gic))
		return -1;
	if (fdt_appendprop_u32(fdt, offs, "interrupt-templates", 1))
		return -1;
	if (fdt_appendprop_u32(fdt, offs, "interrupt-templates", GIC_SPI))
		return -1;
	if (fdt_appendprop_u32(fdt, offs, "interrupt-templates", 4))
		return -1;

	/* CustomIPI range for pre 5.10 linux driver */
	if (fdt_appendprop_u32(fdt, offs, "interrupt-ranges", 0))
		return -1;
	if (fdt_appendprop_u32(fdt, offs, "interrupt-ranges", 15))
		return -1;
	if (fdt_appendprop_u32(fdt, offs, "interrupt-ranges", 0))
		return -1;

	if (fdt_appendprop_u32(fdt, offs, "interrupt-ranges", 16))
		return -1;
	if (fdt_appendprop_u32(fdt, offs, "interrupt-ranges", 31))
		return -1;
	if (fdt_appendprop_u32(fdt, offs, "interrupt-ranges", 1))
		return -1;
	if (fdt_appendprop_u32(fdt, offs, "interrupt-ranges", 32))
		return -1;
	if (fdt_appendprop_u32(fdt, offs, "interrupt-ranges", 63))
		return -1;
	if (fdt_appendprop_u32(fdt, offs, "interrupt-ranges", 2))
		return -1;

	if (fdt_appendprop_u32(fdt, offs, "ipi-range", 8))  /* beg */
		return -1;
	if (fdt_appendprop_u32(fdt, offs, "ipi-range", 15)) /* end */
		return -1;
	if (fdt_appendprop_u32(fdt, offs, "ipi-range", 8))  /* ipi_base */
		return -1;

	offs = fdt_add_subnode(fdt, trusty_offs, "log");
	if (offs < 0)
		return -1;
	if (fdt_appendprop_string(fdt, offs, "compatible", "android,trusty-log-v1"))
		return -1;

	offs = fdt_add_subnode(fdt, trusty_offs, "test");
	if (offs < 0)
		return -1;
	if (fdt_appendprop_string(fdt, offs, "compatible", "android,trusty-test-v1"))
		return -1;

	offs = fdt_add_subnode(fdt, trusty_offs, "virtio");
	if (offs < 0)
		return -1;
	if (fdt_appendprop_string(fdt, offs, "compatible", "android,trusty-virtio-v1"))
		return -1;

	offs = fdt_node_offset_by_compatible(fdt, -1, "arm,armv8-timer");
	if (offs < 0)
		offs = fdt_node_offset_by_compatible(fdt, -1, "arm,armv7-timer");
	if (offs < 0)
		return -1;

	prop = fdt_getprop(fdt, offs, "interrupts", &len);
	if (fdt_setprop_inplace_namelen_partial(fdt, offs, "interrupts",
	                                        strlen("interrupts"), 0,
	                                        prop + len / 4 / 2, len / 4))
		return -1;

	return 0;
}

#else

static int spd_add_dt_node(void *fdt)
{
	return 0;
}

#endif

static int qemu_dt_fixup_securemem(void *fdt)
{
	/*
	 * QEMU adds a device tree node for secure memory. Linux fails to ignore
	 * it and will crash when it allocates memory out of this secure memory
	 * region. We currently don't use this node for anything, remove it.
	 */

	int offs;
	const char *prop;
	const char memory_device_type[] = "memory";

	offs = -1;
	while (true) {
		offs = fdt_node_offset_by_prop_value(fdt, offs, "device_type",
		                                     memory_device_type,
		                                     sizeof(memory_device_type)
		                                     );
		if (offs < 0)
			break;

		prop = fdt_getprop(fdt, offs, "status", NULL);
		if (prop == NULL)
			continue;
		if ((strcmp(prop, "disabled") != 0))
			continue;
		prop = fdt_getprop(fdt, offs, "secure-status", NULL);
		if (prop == NULL)
			continue;
		if ((strcmp(prop, "okay") != 0))
			continue;

		if (fdt_del_node(fdt, offs)) {
			return -1;
		}
		INFO("Removed secure memory node\n");
	}

	return 0;
}

static void update_dt(void)
{
	int ret;
	void *fdt = (void *)(uintptr_t)ARM_PRELOADED_DTB_BASE;

	ret = fdt_open_into(fdt, fdt, PLAT_QEMU_DT_MAX_SIZE);
	if (ret < 0) {
		ERROR("Invalid Device Tree at %p: error %d\n", fdt, ret);
		return;
	}

	if (qemu_dt_fixup_securemem(fdt)) {
		ERROR("Failed to fixup secure-mem Device Tree node\n");
		return;
	}

	if (dt_add_psci_node(fdt)) {
		ERROR("Failed to add PSCI Device Tree node\n");
		return;
	}

	if (dt_add_psci_cpu_enable_methods(fdt)) {
		ERROR("Failed to add PSCI cpu enable methods in Device Tree\n");
		return;
	}

	if (spd_add_dt_node(fdt)) {
		ERROR("Failed to add SPD Device Tree node\n");
		return;
	}

	ret = fdt_pack(fdt);
	if (ret < 0)
		ERROR("Failed to pack Device Tree at %p: error %d\n", fdt, ret);
}

void bl2_platform_setup(void)
{
	security_setup();
	update_dt();

	/* TODO Initialize timer */
}

#ifdef __aarch64__
#define QEMU_CONFIGURE_BL2_MMU(...)	qemu_configure_mmu_el1(__VA_ARGS__)
#else
#define QEMU_CONFIGURE_BL2_MMU(...)	qemu_configure_mmu_svc_mon(__VA_ARGS__)
#endif

void bl2_plat_arch_setup(void)
{
	QEMU_CONFIGURE_BL2_MMU(bl2_tzram_layout.total_base,
			      bl2_tzram_layout.total_size,
			      BL_CODE_BASE, BL_CODE_END,
			      BL_RO_DATA_BASE, BL_RO_DATA_END,
			      BL_COHERENT_RAM_BASE, BL_COHERENT_RAM_END);
}

/*******************************************************************************
 * Gets SPSR for BL32 entry
 ******************************************************************************/
static uint32_t qemu_get_spsr_for_bl32_entry(void)
{
#ifdef __aarch64__
	/*
	 * The Secure Payload Dispatcher service is responsible for
	 * setting the SPSR prior to entry into the BL3-2 image.
	 */
	return 0;
#else
	return SPSR_MODE32(MODE32_svc, SPSR_T_ARM, SPSR_E_LITTLE,
			   DISABLE_ALL_EXCEPTIONS);
#endif
}

/*******************************************************************************
 * Gets SPSR for BL33 entry
 ******************************************************************************/
static uint32_t qemu_get_spsr_for_bl33_entry(void)
{
	uint32_t spsr;
#ifdef __aarch64__
	unsigned int mode;

	/* Figure out what mode we enter the non-secure world in */
	mode = (el_implemented(2) != EL_IMPL_NONE) ? MODE_EL2 : MODE_EL1;

	/*
	 * TODO: Consider the possibility of specifying the SPSR in
	 * the FIP ToC and allowing the platform to have a say as
	 * well.
	 */
	spsr = SPSR_64(mode, MODE_SP_ELX, DISABLE_ALL_EXCEPTIONS);
#else
	spsr = SPSR_MODE32(MODE32_svc,
		    plat_get_ns_image_entrypoint() & 0x1,
		    SPSR_E_LITTLE, DISABLE_ALL_EXCEPTIONS);
#endif
	return spsr;
}

static int qemu_bl2_handle_post_image_load(unsigned int image_id)
{
	int err = 0;
	bl_mem_params_node_t *bl_mem_params = get_bl_mem_params_node(image_id);
#if defined(SPD_opteed) || defined(AARCH32_SP_OPTEE)
	bl_mem_params_node_t *pager_mem_params = NULL;
	bl_mem_params_node_t *paged_mem_params = NULL;
#endif

	assert(bl_mem_params);

	switch (image_id) {
	case BL32_IMAGE_ID:
#if defined(SPD_opteed) || defined(AARCH32_SP_OPTEE)
		pager_mem_params = get_bl_mem_params_node(BL32_EXTRA1_IMAGE_ID);
		assert(pager_mem_params);

		paged_mem_params = get_bl_mem_params_node(BL32_EXTRA2_IMAGE_ID);
		assert(paged_mem_params);

		err = parse_optee_header(&bl_mem_params->ep_info,
					 &pager_mem_params->image_info,
					 &paged_mem_params->image_info);
		if (err != 0) {
			WARN("OPTEE header parse error.\n");
		}

#if defined(SPD_opteed)
		/*
		 * OP-TEE expect to receive DTB address in x2.
		 * This will be copied into x2 by dispatcher.
		 */
		bl_mem_params->ep_info.args.arg3 = ARM_PRELOADED_DTB_BASE;
#else /* case AARCH32_SP_OPTEE */
		bl_mem_params->ep_info.args.arg0 =
					bl_mem_params->ep_info.args.arg1;
		bl_mem_params->ep_info.args.arg1 = 0;
		bl_mem_params->ep_info.args.arg2 = ARM_PRELOADED_DTB_BASE;
		bl_mem_params->ep_info.args.arg3 = 0;
#endif
#endif
		bl_mem_params->ep_info.spsr = qemu_get_spsr_for_bl32_entry();
		break;

	case BL33_IMAGE_ID:
#ifdef AARCH32_SP_OPTEE
		/* AArch32 only core: OP-TEE expects NSec EP in register LR */
		pager_mem_params = get_bl_mem_params_node(BL32_IMAGE_ID);
		assert(pager_mem_params);
		pager_mem_params->ep_info.lr_svc = bl_mem_params->ep_info.pc;
#endif

#if ARM_LINUX_KERNEL_AS_BL33
		/*
		 * According to the file ``Documentation/arm64/booting.txt`` of
		 * the Linux kernel tree, Linux expects the physical address of
		 * the device tree blob (DTB) in x0, while x1-x3 are reserved
		 * for future use and must be 0.
		 */
		bl_mem_params->ep_info.args.arg0 =
			(u_register_t)ARM_PRELOADED_DTB_BASE;
		bl_mem_params->ep_info.args.arg1 = 0U;
		bl_mem_params->ep_info.args.arg2 = 0U;
		bl_mem_params->ep_info.args.arg3 = 0U;
#else
		/* BL33 expects to receive the primary CPU MPID (through r0) */
		bl_mem_params->ep_info.args.arg0 = 0xffff & read_mpidr();
#endif

		bl_mem_params->ep_info.spsr = qemu_get_spsr_for_bl33_entry();
		break;
	default:
		/* Do nothing in default case */
		break;
	}

	return err;
}

/*******************************************************************************
 * This function can be used by the platforms to update/use image
 * information for given `image_id`.
 ******************************************************************************/
int bl2_plat_handle_post_image_load(unsigned int image_id)
{
	return qemu_bl2_handle_post_image_load(image_id);
}

uintptr_t plat_get_ns_image_entrypoint(void)
{
	return NS_IMAGE_OFFSET;
}

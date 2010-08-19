/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at:
 *      http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When using or redistributing this file, you may do so under the
 * License only. No other modification of this header is permitted.
 *
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright(c) 2007-2010 Intel Corporation. All rights reserved.
 */

/*
 * Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.
 */

/* IntelVersion: 1.167 scm_061610_003709 */

#include "ixgbe_type.h"
#include "ixgbe_api.h"
#include "ixgbe_common.h"
#include "ixgbe_phy.h"

u32 ixgbe_get_pcie_msix_count_82598(struct ixgbe_hw *hw);
s32 ixgbe_init_ops_82598(struct ixgbe_hw *hw);
static s32 ixgbe_get_link_capabilities_82598(struct ixgbe_hw *hw,
    ixgbe_link_speed *speed, bool *autoneg);
static enum ixgbe_media_type ixgbe_get_media_type_82598(struct ixgbe_hw *hw);
s32 ixgbe_fc_enable_82598(struct ixgbe_hw *hw, s32 packetbuf_num);
static s32 ixgbe_start_mac_link_82598(struct ixgbe_hw *hw,
					bool autoneg_wait_to_complete);
static s32 ixgbe_check_mac_link_82598(struct ixgbe_hw *hw,
    ixgbe_link_speed *speed, bool *link_up, bool link_up_wait_to_complete);
static s32 ixgbe_setup_mac_link_82598(struct ixgbe_hw *hw,
    ixgbe_link_speed speed, bool autoneg,
    bool autoneg_wait_to_complete);
static s32 ixgbe_setup_copper_link_82598(struct ixgbe_hw *hw,
    ixgbe_link_speed speed, bool autoneg, bool autoneg_wait_to_complete);
static s32 ixgbe_reset_hw_82598(struct ixgbe_hw *hw);
s32 ixgbe_start_hw_82598(struct ixgbe_hw *hw);
void ixgbe_enable_relaxed_ordering_82598(struct ixgbe_hw *hw);
s32 ixgbe_set_vmdq_82598(struct ixgbe_hw *hw, u32 rar, u32 vmdq);
static s32 ixgbe_clear_vmdq_82598(struct ixgbe_hw *hw, u32 rar, u32 vmdq);
s32 ixgbe_set_vfta_82598(struct ixgbe_hw *hw, u32 vlan,
    u32 vind, bool vlan_on);
static s32 ixgbe_clear_vfta_82598(struct ixgbe_hw *hw);
s32 ixgbe_read_analog_reg8_82598(struct ixgbe_hw *hw, u32 reg, u8 *val);
s32 ixgbe_write_analog_reg8_82598(struct ixgbe_hw *hw, u32 reg, u8 val);
s32 ixgbe_read_i2c_eeprom_82598(struct ixgbe_hw *hw, u8 byte_offset,
    u8 *eeprom_data);
u32 ixgbe_get_supported_physical_layer_82598(struct ixgbe_hw *hw);
s32 ixgbe_init_phy_ops_82598(struct ixgbe_hw *hw);
void ixgbe_set_lan_id_multi_port_pcie_82598(struct ixgbe_hw *hw);
void ixgbe_set_pcie_completion_timeout(struct ixgbe_hw *hw);
static s32 ixgbe_validate_link_ready(struct ixgbe_hw *hw);

/*
 * ixgbe_set_pcie_completion_timeout - set pci-e completion timeout
 * @hw: pointer to the HW structure
 *
 * The defaults for 82598 should be in the range of 50us to 50ms,
 * however the hardware default for these parts is 500us to 1ms which is less
 * than the 10ms recommended by the pci-e spec.  To address this we need to
 * increase the value to either 10ms to 250ms for capability version 1 config,
 * or 16ms to 55ms for version 2.
 */
void
ixgbe_set_pcie_completion_timeout(struct ixgbe_hw *hw)
{
	u32 gcr = IXGBE_READ_REG(hw, IXGBE_GCR);
	u16 pcie_devctl2;

	/* only take action if timeout value is defaulted to 0 */
	if (gcr & IXGBE_GCR_CMPL_TMOUT_MASK)
		goto out;

	/*
	 * if capababilities version is type 1 we can write the
	 * timeout of 10ms to 250ms through the GCR register
	 */
	if (!(gcr & IXGBE_GCR_CAP_VER2)) {
		gcr |= IXGBE_GCR_CMPL_TMOUT_10ms;
		goto out;
	}

	/*
	 * for version 2 capabilities we need to write the config space
	 * directly in order to set the completion timeout value for
	 * 16ms to 55ms
	 */
	pcie_devctl2 = IXGBE_READ_PCIE_WORD(hw, IXGBE_PCI_DEVICE_CONTROL2);
	pcie_devctl2 |= IXGBE_PCI_DEVICE_CONTROL2_16ms;
	IXGBE_WRITE_PCIE_WORD(hw, IXGBE_PCI_DEVICE_CONTROL2, pcie_devctl2);
out:
	/* disable completion timeout resend */
	gcr &= ~IXGBE_GCR_CMPL_TMOUT_RESEND;
	IXGBE_WRITE_REG(hw, IXGBE_GCR, gcr);
}

/*
 * ixgbe_get_pcie_msix_count_82598 - Gets MSI-X vector count
 * @hw: pointer to hardware structure
 *
 * Read PCIe configuration space, and get the MSI-X vector count from
 * the capabilities table.
 */
u32
ixgbe_get_pcie_msix_count_82598(struct ixgbe_hw *hw)
{
	u32 msix_count = 18;

	DEBUGFUNC("ixgbe_get_pcie_msix_count_82598");

	if (hw->mac.msix_vectors_from_pcie) {
		msix_count = IXGBE_READ_PCIE_WORD(hw,
		    IXGBE_PCIE_MSIX_82598_CAPS);
		msix_count &= IXGBE_PCIE_MSIX_TBL_SZ_MASK;

		/*
		 * MSI-X count is zero-based in HW, so increment to give
		 * proper value
		 */
		msix_count++;
	}
	return (msix_count);
}

/*
 * ixgbe_init_ops_82598 - Inits func ptrs and MAC type
 * @hw: pointer to hardware structure
 *
 * Initialize the function pointers and assign the MAC type for 82598.
 * Does not touch the hardware.
 */
s32
ixgbe_init_ops_82598(struct ixgbe_hw *hw)
{
	struct ixgbe_mac_info *mac = &hw->mac;
	struct ixgbe_phy_info *phy = &hw->phy;
	s32 ret_val;

	DEBUGFUNC("ixgbe_init_ops_82598");

	ret_val = ixgbe_init_phy_ops_generic(hw);
	ret_val = ixgbe_init_ops_generic(hw);

	/* PHY */
	phy->ops.init = &ixgbe_init_phy_ops_82598;

	/* MAC */
	mac->ops.start_hw = &ixgbe_start_hw_82598;
	mac->ops.enable_relaxed_ordering = &ixgbe_enable_relaxed_ordering_82598;
	mac->ops.reset_hw = &ixgbe_reset_hw_82598;
	mac->ops.get_media_type = &ixgbe_get_media_type_82598;
	mac->ops.get_supported_physical_layer =
	    &ixgbe_get_supported_physical_layer_82598;
	mac->ops.read_analog_reg8 = &ixgbe_read_analog_reg8_82598;
	mac->ops.write_analog_reg8 = &ixgbe_write_analog_reg8_82598;
	mac->ops.set_lan_id = &ixgbe_set_lan_id_multi_port_pcie_82598;

	/* RAR, Multicast, VLAN */
	mac->ops.set_vmdq = &ixgbe_set_vmdq_82598;
	mac->ops.clear_vmdq = &ixgbe_clear_vmdq_82598;
	mac->ops.set_vfta = &ixgbe_set_vfta_82598;
	mac->ops.clear_vfta = &ixgbe_clear_vfta_82598;

	/* Flow Control */
	mac->ops.fc_enable = &ixgbe_fc_enable_82598;

	mac->mcft_size = 128;
	mac->vft_size = 128;
	mac->num_rar_entries = 16;
	mac->max_tx_queues = 32;
	mac->max_rx_queues = 64;
	mac->max_msix_vectors = ixgbe_get_pcie_msix_count_82598(hw);

	/* SFP+ Module */
	phy->ops.read_i2c_eeprom = &ixgbe_read_i2c_eeprom_82598;

	/* Link */
	mac->ops.check_link = &ixgbe_check_mac_link_82598;
	mac->ops.setup_link = &ixgbe_setup_mac_link_82598;
	mac->ops.flap_tx_laser = NULL;
	mac->ops.get_link_capabilities =
	    &ixgbe_get_link_capabilities_82598;

	return (ret_val);
}

/*
 * ixgbe_init_phy_ops_82598 - PHY/SFP specific init
 * @hw: pointer to hardware structure
 *
 * Initialize any function pointers that were not able to be
 * set during init_shared_code because the PHY/SFP type was
 * not known.  Perform the SFP init if necessary.
 *
 */
s32
ixgbe_init_phy_ops_82598(struct ixgbe_hw *hw)
{
	struct ixgbe_mac_info *mac = &hw->mac;
	struct ixgbe_phy_info *phy = &hw->phy;
	s32 ret_val = IXGBE_SUCCESS;
	u16 list_offset, data_offset;

	DEBUGFUNC("ixgbe_init_phy_ops_82598");

	/* Identify the PHY */
	phy->ops.identify(hw);

	/* Overwrite the link function pointers if copper PHY */
	if (mac->ops.get_media_type(hw) == ixgbe_media_type_copper) {
		mac->ops.setup_link = &ixgbe_setup_copper_link_82598;
		mac->ops.get_link_capabilities =
		    &ixgbe_get_copper_link_capabilities_generic;
	}

	switch (hw->phy.type) {
	case ixgbe_phy_tn:
		phy->ops.setup_link = &ixgbe_setup_phy_link_tnx;
		phy->ops.check_link = &ixgbe_check_phy_link_tnx;
		phy->ops.get_firmware_version =
		    &ixgbe_get_phy_firmware_version_tnx;
		break;
	case ixgbe_phy_aq:
		phy->ops.get_firmware_version =
		    &ixgbe_get_phy_firmware_version_generic;
		break;
	case ixgbe_phy_nl:
		phy->ops.reset = &ixgbe_reset_phy_nl;

		/* Call SFP+ identify routine to get the SFP+ module type */
		ret_val = phy->ops.identify_sfp(hw);
		if (ret_val != IXGBE_SUCCESS)
			goto out;
		else if (hw->phy.sfp_type == ixgbe_sfp_type_unknown) {
			ret_val = IXGBE_ERR_SFP_NOT_SUPPORTED;
			goto out;
		}

		/* Check to see if SFP+ module is supported */
		ret_val = ixgbe_get_sfp_init_sequence_offsets(hw,
		    &list_offset, &data_offset);
		if (ret_val != IXGBE_SUCCESS) {
			ret_val = IXGBE_ERR_SFP_NOT_SUPPORTED;
			goto out;
		}
		break;
	default:
		break;
	}
out:
	return (ret_val);
}

/*
 * ixgbe_start_hw_82598 - Prepare hardware for Tx/Rx
 * @hw: pointer to hardware structure
 *
 * Starts the hardware using the generic start_hw function.
 * Disables relaxed ordering Then set pcie completion timeout
 */
s32
ixgbe_start_hw_82598(struct ixgbe_hw *hw)
{
	u32 regval;
	u32 i;
	s32 ret_val = IXGBE_SUCCESS;

	DEBUGFUNC("ixgbe_start_hw_82598");

	ret_val = ixgbe_start_hw_generic(hw);

	/*
	 * Disable relaxed ordering
	 */
	for (i = 0; ((i < hw->mac.max_tx_queues) &&
	    (i < IXGBE_DCA_MAX_QUEUES_82598)); i++) {
		regval = IXGBE_READ_REG(hw, IXGBE_DCA_TXCTRL(i));
		regval &= ~IXGBE_DCA_TXCTRL_TX_WB_RO_EN;
		IXGBE_WRITE_REG(hw, IXGBE_DCA_TXCTRL(i), regval);
	}

	for (i = 0; ((i < hw->mac.max_rx_queues) &&
	    (i < IXGBE_DCA_MAX_QUEUES_82598)); i++) {
		regval = IXGBE_READ_REG(hw, IXGBE_DCA_RXCTRL(i));
		regval &= ~(IXGBE_DCA_RXCTRL_DESC_WRO_EN |
		    IXGBE_DCA_RXCTRL_DESC_HSRO_EN);
		IXGBE_WRITE_REG(hw, IXGBE_DCA_RXCTRL(i), regval);
	}

	/* set the completion timeout for interface */
	if (ret_val == IXGBE_SUCCESS)
		ixgbe_set_pcie_completion_timeout(hw);

	return (ret_val);
}

/*
 * ixgbe_get_link_capabilities_82598 - Determines link capabilities
 * @hw: pointer to hardware structure
 * @speed: pointer to link speed
 * @autoneg: boolean auto-negotiation value
 *
 * Determines the link capabilities by reading the AUTOC register.
 */
static s32
ixgbe_get_link_capabilities_82598(struct ixgbe_hw *hw,
    ixgbe_link_speed *speed, bool *autoneg)
{
	s32 status = IXGBE_SUCCESS;
	u32 autoc = 0;

	DEBUGFUNC("ixgbe_get_link_capabilities_82598");

	/*
	 * Determine link capabilities based on the stored value of AUTOC,
	 * which represents EEPROM defaults.  If AUTOC value has not been
	 * stored, use the current register value.
	 */
	if (hw->mac.orig_link_settings_stored)
		autoc = hw->mac.orig_autoc;
	else
		autoc = IXGBE_READ_REG(hw, IXGBE_AUTOC);

	switch (autoc & IXGBE_AUTOC_LMS_MASK) {
	case IXGBE_AUTOC_LMS_1G_LINK_NO_AN:
		*speed = IXGBE_LINK_SPEED_1GB_FULL;
		*autoneg = false;
		break;

	case IXGBE_AUTOC_LMS_10G_LINK_NO_AN:
		*speed = IXGBE_LINK_SPEED_10GB_FULL;
		*autoneg = false;
		break;

	case IXGBE_AUTOC_LMS_1G_AN:
		*speed = IXGBE_LINK_SPEED_1GB_FULL;
		*autoneg = true;
		break;

	case IXGBE_AUTOC_LMS_KX4_AN:
	case IXGBE_AUTOC_LMS_KX4_AN_1G_AN:
		*speed = IXGBE_LINK_SPEED_UNKNOWN;
		if (autoc & IXGBE_AUTOC_KX4_SUPP)
			*speed |= IXGBE_LINK_SPEED_10GB_FULL;
		if (autoc & IXGBE_AUTOC_KX_SUPP)
			*speed |= IXGBE_LINK_SPEED_1GB_FULL;
		*autoneg = true;
		break;

	default:
		status = IXGBE_ERR_LINK_SETUP;
		break;
	}

	return (status);
}

/*
 * ixgbe_get_media_type_82598 - Determines media type
 * @hw: pointer to hardware structure
 *
 * Returns the media type (fiber, copper, backplane)
 */
static enum ixgbe_media_type
ixgbe_get_media_type_82598(struct ixgbe_hw *hw)
{
	enum ixgbe_media_type media_type;

	DEBUGFUNC("ixgbe_get_media_type_82598");

	/* Detect if there is a copper PHY attached. */
	if (hw->phy.type == ixgbe_phy_cu_unknown ||
	    hw->phy.type == ixgbe_phy_tn ||
	    hw->phy.type == ixgbe_phy_aq) {
		media_type = ixgbe_media_type_copper;
		goto out;
	}

	/* Media type for I82598 is based on device ID */
	switch (hw->device_id) {
	case IXGBE_DEV_ID_82598:
	case IXGBE_DEV_ID_82598_BX:
		/* Default device ID is mezzanine card KX/KX4 */
		media_type = ixgbe_media_type_backplane;
		break;
	case IXGBE_DEV_ID_82598AF_DUAL_PORT:
	case IXGBE_DEV_ID_82598AF_SINGLE_PORT:
	case IXGBE_DEV_ID_82598_DA_DUAL_PORT:
	case IXGBE_DEV_ID_82598_SR_DUAL_PORT_EM:
	case IXGBE_DEV_ID_82598EB_XF_LR:
	case IXGBE_DEV_ID_82598EB_SFP_LOM:
		media_type = ixgbe_media_type_fiber;
		break;
	case IXGBE_DEV_ID_82598EB_CX4:
	case IXGBE_DEV_ID_82598_CX4_DUAL_PORT:
		media_type = ixgbe_media_type_cx4;
		break;
	case IXGBE_DEV_ID_82598AT:
	case IXGBE_DEV_ID_82598AT2:
		media_type = ixgbe_media_type_copper;
		break;
	default:
		media_type = ixgbe_media_type_unknown;
		break;
	}
out:
	return (media_type);
}

/*
 * ixgbe_fc_enable_82598 - Enable flow control
 * @hw: pointer to hardware structure
 * @packetbuf_num: packet buffer number (0-7)
 *
 * Enable flow control according to the current settings.
 */
s32
ixgbe_fc_enable_82598(struct ixgbe_hw *hw, s32 packetbuf_num)
{
	s32 ret_val = IXGBE_SUCCESS;
	u32 fctrl_reg;
	u32 rmcs_reg;
	u32 reg;
	u32 link_speed = 0;
	bool link_up;

	DEBUGFUNC("ixgbe_fc_enable_82598");

	/*
	 * On 82598 having Rx FC on causes resets while doing 1G
	 * so if it's on turn it off once we know link_speed. For
	 * more details see 82598 Specification update.
	 */
	hw->mac.ops.check_link(hw, &link_speed, &link_up, false);
	if (link_up && link_speed == IXGBE_LINK_SPEED_1GB_FULL) {
		switch (hw->fc.requested_mode) {
		case ixgbe_fc_full:
			hw->fc.requested_mode = ixgbe_fc_tx_pause;
			break;
		case ixgbe_fc_rx_pause:
			hw->fc.requested_mode = ixgbe_fc_none;
			break;
		default:
			/* no change */
			break;
		}
	}

	/* Negotiate the fc mode to use */
	ret_val = ixgbe_fc_autoneg(hw);
	if (ret_val == IXGBE_ERR_FLOW_CONTROL)
		goto out;

	/* Disable any previous flow control settings */
	fctrl_reg = IXGBE_READ_REG(hw, IXGBE_FCTRL);
	fctrl_reg &= ~(IXGBE_FCTRL_RFCE | IXGBE_FCTRL_RPFCE);

	rmcs_reg = IXGBE_READ_REG(hw, IXGBE_RMCS);
	rmcs_reg &= ~(IXGBE_RMCS_TFCE_PRIORITY | IXGBE_RMCS_TFCE_802_3X);

	/*
	 * The possible values of fc.current_mode are:
	 * 0: Flow control is completely disabled
	 * 1: Rx flow control is enabled (we can receive pause frames,
	 *    but not send pause frames).
	 * 2: Tx flow control is enabled (we can send pause frames but
	 *    we do not support receiving pause frames).
	 * 3: Both Rx and Tx flow control (symmetric) are enabled.
	 * other: Invalid.
	 */
	switch (hw->fc.current_mode) {
	case ixgbe_fc_none:
		/*
		 * Flow control is disabled by software override or autoneg.
		 * The code below will actually disable it in the HW.
		 */
		break;
	case ixgbe_fc_rx_pause:
		/*
		 * Rx Flow control is enabled and Tx Flow control is
		 * disabled by software override. Since there really
		 * isn't a way to advertise that we are capable of RX
		 * Pause ONLY, we will advertise that we support both
		 * symmetric and asymmetric Rx PAUSE.  Later, we will
		 * disable the adapter's ability to send PAUSE frames.
		 */
		fctrl_reg |= IXGBE_FCTRL_RFCE;
		break;
	case ixgbe_fc_tx_pause:
		/*
		 * Tx Flow control is enabled, and Rx Flow control is
		 * disabled by software override.
		 */
		rmcs_reg |= IXGBE_RMCS_TFCE_802_3X;
		break;
	case ixgbe_fc_full:
		/* Flow control (both Rx and Tx) is enabled by SW override. */
		fctrl_reg |= IXGBE_FCTRL_RFCE;
		rmcs_reg |= IXGBE_RMCS_TFCE_802_3X;
		break;
	default:
		DEBUGOUT("Flow control param set incorrectly\n");
		ret_val = IXGBE_ERR_CONFIG;
		goto out;
	}

	/* Set 802.3x based flow control settings. */
	fctrl_reg |= IXGBE_FCTRL_DPF;
	IXGBE_WRITE_REG(hw, IXGBE_FCTRL, fctrl_reg);
	IXGBE_WRITE_REG(hw, IXGBE_RMCS, rmcs_reg);

	/* Set up and enable Rx high/low water mark thresholds, enable XON. */
	if (hw->fc.current_mode & ixgbe_fc_tx_pause) {
		if (hw->fc.send_xon) {
			IXGBE_WRITE_REG(hw, IXGBE_FCRTL(packetbuf_num),
			    (hw->fc.low_water | IXGBE_FCRTL_XONE));
		} else {
			IXGBE_WRITE_REG(hw, IXGBE_FCRTL(packetbuf_num),
			    hw->fc.low_water);
		}

		IXGBE_WRITE_REG(hw, IXGBE_FCRTH(packetbuf_num),
		    (hw->fc.high_water | IXGBE_FCRTH_FCEN));
	}

	/* Configure pause time (2 TCs per register) */
	reg = IXGBE_READ_REG(hw, IXGBE_FCTTV(packetbuf_num / 2));
	if ((packetbuf_num & 1) == 0)
		reg = (reg & 0xFFFF0000) | hw->fc.pause_time;
	else
		reg = (reg & 0x0000FFFF) | (hw->fc.pause_time << 16);
	IXGBE_WRITE_REG(hw, IXGBE_FCTTV(packetbuf_num / 2), reg);

	IXGBE_WRITE_REG(hw, IXGBE_FCRTV, (hw->fc.pause_time >> 1));

out:
	return (ret_val);
}

/*
 * ixgbe_start_mac_link_82598 - Configures MAC link settings
 * @hw: pointer to hardware structure
 *
 * Configures link settings based on values in the ixgbe_hw struct.
 * Restarts the link.  Performs autonegotiation if needed.
 */
static s32
ixgbe_start_mac_link_82598(struct ixgbe_hw *hw, bool autoneg_wait_to_complete)
{
	u32 autoc_reg;
	u32 links_reg;
	u32 i;
	s32 status = IXGBE_SUCCESS;

	DEBUGFUNC("ixgbe_start_mac_link_82598");

	/* Restart link */
	autoc_reg = IXGBE_READ_REG(hw, IXGBE_AUTOC);
	autoc_reg |= IXGBE_AUTOC_AN_RESTART;
	IXGBE_WRITE_REG(hw, IXGBE_AUTOC, autoc_reg);

	/* Only poll for autoneg to complete if specified to do so */
	if (autoneg_wait_to_complete) {
		if ((autoc_reg & IXGBE_AUTOC_LMS_MASK) ==
		    IXGBE_AUTOC_LMS_KX4_AN ||
		    (autoc_reg & IXGBE_AUTOC_LMS_MASK) ==
		    IXGBE_AUTOC_LMS_KX4_AN_1G_AN) {
			links_reg = 0; /* Just in case Autoneg time = 0 */
			for (i = 0; i < IXGBE_AUTO_NEG_TIME; i++) {
				links_reg = IXGBE_READ_REG(hw, IXGBE_LINKS);
				if (links_reg & IXGBE_LINKS_KX_AN_COMP)
					break;
				msec_delay(100);
			}
			if (!(links_reg & IXGBE_LINKS_KX_AN_COMP)) {
				status = IXGBE_ERR_AUTONEG_NOT_COMPLETE;
				DEBUGOUT("Autonegotiation did not complete.\n");
			}
		}
	}

	/* Add delay to filter out noises during initial link setup */
	msec_delay(50);

	return (status);
}

/*
 * ixgbe_check_mac_link_82598 - Get link/speed status
 * @hw: pointer to hardware structure
 * @speed: pointer to link speed
 * @link_up: true is link is up, false otherwise
 * @link_up_wait_to_complete: bool used to wait for link up or not
 *
 * Reads the links register to determine if link is up and the current speed
 */
static s32
ixgbe_check_mac_link_82598(struct ixgbe_hw *hw, ixgbe_link_speed *speed,
    bool *link_up, bool link_up_wait_to_complete)
{
	u32 links_reg;
	u32 i;
	u16 link_reg, adapt_comp_reg;

	DEBUGFUNC("ixgbe_check_mac_link_82598");

	/*
	 * SERDES PHY requires us to read link status from undocumented
	 * register 0xC79F.  Bit 0 set indicates link is up/ready; clear
	 * indicates link down.  OxC00C is read to check that the XAUI lanes
	 * are active.  Bit 0 clear indicates active; set indicates inactive.
	 */
	if (hw->phy.type == ixgbe_phy_nl) {
		hw->phy.ops.read_reg(hw, 0xC79F, IXGBE_TWINAX_DEV, &link_reg);
		hw->phy.ops.read_reg(hw, 0xC79F, IXGBE_TWINAX_DEV, &link_reg);
		hw->phy.ops.read_reg(hw, 0xC00C, IXGBE_TWINAX_DEV,
		    &adapt_comp_reg);
		if (link_up_wait_to_complete) {
			for (i = 0; i < IXGBE_LINK_UP_TIME; i++) {
				if ((link_reg & 1) &&
				    ((adapt_comp_reg & 1) == 0)) {
					*link_up = true;
					break;
				} else {
					*link_up = false;
				}
				msec_delay(100);
				hw->phy.ops.read_reg(hw, 0xC79F,
				    IXGBE_TWINAX_DEV, &link_reg);
				hw->phy.ops.read_reg(hw, 0xC00C,
				    IXGBE_TWINAX_DEV, &adapt_comp_reg);
			}
		} else {
			if ((link_reg & 1) &&
			    ((adapt_comp_reg & 1) == 0))
				*link_up = true;
			else
				*link_up = false;
		}

		if (*link_up == false)
			goto out;
	}

	links_reg = IXGBE_READ_REG(hw, IXGBE_LINKS);
	if (link_up_wait_to_complete) {
		for (i = 0; i < IXGBE_LINK_UP_TIME; i++) {
			if (links_reg & IXGBE_LINKS_UP) {
				*link_up = true;
				break;
			} else {
				*link_up = false;
			}
			msec_delay(100);
			links_reg = IXGBE_READ_REG(hw, IXGBE_LINKS);
		}
	} else {
		if (links_reg & IXGBE_LINKS_UP)
			*link_up = true;
		else
			*link_up = false;
	}

	if (links_reg & IXGBE_LINKS_SPEED)
		*speed = IXGBE_LINK_SPEED_10GB_FULL;
	else
		*speed = IXGBE_LINK_SPEED_1GB_FULL;

	if ((hw->device_id == IXGBE_DEV_ID_82598AT2) && (*link_up == true) &&
	    (ixgbe_validate_link_ready(hw) != IXGBE_SUCCESS))
		*link_up = false;

	/* if link is down, zero out the current_mode */
	if (*link_up == false) {
		hw->fc.current_mode = ixgbe_fc_none;
		hw->fc.fc_was_autonegged = false;
	}

out:
	return (IXGBE_SUCCESS);
}

/*
 * ixgbe_setup_mac_link_82598 - Set MAC link speed
 * @hw: pointer to hardware structure
 * @speed: new link speed
 * @autoneg: true if autonegotiation enabled
 * @autoneg_wait_to_complete: true when waiting for completion is needed
 *
 * Set the link speed in the AUTOC register and restarts link.
 */
static s32
ixgbe_setup_mac_link_82598(struct ixgbe_hw *hw,
    ixgbe_link_speed speed, bool autoneg,
    bool autoneg_wait_to_complete)
{
	s32 status = IXGBE_SUCCESS;
	ixgbe_link_speed link_capabilities = IXGBE_LINK_SPEED_UNKNOWN;
	u32 curr_autoc = IXGBE_READ_REG(hw, IXGBE_AUTOC);
	u32 autoc = curr_autoc;
	u32 link_mode = autoc & IXGBE_AUTOC_LMS_MASK;

	DEBUGFUNC("ixgbe_setup_mac_link_82598");

	/* Check to see if speed passed in is supported. */
	(void) ixgbe_get_link_capabilities(hw, &link_capabilities, &autoneg);
	speed &= link_capabilities;

	if (speed == IXGBE_LINK_SPEED_UNKNOWN) {
		status = IXGBE_ERR_LINK_SETUP;
	} else if (link_mode == IXGBE_AUTOC_LMS_KX4_AN ||
	    link_mode == IXGBE_AUTOC_LMS_KX4_AN_1G_AN) {
		/* Set KX4/KX support according to speed requested */
		autoc &= ~IXGBE_AUTOC_KX4_KX_SUPP_MASK;
		if (speed & IXGBE_LINK_SPEED_10GB_FULL)
			autoc |= IXGBE_AUTOC_KX4_SUPP;
		if (speed & IXGBE_LINK_SPEED_1GB_FULL)
			autoc |= IXGBE_AUTOC_KX_SUPP;
		if (autoc != curr_autoc)
			IXGBE_WRITE_REG(hw, IXGBE_AUTOC, autoc);
	}

	if (status == IXGBE_SUCCESS) {
		/*
		 * Setup and restart the link based on the new values in
		 * ixgbe_hw This will write the AUTOC register based on the new
		 * stored values
		 */
		status = ixgbe_start_mac_link_82598(hw,
		    autoneg_wait_to_complete);
	}

	return (status);
}

/*
 * ixgbe_setup_copper_link_82598 - Set the PHY autoneg advertised field
 * @hw: pointer to hardware structure
 * @speed: new link speed
 * @autoneg: true if autonegotiation enabled
 * @autoneg_wait_to_complete: true if waiting is needed to complete
 *
 * Sets the link speed in the AUTOC register in the MAC and restarts link.
 */
static s32
ixgbe_setup_copper_link_82598(struct ixgbe_hw *hw,
    ixgbe_link_speed speed,
    bool autoneg,
    bool autoneg_wait_to_complete)
{
	s32 status;

	DEBUGFUNC("ixgbe_setup_copper_link_82598");

	/* Setup the PHY according to input speed */
	status = hw->phy.ops.setup_link_speed(hw, speed, autoneg,
	    autoneg_wait_to_complete);

	/* Set up MAC */
	(void) ixgbe_start_mac_link_82598(hw, autoneg_wait_to_complete);

	return (status);
}

/*
 * ixgbe_reset_hw_82598 - Performs hardware reset
 * @hw: pointer to hardware structure
 *
 * Resets the hardware by resetting the transmit and receive units, masks and
 * clears all interrupts, performing a PHY reset, and performing a link (MAC)
 * reset.
 */
static s32
ixgbe_reset_hw_82598(struct ixgbe_hw *hw)
{
	s32 status = IXGBE_SUCCESS;
	s32 phy_status = IXGBE_SUCCESS;
	u32 ctrl;
	u32 gheccr;
	u32 i;
	u32 autoc;
	u8  analog_val;

	DEBUGFUNC("ixgbe_reset_hw_82598");

	/* Call adapter stop to disable tx/rx and clear interrupts */
	hw->mac.ops.stop_adapter(hw);

	/*
	 * Power up the Atlas Tx lanes if they are currently powered down.
	 * Atlas Tx lanes are powered down for MAC loopback tests, but
	 * they are not automatically restored on reset.
	 */
	hw->mac.ops.read_analog_reg8(hw, IXGBE_ATLAS_PDN_LPBK, &analog_val);
	if (analog_val & IXGBE_ATLAS_PDN_TX_REG_EN) {
		/* Enable Tx Atlas so packets can be transmitted again */
		hw->mac.ops.read_analog_reg8(hw, IXGBE_ATLAS_PDN_LPBK,
		    &analog_val);
		analog_val &= ~IXGBE_ATLAS_PDN_TX_REG_EN;
		hw->mac.ops.write_analog_reg8(hw, IXGBE_ATLAS_PDN_LPBK,
		    analog_val);

		hw->mac.ops.read_analog_reg8(hw, IXGBE_ATLAS_PDN_10G,
		    &analog_val);
		analog_val &= ~IXGBE_ATLAS_PDN_TX_10G_QL_ALL;
		hw->mac.ops.write_analog_reg8(hw, IXGBE_ATLAS_PDN_10G,
		    analog_val);

		hw->mac.ops.read_analog_reg8(hw, IXGBE_ATLAS_PDN_1G,
		    &analog_val);
		analog_val &= ~IXGBE_ATLAS_PDN_TX_1G_QL_ALL;
		hw->mac.ops.write_analog_reg8(hw, IXGBE_ATLAS_PDN_1G,
		    analog_val);

		hw->mac.ops.read_analog_reg8(hw, IXGBE_ATLAS_PDN_AN,
		    &analog_val);
		analog_val &= ~IXGBE_ATLAS_PDN_TX_AN_QL_ALL;
		hw->mac.ops.write_analog_reg8(hw, IXGBE_ATLAS_PDN_AN,
		    analog_val);
	}

	/* Reset PHY */
	if (hw->phy.reset_disable == false) {
		/* PHY ops must be identified and initialized prior to reset */

		/* Init PHY and function pointers, perform SFP setup */
		phy_status = hw->phy.ops.init(hw);
		if (phy_status == IXGBE_ERR_SFP_NOT_SUPPORTED)
			goto reset_hw_out;
		else if (phy_status == IXGBE_ERR_SFP_NOT_PRESENT)
			goto no_phy_reset;

		hw->phy.ops.reset(hw);
	}

no_phy_reset:
	/*
	 * Prevent the PCI-E bus from from hanging by disabling PCI-E master
	 * access and verify no pending requests before reset
	 */
	(void) ixgbe_disable_pcie_master(hw);

mac_reset_top:

	/*
	 * Issue global reset to the MAC.  This needs to be a SW reset.
	 * If link reset is used, it might reset the MAC when mng is using it
	 */
	ctrl = IXGBE_READ_REG(hw, IXGBE_CTRL);
	IXGBE_WRITE_REG(hw, IXGBE_CTRL, (ctrl | IXGBE_CTRL_RST));
	IXGBE_WRITE_FLUSH(hw);

	/* Poll for reset bit to self-clear indicating reset is complete */
	for (i = 0; i < 10; i++) {
		usec_delay(1);
		ctrl = IXGBE_READ_REG(hw, IXGBE_CTRL);
		if (!(ctrl & IXGBE_CTRL_RST))
			break;
	}
	if (ctrl & IXGBE_CTRL_RST) {
		status = IXGBE_ERR_RESET_FAILED;
		DEBUGOUT("Reset polling failed to complete.\n");
	}

	/*
	 * Double resets are required for recovery from certain error
	 * conditions.  Between resets, it is necessary to stall to allow time
	 * for any pending HW events to complete.  We use 1usec since that is
	 * what is needed for ixgbe_disable_pcie_master().  The second reset
	 * then clears out any effects of those events.
	 */
	if (hw->mac.flags & IXGBE_FLAGS_DOUBLE_RESET_REQUIRED) {
		hw->mac.flags &= ~IXGBE_FLAGS_DOUBLE_RESET_REQUIRED;
		usec_delay(1);
		goto mac_reset_top;
	}
	msec_delay(50);

	gheccr = IXGBE_READ_REG(hw, IXGBE_GHECCR);
	gheccr &= ~((1 << 21) | (1 << 18) | (1 << 9) | (1 << 6));
	IXGBE_WRITE_REG(hw, IXGBE_GHECCR, gheccr);

	/*
	 * Store the original AUTOC value if it has not been
	 * stored off yet.  Otherwise restore the stored original
	 * AUTOC value since the reset operation sets back to deaults.
	 */
	autoc = IXGBE_READ_REG(hw, IXGBE_AUTOC);
	if (hw->mac.orig_link_settings_stored == false) {
		hw->mac.orig_autoc = autoc;
		hw->mac.orig_link_settings_stored = true;
	} else if (autoc != hw->mac.orig_autoc) {
		IXGBE_WRITE_REG(hw, IXGBE_AUTOC, hw->mac.orig_autoc);
	}

	/* Store the permanent mac address */
	hw->mac.ops.get_mac_addr(hw, hw->mac.perm_addr);

	/*
	 * Store MAC address from RAR0, clear receive address registers, and
	 * clear the multicast table
	 */
	hw->mac.ops.init_rx_addrs(hw);

reset_hw_out:
	if (phy_status != IXGBE_SUCCESS)
		status = phy_status;

	return (status);
}

/*
 * ixgbe_set_vmdq_82598 - Associate a VMDq set index with a rx address
 * @hw: pointer to hardware struct
 * @rar: receive address register index to associate with a VMDq index
 * @vmdq: VMDq set index
 */
s32
ixgbe_set_vmdq_82598(struct ixgbe_hw *hw, u32 rar, u32 vmdq)
{
	u32 rar_high;

	DEBUGFUNC("ixgbe_set_vmdq_82598");

	rar_high = IXGBE_READ_REG(hw, IXGBE_RAH(rar));
	rar_high &= ~IXGBE_RAH_VIND_MASK;
	rar_high |= ((vmdq << IXGBE_RAH_VIND_SHIFT) & IXGBE_RAH_VIND_MASK);
	IXGBE_WRITE_REG(hw, IXGBE_RAH(rar), rar_high);
	return (IXGBE_SUCCESS);
}

/*
 * ixgbe_clear_vmdq_82598 - Disassociate a VMDq set index from an rx address
 * @hw: pointer to hardware struct
 * @rar: receive address register index to associate with a VMDq index
 * @vmdq: VMDq clear index (not used in 82598, but elsewhere)
 */
static s32
ixgbe_clear_vmdq_82598(struct ixgbe_hw *hw, u32 rar, u32 vmdq)
{
	u32 rar_high;
	u32 rar_entries = hw->mac.num_rar_entries;

	UNREFERENCED_PARAMETER(vmdq);

	if (rar < rar_entries) {
		rar_high = IXGBE_READ_REG(hw, IXGBE_RAH(rar));
		if (rar_high & IXGBE_RAH_VIND_MASK) {
			rar_high &= ~IXGBE_RAH_VIND_MASK;
			IXGBE_WRITE_REG(hw, IXGBE_RAH(rar), rar_high);
		}
	} else {
		DEBUGOUT1("RAR index %d is out of range.\n", rar);
	}

	return (IXGBE_SUCCESS);
}

/*
 * ixgbe_set_vfta_82598 - Set VLAN filter table
 * @hw: pointer to hardware structure
 * @vlan: VLAN id to write to VLAN filter
 * @vind: VMDq output index that maps queue to VLAN id in VFTA
 * @vlan_on: boolean flag to turn on/off VLAN in VFTA
 *
 * Turn on/off specified VLAN in the VLAN filter table.
 */
s32
ixgbe_set_vfta_82598(struct ixgbe_hw *hw, u32 vlan, u32 vind, bool vlan_on)
{
	u32 regindex;
	u32 bitindex;
	u32 bits;
	u32 vftabyte;

	DEBUGFUNC("ixgbe_set_vfta_82598");

	if (vlan > 4095)
		return (IXGBE_ERR_PARAM);

	/* Determine 32-bit word position in array */
	regindex = (vlan >> 5) & 0x7F;   /* upper seven bits */

	/* Determine the location of the (VMD) queue index */
	vftabyte =  ((vlan >> 3) & 0x03); /* bits (4:3) indicating byte array */
	bitindex = (vlan & 0x7) << 2;    /* lower 3 bits indicate nibble */

	/* Set the nibble for VMD queue index */
	bits = IXGBE_READ_REG(hw, IXGBE_VFTAVIND(vftabyte, regindex));
	bits &= (~(0x0F << bitindex));
	bits |= (vind << bitindex);
	IXGBE_WRITE_REG(hw, IXGBE_VFTAVIND(vftabyte, regindex), bits);

	/* Determine the location of the bit for this VLAN id */
	bitindex = vlan & 0x1F;   /* lower five bits */

	bits = IXGBE_READ_REG(hw, IXGBE_VFTA(regindex));
	if (vlan_on)
		/* Turn on this VLAN id */
		bits |= (1 << bitindex);
	else
		/* Turn off this VLAN id */
		bits &= ~(1 << bitindex);
	IXGBE_WRITE_REG(hw, IXGBE_VFTA(regindex), bits);

	return (IXGBE_SUCCESS);
}

/*
 * ixgbe_clear_vfta_82598 - Clear VLAN filter table
 * @hw: pointer to hardware structure
 *
 * Clears the VLAN filer table, and the VMDq index associated with the filter
 */
static s32
ixgbe_clear_vfta_82598(struct ixgbe_hw *hw)
{
	u32 offset;
	u32 vlanbyte;

	DEBUGFUNC("ixgbe_clear_vfta_82598");

	for (offset = 0; offset < hw->mac.vft_size; offset++)
		IXGBE_WRITE_REG(hw, IXGBE_VFTA(offset), 0);

	for (vlanbyte = 0; vlanbyte < 4; vlanbyte++)
		for (offset = 0; offset < hw->mac.vft_size; offset++)
			IXGBE_WRITE_REG(hw,
			    IXGBE_VFTAVIND(vlanbyte, offset), 0);

	return (IXGBE_SUCCESS);
}

/*
 * ixgbe_read_analog_reg8_82598 - Reads 8 bit Atlas analog register
 * @hw: pointer to hardware structure
 * @reg: analog register to read
 * @val: read value
 *
 * Performs read operation to Atlas analog register specified.
 */
s32
ixgbe_read_analog_reg8_82598(struct ixgbe_hw *hw, u32 reg, u8 *val)
{
	u32  atlas_ctl;

	DEBUGFUNC("ixgbe_read_analog_reg8_82598");

	IXGBE_WRITE_REG(hw, IXGBE_ATLASCTL,
	    IXGBE_ATLASCTL_WRITE_CMD | (reg << 8));
	IXGBE_WRITE_FLUSH(hw);
	usec_delay(10);
	atlas_ctl = IXGBE_READ_REG(hw, IXGBE_ATLASCTL);
	*val = (u8)atlas_ctl;

	return (IXGBE_SUCCESS);
}

/*
 * ixgbe_write_analog_reg8_82598 - Writes 8 bit Atlas analog register
 * @hw: pointer to hardware structure
 * @reg: atlas register to write
 * @val: value to write
 *
 * Performs write operation to Atlas analog register specified.
 */
s32
ixgbe_write_analog_reg8_82598(struct ixgbe_hw *hw, u32 reg, u8 val)
{
	u32  atlas_ctl;

	DEBUGFUNC("ixgbe_write_analog_reg8_82598");

	atlas_ctl = (reg << 8) | val;
	IXGBE_WRITE_REG(hw, IXGBE_ATLASCTL, atlas_ctl);
	IXGBE_WRITE_FLUSH(hw);
	usec_delay(10);

	return (IXGBE_SUCCESS);
}

/*
 * ixgbe_read_i2c_eeprom_82598 - Reads 8 bit word over I2C interface.
 * @hw: pointer to hardware structure
 * @byte_offset: EEPROM byte offset to read
 * @eeprom_data: value read
 *
 * Performs 8 byte read operation to SFP module's EEPROM over I2C interface.
 */
s32
ixgbe_read_i2c_eeprom_82598(struct ixgbe_hw *hw, u8 byte_offset,
    u8 *eeprom_data)
{
	s32 status = IXGBE_SUCCESS;
	u16 sfp_addr = 0;
	u16 sfp_data = 0;
	u16 sfp_stat = 0;
	u32 i;

	DEBUGFUNC("ixgbe_read_i2c_eeprom_82598");

	if (hw->phy.type == ixgbe_phy_nl) {
		/*
		 * NetLogic phy SDA/SCL registers are at addresses 0xC30A to
		 * 0xC30D. These registers are used to talk to the SFP+
		 * module's EEPROM through the SDA/SCL (I2C) interface.
		 */
		sfp_addr = (IXGBE_I2C_EEPROM_DEV_ADDR << 8) + byte_offset;
		sfp_addr = (sfp_addr | IXGBE_I2C_EEPROM_READ_MASK);
		hw->phy.ops.write_reg(hw, IXGBE_MDIO_PMA_PMD_SDA_SCL_ADDR,
		    IXGBE_MDIO_PMA_PMD_DEV_TYPE, sfp_addr);

		/* Poll status */
		for (i = 0; i < 100; i++) {
			hw->phy.ops.read_reg(hw,
			    IXGBE_MDIO_PMA_PMD_SDA_SCL_STAT,
			    IXGBE_MDIO_PMA_PMD_DEV_TYPE, &sfp_stat);
			sfp_stat = sfp_stat & IXGBE_I2C_EEPROM_STATUS_MASK;
			if (sfp_stat != IXGBE_I2C_EEPROM_STATUS_IN_PROGRESS)
				break;
			msec_delay(10);
		}

		if (sfp_stat != IXGBE_I2C_EEPROM_STATUS_PASS) {
			DEBUGOUT("EEPROM read did not pass.\n");
			status = IXGBE_ERR_SFP_NOT_PRESENT;
			goto out;
		}

		/* Read data */
		hw->phy.ops.read_reg(hw, IXGBE_MDIO_PMA_PMD_SDA_SCL_DATA,
		    IXGBE_MDIO_PMA_PMD_DEV_TYPE, &sfp_data);

		*eeprom_data = (u8)(sfp_data >> 8);
	} else {
		status = IXGBE_ERR_PHY;
		goto out;
	}

out:
	return (status);
}

/*
 * ixgbe_get_supported_physical_layer_82598 - Returns physical layer type
 * @hw: pointer to hardware structure
 *
 * Determines physical layer capabilities of the current configuration.
 */
u32
ixgbe_get_supported_physical_layer_82598(struct ixgbe_hw *hw)
{
	u32 physical_layer = IXGBE_PHYSICAL_LAYER_UNKNOWN;
	u32 autoc = IXGBE_READ_REG(hw, IXGBE_AUTOC);
	u32 pma_pmd_10g = autoc & IXGBE_AUTOC_10G_PMA_PMD_MASK;
	u32 pma_pmd_1g = autoc & IXGBE_AUTOC_1G_PMA_PMD_MASK;
	u16 ext_ability = 0;

	DEBUGFUNC("ixgbe_get_supported_physical_layer_82598");

	hw->phy.ops.identify(hw);

	/*
	 * Copper PHY must be checked before AUTOC LMS to determine correct
	 * physical layer because 10GBase-T PHYs use LMS = KX4/KX
	 */
	if (hw->phy.type == ixgbe_phy_tn ||
	    hw->phy.type == ixgbe_phy_cu_unknown) {
		hw->phy.ops.read_reg(hw, IXGBE_MDIO_PHY_EXT_ABILITY,
		    IXGBE_MDIO_PMA_PMD_DEV_TYPE, &ext_ability);
		if (ext_ability & IXGBE_MDIO_PHY_10GBASET_ABILITY)
			physical_layer |= IXGBE_PHYSICAL_LAYER_10GBASE_T;
		if (ext_ability & IXGBE_MDIO_PHY_1000BASET_ABILITY)
			physical_layer |= IXGBE_PHYSICAL_LAYER_1000BASE_T;
		if (ext_ability & IXGBE_MDIO_PHY_100BASETX_ABILITY)
			physical_layer |= IXGBE_PHYSICAL_LAYER_100BASE_TX;
		goto out;
	}

	switch (autoc & IXGBE_AUTOC_LMS_MASK) {
	case IXGBE_AUTOC_LMS_1G_AN:
	case IXGBE_AUTOC_LMS_1G_LINK_NO_AN:
		if (pma_pmd_1g == IXGBE_AUTOC_1G_KX)
			physical_layer = IXGBE_PHYSICAL_LAYER_1000BASE_KX;
		else
			physical_layer = IXGBE_PHYSICAL_LAYER_1000BASE_BX;
		break;
	case IXGBE_AUTOC_LMS_10G_LINK_NO_AN:
		if (pma_pmd_10g == IXGBE_AUTOC_10G_CX4)
			physical_layer = IXGBE_PHYSICAL_LAYER_10GBASE_CX4;
		else if (pma_pmd_10g == IXGBE_AUTOC_10G_KX4)
			physical_layer = IXGBE_PHYSICAL_LAYER_10GBASE_KX4;
		else /* XAUI */
			physical_layer = IXGBE_PHYSICAL_LAYER_UNKNOWN;
		break;
	case IXGBE_AUTOC_LMS_KX4_AN:
	case IXGBE_AUTOC_LMS_KX4_AN_1G_AN:
		if (autoc & IXGBE_AUTOC_KX_SUPP)
			physical_layer |= IXGBE_PHYSICAL_LAYER_1000BASE_KX;
		if (autoc & IXGBE_AUTOC_KX4_SUPP)
			physical_layer |= IXGBE_PHYSICAL_LAYER_10GBASE_KX4;
		break;
	default:
		break;
	}

	if (hw->phy.type == ixgbe_phy_nl) {
		hw->phy.ops.identify_sfp(hw);

		switch (hw->phy.sfp_type) {
		case ixgbe_sfp_type_da_cu:
			physical_layer = IXGBE_PHYSICAL_LAYER_SFP_PLUS_CU;
			break;
		case ixgbe_sfp_type_sr:
			physical_layer = IXGBE_PHYSICAL_LAYER_10GBASE_SR;
			break;
		case ixgbe_sfp_type_lr:
			physical_layer = IXGBE_PHYSICAL_LAYER_10GBASE_LR;
			break;
		default:
			physical_layer = IXGBE_PHYSICAL_LAYER_UNKNOWN;
			break;
		}
	}

	switch (hw->device_id) {
	case IXGBE_DEV_ID_82598_DA_DUAL_PORT:
		physical_layer = IXGBE_PHYSICAL_LAYER_SFP_PLUS_CU;
		break;
	case IXGBE_DEV_ID_82598AF_DUAL_PORT:
	case IXGBE_DEV_ID_82598AF_SINGLE_PORT:
	case IXGBE_DEV_ID_82598_SR_DUAL_PORT_EM:
		physical_layer = IXGBE_PHYSICAL_LAYER_10GBASE_SR;
		break;
	case IXGBE_DEV_ID_82598EB_XF_LR:
		physical_layer = IXGBE_PHYSICAL_LAYER_10GBASE_LR;
		break;
	default:
		break;
	}

out:
	return (physical_layer);
}

/*
 * ixgbe_set_lan_id_multi_port_pcie_82598 - Set LAN id for PCIe multiple
 * port devices.
 * @hw: pointer to the HW structure
 *
 * Calls common function and corrects issue with some single port devices
 * that enable LAN1 but not LAN0.
 */
void
ixgbe_set_lan_id_multi_port_pcie_82598(struct ixgbe_hw *hw)
{
	struct ixgbe_bus_info *bus = &hw->bus;
	u16 pci_gen, pci_ctrl2;

	DEBUGFUNC("ixgbe_set_lan_id_multi_port_pcie_82598");

	ixgbe_set_lan_id_multi_port_pcie(hw);

	/* check if LAN0 is disabled */
	hw->eeprom.ops.read(hw, IXGBE_PCIE_GENERAL_PTR, &pci_gen);
	if ((pci_gen != 0) && (pci_gen != 0xFFFF)) {
		hw->eeprom.ops.read(hw, pci_gen + IXGBE_PCIE_CTRL2, &pci_ctrl2);

		/* if LAN0 is completely disabled force function to 0 */
		if ((pci_ctrl2 & IXGBE_PCIE_CTRL2_LAN_DISABLE) &&
		    !(pci_ctrl2 & IXGBE_PCIE_CTRL2_DISABLE_SELECT) &&
		    !(pci_ctrl2 & IXGBE_PCIE_CTRL2_DUMMY_ENABLE)) {
			bus->func = 0;
		}
	}
}

/*
 * ixgbe_validate_link_ready - Function looks for phy link
 * @hw: pointer to hardware structure
 *
 * Function indicates success when phy link is available. If phy is not ready
 * within 5 seconds of MAC indicating link, the function returns error.
 */
static s32
ixgbe_validate_link_ready(struct ixgbe_hw *hw)
{
	u32 timeout;
	u16 an_reg;

	if (hw->device_id != IXGBE_DEV_ID_82598AT2)
		return (IXGBE_SUCCESS);

	for (timeout = 0;
	    timeout < IXGBE_VALIDATE_LINK_READY_TIMEOUT; timeout++) {
		hw->phy.ops.read_reg(hw, IXGBE_MDIO_AUTO_NEG_STATUS,
		    IXGBE_MDIO_AUTO_NEG_DEV_TYPE, &an_reg);

		if ((an_reg & IXGBE_MII_AUTONEG_COMPLETE) &&
		    (an_reg & IXGBE_MII_AUTONEG_LINK_UP))
			break;

		msec_delay(100);
	}

	if (timeout == IXGBE_VALIDATE_LINK_READY_TIMEOUT) {
		DEBUGOUT("Link was indicated but link is down\n");
		return (IXGBE_ERR_LINK_SETUP);
	}

	return (IXGBE_SUCCESS);
}

/*
 * ixgbe_enable_relaxed_ordering_82598 - enable relaxed ordering
 * @hw: pointer to hardware structure
 */
void
ixgbe_enable_relaxed_ordering_82598(struct ixgbe_hw *hw)
{
	u32 regval;
	u32 i;

	DEBUGFUNC("ixgbe_enable_relaxed_ordering_82598");

	/* Enable relaxed ordering */
	for (i = 0; ((i < hw->mac.max_tx_queues) &&
	    (i < IXGBE_DCA_MAX_QUEUES_82598)); i++) {
		regval = IXGBE_READ_REG(hw, IXGBE_DCA_TXCTRL(i));
		regval |= IXGBE_DCA_TXCTRL_TX_WB_RO_EN;
		IXGBE_WRITE_REG(hw, IXGBE_DCA_TXCTRL(i), regval);
	}

	for (i = 0; ((i < hw->mac.max_rx_queues) &&
	    (i < IXGBE_DCA_MAX_QUEUES_82598)); i++) {
		regval = IXGBE_READ_REG(hw, IXGBE_DCA_RXCTRL(i));
		regval |= (IXGBE_DCA_RXCTRL_DESC_WRO_EN |
		    IXGBE_DCA_RXCTRL_DESC_HSRO_EN);
		IXGBE_WRITE_REG(hw, IXGBE_DCA_RXCTRL(i), regval);
	}
}

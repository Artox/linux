#include <linux/mfd/ntx_msp430.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/of_device.h>

struct ntx_msp430_battery_device_info {
	struct ntx_msp430 *mfd;
	struct power_supply *bat;
};

static enum power_supply_property ntx_msp430_battery_props[] = {
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
};

static int ntx_msp430_battery_read_voltage(struct ntx_msp430_battery_device_info *di)
{
	int val;
	int voltage;

	// voltage at 0x41
	val = ntx_msp430_read16(di->mfd, 0x41);
	if (val < 0) {
		//dev_err(di->dev, "Could not read ADC: %d\n", voltage);
		return val;
	}

	/* value to uV
	 * vendor kernel source suggests linear behaviour from 3V to 4.2V with readings 767 to 1023
	 * this gives 4687,5uV per step
	 * adjusted 3V value in favour of an exact result 4.2V result at 1023
	 */
	voltage = 2999872;
	voltage += (val - 767) * 4688;

	return voltage;
}

int ntx_msp430_battery_get_property(struct power_supply *psy, enum power_supply_property psp, union power_supply_propval *val) {
	struct ntx_msp430_battery_device_info *di = power_supply_get_drvdata(psy);
	struct power_supply_battery_info battery_info = {};
	int r;

	r = power_supply_get_battery_info(psy, &battery_info);
	if(r) {
		dev_warn(&psy->dev, "Could not get battery info: %d\n", r);
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = battery_info.voltage_max_design_uv;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		val->intval = battery_info.voltage_min_design_uv;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = ntx_msp430_battery_read_voltage(di);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = battery_info.charge_full_design_uah;
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		val->intval = battery_info.energy_full_design_uwh;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int ntx_msp430_battery_probe(struct platform_device *pdev) {
	struct ntx_msp430 *mfd;
	struct ntx_msp430_battery_device_info *di;
	struct power_supply_desc *psy_desc;
	struct power_supply_config psy_cfg = {};

	mfd = dev_get_drvdata(pdev->dev.parent);
	if(!mfd) {
		return -EPROBE_DEFER;
	}

	di = devm_kzalloc(&pdev->dev, sizeof(*di), GFP_KERNEL);
	if (!di)
		return -ENOMEM;
	platform_set_drvdata(pdev, di);

	psy_desc = devm_kzalloc(&pdev->dev, sizeof(*psy_desc), GFP_KERNEL);
	if (!psy_desc)
		return -ENOMEM;

	psy_cfg.drv_data = di;
	psy_cfg.of_node = pdev->dev.of_node;

	psy_desc->name = "ntx-msp430-ec-battery";
	psy_desc->type = POWER_SUPPLY_TYPE_BATTERY;
	psy_desc->properties = ntx_msp430_battery_props;
	psy_desc->num_properties = ARRAY_SIZE(ntx_msp430_battery_props);
	psy_desc->get_property = ntx_msp430_battery_get_property;

	di->mfd = mfd;
	di->bat = power_supply_register_no_ws(&pdev->dev, psy_desc, &psy_cfg);
	if (IS_ERR(di->bat)) {
		dev_err(&pdev->dev, "failed to register battery\n");
		return PTR_ERR(di->bat);
	}

	return 0;
}

static int ntx_msp430_battery_remove(struct platform_device *pdev) {
	struct ntx_msp430_battery_device_info *di = platform_get_drvdata(pdev);

	power_supply_unregister(di->bat);

	return 0;
}

static const struct of_device_id ntx_msp430_battery_of_match[] = {
	{ .compatible = "netronix,msp430-ec-battery" },
	{ },
};
MODULE_DEVICE_TABLE(of, ntx_msp430_battery_of_match);

static struct platform_driver ntx_msp430_battery_driver = {
	.driver = {
		.name = "ntx-msp430-pwm",
		.of_match_table = ntx_msp430_battery_of_match,
	},
	.probe = ntx_msp430_battery_probe,
	.remove = ntx_msp430_battery_remove,
};
module_platform_driver(ntx_msp430_battery_driver);

MODULE_AUTHOR("Josua Mayer <josua.mayer@jm0.eu>");
MODULE_DESCRIPTION("NTX MSP430 battery monitor driver");
MODULE_LICENSE("GPL");

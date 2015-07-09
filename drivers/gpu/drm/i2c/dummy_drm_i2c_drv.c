/*
 * This file was originally based on tda998x_drv.c which has the following
 * copyright and licence...
 *
 * Copyright (C) 2012 Texas Instruments
 * Author: Rob Clark <robdclark@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/component.h>
#include <linux/module.h>

#include <drm/drmP.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_encoder_slave.h>
#include <drm/drm_edid.h>
#include <drm/drm_of.h>

struct dummy_priv {
	struct drm_encoder encoder;
	struct drm_connector connector;
};

#define conn_to_dummy_priv(x) \
	container_of(x, struct dummy_priv, connector);

/* DRM encoder functions */

static void dummy_encoder_dpms(struct drm_encoder *encoder, int mode)
{
}

static bool
dummy_encoder_mode_fixup(struct drm_encoder *encoder,
			  const struct drm_display_mode *mode,
			  struct drm_display_mode *adjusted_mode)
{
	return true;
}

static int dummy_connector_mode_valid(struct drm_connector *connector,
					struct drm_display_mode *mode)
{
	return MODE_OK;
}

static void 
dummy_encoder_mode_set(struct drm_encoder *encoder,
		       struct drm_display_mode *mode,
		       struct drm_display_mode *adjusted_mode)
{
}

static enum drm_connector_status
dummy_connector_detect(struct drm_connector *connector, bool force)
{
	return connector_status_connected;
}

static const u8 edid_1024x768[] = {
	/*
	 * These values are a copy of Documentation/EDID/1024x768.c
	 * produced by executing "make -C Documentation/EDID"
	 */
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
	0x31, 0xd8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x05, 0x16, 0x01, 0x03, 0x6d, 0x23, 0x1a, 0x78,
	0xea, 0x5e, 0xc0, 0xa4, 0x59, 0x4a, 0x98, 0x25,
	0x20, 0x50, 0x54, 0x00, 0x08, 0x00, 0x61, 0x40,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x64, 0x19,
	0x00, 0x40, 0x41, 0x00, 0x26, 0x30, 0x08, 0x90,
	0x36, 0x00, 0x63, 0x0a, 0x11, 0x00, 0x00, 0x18,
	0x00, 0x00, 0x00, 0xff, 0x00, 0x4c, 0x69, 0x6e,
	0x75, 0x78, 0x20, 0x23, 0x30, 0x0a, 0x20, 0x20,
	0x20, 0x20, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x3b,
	0x3d, 0x2f, 0x31, 0x07, 0x00, 0x0a, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xfc,
	0x00, 0x4c, 0x69, 0x6e, 0x75, 0x78, 0x20, 0x58,
	0x47, 0x41, 0x0a, 0x20, 0x20, 0x20, 0x00, 0x55
};

static int dummy_read_edid_block(void *data, u8 *buf, unsigned int blk, size_t length)
{
	memcpy(buf, edid_1024x768, min(sizeof(edid_1024x768),length));
	return 0;
}

static int dummy_connector_get_modes(struct drm_connector *connector)
{
	struct edid *edid;
	int n;

	edid = drm_do_get_edid(connector, dummy_read_edid_block, NULL);
	if (!edid)
		return 0;

	drm_mode_connector_update_edid_property(connector, edid);
	n = drm_add_edid_modes(connector, edid);
	kfree(edid);

	return n;
}

/* I2C driver functions */

static void dummy_encoder_prepare(struct drm_encoder *encoder)
{
}

static void dummy_encoder_commit(struct drm_encoder *encoder)
{
}

static const struct drm_encoder_helper_funcs dummy_encoder_helper_funcs = {
	.dpms = dummy_encoder_dpms,
	.mode_fixup = dummy_encoder_mode_fixup,
	.prepare = dummy_encoder_prepare,
	.commit = dummy_encoder_commit,
	.mode_set = dummy_encoder_mode_set,
};

static void dummy_encoder_destroy(struct drm_encoder *encoder)
{
	drm_encoder_cleanup(encoder);
}

static const struct drm_encoder_funcs dummy_encoder_funcs = {
	.destroy = dummy_encoder_destroy,
};

static struct drm_encoder *
dummy_connector_best_encoder(struct drm_connector *connector)
{
	struct dummy_priv *priv = conn_to_dummy_priv(connector);

	return &priv->encoder;
}

static
const struct drm_connector_helper_funcs dummy_connector_helper_funcs = {
	.get_modes = dummy_connector_get_modes,
	.mode_valid = dummy_connector_mode_valid,
	.best_encoder = dummy_connector_best_encoder,
};

static void dummy_connector_destroy(struct drm_connector *connector)
{
	drm_connector_cleanup(connector);
}

static const struct drm_connector_funcs dummy_connector_funcs = {
	.dpms = drm_atomic_helper_connector_dpms,
	.reset = drm_atomic_helper_connector_reset,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = dummy_connector_detect,
	.destroy = dummy_connector_destroy,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static int dummy_bind(struct device *dev, struct device *master, void *data)
{
	struct drm_device *drm = data;
	struct dummy_priv *priv;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	dev_set_drvdata(dev, priv);

	priv->connector.interlace_allowed = 1;
	priv->encoder.possible_crtcs = 1 << 0;

	drm_encoder_helper_add(&priv->encoder, &dummy_encoder_helper_funcs);
	ret = drm_encoder_init(drm, &priv->encoder, &dummy_encoder_funcs,
			       DRM_MODE_ENCODER_TMDS, NULL);
	if (ret)
		goto err_encoder;

	drm_connector_helper_add(&priv->connector,
				 &dummy_connector_helper_funcs);
	ret = drm_connector_init(drm, &priv->connector,
				 &dummy_connector_funcs,
				 DRM_MODE_CONNECTOR_HDMIA);
	if (ret)
		goto err_connector;

	drm_mode_connector_attach_encoder(&priv->connector, &priv->encoder);

	return 0;

err_connector:
	drm_encoder_cleanup(&priv->encoder);
err_encoder:
	return ret;
}

static void dummy_unbind(struct device *dev, struct device *master,
			   void *data)
{
	struct dummy_priv *priv = dev_get_drvdata(dev);

	drm_connector_cleanup(&priv->connector);
	drm_encoder_cleanup(&priv->encoder);
}

static const struct component_ops dummy_ops = {
	.bind = dummy_bind,
	.unbind = dummy_unbind,
};

static int
dummy_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	return component_add(&client->dev, &dummy_ops);
}

static int dummy_remove(struct i2c_client *client)
{
	component_del(&client->dev, &dummy_ops);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id dummy_of_ids[] = {
	{ .compatible = "sil,sii9022-tpi", },
	{ }
};
MODULE_DEVICE_TABLE(of, dummy_of_ids);
#endif

static struct i2c_device_id dummy_ids[] = {
	{ }
};
MODULE_DEVICE_TABLE(i2c, dummy_ids);

static struct i2c_driver dummy_driver = {
	.probe = dummy_probe,
	.remove = dummy_remove,
	.driver = {
		.name = "dummy_drm_i2c",
		.of_match_table = dummy_of_ids
	},
	.id_table = dummy_ids,
};

module_i2c_driver(dummy_driver);

MODULE_LICENSE("GPL");


#include <linux/kernel.h>
#include <linux/amba/bus.h>
#include <linux/amba/pl080.h>
#include <linux/amba/pl08x.h>
#include <linux/of.h>


int pl08x_get_xfer_signal(const struct pl08x_channel_data *cd)
{
	return cd->min_signal;
}

void pl08x_put_xfer_signal(const struct pl08x_channel_data *cd, int ch)
{
}

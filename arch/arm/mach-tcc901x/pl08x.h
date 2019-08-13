
#ifndef _PL08X_H_
#define _PL08X_H_

int pl08x_get_xfer_signal(const struct pl08x_channel_data *cd);
void pl08x_put_xfer_signal(const struct pl08x_channel_data *cd, int signal);

#endif

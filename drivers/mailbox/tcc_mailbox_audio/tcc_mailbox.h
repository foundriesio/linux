/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#ifndef _TCC_MAILBOX_H_
#define _TCC_MAILBOX_H_

#define PES_TIMER_CALLBACK    0x00000001
#define MBOX_REPLY_ID_BASE    0x0CA70000

struct ca7_mbox_msg {
    unsigned int command;
    unsigned int id;

    unsigned int params[128-2];
};

struct ca7_mbox_instance {
    wait_queue_head_t wait;
    unsigned int id;
    int done;
    void *mbox;

    struct ca7_mbox_msg cmd;
    struct ca7_mbox_msg reply;

    struct list_head elem;
};

void* tcc_sap_init_mbox(struct platform_device *pdev);
int tcc_sap_close_mbox(void *client);

int tcc_sap_init_ca7(void);
int tcc_sap_deinit_ca7(void);

struct ca7_mbox_instance* tcc_sap_get_new_id(void *client);

int tcc_sap_send_command(struct ca7_mbox_instance *mbox_inst, unsigned int cmd, unsigned int size, int wait);
int tcc_sap_remove_component(struct ca7_mbox_instance *reply);

ssize_t tcc_sap_mbox_send(void *client, unsigned int *msg, size_t count);

#endif//_TCC_MAILBOX_H_

/*
 * Copyright (C) 2016 Telechips
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

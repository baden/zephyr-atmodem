#pragma once
#include "../simcom-a7682e.h"

#include<stdbool.h>

// #include "modem_cmd_handler.h"
// #include "modem_context.h"



int query_ids1(struct modem_data *mdata);
int query_ids2(struct modem_data *mdata);

char* query_operator(struct modem_data *mdata);
char* operator();

int sim7600g_query_CCID(struct modem_data *mdata);

#pragma once
#include "../simcom-sim7600g.h"

#include<stdbool.h>

// #include "modem_cmd_handler.h"
// #include "modem_context.h"



int query_ids(struct modem_data *mdata);

char* query_operator(struct modem_data *mdata);
char* operator();

int sim7600g_query_CCID(struct modem_data *mdata);

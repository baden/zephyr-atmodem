#include "tests.h"
#include <zephyr/kernel.h>

#include<stdlib.h>  // atoi
// #include "../utils.h"

// #include <zephyr.h>


// Expect response:
//  +CSQ: <rssi>,<ber>

// Defined Values
//      <rssi>  0 -113 dBm or less
//              1 -111 dBm
//              2…30 -109… -53 dBm
//              31 -51 dBm or greater
//              99 not known or not detectable
// NOTE
// A76XX Series_AT Command Manual_V1.08www.simcom.com 67/ 640<ber> (in percent)
//      0 <0.01%
//      1 0.01% --- 0.1%
//      2 0.1% --- 0.5%
//      3 0.5% --- 1.0%
//      4 1.0% --- 2.0%
//      5 2.0% --- 4.0%
//      6 4.0% --- 8.0%
//      7 >=8.0%
//      99 not known or not


MODEM_CMD_DEFINE(on_cmd_csq)
{
	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
	int rssi = atoi(argv[0]);

	if (rssi == 0) {
		mdata->mdm_rssi = -114;
	} else if (rssi == 1) {
		mdata->mdm_rssi = -111;
	} else if (rssi >= 2 && rssi <= 30) {
		mdata->mdm_rssi = -114 + 2 * rssi;
	} else if (rssi == 31) {
		mdata->mdm_rssi = -52;
	} else {
		mdata->mdm_rssi = -1000;
	}

	return 0;
}

int modem_rssi_query(struct modem_data *mdata)
{
    struct modem_cmd cmd = MODEM_CMD("+CSQ: ", on_cmd_csq, 2U, ",");
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, &cmd, 1U, "AT+CSQ", &mdata->sem_response, K_SECONDS(3));
}

#include "gprs.h"
// #include "nvm/nvm.h"
#include "id.h"
#include <zephyr/sys/printk.h>
#include "tpf.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(modem_a7682e, CONFIG_MMODEM_LOG_LEVEL);

TPF_DEF(STR64, gsm_apn1, "*auto");
TPF_DEF(STR64, gsm_apn2, "*auto");

int modem_pdp_context_enable(struct modem_data *mdata)
{
    char apn[32] = {0};
    if(modem_apn(mdata, apn, sizeof(apn)) < 0) {
        LOG_ERR("Failed to get APN");
        return -1;
    }

    char send_buf[sizeof("AT+CGSOCKCONT=1,\"IP\",\"################################\"")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CGSOCKCONT=1,\"IP\",\"%s\"", apn);
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, send_buf, &mdata->sem_response, K_SECONDS(2));
}

int modem_apn(struct modem_data *mdata, char *apn, size_t len)
{
    #if 1
        // TODO: Hardcoded for now
        // strncpy(apn, "internet", len);
        if(strcmp((const char*)TPF(gsm_apn1), "*auto") != 0) {
            strncpy(apn, (const char*)TPF(gsm_apn1), len);
        } else {
            char *op = query_operator(mdata);
            if(strstr(op, "KYIVSTAR") != NULL) {
                strncpy(apn, "internet", len);
            } else if(strstr(op, "lifecell") != NULL) {
                strncpy(apn, "internet", len);
                // T-Mobile SORACOM
            } else if(strstr(op, "AT&T") != NULL) {
                strncpy(apn, "PHONE", len);
            } else if(strcmp(op, "T-Mobile SORACOM") != 0) {
                strncpy(apn, "openmobile", len);
            } else if(strcmp(op, "AT&T SORACOM") != 0) {
                strncpy(apn, "openmobile", len);
            } else {
                strncpy(apn, "", len);
            }
        }
    #else
    #if defined(CONFIG_DEBUG_OPENMOBILE)
        strncpy(apn, "openmobile", len);
    #else
        char lte_apn[NVM_LTE_APN_LENGTH] = {0};
        int ret = nvm_load_value(NVM_LTE_APN, lte_apn, NVM_LTE_APN_LENGTH);
        if((ret > 0) && (strcmp(lte_apn, "*auto") != 0)) {
            lte_apn[NVM_LTE_APN_LENGTH-1] = 0;
            strncpy(apn, lte_apn, len);
        } else {
            char *op = query_operator(mdata);
            if(strstr(op, "KYIVSTAR") != NULL) {
                strncpy(apn, "internet", len);
            } else if(strstr(op, "lifecell") != NULL) {
                strncpy(apn, "internet", len);
                // T-Mobile SORACOM
            } else if(strstr(op, "AT&T") != NULL) {
                strncpy(apn, "PHONE", len);
            } else if(strcmp(op, "T-Mobile SORACOM") != 0) {
                strncpy(apn, "openmobile", len);
            } else if(strcmp(op, "AT&T SORACOM") != 0) {
                strncpy(apn, "openmobile", len);
            } else {
                strncpy(apn, "", len);
            }
        }
    #endif
    #endif
    return 0;
}

#if 0
#include <zephyr.h>

#include "gprs.h"
#include "../cmd.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(modem_sim7600g_gprs, CONFIG_MODEM_LOG_LEVEL);

// TODO: Hardcoded for now
#define gsm_apn "internet"
#define gsm_user ""
#define gsm_pwd ""

int gprsCGAT()
{
	int cgatt;
	LOG_DBG("gprsCGAT");

	// Activating network bearing
	gsmCmd("AT+CGACT=0,1");
	gsmWokTo(30);
	// uart_wait_exact("OK", 30);

	k_msleep(1000);

	gsmCmd("AT+CGATT?");
	if(gsmScanfAnswer(true, "+CGATT: %d", &cgatt)){
		return cgatt;
	}
	return -1;
}

// 1 - Home network
// 2 - Registering in progress..
// 5 - Roaming
// Other value -> need reconnect to network
int getCreg()
{
	int n;
    int creg_state;
	// TODO: Check AT+CREG?
	// Now just control
	gsmCmd("AT+CREG?");

	if(gsmScanfAnswer(true, "+CREG: %d,%d", &n, &creg_state)){
        return creg_state;
    }
    return -1;
}

int rssi;
int ber;

int getCsq()
{
	// char *answer;
	// Dummy?
	gsmCmd("AT+CSQ");

	if(gsmScanfAnswer(true, "+CSQ: %d,%d", &rssi, &ber)){
        // DEBUG_PRINT("CSQ=%d", rssi);
		return rssi;
    }
	return -1;
}


bool gsm_gprs_on(void)
{
	// char *answer;
	// TODO: Close last session before
	// if(ds_gsm.GPRS) gsm_gprs_off();	// Гарантируем что предыдущая GPRS-сессия завершена
	// ds_gsm.GPRS = 1;

	#if 0
		if(!gsmTestSim()){
			ASSERT("Error SIM card.");
			gprs_fails++;
			ds_gsm.GPRS = 0;
			return 105;
		}

	#endif

	#if 1
	// Need wait 1 or 5
	for(int i=0; i<10; i++) {
		int creg = getCreg();
		if(creg == 1 || creg == 5) break;
		k_sleep(K_SECONDS(2));
	}
	#endif

	// ?

	int cgatt;
	cgatt = gprsCGAT();
	LOG_DBG("CGAT:%d", cgatt);


	// TODO: Check CQS

	getCsq();
	LOG_DBG("rssi=%d ber=%d", rssi, ber);


	k_msleep(1000);

	#if 0
		gsmCmdWok("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"");
		// TODO: Hardcoded for now
		gsmCmdWok("AT+SAPBR=3,1,\"APN\",\"" gsm_apn  "\"");
		gsmCmdWok("AT+SAPBR=3,1,\"USER\",\"" gsm_user "\"");
		gsmCmdWok("AT+SAPBR=3,1,\"PWD\",\"" gsm_pwd "\"");

		k_msleep(1000);

		// It can took lot of time. And maybe need to be repeated.
		gsmCmd("AT+SAPBR=1,1");
		uart_wait_exact("OK", 60);

		gsmCmdWok("AT+SAPBR=2,1");
		// It took IP?
		// +SAPBR: 1,1,\"...\"

	#endif

	return true;
}


bool gsmTestSim()
{
    // DEBUG_PRINT(" Check CPIN.");
    gsmCmd("AT+CPIN?");
    return gsmScanfAnswer(true, "+CPIN: READY"); // На данный момент без параметров не работает
    // return _gsm_scanf_answer(true, _s(3), 0, "+CPIN: READY");
}
#endif

// AT+CGDCONT=1,"IPV4V6","APN"
int modem_set_apn(struct modem_data *mdata, int n, const char* apn)
{
    char send_buf[sizeof("AT+CGDCONT=#,\"IPV4V6\",\"#####################\"")] = {0};
    if(apn == NULL) return 0;
    // snprintk(send_buf, sizeof(send_buf), "AT+CGDCONT=%d,\"IPV4V6\",\"%s\"", n, apn);
    snprintk(send_buf, sizeof(send_buf), "AT+CGDCONT=%d,\"IP\",\"%s\"", n, apn);
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, send_buf, &mdata->sem_response, K_SECONDS(2));
}

// AT+CGACT=1,1
int modem_activate_context(struct modem_data *mdata, int n)
{
    char send_buf[sizeof("AT+CGACT=1,#")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CGACT=1,%d", n);
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, send_buf, &mdata->sem_response, K_SECONDS(30));
}

int modem_gprs_init(struct modem_data *mdata)
{
    // TODO: Do you really need to do this?
    char apn[32] = {0};
    if(modem_apn(mdata, apn, sizeof(apn)) >= 0) {
        modem_set_apn(mdata, 1, apn);
        // modem_set_apn(mdata, 2, "ims");
    }

    for(int i=0; i<=2; i++) {
        if(modem_activate_context(mdata, 1) == 0) break;
        k_sleep(K_SECONDS(2));
    }


    // AT+CNMP=?
    modem_direct_cmd(mdata, "AT+CNMP?"); k_sleep(K_MSEC(100));
    modem_direct_cmd(mdata, "AT+CNMP=?"); k_sleep(K_MSEC(1000));

    return 0;
}

int modem_gprs_report(const struct device *dev)
{
    // TODO:
    struct modem_data *mdata = dev->data;


    modem_direct_cmd(mdata, "AT+CGCONTRDP=?"); k_sleep(K_MSEC(200));

    modem_direct_cmd(mdata, "AT+CPSI?"); k_sleep(K_MSEC(100));
    modem_direct_cmd(mdata, "AT+COPS?"); k_sleep(K_MSEC(100));

    // Show network system mode
    modem_direct_cmd(mdata, "AT+CNSMOD?"); k_sleep(K_MSEC(100));

    // modem_direct_cmd(mdata, "AT+CSDH?"); k_sleep(K_MSEC(100));
    // modem_direct_cmd(mdata, "AT+CSCA?"); k_sleep(K_MSEC(100));
    // modem_direct_cmd(mdata, "AT+CSCS?"); k_sleep(K_MSEC(100));

    // AT+CNSMOD?
    // modem_direct_cmd("AT+CNSMOD?"); k_sleep(K_SECONDS(2));
    modem_pdp_context_enable(mdata);

    modem_direct_cmd(mdata, "AT+CGPADDR"); k_sleep(K_MSEC(100));
    modem_direct_cmd(mdata, "AT+CGDCONT?"); k_sleep(K_MSEC(100));
    modem_direct_cmd(mdata, "AT+CGSOCKCONT?"); k_sleep(K_MSEC(100));
    modem_direct_cmd(mdata, "AT+CGSOCKCONT=?"); k_sleep(K_MSEC(200));

    return 0;
}

int modem_gprs_done(struct modem_data *mdata)
{
    return 0;
}

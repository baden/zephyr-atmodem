#include "fs.h"
#include "common.h"
#include "../utils.h"
// #include "lz4.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(modem_a7682e, CONFIG_MMODEM_LOG_LEVEL);

// AT+FSMEM Check the size of available memory
// +FSMEM: D:(<total>,<used>)
int simcom_files_meminfo(const struct device *dev)
{
    struct modem_data *mdata = dev->data;
    return modem_direct_cmd(mdata, "AT+FSMEM");
}


// AT+FSCD Select directory as current directory
// AT+FSCD=<path>
int simcom_files_cd(const struct device *dev, const char *path)
{
    char send_buf[sizeof("AT+FSCD=\"###########################\"")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+FSCD=\"%s\"", path);
    struct modem_data *mdata = dev->data;
    return modem_direct_cmd(mdata, send_buf);
}

// AT+FSMKDIR Make new directory in current directory
// AT+FSMKDIR=<dir>
// AT+FSMKDIR=AMR
int simcom_files_mkdir(const struct device *dev, const char *dir)
{
    char send_buf[sizeof("AT+FSMKDIR=###########################")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+FSMKDIR=\"%s\"", dir);
    
    struct modem_data *mdata = dev->data;
    // return modem_direct_cmd(mdata, send_buf);
    // return modem_direct_cmd(mdata, "AT+FSMKDIR=\"C:/SIMTech\"");
    // return modem_direct_cmd(mdata, "AT+FSMKDIR=amr");
    
    // return modem_direct_cmd(mdata, "AT+FSMKDIR=?");
    return modem_direct_cmd(mdata, "AT*SELECTVSIM=?");

}

// AT+FSLS List directories/files in current directory
// AT+FSLS=<type>
// <type>
//    0 list both subdirectories and files
//    1 list subdirectories only
//    2 list files only
int simcom_files_ls(const struct device *dev, int type)
{
    char send_buf[sizeof("AT+FSLS=#")] = {0};
    // snprintk(send_buf, sizeof(send_buf), "AT+FSLS=%d", type);
    snprintk(send_buf, sizeof(send_buf), "AT+FSLS");
    struct modem_data *mdata = dev->data;
    return modem_direct_cmd(mdata, send_buf);
}


// AT+FSPRESET Moves the location of a file
// AT+FSPRESET=<fileName>[, <direction>]
// <direction>
//      0 from root directory to the user directory
//      1 from user directory to the root directory

int simcom_fs_moveto(const struct device *dev, const char *filename, enum simcom_fs_move_direction direction)
{
    char send_buf[sizeof("AT+FSPRESET=\"###########################\",#")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+FSPRESET=\"%s\",%d", filename, direction);
    struct modem_data *mdata = dev->data;
    return modem_direct_cmd(mdata, send_buf);
}

#if 0
// +FSOPEN: <filehandle>
MODEM_CMD_DEFINE(on_cmd_fsopen)
{
	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);

    // http_last_action_result.success = true;
    int filehandle = ATOI(argv[0], 0, "filehandle");
    // LOG_DBG("*> Got +HTTPREADFILE=%d", errcode );
    // modem_cmd_handler_set_error(data, errcode);   //  -EIO
    // k_sem_give(&mdata->sem_response);
    // k_sem_give(&mdata->sem_http_file_read);
	return 0;
}

// AT+FSOPEN Open a file
// AT+FSOPEN=<fileName>[,<mode>]
// <filename>
//      The file name to be opened with the path. The maximumlengthis 115bytes.
// <filehandle>
//      The handle of the file.
// <mode>
//  The open mode of the file.
//  0 if the file does not exist,it will be created. If the file exists, it will
//      be directly opened. And both of them can be read and written.
//  1 if the file does not exist,it will be created. If the file exists, it will
//      be overwritten and cleared. And both of them can be read andwritten.
//  2 if the file exist, open it and it can be read only. When thefiledoesnot exist, it will respond an error.

int simcom_fs_open(const struct device *dev, const char *filename, int mode)
{
    char send_buf[sizeof("AT+FSOPEN=###########################,##")] = {0};
    // snprintk(send_buf, sizeof(send_buf), "AT+FSOPEN=%s,%d", filename, mode);
    // snprintk(send_buf, sizeof(send_buf), "AT+FSOPEN=?", filename, mode);
    // snprintk(send_buf, sizeof(send_buf), "AT+FSREAD=?", filename, mode);
    snprintk(send_buf, sizeof(send_buf), "AT+CFTRANTX=\"%s\"", filename, mode);
    
    // AT+CFTRANTX="c:/t1.txt"

    struct modem_data *mdata = dev->data;
    return modem_direct_cmd(mdata, send_buf);
}


// AT+FSCLOSE Close a file
// AT+FSCLOSE=<filehandle>
int simcom_fs_close(const struct device *dev, int filehandle)
{
    char send_buf[sizeof("AT+FSCLOSE=###")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+FSCLOSE=%d", filehandle);
    struct modem_data *mdata = dev->data;
    return modem_direct_cmd(mdata, send_buf);
}
#endif


static K_SEM_DEFINE(trans_read_sem, 0, 1);

static struct http_data {
    uint8_t *recv_buf;
    size_t recv_buf_len;
    size_t recv_buf_goted_data;
} http_data;

static int on_cmd_sockread_common(struct modem_cmd_handler_data *data, uint16_t len)
{
    int ret, i;
	int socket_data_length;
	int bytes_to_skip;

    if (!len) {
        LOG_ERR("Invalid length, Aborting!");
        return -EAGAIN;
    }

    /* Make sure we still have buf data */
	if (!data->rx_buf) {
		LOG_ERR("Incorrect format! Ignoring data!");
		return -EINVAL;
	}

    socket_data_length = find_len(data->rx_buf->data);
    // LOG_ERR("socket_data_length = %d", socket_data_length);

    /* No (or not enough) data available on the socket. */
	bytes_to_skip = digits(socket_data_length) + 2 + 4;
	if (socket_data_length <= 0) {
		LOG_ERR("Length problem (%d).  Aborting!", socket_data_length);
		return -EAGAIN;
	}

    /* check to make sure we have all of the data. */
	if (net_buf_frags_len(data->rx_buf) < (socket_data_length + bytes_to_skip)) {
		// LOG_ERR("     -> TODO: Not enough data -- wait! %d < (%d + %d)",
        //     net_buf_frags_len(data->rx_buf), socket_data_length, bytes_to_skip
        // );
		return -EAGAIN;
	}
    // LOG_ERR("TODO: catched data %d", socket_data_length);

    /* Skip "len" and CRLF */
	bytes_to_skip = digits(socket_data_length) + 2;
	for (i = 0; i < bytes_to_skip; i++) {
		net_buf_pull_u8(data->rx_buf);
	}

    if (!data->rx_buf->len) {
		data->rx_buf = net_buf_frag_del(NULL, data->rx_buf);
	}

    ret = net_buf_linearize(
        &http_data.recv_buf[http_data.recv_buf_goted_data],
        http_data.recv_buf_len - http_data.recv_buf_goted_data,
		data->rx_buf, 0, (uint16_t)socket_data_length);
	data->rx_buf = net_buf_skip(data->rx_buf, ret);
	// _http_tmp_buf_length = ret;
	if (ret != socket_data_length) {
		LOG_ERR("Total copied data is different then received data!"
			" copied:%d vs. received:%d", ret, socket_data_length);
		ret = -EINVAL;
	}
    http_data.recv_buf_goted_data += socket_data_length;

	return ret;
}

MODEM_CMD_DEFINE(on_cmd_trans_data)
{
	return on_cmd_sockread_common(data, len);
}


// +HTTPREAD: 0 - Done
MODEM_CMD_DEFINE(on_cmd_trans_done)
{
    k_sem_give(&trans_read_sem);
	return 0;
}

// AT+CFTRANTX Transfer a file from EFS to host
// AT+CFTRANTX=<filepath>[,<location>][,<size>][,<transMode>]
// <filepath>
//      The path of the file on EFS
// <len>
//      The length of the following file data to output.
// <location>
//      The beginning of the file data to output.
// <size>
//      The length of the file data to output.
// <transMode>
//      Whether there is no urc in data output
// 0 normal mode
// 1 data output directly without urc.

// Response
// 1) If successfully:
// [+CFTRANTX: DATA,<len> …
// +CFTRANTX: DATA,<len>]
// +CFTRANTX: 0
// OK
// 2) If <transMode> is 1 :
// >…
// OK
// 3) If failed:
// ERROR

int simcom_fs_transfer(const struct device *dev,
    const char *filepath, int location, int size,
    char *buf, size_t buf_len, size_t *bytes_read)
{
    char send_buf[sizeof("AT+CFTRANTX=\"###########################\",######,####")] = {0};
    int ret;

    /* Modem command to read the data. */
	struct modem_cmd data_cmd[] = {
        MODEM_CMD("+CFTRANTX: DATA,", on_cmd_trans_data, 0U, ""),
        MODEM_CMD("+CFTRANTX: 0", on_cmd_trans_done, 0U, ""),
    };

    if(buf == NULL || buf_len == 0) {
        LOG_ERR("Invalid buffer, Aborting!");
        return -EINVAL;
    }

    (void) memset(buf, 0, buf_len);
    *bytes_read = 0;

    // TODO: I don't like this. I want to use http_data from modem_data
	http_data.recv_buf     = buf;
	http_data.recv_buf_len = buf_len;
	http_data.recv_buf_goted_data = 0;

    k_sem_reset(&trans_read_sem);

    snprintk(send_buf, sizeof(send_buf), "AT+CFTRANTX=\"%s\",%d,%d", filepath, location, size);
    struct modem_data *mdata = dev->data;

	ret = modem_cmd_send_ext(&mdata->mctx.iface, &mdata->mctx.cmd_handler,
			     data_cmd, ARRAY_SIZE(data_cmd), send_buf, &mdata->sem_response,
			     K_SECONDS(30), MODEM_NO_UNSET_CMDS);
	if (ret < 0) {
        LOG_ERR("modem_cmd_send_ext error. What? Why?");
		errno = -ret;
		ret = -1;
		goto exit;
	}
    // Wait semaphore (?)
    ret = k_sem_take(&trans_read_sem, K_SECONDS(2));
	if (ret < 0) {
		LOG_ERR("Timeout waiting for HTTPREAD");
        goto exit;
    }

    *bytes_read = http_data.recv_buf_goted_data;

    // LOG_ERR("TODO: what we read (%d bytes)?", http_data.recv_buf_goted_data);

    // return modem_direct_cmd(mdata, send_buf);
exit:
    (void)modem_cmd_handler_update_cmds(&mdata->cmd_handler_data, NULL, 0U, false);
    return ret;
}

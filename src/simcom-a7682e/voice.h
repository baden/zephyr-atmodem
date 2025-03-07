#pragma once

#include "../simcom-a7682e.h"

void lte_voice_on_clcc(struct modem_data *mdata, int id, int dir, int stat, char *number);
int simcom_voice_answer_call(const struct device *dev);
int simcom_efs_download(const struct device *dev, const char *filename, const char *data, size_t data_len);
int simcom_voice_play(const struct device *dev, const char *filename, int play_path, int repeat);

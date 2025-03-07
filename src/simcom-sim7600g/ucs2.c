#include "utils.h"
#include <stdlib.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(modem_utils, CONFIG_MODEM_LOG_LEVEL);

// TODO: Move to library
static char ctohh(char v)
{
	if(v >= 'a') return(v - 'a' + 10);
	else if(v >= 'A') return(v - 'A' + 10);
	else return (v - '0');
}

static unsigned char atoh2(const char *s)
{
	return (ctohh(s[0]) << 4) | ctohh(s[1]);
}

static unsigned short atoh4(const char *s)
{
	return ctohh(s[0]) * 16*16*16 + ctohh(s[1]) * 16*16 + ctohh(s[2]) * 16 + ctohh(s[3]);
}

// Транскодирует из шестнадцатиричного представления в ascii
// Строка завершается нулем или \"
void hex_to_ascii(char *dest, const char *source, int length)
{
	int i=0;
	if(length == 0) return;
    if(*source == '"') source++;
	length--;
	while((*source != 0) && (*source != '\"') && (i < length)) {
		*dest++ = atoh2(source);
		source += 2;
		i++;
	}
	*dest = 0;
}

// Транскодирует из шестнадцатиричного представления в utf-8 (до 4 байт)
void ucs2_to_utf8(char *dest, const char *source, int length)
{
    if(*source == '"') source++;
    while((length > 1) && (*source != 0) && (*source != '\"')){
        unsigned int v = atoh4(source);
        source += 4;

        // (1 байт ) 0aaa aaaa                                              (7 бит)
        // (2 байта) 110x xxxx 10xx xxxx                                    (11 бит)
        // (3 байта) 1110 xxxx 10xx xxxx 10xx xxxx                          (16 бит)
        // опционально (для UCS4)
        // (4 байта) 1111 0xxx 10xx xxxx 10xx xxxx 10xx xxxx                (21 бит)
        // (5 байт ) 1111 10xx 10xx xxxx 10xx xxxx 10xx xxxx 10xx xxxx      (26 бит) - не поддерживается
        // (6 байт ) 1111 110x 10xx xxxx 10xx xxxx 10xx xxxx 10xx xxxx 10xx xxxx      (31 бит) - не поддерживается
        if(v <= 0x07F) {        // 1 байт  (для значений до 0x007F, 7 бит)
            *dest++ = v;
            length--;
        } else if(v <= 0x7FF) { // 2 байта (для значений до 0x07FF, 11 бит)
            if(length < 3) break;
            *dest++ = 0xC0 | ((v >> 6) & 0x1F);
            *dest++ = 0x80 | (v & 0x3F);
            length -= 2;
        } else if(v <= 0xD7FF) { // 3 байта (для значений до 0xD700, 16 бит)    // Коды D8xx указывают на символы UCS4
            if(length < 4) break;
            *dest++ = 0xE0 | ((v >> 12) & 0x0F);
            *dest++ = 0x80 | ((v >> 6) & 0x3F);
            *dest++ = 0x80 | (v & 0x3F);
            length -= 3;
        } else if(v <= 0xF8FF) { // 4 байта (для значений до 0x100000, 21 бит)    // Коды D800...F8FF указывают на символы UCS4
            if(length < 5) break;
            // Тут процедура чуть упрощена только ради emoji (4 байта, 21 бит)
            // Старшие 11 бит
            // unsigned int v1 = v & 0x03FF;
            // unsigned int v1 = v;
            // + 0x10000  (смещенные на 10 бит)
            v += 0x40;
            #define v1 v
            // Младшие 10 бит
            unsigned int v2 = atoh4(source);
            source += 4;
            // unsigned int v2 = v & 0x03FF;
            // Полное значение
            // unsigned long res = (((unsigned long)v1) << 10) | v2;

            // (4 байта) 1111 0HHH, 10HH HHHH, 10HH LLLL, 10LL LLLL                (21 бит, точнее только 20)
            *dest++ = 0xF0 | ((v1 >> 8) & 0x07);            // 3 бита от старших 10ти бит
            *dest++ = 0x80 | ((v1 >> 2) & 0x3F);            // 6 бит от старших 10ти бит (начиная со второго)
            *dest++ = 0x80 | ((v2 >> 6) & 0x0F) | ((v1 << 4) & 0x30);    // 4 бита от младших 10ти бит + 2 бита от старших 10ти бит
            *dest++ = 0x80 | (v2 & 0x3F);           // 6 бит от младших 10ти бит
            length -= 4;
            #undef v1
        } else {                // Остальное заменяем пробелами
            *dest++ = ' ';
            length--;
        }
    }
    *dest = 0;  // Завершающий ноль.
}

// Транскодирует из UCS2 представления в ascii
void ucs2_to_ascii(char *dest, const char *source, int length)
{
    if(*source == '"') source++;
    while((length > 1) && (*source != 0) && (*source != '\"')){
        unsigned int v = atoh4(source);
        source += 4;

        // (1 байт ) 0aaa aaaa                                              (7 бит)
        // (2 байта) 110x xxxx 10xx xxxx                                    (11 бит)
        // (3 байта) 1110 xxxx 10xx xxxx 10xx xxxx                          (16 бит)
        // опционально (для UCS4)
        // (4 байта) 1111 0xxx 10xx xxxx 10xx xxxx 10xx xxxx                (21 бит)
        // (5 байт ) 1111 10xx 10xx xxxx 10xx xxxx 10xx xxxx 10xx xxxx      (26 бит) - не поддерживается
        // (6 байт ) 1111 110x 10xx xxxx 10xx xxxx 10xx xxxx 10xx xxxx 10xx xxxx      (31 бит) - не поддерживается
        if(v <= 0x07F) {        // 1 байт  (для значений до 0x007F, 7 бит)
            *dest++ = v;
            length--;
        } else {                // Остальное заменяем символом '?'
            *dest++ = '?';
            length--;
        }
    }
    *dest = 0;  // Завершающий ноль.
}


int modem_atoi(const char *s, const int err_value,
		      const char *desc, const char *func)
{
	int   ret;
	char  *endptr;

	ret = (int)strtol(s, &endptr, 10);
	if (!endptr || *endptr != '\0') {
		LOG_ERR("bad %s '%s' in %s", /*log_strdup(*/s/*)*/, /*log_strdup(*/desc/*)*/, /*log_strdup(*/func/*)*/);
		return err_value;
	}

	return ret;
}

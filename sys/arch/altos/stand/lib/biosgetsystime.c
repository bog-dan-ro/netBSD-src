#include <sys/types.h>

#include <lib/libsa/stand.h>
#include <lib/libkern/libkern.h>

#include <libi386.h>

char g_altos_time[0x0a];
static uint64_t inital_date = 0;

static inline uint8_t hexToNr(uint8_t nr)
{
    return ((nr >> 4) * 10) + (nr & 0xf);
}

uint32_t altosgetsystime(void);

static uint32_t date_to_unix(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t seconds) {
    if (year < 1970)
        return 0;

    uint16_t days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    for (int i = 1972; i <= year; i += 4) {
        if ((i % 100 != 0) || (i % 400 == 0))
            days_in_month[2] = 29;
    }

    if (month < 1 || month > 12 || day < 1 || day > days_in_month[month])
        return 0;

    uint32_t days = 0;
    for (int i = 1970; i < year; ++i) {
        days += 365;
        if ((i % 100 != 0) || (i % 400 == 0))
            days += 1;
    }

    for (int i = 1; i < month; ++i)
        days += days_in_month[i];

    days += day - 1;

    uint32_t unix_time = days * 24 * 60 * 60 + hour * 60 * 60 + minute * 60 + seconds;
    return unix_time;
}

int biosgetsystime(void)
{
    uint16_t year = hexToNr(altosgetsystime());
    if (!year)
        return -1;

    year *= 100;
    year += hexToNr(g_altos_time[0x06]); // yr xx
    uint64_t unix_time = date_to_unix(year, hexToNr(g_altos_time[0x05]), hexToNr(g_altos_time[0x04]), hexToNr(g_altos_time[0x03]), hexToNr(g_altos_time[0x02]), hexToNr(g_altos_time[0x01]));
    unix_time *= 1000;
    unix_time += hexToNr(g_altos_time[0x00]) * 10;

    if (!inital_date)
        inital_date = unix_time;
    return unix_time - inital_date;
}

// TV-AC-005: host strings are independent oracles, including binary32 limits.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "iot154_packet.h"
#include "report_value.h"

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); exit(1); \
} } while (0)

int main(void)
{
    const struct { uint8_t event; uint8_t type; uint32_t bits; const char *text; } cases[] = {
        {1,0,0,"closed"}, {1,0,1,"open"}, {5,0,0,"undetected"}, {5,0,1,"detected"},
        {2,0,0,"off"}, {2,0,1,"on"}, {2,0,2,"toggle"},
        {4,0,0,"calibrated"}, {4,0,1,"approximate"}, {4,0,2,"inert"},
        {3,0,65,"65"}, {6,0,100,"100"},
        {255,0,16777217,"16777217"}, {255,0,0x80000000,"-2147483648"},
        {255,0,0x7fffffff,"2147483647"}, {255,0,0xffffffff,"-1"},
        {3,1,0x4283bd71,"65.87"}, {6,1,0x4283bd71,"65.87"},
        {3,1,0,"0.00"}, {6,1,0x42c80000,"100.00"},
        {255,1,0x3f900000,"1.13"}, {255,1,0xbf900000,"-1.13"},
        {255,1,1,"0.00"}, {255,1,0x80000001,"0.00"},
        {255,1,0x4b000000,"8388608.00"},
        {255,1,0x7f7fffff,"340282346638528859811704183484516925440.00"},
        {255,1,0xff7fffff,"-340282346638528859811704183484516925440.00"},
    };
    unsigned count = 0;
    for (size_t i=0; i<sizeof(cases)/sizeof(cases[0]); ++i) {
        char text[REPORT_VALUE_TEXT_SIZE];
        CHECK(report_value_format(cases[i].event,cases[i].type,cases[i].bits,text,sizeof(text)));
        CHECK(strcmp(text,cases[i].text)==0);
        char sentinel[REPORT_VALUE_TEXT_SIZE];
        memset(sentinel,'X',sizeof(sentinel));
        CHECK(!report_value_format(cases[i].event,cases[i].type,cases[i].bits,sentinel,strlen(cases[i].text)));
        CHECK(sentinel[0]=='X');
        ++count;
    }
    const struct { uint8_t event; uint8_t type; uint32_t bits; } invalid[] = {
        {1,1,0}, {5,1,0x3f800000}, {2,1,0}, {4,1,0}, {1,0,2}, {5,0,2},
        {2,0,3}, {4,0,3}, {3,0,101}, {6,0,0xffffffff}, {3,1,0xbf800000},
        {6,1,0x42ca0000}, {255,1,0x7f800000}, {255,1,0xff800000},
        {255,1,0x7fc00000}, {255,1,0x80000000}, {255,2,0},
    };
    for (size_t i=0; i<sizeof(invalid)/sizeof(invalid[0]); ++i) {
        char text[REPORT_VALUE_TEXT_SIZE]="unchanged";
        CHECK(!report_value_format(invalid[i].event,invalid[i].type,invalid[i].bits,text,sizeof(text)));
        CHECK(strcmp(text,"unchanged")==0);
        ++count;
    }
    // Wire-to-host path: independently authored v3 frame for float 65.87.
    const uint8_t wire[24] = {
        3,1,1,0,0,0,0,0,1,0,0,0,0,0,0,0,1,3,1,0x71,0xbd,0x83,0x42,0xfe
    };
    iot154_packet_t packet;
    iot154_packet_decode(wire,&packet);
    CHECK(iot154_packet_is_valid(&packet));
    char text[REPORT_VALUE_TEXT_SIZE];
    CHECK(report_value_format(packet.event_type,packet.value_type,packet.value,text,sizeof(text)));
    CHECK(strcmp(text,"65.87")==0);
    printf("%u Tests 0 Failures 0 Ignored\n",count+1);
    return 0;
}

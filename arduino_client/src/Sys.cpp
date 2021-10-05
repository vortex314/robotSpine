#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <log.h>
#include <Sys.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/time.h>

uint64_t Sys::_upTime;
char Sys::_hostname[30] = "";
uint64_t Sys::_boot_time = 0;



//#include <espressif/esp_wifi.h>

uint32_t Sys::getSerialId()
{
    union
    {
        uint8_t my_id[6];
        uint32_t lsb;
    };
    esp_efuse_mac_get_default(my_id);
    //      esp_efuse_mac_get_custom(my_id);
    //   sdk_wifi_get_macaddr(STATION_IF, my_id);
    return lsb;
}

const char *Sys::getProcessor() { return "ESP8266"; }
const char *Sys::getBuild() { return __DATE__ " " __TIME__; }

uint32_t Sys::getFreeHeap() { return xPortGetFreeHeapSize(); };

const char *Sys::hostname()
{
    if (_hostname[0] == 0)
    {
        Sys::init();
    }
    return _hostname;
}

void Sys::init()
{
    std::string hn;
    union
    {
        uint8_t macBytes[6];
        uint64_t macInt;
    };
    macInt = 0L;
    if (esp_read_mac(macBytes, ESP_MAC_WIFI_STA) != ESP_OK)
        WARN(" esp_base_mac_addr_get() failed.");
    hn = stringFormat("ESP32-%d", macInt & 0xFFFF);
    Sys::hostname(hn.c_str());
}

uint64_t Sys::millis() { return Sys::micros() / 1000; }

uint32_t Sys::sec() { return millis() / 1000; }

uint64_t Sys::micros() { return esp_timer_get_time(); }

uint64_t Sys::now() { return _boot_time + Sys::millis(); }

void Sys::setNow(uint64_t n) { _boot_time = n - Sys::millis(); }

void Sys::hostname(const char *h) { strncpy(_hostname, h, strlen(h) + 1); }

void Sys::setHostname(const char *h) { strncpy(_hostname, h, strlen(h) + 1); }

const char* Sys::board() { return "ESP32-NODEMCU"; }
const char* Sys::cpu() { return "ESP32";}

void Sys::delay(unsigned int delta)
{
    uint32_t end = Sys::millis() + delta;
    while (Sys::millis() < end)
    {
    };
}

extern "C" uint64_t SysMillis() { return Sys::millis(); }

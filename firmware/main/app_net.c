#include "app_net.h"

app_net_link_t app_net_active(bool eth_up, bool wifi_up)
{
    if (eth_up) {
        return APP_NET_ETH;
    }
    return wifi_up ? APP_NET_WIFI : APP_NET_NONE;
}

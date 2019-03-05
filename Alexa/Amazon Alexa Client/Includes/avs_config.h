#ifndef AVS_CONFIG_H
#define AVS_CONFIG_H



///////////////////////////////////////////////////////////////////////////////////
// Set your server IP address
///////////////////////////////////////////////////////////////////////////////////
#define AVS_CONFIG_SERVER_ADDR      PP_HTONL(LWIP_MAKEU32(192, 168, 22, 6))


///////////////////////////////////////////////////////////////////////////////////
// Choose your sampling rate
//   SAMPLING_RATE_44100HZ
//   SAMPLING_RATE_48KHZ
//   SAMPLING_RATE_32KHZ
//   SAMPLING_RATE_8KHZ
///////////////////////////////////////////////////////////////////////////////////
#define AVS_CONFIG_SAMPLING_RATE    SAMPLING_RATE_32KHZ



#include <avs/avs_config_defaults.h>
#endif // AVS_CONFIG_H

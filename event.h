EventFlags mdmEvent;

#define MDM_EVENT_SETUP             0b1
#define MDM_EVENT_CONNECT           0b10
#define MDM_EVENT_OFF               0b100
#define MDM_EVENT_RESET             0b1000
#define MDM_EVENT_SERVER_CONNECT    0b10000
#define MDM_EVENT_SERVER_DISPATCH   0b100000
#define MDM_EVENT_SERVER_DATA       0b1000000
#define MDM_EVENT_SMS_SETUP         0b10000000
#define MDM_EVENT_SMS_READ          0b100000000

#define MDM_EVENT_ALL MDM_EVENT_SETUP | MDM_EVENT_CONNECT | MDM_EVENT_OFF | MDM_EVENT_RESET | MDM_EVENT_SERVER_CONNECT | MDM_EVENT_SERVER_DISPATCH | MDM_EVENT_SERVER_DATA | MDM_EVENT_SMS_SETUP | MDM_EVENT_SMS_READ

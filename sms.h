#include "CellularSMS.h"

bool sms_done = false;
int sms_setup_id = 0;

void smsRead() {
    char sms_buf[SMS_MAX_SIZE_GSM7_SINGLE_SMS_SIZE];
    char num[SMS_MAX_PHONE_NUMBER_SIZE];
    char timestamp[SMS_MAX_TIME_STAMP_SIZE];
    int overflow = 0;

    CellularSMS *sms;
    sms = mdm_device->open_sms();

    if (sms) {
        while (true) {
            nsapi_size_or_error_t sms_len = sms->get_sms(
                                                sms_buf, sizeof(sms_buf),
                                                num, sizeof(num),
                                                timestamp, sizeof(timestamp),
                                                &overflow);

            if (sms_len < NSAPI_ERROR_OK) {
                if (sms_len == -1) {
                    tr_debug("No SMS to read");

                } else {
                    tr_error("SMS error: %i", sms_len);
                }

                return;
            }

            for (uint8_t i = 0; i < sizeof(sms_buf); i++) {
                sms_buf[i] = tolower(sms_buf[i]);
            }

            // Trim trailing spaces, CR, LF
            sms_len = strlen(sms_buf);

            while (sms_buf[sms_len - 1] == ' ' ||    // space
                    sms_buf[sms_len - 1] == 0x0A ||  // CR
                    sms_buf[sms_len - 1] == 0x0D) {  // LF
                sms_buf[sms_len - 1] = 0;
                sms_len = strlen(sms_buf);
            }

            tr_info("Got SMS from \"%s\" with text (%u) \"%s\"", num, sms_len, sms_buf);
        }
    }
}

void smsEventReadSms() {
    mdmEvent.set(MDM_EVENT_SMS_READ);
}

void smsSetupRepeat() {
    sms_setup_id = 0;
    mdmEvent.set(MDM_EVENT_SMS_SETUP);
}

void smsSetup() {
    if (sms_setup_id) {
        tr_debug("SMS setup in progress");
        return;
    }

    if (sms_done) {
        tr_debug("SMS already done");
        return;
    }

    tr_debug("SMS setup");

    CellularSMS *sms;
    sms = mdm_device->open_sms();

    if (sms == nullptr) {
        tr_error("SMS open failed");
        goto TRY_AGAIN;
    }

    if (sms->initialize(CellularSMS::CellularSMSMmodeText) != NSAPI_ERROR_OK) {
        tr_error("SMS init failed");
        goto TRY_AGAIN;
    }

    if (sms->set_cpms("ME", "ME", "ME") != NSAPI_ERROR_OK) {
        tr_error("SMS storage failed");
        goto TRY_AGAIN;
    }

    sms->set_sms_callback(smsEventReadSms);

    tr_info("SMS setup OK");

    sms_done = true;
    mdmEvent.set(MDM_EVENT_SMS_READ);

    return;

TRY_AGAIN:
    sms_setup_id = eQueue.call_in(5s, smsSetupRepeat);

    if (!sms_setup_id) {
        tr_error("No memory, reseting MCU");
        NVIC_SystemReset();
    }
}

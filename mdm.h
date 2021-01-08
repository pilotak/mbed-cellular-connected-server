#include "CellularContext.h"
#include "event.h"
#include "myModem.h"
#include "server.h"
#include "sms.h"

uint8_t registration_status = CellularNetwork::StatusNotAvailable;
int mdm_connect_id = 0;
int mdm_setup_id = 0;
const uint16_t mdm_timeout[7] = {1, 4, 8, 16, 32, 64, 128};

void mdmCb(nsapi_event_t type, intptr_t ptr);

void mdmConnectRepeat() {
    mdm_connect_id = 0;
    mdmEvent.set(MDM_EVENT_CONNECT);
}

void mdmConnect() {
    if (mdm_connect_id) {
        tr_debug("Connect in progress");
        return;
    }

    tr_debug("MDM connect");
    nsapi_error_t ret = mdm->connect();

    if (ret == NSAPI_ERROR_OK) {
        tr_info("Network connected");
        mdmEvent.set(MDM_EVENT_SMS_SETUP | MDM_EVENT_SERVER_CONNECT);
        return;

    } else if (ret == NSAPI_ERROR_AUTH_FAILURE) {
        tr_error("Authentication Failure. Exiting application");

        while (1) {};

    } else if (ret == NSAPI_ERROR_NO_MEMORY) {
        tr_error("No memory, reseting MCU");
        NVIC_SystemReset();
    }

    tr_error("Connecting failed, will try again: %i", ret);
    mdm_connect_id = eQueue.call_in(5s, mdmConnectRepeat);

    if (!mdm_connect_id) {
        tr_error("Calling mdm connect failed, no memory");
    }
}

void mdmSetupRepeat() { // slowEventQueue cb
    mdm_setup_id = 0;
    mdmEvent.set(MDM_EVENT_SETUP);
}

void mdmSetup() {
    if (mdm_setup_id) {
        tr_debug("Setup in progress");
        return;
    }

    tr_debug("Device setup");
    mdm = CellularContext::get_default_instance();

    if (mdm == nullptr) {
        tr_error("No device");
        goto TRY_AGAIN;
    }

    mdm_device = mdm->get_device();

    if (mdm_device == nullptr) {
        tr_error("No interface");
        goto TRY_AGAIN;
    }

    if (mdm_device->hard_power_on() != NSAPI_ERROR_OK) {
        tr_error("Could not power on modem");
        goto TRY_AGAIN;
    }

    mdm_device->set_timeout(10000); // ms
    mdm_device->set_retry_timeout_array(mdm_timeout, 7);
    mdm->set_credentials(MBED_CONF_APP_APN);
    mdm->attach(mdmCb);

#if defined(MBED_CONF_APP_SIM_PIN)
    mdm->set_sim_pin(MBED_CONF_APP_SIM_PIN);
#endif

    tr_info("Modem setup OK");
    mdmEvent.set(MDM_EVENT_CONNECT);

    return;

TRY_AGAIN:
    mdm_setup_id = eQueue.call_in(5s, mdmSetupRepeat);

    if (!mdm_setup_id) {
        tr_error("No memory, reseting MCU");
        NVIC_SystemReset();
    }
}

void mdmOffHelper() {
    if (mdm_connect_id) {
        eQueue.cancel(mdm_connect_id);
        mdm_connect_id = 0;
    }

    if (mdm_setup_id) {
        eQueue.cancel(mdm_setup_id);
        mdm_setup_id = 0;
    }

    if (server_data_id) {
        eQueue.cancel(server_data_id);
        server_data_id = 0;
    }

    if (server_connect_id) {
        eQueue.cancel(server_connect_id);
        server_connect_id = 0;
    }

    mdm->attach(nullptr);
    mdm->disconnect();
    mdm_device->shutdown();
}

void mdmOff() {
    tr_info("Turning modem OFF");
    mdmOffHelper();
}

void mdmReset() {
    tr_info("Reseting modem");
    mdmOffHelper();
    mdm_device->hard_power_off();

    mdm_setup_id = eQueue.call_in(5s, mdmSetupRepeat);

    if (!mdm_setup_id) {
        tr_error("No memory, reseting MCU");
        NVIC_SystemReset();
    }
}

void mdmCb(nsapi_event_t type, intptr_t ptr) {
    if (type >= NSAPI_EVENT_CELLULAR_STATUS_BASE && type <= NSAPI_EVENT_CELLULAR_STATUS_END) {
        cell_callback_data_t *ptr_data = reinterpret_cast<cell_callback_data_t *>(ptr);
        cellular_connection_status_t event = (cellular_connection_status_t)type;

        if (ptr_data->error == NSAPI_ERROR_OK) {
            if (ptr_data->final_try) {
                tr_info("Final_try");
                mdmEvent.set(MDM_EVENT_RESET);
            }

            if (event == CellularDeviceReady) {
                tr_debug("DeviceReady");

            } else if (event == CellularSIMStatusChanged) {
                tr_debug("SIM: %i", static_cast<int>(ptr_data->status_data));

            } else if (event == CellularRegistrationStatusChanged) {
                if (registration_status != ptr_data->status_data && ptr_data->status_data != CellularNetwork::StatusNotAvailable) {
                    tr_debug("Registration: %i", ptr_data->status_data);

                    if (registration_status != CellularNetwork::RegisteredHomeNetwork &&
                            registration_status != CellularNetwork::RegisteredRoaming &&
                            registration_status != CellularNetwork::AlreadyRegistered) {
                        tr_info("Connection reastablished");
                    }

                    if (registration_status == CellularNetwork::RegisteredHomeNetwork ||
                            registration_status == CellularNetwork::RegisteredRoaming ||
                            registration_status == CellularNetwork::AlreadyRegistered) {
                        tr_info("Connection lost");
                    }

                    registration_status = ptr_data->status_data;
                }

            } else if (event == CellularAttachNetwork) {
                if (ptr_data->status_data == CellularNetwork::Attached) {
                    tr_debug("Attached");

                } else {
                    tr_debug("Dettached");
                }

            } else if (event == CellularStateRetryEvent) {
                int retry = *(const int *)ptr_data->data;
                tr_debug("Retry count: %i", retry);

            } else if (event == CellularCellIDChanged) {
                tr_debug("Cell ID changed: %i", static_cast<int>(ptr_data->status_data));

            } else if (event == CellularRegistrationTypeChanged) {
                tr_debug("RegistrationType changed: %i", static_cast<int>(ptr_data->status_data));
            }
        }

    } else if (type == NSAPI_EVENT_CONNECTION_STATUS_CHANGE) {
        tr_debug("Connection status: %i", ptr);
    }
}

void mdmLoop() {
    uint32_t event = mdmEvent.wait_any(MDM_EVENT_ALL);

    if (event & MDM_EVENT_SETUP) {
        mdmSetup();
    }

    if (event & MDM_EVENT_CONNECT) {
        mdmConnect();
    }

    if (event & MDM_EVENT_RESET) {
        mdmReset();
    }

    if (event & MDM_EVENT_SMS_SETUP) {
        smsSetup();
    }

    if (event & MDM_EVENT_SMS_READ) {
        smsRead();
    }

    if (event & MDM_EVENT_SERVER_CONNECT) {
        serverConnect();
    }

    if (event & MDM_EVENT_SERVER_DISPATCH) {
        serverSend();
    }

    if (event & MDM_EVENT_SERVER_DATA) {
        serverData();
    }
}
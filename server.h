TCPSocket socket;
int server_connect_id = 0;
int server_data_id = 0;

void serverDispatch() {
    mdmEvent.set(MDM_EVENT_SERVER_DISPATCH);
}

void serverCb() {
    tr_debug("Server socket cb");
    mdmEvent.set(MDM_EVENT_SERVER_DATA);
}

void serverConnectRepeat() {
    server_connect_id = 0;
    mdmEvent.set(MDM_EVENT_SERVER_CONNECT);
}

void serverConnect() {
    nsapi_error_t ret;
    SocketAddress addr;

    if (server_connect_id) {
        tr_debug("Server connect in progress");
        return;
    }

    tr_debug("Connecting to server");
    ret = socket.open(mdm);

    if (ret != NSAPI_ERROR_OK) {
        tr_error("Socket open error: %i", ret);
        goto TRY_AGAIN;
    }

    ret = mdm->gethostbyname("your-server.com", &addr);

    if (ret != NSAPI_ERROR_OK) {
        goto TRY_AGAIN;
    }

    addr.set_port(8080);
    socket.sigio(serverCb);
    socket.set_blocking(true);

    ret = socket.connect(addr);

    socket.set_blocking(false);

    if (ret != NSAPI_ERROR_OK) {
        tr_error("Server connect error: %i", ret);
        goto TRY_AGAIN;
    }

    tr_info("Server connected");

    if (server_data_id) {
        eQueue.cancel(server_data_id);
    }

    server_data_id = eQueue.call_every(5s, serverDispatch);

    if (!server_data_id) {
        tr_error("No memory, reseting MCU");
        NVIC_SystemReset();
    }

    return;


TRY_AGAIN:
    socket.sigio(nullptr);

    if (ret == NSAPI_ERROR_NO_CONNECTION || ret == NSAPI_ERROR_DEVICE_ERROR) {
        if (server_connect_id) {
            eQueue.cancel(server_connect_id);
            server_connect_id = 0;
        }

        socket.close();
        mdmEvent.set(MDM_EVENT_CONNECT);

    } else {
        if (ret == NSAPI_ERROR_PARAMETER) {
            socket.close();
        }

        server_connect_id = eQueue.call_in(5s, serverConnectRepeat);

        if (!server_connect_id) {
            tr_error("No memory, reseting MCU");
            NVIC_SystemReset();
        }
    }
}

void serverSend() {
    static uint16_t counter = 0;

    char data[6];
    uint16_t len = snprintf(data, sizeof(data), "%u", counter);
    counter++;

    nsapi_size_or_error_t ret = socket.send(data, len);

    if (ret == len) {
        tr_info("Data sent: %s", data);

    } else {
        tr_error("Sending failed");

        if (ret <= NSAPI_ERROR_OK) {
            tr_error("Server error: %i", ret);
            socket.sigio(nullptr);

            if (server_data_id) {
                eQueue.cancel(server_data_id);
                server_data_id = 0;
            }

            if (ret == NSAPI_ERROR_NO_CONNECTION || ret == NSAPI_ERROR_DEVICE_ERROR) {
                mdmEvent.set(MDM_EVENT_CONNECT);

            } else {
                mdmEvent.set(MDM_EVENT_SERVER_CONNECT);
            }
        }
    }
}

void serverData() {
    char data[4] = {0};
    nsapi_size_or_error_t ret;

    while (1) {
        ret = socket.recv(data, sizeof(data));

        if (ret > NSAPI_ERROR_OK) {
            tr_info("Received from server: %.*s", ret, data);
            continue;
        }

        tr_debug("End of data\n");

        if (ret != NSAPI_ERROR_WOULD_BLOCK) {
            tr_error("Server error: %i", ret);
            socket.sigio(nullptr);

            if (server_data_id) {
                eQueue.cancel(server_data_id);
                server_data_id = 0;
            }

            if (ret == NSAPI_ERROR_NO_CONNECTION || ret == NSAPI_ERROR_DEVICE_ERROR) {
                mdmEvent.set(MDM_EVENT_CONNECT);

            } else {
                mdmEvent.set(MDM_EVENT_SERVER_CONNECT);
            }
        }

        break;
    }
}
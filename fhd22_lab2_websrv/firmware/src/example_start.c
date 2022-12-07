#include "app.h"
#include "wdrv_winc_client_api.h"
#include "example_conf.h"

extern APP_DATA appData;

typedef enum
{
    /* Example's state machine's initial state. */
    EXAMP_STATE_INIT=0,
    EXAMP_STATE_WAIT_FOR_STATION,
    EXAMP_STATE_START_TCP_SERVER,
    EXAMP_STATE_SOCKET_LISTENING,
    EXAMP_STATE_DONE,
    EXAMP_STATE_ERROR,
} EXAMP_STATES;

SOCKET serverSocket = -1;
SOCKET clientSocket = -1;
char receivedChar;

static EXAMP_STATES state;
static WDRV_WINC_BSS_CONTEXT  bssCtx;
static WDRV_WINC_AUTH_CONTEXT authCtx;


static void APP_ExampleSocketEventCallback(SOCKET socket, uint8_t messageType, void *pMessage)
{
    switch(messageType)
    {
        case SOCKET_MSG_BIND:
        {
            tstrSocketBindMsg *pBindMessage = (tstrSocketBindMsg*)pMessage;
            if ((NULL != pBindMessage) && (0 == pBindMessage->status))
            {
                SYS_CONSOLE_Print(appData.consoleHandle, "Bind on socket %d successful, server_socket = %d\r\n", socket, serverSocket);
                listen(serverSocket, 0);
            }
            else
            {
                SYS_CONSOLE_Print(appData.consoleHandle, "Bind on socket %d failed\r\n", socket);
                shutdown(serverSocket);
                serverSocket =  -1;
                state = EXAMP_STATE_ERROR;
            }
            break;
        }

        case SOCKET_MSG_LISTEN:
        {
            tstrSocketListenMsg *pListenMessage = (tstrSocketListenMsg*)pMessage;
            if ((NULL != pListenMessage) && (0 == pListenMessage->status))
            {
                SYS_CONSOLE_Print(appData.consoleHandle, "Listen on socket %d successful\r\n", socket);
            }
            else
            {
                SYS_CONSOLE_Print(appData.consoleHandle, "Listen on socket %d failed\r\n", socket);
                shutdown(serverSocket);
                serverSocket =  -1;
                state = EXAMP_STATE_ERROR;
            }
            break;
        }

        case SOCKET_MSG_ACCEPT:
        {
            tstrSocketAcceptMsg *pAcceptMessage = (tstrSocketAcceptMsg*)pMessage;
            if (NULL != pAcceptMessage)
            {
                //-TODO-1: (start) add new socket instance, debug-print it and assign it  -> L1basic_telnet-0-1-Q
                char s[20] = "";
                int newSock = pAcceptMessage->sock;
                SYS_CONSOLE_Print( appData.consoleHandle, "Connection from %s:%d %i\r\n",
                                   inet_ntop(AF_INET, &pAcceptMessage->strAddr.sin_addr.s_addr, s, sizeof(s)), 
                                   _ntohs(pAcceptMessage->strAddr.sin_port), 
                                   newSock);
                clientSocket = newSock;                 
                //-TODO-1: (end)

                //-TODO-2: (start) use new socket for send() and recv()
                //-TODO-2: (end)
            }
            else
            {
                SYS_CONSOLE_Print(appData.consoleHandle, "Accept on socket %d failed\r\n", socket);
                shutdown(serverSocket);
                serverSocket =  -1;
                state = EXAMP_STATE_ERROR;
            }
            break;
        }

        case SOCKET_MSG_RECV:
        {
            tstrSocketRecvMsg *pRecvMessage = (tstrSocketRecvMsg*)pMessage;
            if ((NULL != pRecvMessage) && (pRecvMessage->s16BufferSize > 0))
            {
                SYS_CONSOLE_Print(appData.consoleHandle, "Received something on socket %d [%c] \r\n", socket, (char)pRecvMessage->pu8Buffer[0] );
                //-TODO-3: (start) now handle '0,1,?' = this is the functionality of your first webserver
                // trigger next receiver, otherwise we will never again get further characters
                //-TODO-3: (end)
            }
            else
            {
                SYS_CONSOLE_Print(appData.consoleHandle, "Receive on socket %d failed\r\n", socket);
            }
            break;
        }

        case SOCKET_MSG_SEND:
        {
            break;
        }

        default:
        {
            break;
        }
    }
}

static void APP_ExampleAPConnectNotifyCallback(DRV_HANDLE handle, WDRV_WINC_ASSOC_HANDLE assocHandle, WDRV_WINC_CONN_STATE currentState, WDRV_WINC_CONN_ERROR errorCode)
{
    if (WDRV_WINC_CONN_STATE_CONNECTED == currentState)
    {
        SYS_CONSOLE_Print(appData.consoleHandle, "AP Mode: Station connected\r\n");
    }
    else if (WDRV_WINC_CONN_STATE_DISCONNECTED == currentState)
    {
        SYS_CONSOLE_Print(appData.consoleHandle, "AP Mode: Station disconnected\r\n");
        if (-1 != serverSocket)
        {
            shutdown(serverSocket);
            serverSocket = -1;
        }
         NVIC_SystemReset();
    }
}

#if defined(WLAN_DHCP_SRV_ADDR) && defined(WLAN_DHCP_SRV_NETMASK)
static void APP_ExampleDHCPAddressEventCallback(DRV_HANDLE handle, uint32_t ipAddress)
{
    char s[20];

    SYS_CONSOLE_Print(appData.consoleHandle, "AP Mode: Station IP address is %s\r\n", inet_ntop(AF_INET, &ipAddress, s, sizeof(s)));
    state = EXAMP_STATE_START_TCP_SERVER;
}
#endif

void APP_ExampleInitialize(DRV_HANDLE handle)
{
    SYS_CONSOLE_Print(appData.consoleHandle, "\r\n");
    SYS_CONSOLE_Print(appData.consoleHandle, "===========================================\r\n");
    SYS_CONSOLE_Print(appData.consoleHandle, "WINC3 WiFi TCP Server Soft AP Example\r\n");
    SYS_CONSOLE_Print(appData.consoleHandle, "===========================================\r\n");
    SYS_CONSOLE_Print(appData.consoleHandle, "\r\n");
    state = EXAMP_STATE_INIT;
    serverSocket = -1;
}

void APP_ExampleTasks(DRV_HANDLE handle)
{
    #ifdef DEBUG_APP_STATES
        static int lastState =-1;
        if( state != lastState )
            SYS_CONSOLE_Print(appData.consoleHandle, "(%i)\r\n", state);
        lastState = state;
    #endif
    switch (state)
    {
        case EXAMP_STATE_INIT:
        {
            /* Preset the error state incase any following operations fail. */

            state = EXAMP_STATE_ERROR;

            /* Create the BSS context using default values and then set SSID
             and channel. */

            if (WDRV_WINC_STATUS_OK != WDRV_WINC_BSSCtxSetDefaults(&bssCtx))
            {
                break;
            }

            if (WDRV_WINC_STATUS_OK != WDRV_WINC_BSSCtxSetSSID(&bssCtx, (uint8_t*)WLAN_SSID, strlen(WLAN_SSID)))
            {
                break;
            }

            if (WDRV_WINC_STATUS_OK != WDRV_WINC_BSSCtxSetChannel(&bssCtx, WLAN_CHANNEL))
            {
                break;
            }

#if defined(WLAN_AUTH_OPEN)
            /* Create authentication context for Open. */

            if (WDRV_WINC_STATUS_OK != WDRV_WINC_AuthCtxSetOpen(&authCtx))
            {
                break;
            }
#elif defined(WLAN_AUTH_WEP)
            /* Create authentication context for WEP. */

            if (WDRV_WINC_STATUS_OK != WDRV_WINC_AuthCtxSetWEP(&authCtx, WLAN_WEB_KEY_INDEX, (uint8_t*)WLAN_WEB_KEY, strlen(WLAN_WEB_KEY)))
            {
                break;
            }
#endif

#if defined(WLAN_DHCP_SRV_ADDR) && defined(WLAN_DHCP_SRV_NETMASK)
            /* Enable use of DHCP for network configuration, DHCP is the default
             but this also registers the callback for notifications. */

            if (WDRV_WINC_STATUS_OK != WDRV_WINC_IPDHCPServerConfigure(handle, inet_addr(WLAN_DHCP_SRV_ADDR), inet_addr(WLAN_DHCP_SRV_NETMASK), &APP_ExampleDHCPAddressEventCallback))
            {
                break;
            }
#endif
            /* Register callback for socket events. */

            WDRV_WINC_SocketRegisterEventCallback(handle, &APP_ExampleSocketEventCallback);

            /* Create the AP using the BSS and authentication context. */

            if (WDRV_WINC_STATUS_OK == WDRV_WINC_APStart(handle, &bssCtx, &authCtx, NULL, &APP_ExampleAPConnectNotifyCallback))
            {
                SYS_CONSOLE_Print(appData.consoleHandle, "AP started, you can connect to %s\r\n", WLAN_SSID);
                SYS_CONSOLE_Print(appData.consoleHandle, "On the connected device, start a TCP client connection to %s on port %d\r\n\r\n", WLAN_DHCP_SRV_ADDR, TCP_LISTEN_PORT);

                state = EXAMP_STATE_WAIT_FOR_STATION;
            }
            break;
        }

        case EXAMP_STATE_WAIT_FOR_STATION:
        {
            break;
        }

        case EXAMP_STATE_START_TCP_SERVER:
        {
            /* Create the server socket. */
            serverSocket = socket(AF_INET, SOCK_STREAM, 0);
            if (serverSocket >= 0)
            {
                struct sockaddr_in addr;
                /* Listen on the socket. */
                addr.sin_family = AF_INET;
                addr.sin_port = _htons(TCP_LISTEN_PORT);
                addr.sin_addr.s_addr = 0;
                if (bind(serverSocket, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) < 0)
                {
                    SYS_CONSOLE_Print(appData.consoleHandle, "Socket bind error\r\n");
                    state = EXAMP_STATE_ERROR;
                    break;
                }
                state = EXAMP_STATE_SOCKET_LISTENING;
            }
            else
            {
                SYS_CONSOLE_Print(appData.consoleHandle, "Socket creation error\r\n");
                state = EXAMP_STATE_ERROR;
                break;
            }
            break;
        }

        case EXAMP_STATE_SOCKET_LISTENING:
        {
            break;
        }

        case EXAMP_STATE_DONE:
        {
            break;
        }

        case EXAMP_STATE_ERROR:
        {
            break;
        }

        default:
        {
            break;
        }
    }
}

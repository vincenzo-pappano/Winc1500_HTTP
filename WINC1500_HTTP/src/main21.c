/**
 *
 * \file
 *
 * \brief HTTP File Downloader Example.
 *
 * Copyright (c) 2016-2018 Microchip Technology Inc. and its subsidiaries.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Subject to your compliance with these terms, you may use Microchip
 * software and any derivatives exclusively with Microchip products.
 * It is your responsibility to comply with third party license terms applicable
 * to your use of third party software (including open source software) that
 * may accompany Microchip software.
 *
 * THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE,
 * INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY,
 * AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT WILL MICROCHIP BE
 * LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, INCIDENTAL OR CONSEQUENTIAL
 * LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND WHATSOEVER RELATED TO THE
 * SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS BEEN ADVISED OF THE
 * POSSIBILITY OR THE DAMAGES ARE FORESEEABLE.  TO THE FULLEST EXTENT
 * ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN ANY WAY
 * RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
 * THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
 *
 * \asf_license_stop
 *
 */

/** \mainpage
 * \section intro Introduction
 * This example demonstrates how to connect to an HTTP server and download
 * a file using HTTP client.<br>
 * It uses the following hardware:
 * - the SAM Xplained Pro.
 * - the WINC1500 on EXT1.
 * - the IO1 Xplained Pro on EXT2.
 *
 * \section files Main Files
 * - main.c : Initialize the SD card and WINC1500 Wi-Fi module, then download 
 * a file using HTTP client.
 *
 * \section usage Usage
 * -# Configure below code in the main.h for AP information to be connected.
 * \code
 *    #define MAIN_WLAN_SSID         "DEMO_AP"
 *    #define MAIN_WLAN_AUTH         M2M_WIFI_SEC_WPA_PSK
 *    #define MAIN_WLAN_PSK          "12345678"
 * \endcode
 *
 * -# Configure HTTP URL macro in the main.h file.
 * \code
 *    #define MAIN_HTTP_FILE_URL                   "http://www.microchip.com/Images/45093A-SmartConnectWINC1500_E_US_101014_web.pdf"
 * \endcode
 *
 * -# Build the program and download it into the board.
 * -# On the computer, open and configure a terminal application as following.
 * \code
 *    Baud Rate : 115200
 *    Data : 8bit
 *    Parity bit : none
 *    Stop bit : 1bit
 *    Flow control : none
 * \endcode
 *
 * -# Start the application.
 * -# In the terminal window, the following text should appear:<br>
 *
 * \code
 *    -- HTTP file downloader example --
 *    -- SAMXXX_XPLAINED_PRO --
 *    -- Compiled: xxx xx xxxx xx:xx:xx --
 *
 *    This example requires the AP to have internet access.
 *
 *    init_storage: please plug an SD/MMC card in slot...
 *    init_storage: mounting SD card...
 *    init_storage: SD card mount OK.
 *    main: connecting to WiFi AP DEMO_AP...
 *    wifi_cb: M2M_WIFI_CONNECTED
 *    wifi_cb: IP address is 192.168.1.107
 *    start_download: sending HTTP request...
 *    resolve_cb: www.atmel.com IP address is 72.246.56.186
 *
 *    http_client_callback: HTTP client socket connected.
 *    http_client_callback: request completed.
 *    http_client_callback: received response 200 data size 1147097
 *    store_file_packet: creating file [0:45093A-SmartConnectWINC1500_E_US_101014_web.pdf]
 *    store_file_packet: received[xxx], file size[1147097]
 *    ...
 *    store_file_packet: received[1147097], file size[1147097]
 *    store_file_packet: file downloaded successfully.
 *    main: please unplug the SD/MMC card.
 *    main: done.
 * \endcode
 *
 * \section compinfo Compilation Information
 * This software was written for the GNU GCC compiler using Atmel Studio 6.2
 * Other compilers may or may not work.
 *
 * \section contactinfo Contact Information
 * For further information, visit
 * <A href="http://www.microchip.com">Microchip</A>.\n
 */

#include <errno.h>
#include "asf.h"
#include "main.h"
#include "stdio_serial.h"
#include "driver/include/m2m_wifi.h"
#include "socket/include/socket.h"
#include "iot/http/http_client.h"

#define STRING_EOL                      "\r\n"
#define STRING_HEADER                   "-- HTTP file downloader example --"STRING_EOL \
	"-- "BOARD_NAME " --"STRING_EOL	\
	"-- Compiled: "__DATE__ " "__TIME__ " --"STRING_EOL

static uint32_t milliSeconds = 0;


/** File download processing state. */
static download_state down_state = NOT_READY;
/** Http content length. */
static uint32_t http_file_size = 0;
/** Receiving content length. */
static uint32_t received_file_size = 0;


/** UART module for debug. */
static struct usart_module cdc_uart_module;

/** Instance of Timer module. */
struct sw_timer_module swt_module_inst;

/** Instance of HTTP client module. */
struct http_client_module http_client_module_inst;

/**
 * \brief Initialize download state to not ready.
 */
static void init_state(void)
{
	down_state = NOT_READY;
}

/**
 * \brief Clear state parameter at download processing state.
 * \param[in] mask Check download_state.
 */
static void clear_state(download_state mask)
{
	down_state &= ~mask;
}

/**
 * \brief Add state parameter at download processing state.
 * \param[in] mask Check download_state.
 */
static void add_state(download_state mask)
{
	down_state |= mask;
}

/**
 * \brief File download processing state check.
 * \param[in] mask Check download_state.
 * \return true if this state is set, false otherwise.
 */

static inline bool is_state_set(download_state mask)
{
	return ((down_state & mask) != 0);
}

/**
 * \brief Start file download via HTTP connection.
 */
static void start_download(void)
{
	if (!is_state_set(WIFI_CONNECTED)) {
		printf("start_download: Wi-Fi is not connected.\r\n");
		return;
	}

	if (is_state_set(GET_REQUESTED)) {
		printf("start_download: request is sent already.\r\n");
		return;
	}

	if (is_state_set(DOWNLOADING)) {
		printf("start_download: running download already.\r\n");
		return;
	}

	/* Send the HTTP request. */
	printf("start_download: sending HTTP request...\r\n");
	http_client_send_request(&http_client_module_inst, MAIN_HTTP_FILE_URL, HTTP_METHOD_GET, NULL, NULL);
}

/**
 * \brief Store received packet to file.
 * \param[in] data Packet data.
 * \param[in] length Packet data length.
 */
static void store_file_packet(char *data, uint32_t length)
{
	if ((data == NULL) || (length < 1)) 
	{
		printf("store_file_packet: empty data.\r\n");
		return;
	}

	if (!is_state_set(DOWNLOADING)) 
	{
		received_file_size = 0;
		add_state(DOWNLOADING);
	}

	if (data != NULL) 
	{
		received_file_size += length;
		printf("Packet size: %4lu,  Total:  %5lu/%5lu\r\n",
				(unsigned long) length, 
				(unsigned long) received_file_size, 
				(unsigned long) http_file_size);
		
		if (received_file_size >= http_file_size) 
		{
			printf("store_file_packet: file downloaded successfully.\r\n");
			add_state(COMPLETED);
			return;
		}
	}
}

/**
 * \brief Callback of the HTTP client.
 *
 * \param[in]  module_inst     Module instance of HTTP client module.
 * \param[in]  type            Type of event.
 * \param[in]  data            Data structure of the event. \refer http_client_data
 */
static void http_client_callback(struct http_client_module *module_inst, int type, union http_client_data *data)
{
	switch (type) 
	{
		case HTTP_CLIENT_CALLBACK_SOCK_CONNECTED:
		{
			printf("http_client_callback: HTTP client socket connected.\r\n");
		}
		break;

		case HTTP_CLIENT_CALLBACK_REQUESTED:
		{
			printf("http_client_callback: request completed.\r\n");
			add_state(GET_REQUESTED);
		}
		break;

		case HTTP_CLIENT_CALLBACK_RECV_RESPONSE:
		{
			printf("http_client_callback: received response %u data size %u\r\n",
					(unsigned int)data->recv_response.response_code,
					(unsigned int)data->recv_response.content_length);
			if ((unsigned int)data->recv_response.response_code == 200) 
			{
				http_file_size = data->recv_response.content_length;
				received_file_size = 0;
			} 
			else 
			{
				add_state(CANCELED);
				return;
			}
			if (data->recv_response.content_length <= MAIN_BUFFER_MAX_SIZE) 
			{
				printf("Download Completed (HTTP_CLIENT_CALLBACK_RECV_RESPONSE): Saving data and closing connection\r\n");
				store_file_packet(data->recv_response.content, data->recv_response.content_length);
				http_client_close(module_inst);
				add_state(COMPLETED);
			}
		}
		break;

		case HTTP_CLIENT_CALLBACK_RECV_CHUNKED_DATA:
		{
			store_file_packet(data->recv_chunked_data.data, data->recv_chunked_data.length);

			if (data->recv_chunked_data.is_complete) 
			{
				printf("Download Completed (HTTP_CLIENT_CALLBACK_RECV_CHUNKED_DATA): Closing connection\r\n");
				http_client_close(module_inst);
				add_state(COMPLETED);
			}
		}
		break;

		case HTTP_CLIENT_CALLBACK_DISCONNECTED:
		{
			printf("http_client_callback ==> disconnect code: %d\r\n", data->disconnected.reason);

			/* If disconnect reason is equal to -ECONNRESET(-104),
			 * It means the server has closed the connection (timeout).
			 * This is normal operation.
			 */
			if (data->disconnected.reason == -EAGAIN) 
			{
				/* Server has not responded. Retry immediately. */
				if (is_state_set(DOWNLOADING)) 
				{
					clear_state(DOWNLOADING);
				}

				if (is_state_set(GET_REQUESTED)) 
				{
					clear_state(GET_REQUESTED);
				}

				start_download();
			}
		}
		break;
	}
}

/**
 * \brief Callback to get the data from socket.
 *
 * \param[in] sock socket handler.
 * \param[in] u8Msg socket event type. Possible values are:
 *  - SOCKET_MSG_BIND
 *  - SOCKET_MSG_LISTEN
 *  - SOCKET_MSG_ACCEPT
 *  - SOCKET_MSG_CONNECT
 *  - SOCKET_MSG_RECV
 *  - SOCKET_MSG_SEND
 *  - SOCKET_MSG_SENDTO
 *  - SOCKET_MSG_RECVFROM
 * \param[in] pvMsg is a pointer to message structure. Existing types are:
 *  - tstrSocketBindMsg
 *  - tstrSocketListenMsg
 *  - tstrSocketAcceptMsg
 *  - tstrSocketConnectMsg
 *  - tstrSocketRecvMsg
 */
static void socket_cb(SOCKET sock, uint8_t u8Msg, void *pvMsg)
{
	http_client_socket_event_handler(sock, u8Msg, pvMsg);
}

/**
 * \brief Callback for the gethostbyname function (DNS Resolution callback).
 * \param[in] pu8DomainName Domain name of the host.
 * \param[in] u32ServerIP Server IPv4 address encoded in NW byte order format. If it is Zero, then the DNS resolution failed.
 */
static void resolve_cb(uint8_t *pu8DomainName, uint32_t u32ServerIP)
{
	printf("resolve_cb: %s IP address is %d.%d.%d.%d\r\n\r\n", pu8DomainName,
			(int)IPV4_BYTE(u32ServerIP, 0), (int)IPV4_BYTE(u32ServerIP, 1),
			(int)IPV4_BYTE(u32ServerIP, 2), (int)IPV4_BYTE(u32ServerIP, 3));
	http_client_socket_resolve_handler(pu8DomainName, u32ServerIP);
}

/**
 * \brief Callback to get the Wi-Fi status update.
 *
 * \param[in] u8MsgType type of Wi-Fi notification. Possible types are:
 *  - [M2M_WIFI_RESP_CURRENT_RSSI](@ref M2M_WIFI_RESP_CURRENT_RSSI)
 *  - [M2M_WIFI_RESP_CON_STATE_CHANGED](@ref M2M_WIFI_RESP_CON_STATE_CHANGED)
 *  - [M2M_WIFI_RESP_CONNTION_STATE](@ref M2M_WIFI_RESP_CONNTION_STATE)
 *  - [M2M_WIFI_RESP_SCAN_DONE](@ref M2M_WIFI_RESP_SCAN_DONE)
 *  - [M2M_WIFI_RESP_SCAN_RESULT](@ref M2M_WIFI_RESP_SCAN_RESULT)
 *  - [M2M_WIFI_REQ_WPS](@ref M2M_WIFI_REQ_WPS)
 *  - [M2M_WIFI_RESP_IP_CONFIGURED](@ref M2M_WIFI_RESP_IP_CONFIGURED)
 *  - [M2M_WIFI_RESP_IP_CONFLICT](@ref M2M_WIFI_RESP_IP_CONFLICT)
 *  - [M2M_WIFI_RESP_P2P](@ref M2M_WIFI_RESP_P2P)
 *  - [M2M_WIFI_RESP_AP](@ref M2M_WIFI_RESP_AP)
 *  - [M2M_WIFI_RESP_CLIENT_INFO](@ref M2M_WIFI_RESP_CLIENT_INFO)
 * \param[in] pvMsg A pointer to a buffer containing the notification parameters
 * (if any). It should be casted to the correct data type corresponding to the
 * notification type. Existing types are:
 *  - tstrM2mWifiStateChanged
 *  - tstrM2MWPSInfo
 *  - tstrM2MP2pResp
 *  - tstrM2MAPResp
 *  - tstrM2mScanDone
 *  - tstrM2mWifiscanResult
 */
static void wifi_cb(uint8_t u8MsgType, void *pvMsg)
{
	switch (u8MsgType) {
	case M2M_WIFI_RESP_CON_STATE_CHANGED:
	{
		tstrM2mWifiStateChanged *pstrWifiState = (tstrM2mWifiStateChanged *)pvMsg;
		if (pstrWifiState->u8CurrState == M2M_WIFI_CONNECTED) 
		{
			printf("wifi_cb: M2M_WIFI_CONNECTED\r\n");
			m2m_wifi_request_dhcp_client();
		} 
		else if (pstrWifiState->u8CurrState == M2M_WIFI_DISCONNECTED) 
		{
			printf("wifi_cb: M2M_WIFI_DISCONNECTED\r\n");
			clear_state(WIFI_CONNECTED);
			if (is_state_set(DOWNLOADING)) 
			{
				clear_state(DOWNLOADING);
			}

			if (is_state_set(GET_REQUESTED)) 
			{
				clear_state(GET_REQUESTED);
			}

			m2m_wifi_connect(
					(char *)MAIN_WLAN_SSID, 
					sizeof(MAIN_WLAN_SSID),
					MAIN_WLAN_AUTH, 
					(char *)MAIN_WLAN_PSK, 
					M2M_WIFI_CH_ALL);
		}
		break;
	}

	case M2M_WIFI_REQ_DHCP_CONF:
	{
		uint8_t *pu8IPAddress = (uint8_t *)pvMsg;
		printf("wifi_cb: IP address is %u.%u.%u.%u\r\n",
				pu8IPAddress[0], pu8IPAddress[1], pu8IPAddress[2], pu8IPAddress[3]);
		add_state(WIFI_CONNECTED);
		start_download();
		break;
	}

	default:
		break;
	}
}


/**
 * \brief Configure UART console.
 */
static void configure_console(void)
{
	struct usart_config usart_conf;

	usart_get_config_defaults(&usart_conf);
	usart_conf.mux_setting = EDBG_CDC_SERCOM_MUX_SETTING;
	usart_conf.pinmux_pad0 = EDBG_CDC_SERCOM_PINMUX_PAD0;
	usart_conf.pinmux_pad1 = EDBG_CDC_SERCOM_PINMUX_PAD1;
	usart_conf.pinmux_pad2 = EDBG_CDC_SERCOM_PINMUX_PAD2;
	usart_conf.pinmux_pad3 = EDBG_CDC_SERCOM_PINMUX_PAD3;
	usart_conf.baudrate    = 115200;

	stdio_serial_init(&cdc_uart_module, EDBG_CDC_MODULE, &usart_conf);
	usart_enable(&cdc_uart_module);
}

/**
 * \brief Configure Timer module.
 */
static void configure_timer(void)
{
	struct sw_timer_config swt_conf;
	sw_timer_get_config_defaults(&swt_conf);

	sw_timer_init(&swt_module_inst, &swt_conf);
	sw_timer_enable(&swt_module_inst);
}

/**
 * \brief Configure HTTP client module.
 */
static void configure_http_client(void)
{
	struct http_client_config httpc_conf;
	int ret;

	http_client_get_config_defaults(&httpc_conf);

	httpc_conf.recv_buffer_size = MAIN_BUFFER_MAX_SIZE;
	httpc_conf.timer_inst = &swt_module_inst;

	ret = http_client_init(&http_client_module_inst, &httpc_conf);
	if (ret < 0) {
		printf("configure_http_client: HTTP client initialization failed! (res %d)\r\n", ret);
		while (1) {
		} /* Loop forever. */
	}

	http_client_register_callback(&http_client_module_inst, http_client_callback);
}

/**
 * \brief Main application function.
 *
 * Application entry point.
 *
 * \return program return value.
 */
int main(void)
{
	tstrWifiInitParam param;
	int8_t ret;
	init_state();

	/* Initialize the board. */
	system_init();

	/* Initialize the UART console. */
	configure_console();
	printf(STRING_HEADER);
	printf("\r\nThis example requires the AP to have internet access.\r\n\r\n");

	/* Initialize the Timer. */
	configure_timer();

	/* Initialize the HTTP client service. */
	configure_http_client();

	/* Initialize the BSP. */
	nm_bsp_init();

	/* Initialize Wi-Fi parameters structure. */
	memset((uint8_t *)&param, 0, sizeof(tstrWifiInitParam));

	/* Initialize Wi-Fi driver with data and status callbacks. */
	param.pfAppWifiCb = wifi_cb;
	ret = m2m_wifi_init(&param);
	if (M2M_SUCCESS != ret) {
		printf("main: m2m_wifi_init call error! (res %d)\r\n", ret);
		while (1) {
		}
	}

	/* Initialize socket module. */
	socketInit();
	/* Register socket callback function. */
	registerSocketCallback(socket_cb, resolve_cb);

	/* Connect to router. */
	printf("main: connecting to WiFi AP %s...\r\n", (char *)MAIN_WLAN_SSID);
	m2m_wifi_connect((char *)MAIN_WLAN_SSID, sizeof(MAIN_WLAN_SSID), MAIN_WLAN_AUTH, (char *)MAIN_WLAN_PSK, M2M_WIFI_CH_ALL);
	
	if (SysTick_Config(system_cpu_clock_get_hz() / 1000)) 
	{
		puts("ERR>> Systick configuration error\r\n");
		while (1);
	}
	
	Timer timer;
	TimerInit(&timer);
	TimerCountdown(&timer, 40);
	
	Timer oneSecondTimer;
	TimerInit(&oneSecondTimer);
	TimerCountdown(&oneSecondTimer, 1);
	
	while (true) {
		/* Handle pending events from network controller. */
		m2m_wifi_handle_events(NULL);
		/* Checks the timer timeout. */
		sw_timer_task(&swt_module_inst);
			
		if(TimerIsExpired(&oneSecondTimer))
		{
			port_pin_toggle_output_level(LED_0_PIN);
			TimerCountdown(&oneSecondTimer, 1);
			printf("    %2u\r", TimerLeftMS(&timer)/1000);
		}
		
		if(TimerIsExpired(&timer))
		{
			TimerCountdown(&timer, 60);
			printf("\r\nTimer Expired\r\n");
			if((is_state_set(COMPLETED) || is_state_set(CANCELED)))
			{
				down_state = NOT_READY;
				add_state(WIFI_CONNECTED);
				start_download();
			}
		}
	}
	printf("main: done.\r\n");

	while (1) {
	} /* Loop forever. */

	return 0;
}


void SysTick_Handler(void){
	milliSeconds++;
}

char TimerIsExpired(Timer* timer) {
	long left = timer->end_time - milliSeconds;
	return (left < 0);
}


void TimerCountdownMS(Timer* timer, unsigned int timeout) {
	timer->end_time = milliSeconds + timeout;
}


void TimerCountdown(Timer* timer, unsigned int timeout) {
	timer->end_time = milliSeconds + (timeout * 1000);
}


int TimerLeftMS(Timer* timer) {
	long left = timer->end_time - milliSeconds;
	return (left < 0) ? 0 : left;
}


void TimerInit(Timer* timer) {
	timer->end_time = 0;
}


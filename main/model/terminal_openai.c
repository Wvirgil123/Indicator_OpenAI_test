#include "terminal_openai.h"
#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include "freertos/semphr.h"

#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

#include "esp_crt_bundle.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/esp_debug.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/platform.h"
#include "mbedtls/ssl.h"
#include "nvs.h"

// #define OPENAI_API_KEY_STORAGE "openaiapikey"
#define OPENAI_API_KEY_STORAGE \
	"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"
#define OPENAI_KEY_NAMESPACE "OPENAI"

// #define AZURE_OPENAI_NAME \
// 	"https://seeworld.openai.azure.com/openai"

struct terminal_openai
{
};
static struct view_data_openai_show show_data;
static struct view_data_openai_request request;
static struct view_data_openai_response response;
static const char *TAG = "openai";

static struct terminal_openai __g_openai_model;

static SemaphoreHandle_t __g_http_com_sem;
static SemaphoreHandle_t __g_gpt_com_sem;
static SemaphoreHandle_t __g_dalle_com_sem;
static bool net_flag = false;

static int mbedtls_send_then_recv(char *p_server, char *p_port, char *p_tx,
								  size_t tx_len, char *p_rx, size_t rx_len,
								  int delay_ms)
{
	int ret, flags, len;
	char buf[512];

	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context ctr_drbg;
	mbedtls_ssl_context ssl;
	mbedtls_x509_crt cacert;
	mbedtls_ssl_config conf;
	mbedtls_net_context server_fd;

	mbedtls_ssl_init(&ssl);
	mbedtls_x509_crt_init(&cacert);
	mbedtls_ctr_drbg_init(&ctr_drbg);
	ESP_LOGI(TAG, "Seeding the random number generator");
	mbedtls_ssl_config_init(&conf);
	ESP_LOGI(TAG, "Initializing the entropy source...");
	mbedtls_entropy_init(&entropy);
	ESP_LOGI(TAG, "Initializing the ctr_drbg...");
	if ((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
									 NULL, 0)) != 0)
	{
		ESP_LOGE(TAG, "mbedtls_ctr_drbg_seed returned %d", ret);
		return -1;
	}

	ESP_LOGI(TAG, "Attaching the certificate bundle...");
	ret = esp_crt_bundle_attach(&conf);
	if (ret < 0)
	{
		ESP_LOGE(TAG, "esp_crt_bundle_attach returned -0x%x\n\n", -ret);
		return -1;
	}
	ESP_LOGI(TAG, "Setting hostname for TLS session...");
	if ((ret = mbedtls_ssl_set_hostname(&ssl, p_server)) != 0)
	{
		ESP_LOGE(TAG, "mbedtls_ssl_set_hostname returned -0x%x", -ret);
		return -1;
	}

	ESP_LOGI(TAG, "Setting up the SSL/TLS structure...");
	if ((ret = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT,
										   MBEDTLS_SSL_TRANSPORT_STREAM,
										   MBEDTLS_SSL_PRESET_DEFAULT)) != 0)
	{
		ESP_LOGE(TAG, "mbedtls_ssl_config_defaults returned %d", ret);
		goto exit;
	}

	mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
	mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
	mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
#ifdef CONFIG_MBEDTLS_DEBUG
	mbedtls_esp_enable_debug_log(&conf, CONFIG_MBEDTLS_DEBUG_LEVEL);
#endif

#ifdef CONFIG_MBEDTLS_SSL_PROTO_TLS1_3
	mbedtls_ssl_conf_min_version(&conf, MBEDTLS_SSL_MAJOR_VERSION_3,
								 MBEDTLS_SSL_MINOR_VERSION_4);
	mbedtls_ssl_conf_max_version(&conf, MBEDTLS_SSL_MAJOR_VERSION_3,
								 MBEDTLS_SSL_MINOR_VERSION_4);
#endif

	if ((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0)
	{
		ESP_LOGE(TAG, "mbedtls_ssl_setup returned -0x%x\n\n", -ret);
		goto exit;
	}

	mbedtls_net_init(&server_fd);

	ESP_LOGI(TAG, "Connecting to %s:%s...", p_server, p_port);

	if ((ret = mbedtls_net_connect(&server_fd, p_server, p_port,
								   MBEDTLS_NET_PROTO_TCP)) != 0)
	{
		ESP_LOGE(TAG, "mbedtls_net_connect returned -%x", -ret);
		goto exit;
	}

	ESP_LOGI(TAG, "Connected.");

	mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv,
						NULL);

	ESP_LOGI(TAG, "Performing the SSL/TLS handshake...");

	while ((ret = mbedtls_ssl_handshake(&ssl)) != 0)
	{
		if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
		{
			ESP_LOGE(TAG, "mbedtls_ssl_handshake returned -0x%x", -ret);
			goto exit;
		}
	}

	ESP_LOGI(TAG, "Verifying peer X.509 certificate...");

	if ((flags = mbedtls_ssl_get_verify_result(&ssl)) != 0)
	{
		/* In real life, we probably want to close connection if ret != 0 */
		ESP_LOGW(TAG, "Failed to verify peer certificate!");
		bzero(buf, sizeof(buf));
		mbedtls_x509_crt_verify_info(buf, sizeof(buf), "  ! ", flags);
		ESP_LOGW(TAG, "verification info: %s", buf);
	}
	else
	{
		ESP_LOGI(TAG, "Certificate verified.");
	}

	ESP_LOGI(TAG, "Cipher suite is %s", mbedtls_ssl_get_ciphersuite(&ssl));

	ESP_LOGI(TAG, "Writing HTTP request\r\n%s", p_tx);

	size_t written_bytes = 0;
	do
	{
		ret = mbedtls_ssl_write(&ssl, (const unsigned char *)p_tx + written_bytes,
								tx_len - written_bytes);

		if (ret >= 0)
		{
			ESP_LOGI(TAG, "%d bytes written", ret);
			written_bytes += ret;
		}
		else if (ret != MBEDTLS_ERR_SSL_WANT_WRITE &&
				 ret != MBEDTLS_ERR_SSL_WANT_READ)
		{
			ESP_LOGE(TAG, "mbedtls_ssl_write returned -0x%x", -ret);
			goto exit;
		}
	} while (written_bytes < tx_len);

	if (delay_ms > 0)
	{
		vTaskDelay(delay_ms / portTICK_PERIOD_MS); // wait
	}

	ESP_LOGI(TAG, "Reading HTTP response..."); // HERE！！！

	size_t recv_len = 0;

	do
	{
		ESP_LOGI(TAG, "mbedtls_ssl_read !!");
		ret = mbedtls_ssl_read(&ssl, (unsigned char *)(p_rx + recv_len), rx_len - recv_len);
		ESP_LOGI(TAG, "mbedtls_ssl_read returned %d", ret);
		if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
			continue;

		if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
		{
			ret = 0;
			break;
		}
		if (ret < 0)
		{
			ESP_LOGE(TAG, "mbedtls_ssl_read returned -0x%x", -ret);
			break;
		}
		if (ret == 0)
		{
			ESP_LOGI(TAG, "connection closed");
			break;
		}
		len = ret;
		for (int i = 0; i < len; i++)
		{
			putchar(p_rx[i + recv_len]);
		}
		recv_len += len;
	} while (1);

	ESP_LOGI(TAG, "recv total: %d bytes ", recv_len);

	mbedtls_ssl_close_notify(&ssl);
exit:
	mbedtls_ssl_session_reset(&ssl);
	mbedtls_net_free(&server_fd);

	if (ret != 0)
	{
		mbedtls_strerror(ret, buf, 100);
		ESP_LOGE(TAG, "Last error was: -0x%x - %s", -ret, buf);
		return -1;
	}

	return recv_len;
}

#define WEB_SERVER "api.openai.com"
#define WEB_PORT "443"

static char *p_recv_buf;
static size_t recv_buf_max_len;

static char openai_api_key[52];
static bool have_key = false;

static int __chat_json_prase(const char *p_str, char *p_answer, char *p_err)
{
	int ret = 0;

	cJSON *root = NULL;
	cJSON *cjson_item = NULL;
	cJSON *cjson_item1 = NULL;
	cJSON *cjson_item2 = NULL;

	root = cJSON_Parse(p_str);
	if (root == NULL)
	{
		strncpy(p_err, "Parse json fail", sizeof(p_err));
		return -1;
	}

	cjson_item = cJSON_GetObjectItem(root, "error");
	if (cjson_item != NULL)
	{
		cjson_item1 = cJSON_GetObjectItem(cjson_item, "message");
		if (cjson_item1 != NULL && cjson_item1->valuestring != NULL)
		{
			strncpy(p_err, cjson_item1->valuestring, sizeof(p_err));
		}
		cJSON_Delete(root);
		return -1;
	}

	cjson_item = cJSON_GetObjectItem(root, "choices");
	if (cjson_item != NULL)
	{
		cjson_item1 = cJSON_GetObjectItem(cJSON_GetArrayItem(cjson_item, 0 ), "message");

		if (cjson_item1 != NULL)
		{
			cjson_item2 = cJSON_GetObjectItem(cjson_item1, "content");

			if (cjson_item2 != NULL && cjson_item2->valuestring != NULL)
			{
				strncpy(p_answer, cjson_item2->valuestring, strlen(cjson_item2->valuestring));
				cJSON_Delete(root);
				return 0;
			}
		}
	}
	strncpy(p_err, "Not find answer", sizeof(p_err));
	return -1;
}


static int chat_request(struct view_data_openai_request *p_req,
						struct view_data_openai_response *p_resp)
{
	char request_buf[1024];
	char data_buf[1024];

	int data_len = 0;
	int ret = 0;
	int len = 0;

	// memset(request_buf, sizeof(request_buf));
	// memset(data_buf, sizeof(data_buf));
	memset(request_buf, 0, sizeof(request_buf));
	memset(data_buf, 0, sizeof(data_buf));
	data_len = sprintf(data_buf,
					   "{\"model\":\"gpt-3.5-turbo\",\"messages\":[{\"role\":"
					   "\"user\",\"content\":\"%s\"}],\"temperature\":0.7}",
					   p_req->question);
	len += sprintf(request_buf + len, "POST /v1/chat/completions HTTP/1.1\r\n");
	len += sprintf(request_buf + len, "Host: %s\r\n", WEB_SERVER);
	len += sprintf(request_buf + len, "Connection: Close\r\n");
	len += sprintf(request_buf + len, "Content-Type: application/json\r\n");
	len += sprintf(request_buf + len, "Content-Length: %d\r\n", data_len);
	len += sprintf(request_buf + len, "Authorization: Bearer %s\r\n",
				   openai_api_key);
	len += sprintf(request_buf + len, "\r\n");
	len += sprintf(request_buf + len, "%s", data_buf);

	memset(p_recv_buf, 0, recv_buf_max_len);
	ret = mbedtls_send_then_recv(WEB_SERVER, WEB_PORT, request_buf, len,
								 p_recv_buf, recv_buf_max_len, 100);
	ESP_LOGI(TAG, "mbedtls ret = %d", ret);
	if (ret < 0)
	{
		ESP_LOGE(TAG, "mbedtls request fail");
		p_resp->ret = 0;
		strcpy(p_resp->err_msg, "Request fail");
		return -1;
	}
	ESP_LOGI(TAG, "Starting using strstr");
	char *p_json = strstr(p_recv_buf, "\r\n\r\n");
	if (p_json == NULL)
	{
		ESP_LOGE(TAG, "Response format error");
		p_resp->ret = 0;
		strcpy(p_resp->err_msg, "Response format error");
		return -1;
	}

	p_json += 4;

	p_resp->p_answer = p_recv_buf + recv_buf_max_len / 2; // use p_recv_buf mem
	ESP_LOGI(TAG, "Starting __chat_json_prase");
	ret = __chat_json_prase(p_json, p_resp->p_answer, p_resp->err_msg);
	if (ret != 0)
	{
		p_resp->ret = 0;
		return -1;
	}

	p_resp->ret = 1;
	return 0;
}

static int __image_json_prase(const char *p_str, char *p_url, char *p_err)
{
	int ret = 0;

	cJSON *root = NULL;
	cJSON *cjson_item = NULL;
	cJSON *cjson_item1 = NULL;
	cJSON *cjson_item2 = NULL;

	root = cJSON_Parse(p_str);
	if (root == NULL)
	{
		strncpy(p_err, "Parse json fail", sizeof(p_err));
		return -1;
	}

	cjson_item = cJSON_GetObjectItem(root, "error");
	if (cjson_item != NULL)
	{
		cjson_item1 = cJSON_GetObjectItem(cjson_item, "message");
		if (cjson_item1 != NULL && cjson_item1->valuestring != NULL)
		{
			strncpy(p_err, cjson_item1->valuestring, sizeof(p_err));
		}
		cJSON_Delete(root);
		return -1;
	}

	cjson_item = cJSON_GetObjectItem(root, "data");
	if (cjson_item != NULL)
	{
		cjson_item1 = cJSON_GetObjectItem(cjson_item, "url");

		if (cjson_item1 != NULL && cjson_item1->valuestring != NULL)
		{
			strncpy(p_url, cjson_item1->valuestring, sizeof(p_url));
			cJSON_Delete(root);
			return 0;
		}
	}
	strncpy(p_err, "Not find url", sizeof(p_err));
	return -1;
}

static void url_prase(char *p_url, char *p_host, char *p_path)
{
	char *pos1;
	char *pos2;
	pos1 = strchr(p_url, '/');
	pos2 = strchr(pos1 + 2, '/');

	strncpy(p_host, pos1 + 2, pos2 - (pos1 + 2));
	strncpy(p_path, pos2, strlen(pos2) + 1);
}

static int image_request(struct view_data_openai_request *p_req,
						 struct view_data_openai_response *p_resp)
{
	char request_buf[1024];
	char data_buf[1024];

	int data_len = 0;
	int ret = 0;
	int len = 0;

	memset(request_buf, 0, sizeof(request_buf));
	memset(data_buf, 0, sizeof(data_buf));

	data_len =
		sprintf(data_buf, "{\"prompt\":\"%s\",\"n\":1,\"size\":\"512x512\"}",
				p_req->question);

	len += sprintf(request_buf + len, "POST /v1/images/generations HTTP/1.1\r\n");
	len += sprintf(request_buf + len, "Host: %s\r\n", WEB_SERVER);
	len += sprintf(request_buf + len, "Content-Type: application/json\r\n");
	len += sprintf(request_buf + len, "Content-Length: %d\r\n", data_len);
	len += sprintf(request_buf + len, "Authorization: Bearer %s\r\n",
				   openai_api_key);
	len += sprintf(request_buf + len, "\r\n");
	len += sprintf(request_buf + len, "%s", data_buf);

	memset(p_recv_buf, 0, recv_buf_max_len);
	ret = mbedtls_send_then_recv(WEB_SERVER, WEB_PORT, request_buf, len,
								 p_recv_buf, recv_buf_max_len, 2000);
	if (ret < 0)
	{
		ESP_LOGE(TAG, "mbedtls request fail");
		p_resp->ret = 0;
		strcpy(p_resp->err_msg, "Request fail");
		return -1;
	}

	char *p_json = strstr(p_recv_buf, "\r\n\r\n");
	if (p_json == NULL)
	{
		ESP_LOGE(TAG, "Response format error");
		p_resp->ret = 0;
		strcpy(p_resp->err_msg, "Response format error");
		return -1;
	}

	p_json += 4;

	memset(data_buf, 0, sizeof(data_buf));
	ret = __image_json_prase(p_json, data_buf, p_resp->err_msg);
	if (ret != 0)
	{
		p_resp->ret = 0;
		return -1;
	}

	// download image
	ESP_LOGI(TAG, "Download image from (%s)...", data_buf);

	char host[64];
	char path[512];

	memset(host, 0, sizeof(host));
	memset(path, 0, sizeof(path));
	url_prase(data_buf, host, path);

	len = memset(request_buf, 0, sizeof(request_buf));
	len += sprintf(request_buf + len, "GET %s HTTP/1.1\r\n", path);
	len += sprintf(request_buf + len, "Host: %s\r\n", host);
	len += sprintf(request_buf + len, "\r\n");

	memset(p_recv_buf, 0, recv_buf_max_len);
	ret = mbedtls_send_then_recv(WEB_SERVER, WEB_PORT, request_buf, len,
								 p_recv_buf, recv_buf_max_len, 2000);
	if (ret < 0)
	{
		ESP_LOGE(TAG, "Download fail");
		p_resp->ret = 0;
		strcpy(p_resp->err_msg, "Download fail");
		return -1;
	}

	char *p_png = strstr(p_recv_buf, "\r\n\r\n");
	if (p_json == NULL)
	{
		ESP_LOGE(TAG, "Response format error");
		p_resp->ret = 0;
		strcpy(p_resp->err_msg, "Response format error");
		return -1;
	}

	p_png += 4;
	p_resp->p_answer = p_png;
	p_resp->ret = 1;
	return 0;
}

static void __openai_api_key_read(void)
{
	esp_err_t ret = 0;

	// size_t len = sizeof(openai_api_key);
	// TODO 前置：Console 输入，然后储存
	// ret = terminal_storage_read(OPENAI_KEY_NAMESPACE, (void *)openai_api_key,
	// 							&len);
	if (strncpy(openai_api_key, OPENAI_API_KEY_STORAGE, sizeof(openai_api_key)) != NULL)
	{
		ret = ESP_OK;
	}
	else
	{
		ret = ESP_ERR_NVS_NOT_FOUND;
	}
	// if (ret == ESP_OK && len == (sizeof(openai_api_key)))
	if (ret == ESP_OK)
	{
		have_key = true;
		esp_event_post_to(view_event_handle, VIEW_EVENT_BASE, VIEW_EVENT_OPENAI_ST, &have_key, sizeof(have_key), portMAX_DELAY);

		ESP_LOGI(TAG, "openai_api_key read successful");
	}
	else
	{
		// err or not find
		have_key = false;
		esp_event_post_to(view_event_handle, VIEW_EVENT_BASE, VIEW_EVENT_OPENAI_ST, &have_key, sizeof(have_key), portMAX_DELAY);
		if (ret == ESP_ERR_NVS_NOT_FOUND)
		{
			ESP_LOGI(TAG, "openai_api_key not find");
		}
		else
		{
			ESP_LOGI(TAG, "openai_api_key read err:%d", ret);
		}
	}
}

static int __openai_init()
{
	recv_buf_max_len = 1024 * 1024;
	p_recv_buf = malloc(recv_buf_max_len); // from psram
	if (p_recv_buf == NULL)
	{
		ESP_LOGE(TAG, "malloc %s bytes fail!", recv_buf_max_len);
	}
}

static void __terminal_http_task(void *p_arg)
{
	int err = -1;

	bool city_flag = false;
	bool ip_flag = false;
	bool time_zone_flag = false;

	xSemaphoreTake(__g_http_com_sem, portMAX_DELAY);

	while (1)
	{
		if (net_flag)
		{
			// ESP_LOGI(TAG, "OPENAI: WIFI Conecnted");

			// 等待 GPT 信号量
			if (xSemaphoreTake(__g_gpt_com_sem, pdMS_TO_TICKS(100)) == pdTRUE) // 下面没有进行完，会堵塞
			{
				// 启动 GPT 请求任务或调用 GPT 请求函数
				// ...
				ESP_LOGI(TAG, "get INTO SEMAPHORE: %s", request.question);

				int ret = chat_request(&request, &response); // DEBUG HERE
				if (ret != 0)
				{
					ESP_LOGI(TAG, "OPENAI RET: %d", response.ret);
					printf("Failed to send OPENAI chat request: %s\n", response.err_msg);
				}
				else
				{
					/* Global Test*/ 
					strncpy(show_data.question, request.question, sizeof(show_data.question));
					show_data.p_answer = response.p_answer;
					ESP_LOGI(TAG, "OPENAI RET: %d", response.ret);
					ESP_LOGI(TAG, "OPENAI chat response: %s", response.p_answer);
					
					ESP_LOGI(TAG, "show_data.question: %s", show_data.question);
					ESP_LOGI(TAG, "show_data.p_answer: %s", show_data.p_answer);
					/* END Global */
					esp_event_post_to(view_event_handle, VIEW_EVENT_BASE, VIEW_EVENT_CHATGPT_VIEW, &show_data, sizeof(struct view_data_openai_show), portMAX_DELAY);
				}
			}

			// 等待 DALL-E 信号量
			if (xSemaphoreTake(__g_dalle_com_sem, pdMS_TO_TICKS(100)) == pdTRUE)
			{
				// 启动 DALL-E 请求任务或调用 DALL-E 请求函数
				// ...
			}
		}
		else
		{
			// ESP_LOGI(TAG, "OPENAI: WIFI Not Conecnted");
		}
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
	vTaskDelete(NULL);
}

static void __view_event_handler(void *handler_args, esp_event_base_t base,
								 int32_t id, void *event_data)
{
	switch (id)
	{
	case VIEW_EVENT_WIFI_ST: // WIFI 會促發此事件
	{
		ESP_LOGI(TAG, "event: VIEW_EVENT_WIFI_ST");
		struct view_data_wifi_st *p_st = (struct view_data_wifi_st *)event_data;
		if (p_st->is_network)
		{
			net_flag = true;				  // 当 WIFI 连接后，才允许网络获取数据
			xSemaphoreGive(__g_http_com_sem); // right away  get city and time zone
		}
		else
		{
			net_flag = false;
		}

		break;
	}
	case VIEW_EVENT_CHATGPT_REQUEST: // 等待 view 給予的事件
	{
		if (net_flag == false)
		{
			ESP_LOGI(TAG, "event: VIEW_EVENT_CHATGPT_REQUEST, net_flag == false");
			break;
		}
		else
		{ // 网络连接正常，放到任务里面

			ESP_LOGI(TAG, "event: VIEW_EVENT_CHATGPT_REQUEST");
			// struct view_data_openai_request *request = (struct view_data_openai_request *)event_data;
			// request = (struct view_data_openai_request *)event_data;

			/* 全局的方式 */
			strncpy(request.question, ((struct view_data_openai_request *)event_data)->question, sizeof(request.question) - 1);
			// 测试 p_post 的内容是否正确
			ESP_LOGI(TAG, "rev: p_post->p_question: %s", request.question);
			response.ret = 0;
			/* END 全局 */

			// __openai_chatgpt_post(p_post);
			// 发送完成信号
			xSemaphoreGive(__g_gpt_com_sem);
		}
		break;
	}

	default:
		break;
	}
}

int terminal_openai_init(void)
{
	__g_http_com_sem = xSemaphoreCreateBinary();
	__g_gpt_com_sem = xSemaphoreCreateBinary();
	__g_dalle_com_sem = xSemaphoreCreateBinary();

	__openai_api_key_read();
	__openai_init();

	// strncpy(request.question, "Say Hi", sizeof(request.question) - 1);

	ESP_ERROR_CHECK(esp_event_handler_instance_register_with(
		view_event_handle, VIEW_EVENT_BASE, VIEW_EVENT_WIFI_ST,
		__view_event_handler, NULL, NULL)); // Check WIFI ST

	ESP_ERROR_CHECK(esp_event_handler_instance_register_with(
		view_event_handle, VIEW_EVENT_BASE, VIEW_EVENT_OPENAI_ST,
		__view_event_handler, NULL, NULL)); // Check OPENAI ST

	xTaskCreate(&__terminal_http_task, "__terminal_http_task", 1024 * 10, NULL, 10,
				NULL);


	ESP_ERROR_CHECK(esp_event_handler_instance_register_with(
		view_event_handle, VIEW_EVENT_BASE, VIEW_EVENT_CHATGPT_REQUEST,
		__view_event_handler, NULL, NULL)); // OPENAI POST TXT

	// while(1) {
	// 	vTaskDelay(pdMS_TO_TICKS(1000));
	// }

}

// View 有 VIEW_EVENT_WIFI_ST
// 在某一個 .c 文件中也有對應的 VIEW_EVENT_WIFI_ST

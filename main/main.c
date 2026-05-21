// main/main.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "driver/i2s_std.h"
#include "soc/soc_caps.h"
#include "assert.h"
#include "pins.h"

// --- Speech Recognition ---
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "model_path.h"

// --- Model Names (as strings) ---
#define MY_WAKENET_KEYWORD "hiesp"
#define MY_MULTINET_KEYWORD ESP_MN_ENGLISH

static const char *TAG = "SR_MINIMAL";
static i2s_chan_handle_t rx_chan; // I2S RX channel handle

// ---------- I2S: INMP441-friendly config (32-bit slot, mono LEFT, no MCLK)
static void i2s_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_chan));

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = SR_SAMPLE_RATE,   // 16000
            .clk_src        = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple  = I2S_MCLK_MULTIPLE_256, // OK even if MCLK not routed
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT,  // INMP441: 24b in 32b frames
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT,
            .slot_mode      = I2S_SLOT_MODE_MONO,        // one active slot
            .slot_mask      = I2S_STD_SLOT_LEFT,         // L/R=GND -> LEFT; flip if needed
            .ws_width       = I2S_SLOT_BIT_WIDTH_32BIT,
            .ws_pol         = false,
            .bit_shift      = true,                      // std I2S framing
#if SOC_I2S_HW_VERSION_1
            .msb_right      = false,
#else
            .left_align     = true,
            .big_endian     = false,
            .bit_order_lsb  = false,
#endif
        },
        .gpio_cfg = {
            .mclk = I2S_MCLK,   // -1 (unused by INMP441)
            .bclk = I2S_BCLK,
            .ws   = I2S_WS,
            .dout = -1,         // RX only
            .din  = I2S_DIN,
            .invert_flags = { 0 },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));
    ESP_LOGI(TAG, "I2S ready: 16kHz, 32-bit slot, mono LEFT, no MCLK");
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    i2s_init();

    srmodel_list_t *models = esp_srmodel_init("model");
    if (!models) {
        ESP_LOGE(TAG, "Failed to initialize speech recognition models");
        return;
    }

    ESP_LOGI(TAG, "Loaded %d speech models from partition", models->num);
    for (int i = 0; i < models->num; ++i) {
        ESP_LOGI(TAG, "Model[%d]: %s", i, models->model_name[i]);
    }

    char *wn_name = esp_srmodel_filter(models, ESP_WN_PREFIX, MY_WAKENET_KEYWORD);
    if (!wn_name) {
        ESP_LOGE(TAG, "WakeNet model %s not found in model partition", MY_WAKENET_KEYWORD);
        esp_srmodel_deinit(models);
        return;
    }

    char *mn_name = esp_srmodel_filter(models, ESP_MN_PREFIX, MY_MULTINET_KEYWORD);
    if (!mn_name) {
        ESP_LOGE(TAG, "MultiNet model %s not found in model partition", MY_MULTINET_KEYWORD);
        esp_srmodel_deinit(models);
        return;
    }

    char wn_model_name[MODEL_NAME_MAX_LENGTH] = {0};
    strncpy(wn_model_name, wn_name, sizeof(wn_model_name) - 1);

    char mn_model_name[MODEL_NAME_MAX_LENGTH] = {0};
    strncpy(mn_model_name, mn_name, sizeof(mn_model_name) - 1);
    ESP_LOGI(TAG, "Using WakeNet model: %s", wn_model_name);
    ESP_LOGI(TAG, "Using MultiNet model: %s", mn_model_name);

    afe_config_t *afe_config = afe_config_init("M", models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);
    if (!afe_config) {
        ESP_LOGE(TAG, "Failed to allocate AFE config");
        esp_srmodel_deinit(models);
        return;
    }

    // Basic PCM + WakeNet
    afe_config->pcm_config.sample_rate = SR_SAMPLE_RATE; // 16kHz
    afe_config->pcm_config.mic_num = 1;
    afe_config->pcm_config.total_ch_num = 1;
    afe_config->wakenet_model_name = wn_model_name;
    afe_config->wakenet_mode = DET_MODE_90;
    afe_config->wakenet_init = true;

    // Lightweight AFE pipeline (fine for WN+MN)
    afe_config->aec_init = false;
    afe_config->se_init  = false;
    afe_config->ns_init  = false;
    afe_config->vad_init = false;
    afe_config->agc_init = false;
    afe_config->afe_mode = AFE_MODE_LOW_COST;
    afe_config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_INTERNAL;
    afe_config->afe_ringbuf_size = 6;

    esp_afe_sr_iface_t *afe = esp_afe_handle_from_config(afe_config);
    if (!afe) {
        ESP_LOGE(TAG, "Failed to acquire AFE interface");
        afe_config_free(afe_config);
        esp_srmodel_deinit(models);
        return;
    }

    esp_afe_sr_data_t *afe_data = afe->create_from_config(afe_config);
    if (!afe_data) {
        ESP_LOGE(TAG, "Failed to create AFE instance");
        afe_config_free(afe_config);
        esp_srmodel_deinit(models);
        return;
    }

    afe_config_free(afe_config);

    esp_mn_iface_t *mn = esp_mn_handle_from_name(mn_model_name);
    if (!mn) {
        ESP_LOGE(TAG, "Failed to load MultiNet interface for %s", mn_model_name);
        afe->destroy(afe_data);
        return;
    }

    model_iface_data_t *mn_data = mn->create(mn_model_name, 6000);
    if (!mn_data) {
        ESP_LOGE(TAG, "Failed to create MultiNet model context");
        afe->destroy(afe_data);
        esp_srmodel_deinit(models);
        return;
    }

    esp_srmodel_deinit(models);

    const int feed_channels  = afe->get_feed_channel_num(afe_data);   // expect 1
    const int feed_chunksize = afe->get_feed_chunksize(afe_data);     // samples per fetch
    const size_t feed_samples = (size_t)feed_chunksize * (size_t)feed_channels;

    // I2S returns 32-bit words; AFE expects 16-bit mono
    uint32_t *raw32  = (uint32_t *)malloc(feed_samples * sizeof(uint32_t));
    int16_t  *feed16 = (int16_t  *)malloc(feed_samples * sizeof(int16_t));
    if (!raw32 || !feed16) {
        ESP_LOGE(TAG, "Failed to allocate feed buffers");
        if (raw32) free(raw32);
        if (feed16) free(feed16);
        mn->destroy(mn_data);
        afe->destroy(afe_data);
        return;
    }

    ESP_LOGI(TAG, "AFE feed: channels=%d, chunk=%d samples", feed_channels, feed_chunksize);
    ESP_LOGI(TAG, "Speech Recognition Ready!");
    ESP_LOGI(TAG, "Say 'Hi ESP' to activate.");

    typedef enum { ST_IDLE, ST_AWAKE } sr_state_t;
    sr_state_t st = ST_IDLE;
    int awake_ticks = 0;
    const int AWAKE_MAX_TICKS = 200; // ~2s depending on chunk time

    while (1) {
        size_t bytes_read = 0;
        // Read 32-bit slots (24-bit signed data left-justified)
        esp_err_t r = i2s_channel_read(rx_chan, raw32, feed_samples * sizeof(uint32_t), &bytes_read, portMAX_DELAY);
        if (r != ESP_OK || bytes_read != feed_samples * sizeof(uint32_t)) {
            ESP_LOGW(TAG, "I2S short read (%d bytes, err=%d)", (int)bytes_read, (int)r);
            continue;
        }

        // Convert 24-bit signed (top bits) to 16-bit
        for (size_t i = 0; i < feed_samples; ++i) {
            int32_t s = (int32_t)raw32[i];
            feed16[i] = (int16_t)(s >> 16); // take top 16 bits with sign
        }

        // Optional: RMS probe (throttled)
        static int rms_div = 0;
        if ((rms_div++ % 50) == 0) {
            int64_t acc = 0;
            for (size_t i = 0; i < feed_samples; ++i) acc += (int32_t)feed16[i] * feed16[i];
            int32_t rms = (int32_t)sqrtf((float)acc / (float)feed_samples);
            ESP_LOGI(TAG, "I2S RMS ~= %d", rms); // expect >~200 when speaking near mic
        }

        // Feed AFE and fetch result
        afe->feed(afe_data, feed16);
        afe_fetch_result_t *res = afe->fetch(afe_data);
        if (!res) {
            // be nice to the watchdog
            vTaskDelay(1);
            continue;
        }

        switch (st) {
        case ST_IDLE:
            if (res->wakeup_state == WAKENET_CHANNEL_VERIFIED) {
                ESP_LOGI(TAG, "AFE channel verified (ch %d)", res->trigger_channel_id);
            } else if (res->wakeup_state == WAKENET_DETECTED) {
                ESP_LOGI(TAG, "Wake word detected! Listening...");
                mn->clean(mn_data);          // reset MN stream
                st = ST_AWAKE;
                awake_ticks = 0;
            }
            break;

        case ST_AWAKE: {
            // Keep passing consecutive chunks to MultiNet until decision
            esp_mn_state_t s = mn->detect(mn_data, res->data);
            if (s == ESP_MN_STATE_DETECTED) {
                esp_mn_results_t *rsl = mn->get_results(mn_data);
                if (rsl && rsl->num > 0) {
                    ESP_LOGI(TAG, "Command: %s (id=%d)", rsl->string, rsl->command_id[0]);
                } else {
                    ESP_LOGI(TAG, "Command detected (no string).");
                }
                mn->clean(mn_data);
                st = ST_IDLE;
            } else if (s == ESP_MN_STATE_TIMEOUT) {
                ESP_LOGI(TAG, "Listening timeout.");
                mn->clean(mn_data);
                st = ST_IDLE;
            } else {
                if (++awake_ticks > AWAKE_MAX_TICKS) {
                    ESP_LOGI(TAG, "Listening window expired.");
                    mn->clean(mn_data);
                    st = ST_IDLE;
                }
            }
        } break;
        }

        // Give IDLE some air so WDT doesn't bark
        vTaskDelay(1);
    }

    // (Unreachable in this simple app)
    free(raw32);
    free(feed16);
    mn->destroy(mn_data);
    afe->destroy(afe_data);
}







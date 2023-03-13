#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "I2SSampler.h"
#include "I2SMEMSSampler.h"
#include "ADCSampler.h"
#include "I2SOutput.h"
#include "SDCard.h"
#include "SPIFFS.h"
#include "WAVFileWriter.h"
#include "WAVFileReader.h"
#include "config.h"
//based on https://medium.com/codex/the-complete-guide-to-recording-an-analog-microphone-with-esp32-to-an-sd-card-60440ec2d1a2
//configuration settings are in config.h
//video with explanation https://www.youtube.com/watch?v=bVru6M862HY
static const char *TAG = "app";

// sdcard
// #define PIN_NUM_MISO GPIO_NUM_19
// #define PIN_NUM_CLK GPIO_NUM_18
// #define PIN_NUM_MOSI GPIO_NUM_23
// #define PIN_NUM_CS GPIO_NUM_5


extern "C"
{
  void app_main(void);
}

void wait_for_button_push()
{
  while (gpio_get_level(GPIO_BUTTON) == 0)
  {
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void record(I2SSampler *input, const char *fname)
{
  int16_t *samples = (int16_t *)malloc(sizeof(int16_t) * 1024);
  ESP_LOGI(TAG, "Start recording");
  input->start();
  // open the file on the sdcard
  FILE *fp = fopen(fname, "wb");
  // create a new wave file writer
  WAVFileWriter *writer = new WAVFileWriter(fp, input->sample_rate());
  // keep writing until the user releases the button
  while (gpio_get_level(GPIO_BUTTON) == 1)
  {
    int samples_read = input->read(samples, 1024);
    // int64_t start = timer_gettime();
    writer->write(samples, samples_read);
    // int64_t end = timer_gettime();
    // ESP_LOGI(TAG, "Wrote %d samples in %lld microseconds", samples_read, end - start);
  }
  // stop the input
  input->stop();
  // and finish the writing
  writer->finish();
  fclose(fp);
  delete writer;
  free(samples);
  ESP_LOGI(TAG, "Finished recording");
}

void play(Output *output, const char *fname)
{
  int16_t *samples = (int16_t *)malloc(sizeof(int16_t) * 1024);
  // open the file on the sdcard
  FILE *fp = fopen(fname, "rb");
  // create a new wave file writer
  WAVFileReader *reader = new WAVFileReader(fp);
  ESP_LOGI(TAG, "Start playing");
  output->start(reader->sample_rate());
  ESP_LOGI(TAG, "Opened wav file");
  // read until theres no more samples
  while (true)
  {
    int samples_read = reader->read(samples, 1024);
    if (samples_read == 0)
    {
      break;
    }
    ESP_LOGI(TAG, "Read %d samples", samples_read);
    output->write(samples, samples_read);
    ESP_LOGI(TAG, "Played samples");
  }
  // stop the input
  output->stop();
  fclose(fp);
  delete reader;
  free(samples);
  ESP_LOGI(TAG, "Finished playing");
}

void app_main(void)
{
  ESP_LOGI(TAG, "Starting up");

#ifdef USE_SPIFFS
  ESP_LOGI(TAG, "Mounting SPIFFS on /sdcard");
  new SPIFFS("/sdcard");
#else
  ESP_LOGI(TAG, "Mounting SDCard on /sdcard");
  new SDCard("/sdcard", PIN_NUM_MISO, PIN_NUM_MOSI, PIN_NUM_CLK, PIN_NUM_CS);
#endif

  ESP_LOGI(TAG, "Creating microphone");
#ifdef USE_I2S_MIC_INPUT
  I2SSampler *input = new I2SMEMSSampler(I2S_NUM_0, i2s_mic_pins, i2s_mic_Config);
#else
  I2SSampler *input = new ADCSampler(ADC_UNIT_1, ADC1_CHANNEL_7, i2s_adc_config);
#endif
  I2SOutput *output = new I2SOutput(I2S_NUM_0, i2s_speaker_pins);

  gpio_set_direction(GPIO_BUTTON, GPIO_MODE_INPUT);
  gpio_set_pull_mode(GPIO_BUTTON, GPIO_PULLDOWN_ONLY);

  while (true)
  {
    // wait for the user to push and hold the button
    wait_for_button_push();
    record(input, "/sdcard/test.wav");
    // wait for the user to push the button again
    wait_for_button_push();
    play(output, "/sdcard/test.wav");
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

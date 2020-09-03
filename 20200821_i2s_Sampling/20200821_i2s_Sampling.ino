
#include <Arduino.h>            //库的引用有先后顺序
#include "I2SSampler.h"
#include "driver/i2s.h"
#include "driver/adc.h"
#include "I2SSampler.h"
#include "arduinoFFT.h"
arduinoFFT FFT = arduinoFFT();    //建立FFT 实例
I2SSampler *sampler = NULL;      //声明指针sampler是I2SSampler 的class
const uint16_t FFT_samples = 16;  //采样点的数量
//I2SSampler sampler;
#define LED_BUILTIN 22              //开发板内置LED管脚，writerTask中用于显示每次接收到数据

double vReal[FFT_samples];          //FFT计算时的采样值和输出值
double vImag[FFT_samples];          //FFT计算时的采样值和输出值
void setup()
{
  Serial.begin(115200);
  pinMode(22, OUTPUT);
  Serial.println(int(adc1_get_raw(adc1_channel_t(ADC1_CHANNEL_6))));    //问题：直接读ADC为正，为什么I2S采样的数是负数？
  // create our sampler
  sampler = new I2SSampler();                         //在Class 中的 function（）,初始化实例sampler
  // set up the sample writer task
  TaskHandle_t writerTaskHandle;                      //该参数用于下一行执行新建Task 时，给该参数赋值，即改Task的参考号
  xTaskCreatePinnedToCore(writerTask, "Writer Task", 8192, sampler, 1, &writerTaskHandle, 1);
               //对应上一行： Task的指针，  Task 名， Task堆栈数(byte)，传递的参数的指针， 优先级， Task参考号，Task运行的Core号
               //堆栈8192对应原采样点数 ADC_SAMPLES_COUNT 32768，这两者之间关系没理解
  // start sampling
  sampler->start(writerTaskHandle);                 //执行 S2SSamping 库中的函数（直接copy到下面了）
}

void loop()
{
  // nothing to do here - everything is taken care of by tasks
}

//处理收到 samples 的Task。 Task to write samples to our server
void writerTask(void *param)
{
  sampler = (I2SSampler *)param;                         //把建立writerTask命令“xTaskCreatePinnedToCore(writerTask,”中传递的参数Sample赋值到本函数中
  const TickType_t xMaxBlockTime = pdMS_TO_TICKS(100);   //设置"ulTaskNotifyTake"函数最长的等待时间为100ms
   
  
  while (true)
  {
    // wait for some samples to save
    uint32_t ulNotificationValue = ulTaskNotifyTake(pdTRUE, xMaxBlockTime);   //检查是否“ulTaskNotifyTake”有return？
    if (ulNotificationValue > 0)
    {
       for(int i =0; i < FFT_samples; i++){                       // 如果有 从中挑选FFT_samples个，赋值到vReal,准备计算   
       vReal[i] = int8_t(sampler->capturedAudioBuffer[i+500]); 
        //Serial.println(vReal[i]);
        vImag[i] = 0;
        Serial.println(vReal[i]);
        }
        Serial.println("==================================");
        FFT.Compute(vReal, vImag, FFT_samples, FFT_FORWARD);      //FFT计算
//         for(int i =0; i < FFT_samples; i++){
//        Serial.println(vReal[i]);
//         }
           Serial.println("==================================");
        FFT.ComplexToMagnitude(vReal, vImag, FFT_samples);        //FFT计算模量
//        for(int i =0; i < FFT_samples; i++){
//        Serial.println(vReal[i]);
//         }
//      原例程在此吧数据用HTTP方式传输到Server
//      httpClient->begin(*wifiClient, SERVER_URL);
//      httpClient->addHeader("content-type", "application/octet-stream");
//      httpClient->POST((uint8_t *)sampler->sampleBuffer(), sampler->numSamples() * sizeof(uint16_t));
//      httpClient->end();
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }
  }
}

// 一下为库 I2SSampler.cpp 部分
void readerTask(void *param)                       //读取数值Task程序
{
    I2SSampler *sampler = (I2SSampler *)param;     //获取传递来的实例
    while (true)
    {
        // wait for some data to arrive on the queue
        i2s_event_t evt;                                                        //定于 evt 为i2s_event_t
        if (xQueueReceive(sampler->i2s_queue, &evt, portMAX_DELAY) == pdPASS)   //如果
                            //xQueue, pvBuffer, xTicksToWait
        {
            if (evt.type == I2S_EVENT_RX_DONE)
            {
                size_t bytesRead = 0;
                do
                {
                    // try and fill up our audio buffer
                    size_t bytesToRead = (ADC_SAMPLES_COUNT - sampler->audioBufferPos) * 2;
                    // read from i2s
                    i2s_read(I2S_NUM_0, (void *)(sampler->currentAudioBuffer + sampler->audioBufferPos), bytesToRead, &bytesRead, 10 / portTICK_PERIOD_MS);
                    sampler->audioBufferPos += bytesRead / 2;
                    if (sampler->audioBufferPos == ADC_SAMPLES_COUNT)
                    {
                        // process the samples  //没理解这里在做什么
//                        for (int i = 0; i < ADC_SAMPLES_COUNT; i++)
//                        {
//                            sampler->currentAudioBuffer[i] = (2048 - (sampler->currentAudioBuffer[i] & 0xfff)) * 15;  
//                        }
                        // swap to the other buffer
                        std::swap(sampler->currentAudioBuffer, sampler->capturedAudioBuffer);
                        // reset the sample position
                        sampler->audioBufferPos = 0;
                        // tell the writer task to save the data
                        xTaskNotify(sampler->writerTaskHandle, 1, eIncrement);
                    }
                } while (bytesRead > 0);
            }
        }
    }
}

#define SAMPLE_BUFFER_SIZE (256)

int16_t sampleBuffer[SAMPLE_BUFFER_SIZE];


I2SSampler::I2SSampler()
{
    audioBuffer1 = (int16_t *)malloc(sizeof(int16_t) * ADC_SAMPLES_COUNT);
    audioBuffer2 = (int16_t *)malloc(sizeof(int16_t) * ADC_SAMPLES_COUNT);

    currentAudioBuffer = audioBuffer1;
    capturedAudioBuffer = audioBuffer2;

    audioBufferPos = 0;
}

I2SSampler::~I2SSampler()
{
    free(audioBuffer1);
    free(audioBuffer2);
}

void I2SSampler::start(TaskHandle_t writerTaskHandle)
{
    this->writerTaskHandle = writerTaskHandle;

    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
        .sample_rate = 10000,                                                         //采样频率和采样数量ADC_SAMPLES_COUNT 共同决定了每一个Queue的周期
        // for median filter use .sample_rate = 160000
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,///??LSB
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 1024,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0};
    //install and start i2s driver
    i2s_driver_install(I2S_NUM_0, &i2s_config, 4, &i2s_queue);
    //init ADC pad
    i2s_set_adc_mode(ADC_UNIT_1, ADC1_CHANNEL_6);
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_6);
    // enable the adc
    i2s_adc_enable(I2S_NUM_0);

    // start a task to read samples from I2S
    TaskHandle_t readerTaskHandle;
    // Median filter xTaskCreatePinnedToCore(readerTaskMedianAverage, "Reader Task", 8192, this, 1, &readerTaskHandle, 0);
    xTaskCreatePinnedToCore(readerTask, "Reader Task", 8192, this, 1, &readerTaskHandle, 0);
}

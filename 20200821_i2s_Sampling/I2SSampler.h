#ifndef __i2s_sampler_h__
#define __i2s_sampler_h__

#define ADC_SAMPLES_COUNT 8192//32768 // approx ~0.8 seconds at 40KHz, ADC的采样计数？

class I2SSampler
{
public:
    // double buffer so we can be capturing samples while sending data
    int16_t *audioBuffer1;         //两个Bufer，一个采样，一个发送？
    int16_t *audioBuffer2;
    // current position in the audio buffer
    int32_t audioBufferPos = 0;             //当前缓存位置？
    // current audio buffer
    int16_t *currentAudioBuffer;            //当前音频缓存
    // buffer containing samples that have been captured already
    int16_t *capturedAudioBuffer;           //获得的音频缓存
    // writer task
    TaskHandle_t writerTaskHandle;          //写数据Task 的Task号
    // reader queue
    QueueHandle_t i2s_queue;                //Queue号

public:
    I2SSampler();                           //建立Class实例
    ~I2SSampler();                          //注销Class实例
    virtual int16_t *sampleBuffer()
    {
        return capturedAudioBuffer;
    }
    int numSamples()
    {
        return ADC_SAMPLES_COUNT;
    }
    void start(TaskHandle_t writerTaskHandle);
};

#endif

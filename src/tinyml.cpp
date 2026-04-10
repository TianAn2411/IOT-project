#include "tinyml.h"
#include "hcm_weather_binary_model.h"
#include "app_rtc.h"
#include <math.h>

// Globals, for the convenience of one-shot setup.
namespace
{
    tflite::ErrorReporter *error_reporter = nullptr;
    const tflite::Model *model = nullptr;
    tflite::MicroInterpreter *interpreter = nullptr;
    TfLiteTensor *input = nullptr;
    TfLiteTensor *output = nullptr;
    constexpr int kTensorArenaSize = 8 * 1024; // Adjust size based on your model
    uint8_t tensor_arena[kTensorArenaSize];
} // namespace

void setupTinyML()
{
    Serial.println("TensorFlow Lite Init....");
    static tflite::MicroErrorReporter micro_error_reporter;
    error_reporter = &micro_error_reporter;

    model = tflite::GetModel(hcm_weather_binary_model_tflite);
    if (model->version() != TFLITE_SCHEMA_VERSION)
    {
        error_reporter->Report("Model provided is schema version %d, not equal to supported version %d.",
                               model->version(), TFLITE_SCHEMA_VERSION);
        return;
    }

    static tflite::AllOpsResolver resolver;
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, kTensorArenaSize, error_reporter);
    interpreter = &static_interpreter;

    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk)
    {
        error_reporter->Report("AllocateTensors() failed");
        return;
    }

    input = interpreter->input(0);
    output = interpreter->output(0);

    Serial.println("TensorFlow Lite Micro initialized on ESP32.");
}

void tiny_ml_task(void *pvParameters)
{
    GlobalContext *ctx = (GlobalContext*)pvParameters;
    setupTinyML();

    while (1)
    {

        float t = -1;
        float h = -1;
        if (ctx) {
            if (xSemaphoreTake(ctx->dataMutex, portMAX_DELAY) == pdTRUE) {
                t = ctx->temperature;
                h = ctx->humidity;
                xSemaphoreGive(ctx->dataMutex);
            }
        }

        float hour_float = app_rtc_get_hour_float();
        float hour_sin = sin(2.0 * PI * hour_float / 24.0);
        float hour_cos = cos(2.0 * PI * hour_float / 24.0);

        // Prepare input data (4 inputs: temp, humi, hour_sin, hour_cos)
        input->data.f[0] = t;
        input->data.f[1] = h;
        input->data.f[2] = hour_sin;
        input->data.f[3] = hour_cos;

        // Run inference
        TfLiteStatus invoke_status = interpreter->Invoke();
        if (invoke_status != kTfLiteOk)
        {
            error_reporter->Report("Invoke failed");
            return;
        }

        // Get and process output (assuming [0]=Clear Sky, [1]=Cloudy or vice versa. Usually index 0 represents Class 0 label).
        float p0 = output->data.f[0];
        float p1 = output->data.f[1];

        String weather = (p0 > p1) ? "Clear Sky" : "Cloudy";
        
        if (ctx) {
            if (xSemaphoreTake(ctx->dataMutex, portMAX_DELAY) == pdTRUE) {
                ctx->predictedWeather = weather;
                xSemaphoreGive(ctx->dataMutex);
            }
        }

        Serial.print("Inference result: ");
        Serial.print(weather);
        Serial.print(" (p0=");
        Serial.print(p0);
        Serial.print(", p1=");
        Serial.print(p1);
        Serial.println(")");

        vTaskDelay(5000);
    }
}
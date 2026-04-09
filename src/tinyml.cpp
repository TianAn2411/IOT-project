#include "tinyml.h"

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

    model = tflite::GetModel(weather_model_tflite);
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

        // Prepare input data (e.g., sensor readings)
        // For a simple example, let's assume a single float input
        input->data.f[0] = t;
        input->data.f[1] = h;

        // Run inference
        TfLiteStatus invoke_status = interpreter->Invoke();
        if (invoke_status != kTfLiteOk)
        {
            error_reporter->Report("Invoke failed");
            return;
        }

        // Get and process output
        // Assuming output[0] is max, output[1] is min based on user prompt 'max - min' typical order, or just assigning them directly.
        float pMax = output->data.f[0];
        float pMin = output->data.f[1];
        
        if (ctx) {
            if (xSemaphoreTake(ctx->dataMutex, portMAX_DELAY) == pdTRUE) {
                ctx->predictedMaxTemp = pMax;
                ctx->predictedMinTemp = pMin;
                xSemaphoreGive(ctx->dataMutex);
            }
        }

        Serial.print("Inference result: Max=");
        Serial.print(pMax);
        Serial.print(", Min=");
        Serial.println(pMin);

        vTaskDelay(5000);
    }
}
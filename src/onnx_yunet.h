/* 
   YuNet ONNX model inference (face + landmarks) using ONNX Runtime C API
   C99 compatible, single header + implementation
*/

#ifndef YUNET_ONNX_H
#define YUNET_ONNX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ONNX Runtime C API header */
#include "onnxruntime_c_api.h"

/* ---- Configuration: adapt as necessary ---- */

/* Input dimensions: the model is exported with fixed input size */
#define YUNET_INPUT_WIDTH 320 /* verify via Netron or model metadata */
#define YUNET_INPUT_HEIGHT 320
#define YUNET_INPUT_CHANNELS 3

/* Node names in the ONNX graph; verify via Netron */
#define YUNET_INPUT_NAME "input.1" /* e.g. often "input.1", but confirm */
#define YUNET_OUTPUT_NAME "output.1" /* this is the “dets”‑style output: top_k × 15 */

/* Thresholds for detection */
#define YUNET_SCORE_THRESHOLD 0.7f
#define YUNET_MAX_DETECTIONS 100

/* Data structures */

typedef struct {
    float x; /* top‑left x */
    float y; /* top‑left y */
    float w; /* width */
    float h; /* height */
    float landmarks[5][2]; /* five (x,y) landmarks */
    float score; /* confidence score */
} YunetDetection;

typedef struct {
    YunetDetection *detections;
    size_t count;
} YunetDetections;

/* API */

/**
 * Create an ONNX Runtime session for YuNet model.
 * Returns NULL on error.
 */
OrtSession* yunet_create_session(const OrtApi *api,
                                  OrtEnv *env,
                                  const char *model_path,
                                  OrtSessionOptions *session_options);

/**
 * Run inference on single image.
 * image_data must be in HWC format, RGB, width = YUNET_INPUT_WIDTH, height = YUNET_INPUT_HEIGHT,
 * normalized to [0,1] (unless you adapt).
 * detections is output; it will allocate memory internally. Caller must free via yunet_free_detections.
 * Returns 0 on success, nonzero on error.
 */
int yunet_run(const OrtApi *api,
              OrtSession *session,
              float *image_data,
              YunetDetections *detections);

/**
 * Free detections allocated by yunet_run.
 */
void yunet_free_detections(YunetDetections *detections);

/* Implementation */
#ifdef YUNET_ONNX_IMPLEMENTATION

static float _iou(const YunetDetection *a, const YunetDetection *b) {
    float x1 = fmaxf(a->x, b->x);
    float y1 = fmaxf(a->y, b->y);
    float x2 = fminf(a->x + a->w, b->x + b->w);
    float y2 = fminf(a->y + a->h, b->y + b->h);
    float w = x2 - x1;
    float h = y2 - y1;
    if (w <= 0.0f || h <= 0.0f) return 0.0f;
    float inter = w * h;
    float areaA = a->w * a->h;
    float areaB = b->w * b->h;
    return inter / (areaA + areaB - inter);
}

static void _nms(YunetDetection *dets, size_t *nd, float iou_threshold) {
    // simple greedy NMS
    size_t out = 0;
    for (size_t i = 0; i < *nd; i++) {
        YunetDetection *di = &dets[i];
        int keep = 1;
        for (size_t j = 0; j < out; j++) {
            if (_iou(di, &dets[j]) > iou_threshold) {
                keep = 0;
                break;
            }
        }
        if (keep) {
            dets[out++] = dets[i];
        }
    }
    *nd = out;
}

OrtSession* yunet_create_session(const OrtApi *api,
                                  OrtEnv *env,
                                  const char *model_path,
                                  OrtSessionOptions *session_options)
{
    if (!api || !env || !model_path) return NULL;
    OrtSession *session = NULL;
    OrtStatus *status = api->CreateSession(env, model_path, session_options, &session);
    if (status) {
        const char *msg = api->GetErrorMessage(status);
        fprintf(stderr, "Failed to create YuNet session: %s\n", msg);
        api->ReleaseStatus(status);
        return NULL;
    }
    return session;
}

int yunet_run(const OrtApi *api,
              OrtSession *session,
              float *image_data,
              YunetDetections *detections)
{
    if (!api || !session || !image_data || !detections) return -1;
    
    /* Prepare input tensor */
    OrtMemoryInfo *mem_info = NULL;
    api->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &mem_info);
    
    int64_t input_shape[4] = {1, YUNET_INPUT_CHANNELS, YUNET_INPUT_HEIGHT, YUNET_INPUT_WIDTH};
    size_t input_tensor_size = (size_t)YUNET_INPUT_CHANNELS * YUNET_INPUT_HEIGHT * YUNET_INPUT_WIDTH;
    
    OrtValue *input_tensor = NULL;
    api->CreateTensorWithDataAsOrtValue(mem_info,
                                        image_data,
                                        input_tensor_size * sizeof(float),
                                        input_shape,
                                        4,
                                        ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
                                        &input_tensor);
    if (!input_tensor) {
        api->ReleaseMemoryInfo(mem_info);
        fprintf(stderr, "Failed to allocate input tensor\n");
        return -1;
    }
    
    int is_tensor = 0;
    api->IsTensor(input_tensor, &is_tensor);
    if (!is_tensor) {
        fprintf(stderr, "Input is not a tensor\n");
        api->ReleaseMemoryInfo(mem_info);
        api->ReleaseValue(input_tensor);
        return -1;
    }
    
    /* Run inference */
    const char *input_names[] = { YUNET_INPUT_NAME };
    const char *output_names[] = { YUNET_OUTPUT_NAME };
    OrtValue *outputs[1] = { NULL };
    
    OrtStatus *status = api->Run(session,
                                 NULL,
                                 input_names,
                                 &input_tensor,
                                 1,
                                 output_names,
                                 outputs,
                                 1);
    if (status) {
        const char *msg = api->GetErrorMessage(status);
        fprintf(stderr, "YuNet Run error: %s\n", msg);
        api->ReleaseStatus(status);
        api->ReleaseMemoryInfo(mem_info);
        api->ReleaseValue(input_tensor);
        return -1;
    }
    
    /* Extract output */
    float *out_data = NULL;
    OrtTensorTypeAndShapeInfo *info = NULL;
    status = api->GetTensorTypeAndShape(outputs[0], &info);
    if (status) {
        fprintf(stderr, "GetTensorTypeAndShape failed\n");
        api->ReleaseStatus(status);
        // cleanup
        api->ReleaseMemoryInfo(mem_info);
        api->ReleaseValue(input_tensor);
        api->ReleaseValue(outputs[0]);
        return -1;
    }
    
    /* Determine number of detections = shape[1] (top_k) */
    size_t num_det = 0;
    {
        int64_t shape[2];
        size_t dim_count = api->GetDimensionsCount(info);
        if (dim_count != 2) {
            // possibly shape is [1, top_k, 15] or similar; adjust accordingly
            // For simplicity, handle dim_count == 3
            if (dim_count == 3) {
                int64_t shape3[3];
                api->GetDimensions(info, shape3, 3);
                // assume shape3 = {1, top_k, 15}
                num_det = (size_t)shape3[1];
            } else {
                // fallback
                num_det = YUNET_MAX_DETECTIONS;
            }
        } else {
            api->GetDimensions(info, shape, 2);
            // maybe shape = {top_k, 15}
            num_det = (size_t)shape[0];
        }
    }
    
    api->GetTensorMutableData(outputs[0], (void**)&out_data);
    
    /* Allocate result array */
    YunetDetection *res = (YunetDetection*)malloc(sizeof(YunetDetection) * num_det);
    size_t res_count = 0;
    
    /* Process each detection */
    for (size_t i = 0; i < num_det; i++) {
        /* the layout: each row has 15 floats:
           0‑3: x, y, w, h
           4‑13: 5 landmarks (x,y) → indices 4..13
           14: score
        */
        float score = out_data[i * 15 + 14];
        if (score < YUNET_SCORE_THRESHOLD) continue;
        
        YunetDetection det;
        det.x = out_data[i * 15 + 0];
        det.y = out_data[i * 15 + 1];
        det.w = out_data[i * 15 + 2];
        det.h = out_data[i * 15 + 3];
        
        for (int lm = 0; lm < 5; lm++) {
            det.landmarks[lm][0] = out_data[i * 15 + 4 + lm*2 + 0];
            det.landmarks[lm][1] = out_data[i * 15 + 4 + lm*2 + 1];
        }
        
        det.score = score;
        
        res[res_count++] = det;
        if (res_count >= YUNET_MAX_DETECTIONS) break;
    }
    
    /* Optional: apply NMS on bounding boxes */
    _nms(res, &res_count, 0.3f); /* you may want this threshold configurable */
    
    /* Fill output */
    detections->detections = res;
    detections->count = res_count;
    
    /* Cleanup */
    api->ReleaseTensorTypeAndShapeInfo(info);
    api->ReleaseMemoryInfo(mem_info);
    api->ReleaseValue(input_tensor);
    api->ReleaseValue(outputs[0]);
    
    return 0;
}

void yunet_free_detections(YunetDetections *detections) {
    if (!detections) return;
    if (detections->detections) {
        free(detections->detections);
        detections->detections = NULL;
    }
    detections->count = 0;
}

#endif /* YUNET_ONNX_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif

#endif /* YUNET_ONNX_H */

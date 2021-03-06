#include "camera.h"

#include "math.h"
#include "engine.h"

void egFPSCameraInit(EgFPSCamera *camera, EgEngine *engine)
{
    *camera = (EgFPSCamera){0};
    camera->engine = engine;
    camera->pos = V3(0.0f, 0.0f, 0.0f);
    camera->yaw = EG_RADIANS(180.0f);
    camera->pitch = 0.0f;
    camera->fovy = EG_RADIANS(75.0f);
    camera->prev_x = 0;
    camera->prev_y = 0;
    camera->sensitivity = 0.14f;
    camera->speed = 1.0f;
}

EgCameraUniform egFPSCameraUpdate(EgFPSCamera *camera, float delta_time)
{
    if (!egEngineGetCursorEnabled(camera->engine))
    {
        double cx, cy;
        egEngineGetCursorPos(camera->engine, &cx, &cy);

        float dx = (float)(cx - camera->prev_x);
        float dy = (float)(cy - camera->prev_y);

        camera->prev_x = cx;
        camera->prev_y = cy;

        camera->yaw -= EG_RADIANS(dx * camera->sensitivity);
        camera->pitch -= EG_RADIANS(dy * camera->sensitivity);
        camera->pitch = EG_CLAMP(camera->pitch, EG_RADIANS(-89.0f), EG_RADIANS(89.0f));
    }

    float3 front = egFloat3Normalize(
        V3(sinf(camera->yaw) * cosf(camera->pitch),
           sinf(camera->pitch),
           cosf(camera->yaw) * cosf(camera->pitch)));

    float3 right = egFloat3Cross(front, V3(0.0f, 1.0f, 0.0f));
    float3 up = egFloat3Cross(right, front);

    float delta = camera->speed * delta_time;

    float3 forward_increment = egFloat3MulScalar(front, delta);
    float3 right_increment = egFloat3MulScalar(right, delta);

    if (egEngineGetKeyState(camera->engine, EG_KEY_W))
    {
        camera->pos = egFloat3Add(camera->pos, forward_increment);
    }
    if (egEngineGetKeyState(camera->engine, EG_KEY_S))
    {
        camera->pos = egFloat3Sub(camera->pos, forward_increment);
    }
    if (egEngineGetKeyState(camera->engine, EG_KEY_A))
    {
        camera->pos = egFloat3Sub(camera->pos, right_increment);
    }
    if (egEngineGetKeyState(camera->engine, EG_KEY_D))
    {
        camera->pos = egFloat3Add(camera->pos, right_increment);
    }

    uint32_t width, height;
    egEngineGetWindowSize(camera->engine, &width, &height);

    float4x4 correction_matrix = egFloat4x4Diagonal(1.0f);
    correction_matrix.yy = -1.0f;

    float aspect_ratio = (float)width / (float)height;
    float4x4 proj = egFloat4x4PerspectiveReserveZ(camera->fovy, aspect_ratio, 0.1f);
    proj = egFloat4x4Mul(&correction_matrix, &proj);
    float4x4 view =
        egFloat4x4LookAt(camera->pos, egFloat3Add(camera->pos, front), up);

    EgCameraUniform uniform = {};
    uniform.pos = (float4){
        camera->pos.x,
        camera->pos.y,
        camera->pos.z,
        1.0f,
    };
    uniform.view = view;
    uniform.proj = proj;

    return uniform;
}

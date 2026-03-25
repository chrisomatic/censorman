#pragma once

// constants
#define PI         3.141592653589793
#define PI_OVER_2  1.570796326794896
#define PI_OVER_4  0.785398163397448
#define TAU        2.0*PI
#define SQRT2      1.414213562
#define SQRT2OVER2 0.707106781187
#define EPSILON    0.001

// helpful functions
#define ABS(x)   ((x) < 0 ? -(x) : (x))
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define CLAMP(x, lo, hi) MAX(MIN((x), (hi)),(lo))
#define BETWEEN(x,min,max) ((x) >= (min) && (x) <= (max))
#define SQUARE(x)    ((x)*(x))
#define RAD(x) (((x) * PI) / 180.0f)
#define DEG(x) (((x) * 180.0f) / PI)
#define IS_ZEROISH(n) (ABS((n)) < EPSILON)

#define VEC2(x,y)     (Vec2){(x),(y)}
#define VEC3(x,y,z)   (Vec3){(x),(y),(z)}
#define VEC4(x,y,z,w) (Vec4){(x),(y),(z),(w)}

#define VEC2_FMT "[ %7.4f %7.4f ]"
#define VEC3_FMT "[ %7.4f %7.4f %7.4f ]"

#define VEC2_ARG(v) v.x, v.y
#define VEC3_ARG(v) v.x, v.y, v.z

typedef struct
{
    u32 x,y,w,h;
} Rectu32;

typedef struct
{
    f32 x,y,w,h;
} Rectf32;

typedef struct
{
    f32 x,y;
} Vec2;

typedef struct
{
    u32 x,y;
} Vec2u;

typedef struct
{
    s32 x,y;
} iVec2;

typedef struct
{
    f32 x,y,z;
} Vec3;

typedef struct
{
    f32 x,y,z,w;
} Vec4;

typedef struct 
{
    f32 data[4][4];
} Matrix44;

typedef Vec2 Vector2;
typedef Vec3 Vector3;
typedef Vec4 Vector4;
typedef Vec4 Quaternion;

// defines for common use
typedef Matrix44 Matrix;
typedef Vec3     Vector;

typedef Rectu32 Rect;

f32 Q_rsqrt(f32 number);

/////////////////////////////////
// Vectors
/////////////////////////////////

/////////////////////////////////
// 2D

f32 vec2_distance(Vec2 a, Vec2 b);
f32 vec2_distance_squared(Vec2 a, Vec2 b);
f32 vec2_length(Vec2 v);
f32 vec2_dot(Vec2 a, Vec2 b);

Vec2 vec2_add(Vec2 a, Vec2 b);
Vec2 vec2_subtract(Vec2 a, Vec2 b);
Vec2 vec2_scale(Vec2 a, f32 f);
Vec2 vec2_negate(Vec2 a);
Vec2 vec2_normalize(Vec2 v);
Vec2 vec2_zero(void);

/////////////////////////////////
// 3D

f32 vec3_distance(Vec3 a, Vec3 b);
f32 vec3_distance_squared(Vec3 a, Vec3 b);
f32 vec3_dot(Vec3 a, Vec3 b);
f32 vec3_length(Vec3 v);

Vec3 vec3_cross(Vec3 a, Vec3 b);
Vec3 vec3_rotate(Vec3 v, const Vec3 axis, f32 angle_rad);
Vec3 vec3_add(Vec3 a, Vec3 b);
Vec3 vec3_subtract(Vec3 a, Vec3 b);
Vec3 vec3_scale(Vec3 a, f32 f);
Vec3 vec3_negate(Vec3 a);
Vec3 vec3_normalize(Vec3 v);
Vec3 vec3_normalize_fast(Vec3 v);
Vec3 vec3_normal(Vec3 a, Vec3 b, Vec3 c);
Vec3 vec3_zero(void);

b32 vec3_is_zero(Vec3 v);
b32 vec3_is_zeroish(Vec3 v);

void vec2_print(Vec2 v, const char *name);
void vec3_print(Vec3 v, const char *name);
void vec4_print(Vec4 v, const char *name);

/////////////////////////////////
// Quaternions
/////////////////////////////////

f32        quat_length(Quaternion q);
Quaternion quat_normalize(Quaternion q);
Quaternion quat_conjugate(Quaternion q);
Quaternion quat_multiply_vec3(Quaternion q, Vec3 v);
Quaternion quat_multiply(Quaternion a, Quaternion b);

/////////////////////////////////
// Matrices
/////////////////////////////////

Matrix matrix_identity(void);
Matrix matrix_dot_matrix(Matrix a, Matrix b);
Matrix matrix_translate(f32 x, f32 y, f32 z);
Matrix matrix_rotation(f32 x, f32 y, f32 z);
Matrix matrix_scale(f32 x, f32 y, f32 z);
Matrix matrix_perspective(f32 view_width, f32 view_height, f32 z_near, f32 z_far, f32 fov);

void matrix_print(Matrix *m, const char *name);

/////////////////////////////////
// Interpolation
/////////////////////////////////

f32 interp_exp_decay(f32 a, f32 b, f32 decay, f32 dt);
f32 interp_lerp(f32 a, f32 b, f32 t);
f32 interp_exp_smooth(f32 start, f32 end, f32 alpha, int frame);

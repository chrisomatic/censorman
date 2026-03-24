
f32 Q_rsqrt(f32 number)
{
	union {
		f32 f;
		u32 i;
	} conv = { .f = number };
	conv.i  = 0x5f3759df - (conv.i >> 1);
	conv.f *= 1.5F - (number * 0.5F * conv.f * conv.f);

	return conv.f;
}

///////////////////////////////////////////
// Vectors
///////////////////////////////////////////

///////////////////////////////////////////
// 2D

f32 vec2_distance(Vec2 a, Vec2 b)
{
    return sqrt(vec2_distance_squared(a,b));
}

f32 vec2_distance_squared(Vec2 a, Vec2 b)
{
    Vec2 s = vec2_subtract(b, a);
    f32 l = vec2_length(s);
    return l;
}

f32 vec2_length(Vec2 v)
{
    return sqrt(v.x*v.x + v.y*v.y);
}

f32 vec2_dot(Vec2 a, Vec2 b)
{
    return (a.x * b.x) + (a.y * b.y);
}

Vec2 vec2_add(Vec2 a, Vec2 b)
{
    return (Vec2){a.x + b.x, a.y + b.y};
}

Vec2 vec2_subtract(Vec2 a, Vec2 b)
{
    return (Vec2){a.x - b.x, a.y - b.y};
}

Vec2 vec2_scale(Vec2 a, f32 f)
{
    return (Vec2){f*a.x, f*a.y};
}

Vec2 vec2_negate(Vec2 a)
{
    return vec2_scale(a, -1.0);
}

Vec2 vec2_normalize(Vec2 v)
{
    f32 magn_squared = v.x*v.x + v.y*v.y;
    f32 r = 1.0/sqrt(magn_squared);

    Vec2 ret = vec2_scale(v, r);
    return ret;
}

Vec2 vec2_zero()
{
    Vec2 v = {0};
    return v;
}

///////////////////////////////////////////
// 3D

f32 vec3_length(Vec3 v)
{
    return sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
}

f32 vec3_dot(Vec3 a, Vec3 b)
{
    return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

Vec3 vec3_cross(Vec3 a, Vec3 b)
{
    Vec3 res = {
        (a.y * b.z) - (a.z * b.y),
        (a.z * b.x) - (a.x * b.z),
        (a.x * b.y) - (a.y * b.x)
    };

    return res;
}

Vec3 vec3_rotate(Vec3 v, const Vec3 axis, f32 angle_rad)
{
    const f32 sin_half_angle = sinf(angle_rad/2.0);
    const f32 cos_half_angle = cosf(angle_rad/2.0);

    const f32 rx = axis.x * sin_half_angle;
    const f32 ry = axis.y * sin_half_angle;
    const f32 rz = axis.z * sin_half_angle;
    const f32 rw = cos_half_angle;

    Quaternion rotation = {rx, ry, rz, rw};

    // W = R*V*R'
    Quaternion conj = quat_conjugate(rotation);
    Quaternion w = quat_multiply_vec3(rotation, v);
    w = quat_multiply(w,conj);

    v.x = w.x;
    v.y = w.y;
    v.z = w.z;

    return v;
}

Vec3 vec3_add(Vec3 a, Vec3 b)
{
    return (Vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 vec3_subtract(Vec3 a, Vec3 b)
{
    return (Vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 vec3_scale(Vec3 a, f32 f)
{
    return (Vec3){f*a.x, f*a.y, f*a.z};
}

Vec3 vec3_negate(Vec3 a)
{
    return vec3_scale(a, -1.0);
}

Vec3 vec3_normalize(Vec3 v)
{
    f32 magn_squared = v.x*v.x + v.y*v.y + v.z*v.z;
    f32 r = 1.0/sqrt(magn_squared);

    Vec3 ret = vec3_scale(v, r);
    return ret;
}

Vec3 vec3_normalize_fast(Vec3 v)
{
    f32 magn_squared = v.x*v.x + v.y*v.y + v.z*v.z;
    f32 r = Q_rsqrt(magn_squared);

    Vec3 ret = vec3_scale(v, r);
    return ret;
}

Vec3 vec3_normal(Vec3 a, Vec3 b, Vec3 c)
{
    Vec3 u = vec3_subtract(b,a);
    Vec3 v = vec3_subtract(c,a);

    Vec3 norm = vec3_cross(u, v);
    norm = vec3_normalize(norm);

    return norm;
}

Vec3 vec3_zero()
{
    Vec3 v = {0};
    return v;
}

b32 vec3_is_zero(Vec3 v)
{
    return (v.x == 0 && v.y == 0 && v.z == 0);
}

b32 vec3_is_zeroish(Vec3 v)
{
    b32 zx = IS_ZEROISH(v.x);
    b32 zy = IS_ZEROISH(v.y);
    b32 zz = IS_ZEROISH(v.z);

    return (zx && zy && zz);
}

f32 vec3_distance(Vec3 a, Vec3 b)
{
    return sqrt(vec3_distance_squared(a,b));
}

f32 vec3_distance_squared(Vec3 a, Vec3 b)
{
    Vec3 s = vec3_subtract(b, a);
    f32 l = vec3_length(s);
    return l;
}

void vec2_print(Vec2 v, const char *name)
{
    logi("%s [ %7.4f %7.4f ]", name, v.x, v.y);
}

void vec3_print(Vec3 v, const char *name)
{
    logi("%s [ %7.4f %7.4f %7.4f ]", name, v.x, v.y, v.z);
}

void vec4_print(Vec4 v, const char *name)
{
    logi("%s [ %7.4f %7.4f %7.4f %7.4f ]", name, v.x, v.y, v.z, v.w);
}

///////////////////////////////////////////
// Quaternions
///////////////////////////////////////////

f32 quat_length(Quaternion q)
{
    return sqrt(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
}

Quaternion quat_normalize(Quaternion q)
{
    f32 l = quat_length(q);
    
    q.x /= l;
    q.y /= l;
    q.z /= l;
    q.w /= l;

    return q;
}

Quaternion quat_conjugate(Quaternion q)
{
    return (Quaternion){ -q.x, -q.y, -q.z, q.w };
}

Quaternion quat_multiply_vec3(Quaternion q, Vec3 v)
{
    Quaternion ret = {
         (q.w * v.x) + (q.y * v.z) - (q.z * v.y),
         (q.w * v.y) + (q.z * v.x) - (q.x * v.z),
         (q.w * v.z) + (q.x * v.y) - (q.y * v.x),
        -(q.x * v.x) - (q.y * v.y) - (q.z * v.z)
    };

    return ret;
}

Quaternion quat_multiply(Quaternion a, Quaternion b)
{
    Quaternion ret = {
        (a.x * b.w) + (a.w * b.x) + (a.y * b.z) - (a.z * b.y),
        (a.y * b.w) + (a.w * b.y) + (a.z * b.x) - (a.x * b.z),
        (a.z * b.w) + (a.w * b.z) + (a.x * b.y) - (a.y * b.x),
        (a.w * b.w) - (a.x * b.x) - (a.y * b.y) - (a.z * b.z)
    };

    return ret;
}

///////////////////////////////////////////
// Matrices
///////////////////////////////////////////

Matrix matrix_identity()
{
    return (Matrix){
        .data = {
            {1.0,0.0,0.0,0.0},
            {0.0,1.0,0.0,0.0},
            {0.0,0.0,1.0,0.0},
            {0.0,0.0,0.0,1.0}
        }
    };
}

Matrix matrix_dot_matrix(Matrix a, Matrix b)
{
    Matrix r = {0};

    for(s64 i = 0; i < 4; ++i)
    {
        for(s64 j = 0; j < 4; ++j)
        {
            r.data[i][j] =
                a.data[i][0] * b.data[0][j] +
                a.data[i][1] * b.data[1][j] +
                a.data[i][2] * b.data[2][j] +
                a.data[i][3] * b.data[3][j];
        }
    }

    return r;
}

Matrix matrix_translate(f32 x, f32 y, f32 z)
{
    Matrix m = {0};
    
    m.data[0][0] = 1.0;
    m.data[3][0] = x;
    m.data[1][1] = 1.0;
    m.data[3][1] = y;
    m.data[2][2] = 1.0;
    m.data[3][2] = z;
    m.data[3][3] = 1.0;

    return m;
}

Matrix matrix_rotation(f32 x, f32 y, f32 z)
{
    Matrix mx = {0};
    mx.data[0][0] = 1.0;
    mx.data[1][1] = cosf(x);
    mx.data[1][2] = -sinf(x);
    mx.data[2][1] = sinf(x);
    mx.data[2][2] = cosf(x);
    mx.data[3][3] = 1.0;

    Matrix my = {0};
    my.data[0][0] = cosf(y);
    my.data[0][2] = -sinf(y);
    my.data[1][1] = 1.0;
    my.data[2][0] = sinf(y);
    my.data[2][2] = cosf(y);
    my.data[3][3] = 1.0;

    Matrix mz = {0};
    mz.data[0][0] = cosf(z);
    mz.data[0][1] = -sinf(z);
    mz.data[1][0] = sinf(z);
    mz.data[1][1] = cosf(z);
    mz.data[2][2] = 1.0;
    mz.data[3][3] = 1.0;

    Matrix rot = matrix_identity();
    rot = matrix_dot_matrix(rot, mz);
    rot = matrix_dot_matrix(rot, my);
    rot = matrix_dot_matrix(rot, mx);

    return rot;
}

Matrix matrix_scale(f32 x, f32 y, f32 z)
{
    Matrix m = {0};

    m.data[0][0] = x;
    m.data[1][1] = y;
    m.data[2][2] = z;
    m.data[3][3] = 1.0;

    return m;
}

Matrix matrix_perspective(f32 view_width, f32 view_height, f32 z_near, f32 z_far, f32 fov)
{
    f32 ar           = view_width / view_height;
    f32 tan_half_fov = tanf(RAD(fov * 0.5f));

    Matrix m = {0};

    m.data[0][0] = 1.0f / (ar * tan_half_fov);
    m.data[1][1] = 1.0f / tan_half_fov;
    m.data[2][2] = -(z_far + z_near) / (z_far - z_near);
    m.data[2][3] = -1.0f;
    m.data[3][2] = -(2.0f * z_far * z_near) / (z_far - z_near);
    m.data[3][3] = 1.0f;

    return m;
}


void matrix_print(Matrix *m, const char *name)
{
    Temp scratch = scratch_begin();
    StringList sl = string_list_create(scratch.arena);

    string_list_add(&sl, S("\n"));
    if(name)
    {
        string_list_addf(&sl, "[%s]\n", name);   
    }

    string_list_addf(&sl, "| %7.4f %7.4f %7.4f %7.4f |\n", m->data[0][0], m->data[0][1], m->data[0][2], m->data[0][3]);
    string_list_addf(&sl, "| %7.4f %7.4f %7.4f %7.4f |\n", m->data[1][0], m->data[1][1], m->data[1][2], m->data[1][3]);
    string_list_addf(&sl, "| %7.4f %7.4f %7.4f %7.4f |\n", m->data[2][0], m->data[2][1], m->data[2][2], m->data[2][3]);
    string_list_addf(&sl, "| %7.4f %7.4f %7.4f %7.4f |\n", m->data[3][0], m->data[3][1], m->data[3][2], m->data[3][3]);

    String str = string_list_collapse(&sl);
    string_print(str);

    scratch_end(scratch);
}

///////////////////////////////////////////
// Interpolation
///////////////////////////////////////////

f32 interp_exp_decay(f32 a, f32 b, f32 decay, f32 dt)
{
    return b + (a - b)*exp(-decay*dt);
}

f32 interp_lerp(f32 a, f32 b, f32 t)
{
    t = CLAMP(t,0.0,1.0);
    f32 r = (1.0-t)*a+(t*b);
    return r;
}

f32 interp_exp_smooth(f32 start, f32 end, f32 alpha, int frame)
{
    alpha = CLAMP(alpha, 0.0f, 1.0f);
    f32 factor = powf(1.0f - alpha, frame + 1);
    return end - (end - start) * factor;
}

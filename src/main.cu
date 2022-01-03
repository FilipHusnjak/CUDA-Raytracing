#include <cmath>
#include <string>
#include <fstream>
#include <chrono>

#define MAX_F 3.4e+38f

#define BLOCK_SIZE 16

#define BYTES_PER_PIXEL 3

#define WIDTH 4096
#define HEIGHT 4096

#define NUM_SPHERES 20

#define MAX_DEPTH 3

using byte = unsigned char;

struct vec3 {
    float x;
    float y;
    float z;

    [[nodiscard]] __device__ __host__ float norm() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    __device__ __host__ void normalize() {
        float n = norm();
        x /= n;
        y /= n;
        z /= n;
    }

    [[nodiscard]] __device__ __host__ float dot(const vec3 &other) const {
        return x * other.x + y * other.y + z * other.z;
    }

    __device__ __host__ vec3 operator+(const vec3 &other) const {
        return {x + other.x, y + other.y, z + other.z};
    }

    __device__ __host__ vec3 operator-(const vec3 &other) const {
        return {x - other.x, y - other.y, z - other.z};
    }

    __device__ __host__ vec3 operator*(float v) const {
        return {x * v, y * v, z * v};
    }
};

struct ray {
    vec3 origin;
    vec3 dir;
};

struct sphere {
    vec3 color;
    vec3 center;
    float r;
};

__device__ __host__ float gpu_min(float a, float b) {
    return a < b ? a : b;
}

__device__ __host__ float gpu_max(float a, float b) {
    return a > b ? a : b;
}

void write_bmp_image(int width, int height, const byte *img, const std::string &filename) {
    std::ofstream ofs;
    ofs.open(filename, std::ios::binary | std::ios::out);
    if (!ofs) throw std::runtime_error("Unable to open file!");
    byte bmp_file_header[14] = {'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0};
    byte bmp_info_header[40] = {40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, BYTES_PER_PIXEL * 8, 0};
    int file_size = 54 + BYTES_PER_PIXEL * width * height;
    bmp_file_header[2] = (byte) (file_size);
    bmp_file_header[3] = (byte) (file_size >> 8);
    bmp_file_header[4] = (byte) (file_size >> 16);
    bmp_file_header[5] = (byte) (file_size >> 24);
    bmp_info_header[4] = (byte) (width);
    bmp_info_header[5] = (byte) (width >> 8);
    bmp_info_header[6] = (byte) (width >> 16);
    bmp_info_header[7] = (byte) (width >> 24);
    bmp_info_header[8] = (byte) (height);
    bmp_info_header[9] = (byte) (height >> 8);
    bmp_info_header[10] = (byte) (height >> 16);
    bmp_info_header[11] = (byte) (height >> 24);
    ofs.write((char *) bmp_file_header, sizeof(bmp_file_header));
    ofs.write((char *) bmp_info_header, sizeof(bmp_info_header));
    ofs.write((char *) img, BYTES_PER_PIXEL * width * height);
    ofs.close();
}

__device__ __host__ bool intersect(const ray &r, const sphere &s, float &t) {
    float a = r.dir.dot(r.dir);
    vec3 l = r.origin - s.center;
    float b = 2 * r.dir.dot(l);
    float c = l.dot(l) - s.r * s.r;
    float d = b * b - 4 * a * c;
    if (d <= 0) return false;
    float d_sqrt = std::sqrt(d);
    float q = b < 0 ? (-b - d_sqrt) / 2.f : (-b + d_sqrt) / 2.f;
    float t0 = q / a;
    float t1 = c / q;
    float t_max = gpu_max(t0, t1);
    float t_min = gpu_min(t0, t1);
    if (t_max <= 0.f) return false;
    t = t_min >= 0 ? t_min : t_max;
    return true;
}

__device__ __host__ vec3 trace_ray(const ray &r, const sphere *spheres, int depth) {
    vec3 light_pos = {2.f, 2.f, -3.f};
    vec3 light_color = {1.f, 1.f, 1.f};
    float diff_power = 1.f;
    float spec_power = 1.f;
    float t_min = MAX_F;
    sphere s{};
    for (int i = 0; i < NUM_SPHERES; i++) {
        float t;
        if (!intersect(r, spheres[i], t)) continue;
        if (t < t_min) {
            t_min = t;
            s = spheres[i];
        }
    }
    if (t_min == MAX_F) return {0.f, 0.f, 0.f};
    vec3 ambient = s.color * 0.2f;
    vec3 point = r.dir * t_min + r.origin;
    vec3 n = point - s.center;
    n.normalize();
    vec3 light_dir = light_pos - point;
    light_dir.normalize();
    float n_dot_l = n.dot(light_dir);
    float intensity = gpu_max(n_dot_l, 0.f);
    vec3 diffuse = s.color * intensity * diff_power;
    vec3 h = light_dir - r.dir;
    h.normalize();
    float n_dot_h = n.dot(h);
    intensity = std::pow(gpu_max(n_dot_h, 0.f), 50.f);
    vec3 specular = light_color * intensity * spec_power;
    vec3 color = ambient + diffuse + specular;
    if (depth >= MAX_DEPTH) return color;
    vec3 refl_dir = r.dir - n * 2 * r.dir.dot(n);
    refl_dir.normalize();
    ray refl_r = {point + refl_dir * 0.01f, refl_dir};
    vec3 refl_color = trace_ray(refl_r, spheres, depth + 1);
    // Fresnel-Schlick approximation
    float f0 = 0.5f;
    float f = f0 + (1 - f0) * std::pow(1 + h.dot(r.dir), 5.f);
    return refl_color * f + color * (1.f - f);
}

__device__ __host__ vec3 trace(int i, int j, const sphere *spheres) {
    auto dist = (float) WIDTH;
    vec3 dir = {(float) WIDTH / 2.f - (float) j, (float) i - (float) HEIGHT / 2.f, dist};
    dir.normalize();
    ray r = {{0.f, 0.f, -1.f}, dir};
    return trace_ray(r, spheres, 0);
}

__device__ __host__ int clamp(float value) {
    return value < 0.f ? 0 : (value > 255.f ? 255 : (int) value);
}

__global__ void gpu_trace(byte *img, const sphere *spheres) {
    const unsigned int j = threadIdx.x + BLOCK_SIZE * blockIdx.x;
    const unsigned int i = threadIdx.y + BLOCK_SIZE * blockIdx.y;
    vec3 color = trace((int) i, (int) j, spheres);
    img[(i * WIDTH + j) * BYTES_PER_PIXEL] = (byte) clamp(color.z * 255.f);
    img[(i * WIDTH + j) * BYTES_PER_PIXEL + 1] = (byte) clamp(color.y * 255.f);
    img[(i * WIDTH + j) * BYTES_PER_PIXEL + 2] = (byte) clamp(color.x * 255.f);
}

float random() {
    return (float) std::rand() / RAND_MAX;
}

int main() {
    srand(time(nullptr));
    sphere spheres[NUM_SPHERES];
    for (auto &sphere: spheres) {
        sphere = {{random(), random(), random()}, {random() * 6.f - 3.f, random() * 6.f - 3.f, random() * 4.f + 6.f},
                  random() + 0.4f};
    }
    auto t1 = std::chrono::system_clock::now();
    byte *img = new byte[WIDTH * HEIGHT * BYTES_PER_PIXEL];
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            vec3 color = trace(i, j, spheres);
            img[(i * WIDTH + j) * BYTES_PER_PIXEL] = (byte) clamp(color.z * 255.f);
            img[(i * WIDTH + j) * BYTES_PER_PIXEL + 1] = (byte) clamp(color.y * 255.f);
            img[(i * WIDTH + j) * BYTES_PER_PIXEL + 2] = (byte) clamp(color.x * 255.f);
        }
    }
    auto t2 = std::chrono::system_clock::now();
    printf("CPU time: %f ms\n", std::chrono::duration<float, std::chrono::milliseconds::period>(t2 - t1).count());
    write_bmp_image(WIDTH, HEIGHT, img, "result.bmp");

    byte *d_img;
    t1 = std::chrono::system_clock::now();
    cudaMalloc(&d_img, WIDTH * HEIGHT * BYTES_PER_PIXEL);
    sphere * d_spheres;
    cudaMalloc(&d_spheres, NUM_SPHERES * sizeof(sphere));
    cudaMemcpy(d_spheres, spheres, NUM_SPHERES * sizeof(sphere), cudaMemcpyHostToDevice);
    dim3 grid = dim3((WIDTH + (BLOCK_SIZE - 1)) / BLOCK_SIZE, (HEIGHT + (BLOCK_SIZE - 1)) / BLOCK_SIZE);
    dim3 block = dim3(BLOCK_SIZE, BLOCK_SIZE);
    gpu_trace<<<grid, block>>>(d_img, d_spheres);
    byte *img_gpu = new byte[WIDTH * HEIGHT * BYTES_PER_PIXEL];
    cudaMemcpy(img_gpu, d_img, WIDTH * HEIGHT * BYTES_PER_PIXEL, cudaMemcpyDeviceToHost);
    t2 = std::chrono::system_clock::now();
    printf("GPU time: %f ms\n", std::chrono::duration<float, std::chrono::milliseconds::period>(t2 - t1).count());
    write_bmp_image(WIDTH, HEIGHT, img_gpu, "result_gpu.bmp");

    delete[] img;
    delete[] img_gpu;
    cudaFree(d_spheres);
    cudaFree(d_img);
}

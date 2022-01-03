#include <cmath>
#include <vector>
#include <string>
#include <fstream>

#define BYTES_PER_PIXEL 3

#define WIDTH 2048
#define HEIGHT 2048

using byte = unsigned char;

struct vec3 {
    float x;
    float y;
    float z;

    [[nodiscard]] float norm() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    void normalize() {
        float n = norm();
        x /= n;
        y /= n;
        z /= n;
    }

    [[nodiscard]] float dot(const vec3 &other) const {
        return x * other.x + y * other.y + z * other.z;
    }

    vec3 operator+(const vec3 &other) const {
        return {x + other.x, y + other.y, z + other.z};
    }

    vec3 operator-(const vec3 &other) const {
        return {x - other.x, y - other.y, z - other.z};
    }

    vec3 operator*(float v) const {
        return {x * v, y * v, z * v};
    }

    vec3 operator/(float v) const {
        return {x / v, y / v, z / v};
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

void write_bmp_image(int width, int height, const std::vector<byte> &img, const std::string &filename) {
    std::ofstream ofs;
    ofs.open(filename, std::ios::binary | std::ios::out);
    if (!ofs) throw std::runtime_error("Unable to open file!");
    byte bmp_file_header[14] = {'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0};
    byte bmp_info_header[40] = {40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, BYTES_PER_PIXEL * 8, 0};
    int file_size = 54 + 3 * width * height;
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
    ofs.write((char *) img.data(), (int) img.size());
    ofs.close();
}

bool intersect(const ray &r, const sphere &s, float &t) {
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
    float t_max = std::max(t0, t1);
    float t_min = std::min(t0, t1);
    if (t_max < 0) return false;
    t = t_min >= 0 ? t_min : t_max;
    return true;
}

vec3 trace_ray(const ray &r, const std::vector<sphere> &spheres) {
    vec3 light_pos = {2.f, 2.f, -3.f};
    vec3 light_color = {1.f, 1.f, 1.f};
    float diff_power = 1.f;
    float spec_power = 1.f;
    for (const auto &sphere: spheres) {
        float t;
        if (!intersect(r, sphere, t)) continue;
        vec3 ambient = sphere.color * 0.2f;
        vec3 point = r.dir * t + r.origin;
        vec3 n = point - sphere.center;
        n.normalize();
        vec3 light_dir = light_pos - point;
        light_dir.normalize();
        float n_dot_l = n.dot(light_dir);
        float intensity = std::max(n_dot_l, 0.f);
        vec3 diffuse = sphere.color * intensity * diff_power;
        vec3 h = light_dir - r.dir;
        h.normalize();
        float n_dot_h = n.dot(h);
        intensity = std::pow(std::max(n_dot_h, 0.f), 50.f);
        vec3 specular = light_color * intensity * spec_power;
        return ambient + diffuse + specular;
    }
    return {0.f, 0.f, 0.f};
}

float clamp(float value) {
    return value < 0.f ? 0.f : (value > 255.f ? 255.f : value);
}

int main() {
    std::vector<sphere> spheres;
    spheres.push_back({{1.f, 0.f, 0.f}, {0.f, 0.f, 4.f}, 0.5f});
    spheres.push_back({{0.f, 0.f, 1.f}, {2.f, 0.f, 4.f}, 0.5f});
    auto dist = (float) WIDTH;
    std::vector<byte> img(WIDTH * HEIGHT * BYTES_PER_PIXEL, 0);
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            vec3 dir = {(float) WIDTH / 2.f - (float) j, (float) i - (float) HEIGHT / 2.f, dist};
            dir.normalize();
            ray r = {{0.f, 0.f, -1.f}, dir};
            vec3 color = trace_ray(r, spheres);
            img[(i * WIDTH + j) * BYTES_PER_PIXEL] = (byte) clamp(color.z * 255.f);
            img[(i * WIDTH + j) * BYTES_PER_PIXEL + 1] = (byte) clamp(color.y * 255.f);
            img[(i * WIDTH + j) * BYTES_PER_PIXEL + 2] = (byte) clamp(color.x * 255.f);
        }
    }
    write_bmp_image(WIDTH, HEIGHT, img, "result.bmp");
}

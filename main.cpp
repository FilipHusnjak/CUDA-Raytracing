#include <cmath>
#include <vector>
#include <string>
#include <fstream>

#define BYTES_PER_PIXEL 3

#define WIDTH 1920
#define HEIGHT 1080

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

    vec3 operator-(const vec3 &other) const {
        return {x - other.x, y - other.y, z - other.z};
    }
};

struct ray {
    vec3 origin;
    vec3 dir;
};

struct sphere {
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
    vec3 l = s.center - r.origin;
    float tca = l.dot(r.dir);
    float d = std::sqrt(l.dot(l) - tca * tca);
    if (d > s.r) return false;
    float thc = std::sqrt(s.r * s.r - d * d);
    t = tca - thc;
    return true;
}

byte trace_ray(const ray &r, const std::vector<sphere> &spheres) {
    for (const auto &sphere: spheres) {
        float t;
        if (intersect(r, sphere, t)) {
            return 255;
        }
    }
    return 0;
}

int main() {
    std::vector<sphere> spheres;
    spheres.push_back({{0.f, 0.f, 4.f}, 0.5f});
    spheres.push_back({{2.f, 0.f, 4.f}, 0.5f});
    auto dist = (float) WIDTH;
    std::vector<byte> img(WIDTH * HEIGHT * BYTES_PER_PIXEL, 0);
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            vec3 dir = {(float) WIDTH / 2.f - (float) j, (float) i - (float) HEIGHT / 2.f, dist};
            dir.normalize();
            ray r = {{0.f, 0.f, -1.f}, dir};
            img[(i * WIDTH + j) * BYTES_PER_PIXEL] = trace_ray(r, spheres);
        }
    }
    write_bmp_image(WIDTH, HEIGHT, img, "result.bmp");
}

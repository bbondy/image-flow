#include "webp.h"

#include <cctype>
#include <cstdlib>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {
std::size_t pixelIndex(int x, int y, int width) {
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
}

class TempPathGuard {
public:
    explicit TempPathGuard(std::string path) : m_path(std::move(path)) {}

    const std::string& path() const { return m_path; }

    void release() { m_path.clear(); }

    ~TempPathGuard() {
        if (!m_path.empty()) {
            std::error_code ec;
            std::filesystem::remove(m_path, ec);
        }
    }

private:
    std::string m_path;
};

std::string createSecureTempFilename(const std::string& suffix) {
    if (suffix.empty()) {
        throw std::runtime_error("Temporary file suffix must be non-empty");
    }
    const std::filesystem::path pattern = std::filesystem::temp_directory_path() /
                                          ("imageflow_webp_XXXXXX" + suffix);
    std::string mutablePattern = pattern.string();
    std::vector<char> buffer(mutablePattern.begin(), mutablePattern.end());
    buffer.push_back('\0');

    const int fd = ::mkstemps(buffer.data(), static_cast<int>(suffix.size()));
    if (fd < 0) {
        throw std::runtime_error("Failed to create temporary file");
    }
    ::close(fd);
    return std::string(buffer.data());
}

int runProcess(const std::string& executable, const std::vector<std::string>& args) {
    std::vector<char*> argv;
    argv.reserve(args.size() + 2);
    argv.push_back(const_cast<char*>(executable.c_str()));
    for (const std::string& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);

    const pid_t pid = ::fork();
    if (pid < 0) {
        throw std::runtime_error("Failed to fork child process");
    }
    if (pid == 0) {
        ::execv(executable.c_str(), argv.data());
        _exit(127);
    }

    int status = 0;
    while (::waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) {
            continue;
        }
        throw std::runtime_error("Failed waiting on child process");
    }
    if (!WIFEXITED(status)) {
        return -1;
    }
    return WEXITSTATUS(status);
}

bool isExecutable(const std::filesystem::path& candidate) {
    return std::filesystem::exists(candidate) &&
           std::filesystem::is_regular_file(candidate) &&
           access(candidate.c_str(), X_OK) == 0;
}

std::string findInPath(const std::string& program) {
    const char* envPath = std::getenv("PATH");
    if (envPath == nullptr) {
        return "";
    }
    const std::string pathValue(envPath);
    std::size_t start = 0;
    while (start <= pathValue.size()) {
        const std::size_t end = pathValue.find(':', start);
        const std::string dir = pathValue.substr(start, end == std::string::npos ? std::string::npos : end - start);
        const std::filesystem::path base = dir.empty() ? "." : dir;
        const std::filesystem::path candidate = base / program;
        if (isExecutable(candidate)) {
            return candidate.string();
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return "";
}

bool writePPM(const std::string& filename, int width, int height, const std::vector<Color>& pixels) {
    std::ofstream out(filename, std::ios::binary);
    if (!out) {
        return false;
    }

    out << "P6\n" << width << " " << height << "\n255\n";
    for (const Color& p : pixels) {
        const std::uint8_t rgb[3] = {p.r, p.g, p.b};
        out.write(reinterpret_cast<const char*>(rgb), 3);
    }
    return static_cast<bool>(out);
}

std::string readTokenSkippingComments(std::istream& in) {
    std::string token;
    char ch = '\0';

    while (in.get(ch)) {
        if (std::isspace(static_cast<unsigned char>(ch))) {
            continue;
        }
        if (ch == '#') {
            std::string discard;
            std::getline(in, discard);
            continue;
        }
        token.push_back(ch);
        break;
    }

    while (in.get(ch)) {
        if (std::isspace(static_cast<unsigned char>(ch))) {
            break;
        }
        token.push_back(ch);
    }
    return token;
}

WEBPImage readPPM(const std::string& filename) {
    std::ifstream in(filename, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Cannot open converted PPM file: " + filename);
    }

    const std::string magic = readTokenSkippingComments(in);
    if (magic != "P6") {
        throw std::runtime_error("Unsupported converted PPM magic");
    }

    const std::string widthToken = readTokenSkippingComments(in);
    const std::string heightToken = readTokenSkippingComments(in);
    const std::string maxValueToken = readTokenSkippingComments(in);

    if (widthToken.empty() || heightToken.empty() || maxValueToken.empty()) {
        throw std::runtime_error("Invalid converted PPM header");
    }

    const int width = std::stoi(widthToken);
    const int height = std::stoi(heightToken);
    const int maxValue = std::stoi(maxValueToken);

    if (width <= 0 || height <= 0 || maxValue != 255) {
        throw std::runtime_error("Unsupported converted PPM dimensions or max value");
    }

    WEBPImage image(width, height, Color(0, 0, 0));

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3);
    in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!in) {
        throw std::runtime_error("Truncated converted PPM data");
    }

    std::size_t byteIndex = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            image.setPixel(x, y, Color(bytes[byteIndex], bytes[byteIndex + 1], bytes[byteIndex + 2]));
            byteIndex += 3;
        }
    }

    return image;
}

} // namespace

WEBPImage::WEBPImage() : m_width(0), m_height(0) {}

WEBPImage::WEBPImage(int width, int height, const Color& fill)
    : m_width(width), m_height(height), m_pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), fill) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("Image dimensions must be positive");
    }
}

int WEBPImage::width() const {
    return m_width;
}

int WEBPImage::height() const {
    return m_height;
}

bool WEBPImage::inBounds(int x, int y) const {
    return x >= 0 && x < m_width && y >= 0 && y < m_height;
}

const Color& WEBPImage::getPixel(int x, int y) const {
    if (!inBounds(x, y)) {
        throw std::out_of_range("Pixel out of bounds");
    }
    return m_pixels[pixelIndex(x, y, m_width)];
}

void WEBPImage::setPixel(int x, int y, const Color& color) {
    if (!inBounds(x, y)) {
        return;
    }
    m_pixels[pixelIndex(x, y, m_width)] = color;
}

bool WEBPImage::isToolingAvailable() {
    return !findInPath("cwebp").empty() && !findInPath("dwebp").empty();
}

bool WEBPImage::save(const std::string& filename) const {
    if (m_width <= 0 || m_height <= 0 || !isToolingAvailable()) {
        return false;
    }

    const std::string cwebpPath = findInPath("cwebp");
    if (cwebpPath.empty()) {
        return false;
    }

    TempPathGuard tempPPM(createSecureTempFilename(".ppm"));
    if (!writePPM(tempPPM.path(), m_width, m_height, m_pixels)) {
        return false;
    }

    const int rc = runProcess(cwebpPath, {"-quiet", "-lossless", tempPPM.path(), "-o", filename});
    return rc == 0;
}

WEBPImage WEBPImage::load(const std::string& filename) {
    if (!isToolingAvailable()) {
        throw std::runtime_error("WebP support requires cwebp and dwebp tools installed");
    }

    const std::string dwebpPath = findInPath("dwebp");
    if (dwebpPath.empty()) {
        throw std::runtime_error("Cannot find dwebp in PATH");
    }

    TempPathGuard tempPPM(createSecureTempFilename(".ppm"));
    const int rc = runProcess(dwebpPath, {"-quiet", "-ppm", filename, "-o", tempPPM.path()});
    if (rc != 0) {
        throw std::runtime_error("Failed to decode WebP file: " + filename);
    }

    WEBPImage decoded = readPPM(tempPPM.path());
    return decoded;
}

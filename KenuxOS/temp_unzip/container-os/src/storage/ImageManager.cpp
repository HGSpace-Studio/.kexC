#include "storage/ImageManager.h"

#include "storage/Filesystem.h"

#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <iterator>      // istreambuf_iterator

// ====== 匿名命名空间: 内部辅助工具 ======
namespace {

// JSON 值类型, 用于解析 manifest.json 与配置文件
struct JsonValue {
    enum Type { Null, Bool, Number, String, Array, Object };
    Type type = Null;
    bool b = false;
    double num = 0;
    std::string str;
    std::vector<JsonValue> arr;
    std::vector<std::pair<std::string, JsonValue>> obj;

    // 在对象中按键查找, 未找到返回 nullptr
    const JsonValue* find(const std::string& key) const {
        if (type != Object) return nullptr;
        for (const auto& kv : obj) {
            if (kv.first == key) return &kv.second;
        }
        return nullptr;
    }
};

// 极简 JSON 解析器 (递归下降)
// 支持 object / array / string / number / bool / null
class JsonParser {
public:
    explicit JsonParser(const std::string& s) : s_(s), i_(0) {}

    // 解析整个 JSON 文本
    bool parse(JsonValue& out) {
        skipWs();
        return parseValue(out);
    }

private:
    const std::string& s_;
    size_t i_;

    void skipWs() {
        while (i_ < s_.size() &&
               (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\n' || s_[i_] == '\r')) {
            ++i_;
        }
    }

    bool parseValue(JsonValue& out) {
        skipWs();
        if (i_ >= s_.size()) return false;
        char c = s_[i_];
        if (c == '{') return parseObject(out);
        if (c == '[') return parseArray(out);
        if (c == '"') { out.type = JsonValue::String; return parseString(out.str); }
        if (c == 't' || c == 'f') return parseBool(out);
        if (c == 'n') return parseNull(out);
        return parseNumber(out);
    }

    bool parseString(std::string& out) {
        if (i_ >= s_.size() || s_[i_] != '"') return false;
        ++i_;  // 跳过开引号
        out.clear();
        while (i_ < s_.size() && s_[i_] != '"') {
            if (s_[i_] == '\\' && i_ + 1 < s_.size()) {
                ++i_;  // 跳过反斜杠
                switch (s_[i_]) {
                    case '"':  out += '"';  break;
                    case '\\': out += '\\'; break;
                    case '/':  out += '/';  break;
                    case 'n':  out += '\n'; break;
                    case 't':  out += '\t'; break;
                    case 'r':  out += '\r'; break;
                    case 'b':  out += '\b'; break;
                    case 'f':  out += '\f'; break;
                    case 'u':  // 跳过 4 个十六进制数字 (简化处理)
                        for (int k = 0; k < 4 && i_ + 1 < s_.size(); ++k) ++i_;
                        break;
                    default:   out += s_[i_]; break;
                }
                ++i_;
            } else {
                out += s_[i_];
                ++i_;
            }
        }
        if (i_ < s_.size()) ++i_;  // 跳过闭引号
        return true;
    }

    bool parseObject(JsonValue& out) {
        out.type = JsonValue::Object;
        ++i_;  // 跳过 '{'
        skipWs();
        if (i_ < s_.size() && s_[i_] == '}') { ++i_; return true; }
        while (i_ < s_.size()) {
            skipWs();
            std::string key;
            if (!parseString(key)) return false;
            skipWs();
            if (i_ >= s_.size() || s_[i_] != ':') return false;
            ++i_;  // 跳过 ':'
            JsonValue v;
            if (!parseValue(v)) return false;
            out.obj.emplace_back(std::move(key), std::move(v));
            skipWs();
            if (i_ < s_.size() && s_[i_] == ',') { ++i_; continue; }
            if (i_ < s_.size() && s_[i_] == '}') { ++i_; return true; }
            return false;
        }
        return false;
    }

    bool parseArray(JsonValue& out) {
        out.type = JsonValue::Array;
        ++i_;  // 跳过 '['
        skipWs();
        if (i_ < s_.size() && s_[i_] == ']') { ++i_; return true; }
        while (i_ < s_.size()) {
            JsonValue v;
            if (!parseValue(v)) return false;
            out.arr.push_back(std::move(v));
            skipWs();
            if (i_ < s_.size() && s_[i_] == ',') { ++i_; continue; }
            if (i_ < s_.size() && s_[i_] == ']') { ++i_; return true; }
            return false;
        }
        return false;
    }

    bool parseNumber(JsonValue& out) {
        out.type = JsonValue::Number;
        size_t start = i_;
        while (i_ < s_.size()) {
            char c = s_[i_];
            if (std::isdigit(static_cast<unsigned char>(c)) ||
                c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E') {
                ++i_;
            } else {
                break;
            }
        }
        try {
            out.num = std::stod(s_.substr(start, i_ - start));
        } catch (...) {
            return false;
        }
        return true;
    }

    bool parseBool(JsonValue& out) {
        if (s_.compare(i_, 4, "true") == 0) {
            out.type = JsonValue::Bool;
            out.b = true;
            i_ += 4;
            return true;
        }
        if (s_.compare(i_, 5, "false") == 0) {
            out.type = JsonValue::Bool;
            out.b = false;
            i_ += 5;
            return true;
        }
        return false;
    }

    bool parseNull(JsonValue& out) {
        if (s_.compare(i_, 4, "null") == 0) {
            out.type = JsonValue::Null;
            i_ += 4;
            return true;
        }
        return false;
    }
};

// JSON 字符串转义
std::string escapeJson(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

// 读取文件全部内容
std::string readFile(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) return "";
    return std::string(std::istreambuf_iterator<char>(ifs),
                       std::istreambuf_iterator<char>());
}

// FNV-1a 哈希 (确定性, 用于生成镜像 ID 与层摘要)
uint64_t fnv1aHash(const std::string& s) {
    uint64_t h = 14695981039346656037ULL;
    for (char c : s) {
        h ^= static_cast<unsigned char>(c);
        h *= 1099511628211ULL;
    }
    return h;
}

// 根据镜像名和标签生成镜像 ID (12 位十六进制)
std::string generateImageId(const std::string& name, const std::string& tag) {
    uint64_t h = fnv1aHash(name + ":" + tag);
    std::ostringstream oss;
    oss << std::hex << std::setw(12) << std::setfill('0') << (h & 0xFFFFFFFFFFFFULL);
    return oss.str();
}

// 生成 16 位十六进制摘要后缀
std::string generateDigestSuffix(const std::string& seed) {
    uint64_t h = fnv1aHash(seed);
    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << (h & 0xFFFFFFFFFFFFFFFFULL);
    return oss.str();
}

// 将镜像信息与层列表写入 manifest.json
bool writeManifest(const std::string& image_dir, const ImageInfo& info,
                   const std::vector<ImageLayer>& layers) {
    std::ofstream ofs(image_dir + "/manifest.json", std::ios::binary);
    if (!ofs.is_open()) return false;
    ofs << "{\n";
    ofs << "  \"id\": \"" << info.id << "\",\n";
    ofs << "  \"name\": \"" << escapeJson(info.name) << "\",\n";
    ofs << "  \"tag\": \"" << escapeJson(info.tag) << "\",\n";
    ofs << "  \"os\": \"" << info.os << "\",\n";
    ofs << "  \"arch\": \"" << info.arch << "\",\n";
    ofs << "  \"binary_format\": \"" << info.binary_format << "\",\n";
    ofs << "  \"size\": " << info.size << ",\n";
    ofs << "  \"config\": \"" << escapeJson(info.config) << "\",\n";
    ofs << "  \"layers\": [\n";
    for (size_t i = 0; i < layers.size(); ++i) {
        ofs << "    {";
        ofs << "\"digest\": \"" << layers[i].digest << "\", ";
        ofs << "\"path\": \"" << layers[i].path << "\", ";
        ofs << "\"size\": " << layers[i].size << ", ";
        ofs << "\"cached\": " << (layers[i].cached ? "true" : "false");
        ofs << "}";
        if (i + 1 < layers.size()) ofs << ",";
        ofs << "\n";
    }
    ofs << "  ]\n";
    ofs << "}\n";
    return ofs.good();
}

// 从 manifest.json 解析层列表
std::vector<ImageLayer> parseLayersFromFile(const std::string& path) {
    std::vector<ImageLayer> layers;
    std::string content = readFile(path);
    if (content.empty()) return layers;

    JsonValue root;
    JsonParser parser(content);
    if (!parser.parse(root)) return layers;

    const JsonValue* arr = root.find("layers");
    if (!arr || arr->type != JsonValue::Array) return layers;

    for (const auto& item : arr->arr) {
        ImageLayer layer;
        if (const JsonValue* v = item.find("digest")) {
            if (v->type == JsonValue::String) layer.digest = v->str;
        }
        if (const JsonValue* v = item.find("path")) {
            if (v->type == JsonValue::String) layer.path = v->str;
        }
        if (const JsonValue* v = item.find("size")) {
            if (v->type == JsonValue::Number) layer.size = static_cast<int64_t>(v->num);
        }
        if (const JsonValue* v = item.find("cached")) {
            if (v->type == JsonValue::Bool) layer.cached = v->b;
        }
        layers.push_back(layer);
    }
    return layers;
}

// 递归删除目录树
bool removeTree(const std::string& path) {
    for (const auto& entry : Filesystem::listDirectory(path)) {
        std::string child = path + "/" + entry;
        if (Filesystem::isDirectory(child)) {
            removeTree(child);
        } else {
            ::unlink(child.c_str());
        }
    }
    return Filesystem::removeDirectory(path);
}

}  // namespace

// ====== ImageManager 实现 ======

// 构造函数: 创建存储目录并加载已有镜像
ImageManager::ImageManager(const std::string& storage_path)
    : storage_path_(storage_path) {
    std::string images_dir = storage_path_ + "/images";
    std::string containers_dir = storage_path_ + "/containers";

    // 确保存储目录存在
    Filesystem::makeDirectory(images_dir, 0755);
    Filesystem::makeDirectory(containers_dir, 0755);

    // 扫描已有镜像, 重建缓存
    for (const auto& image_id : Filesystem::listDirectory(images_dir)) {
        std::string image_dir = images_dir + "/" + image_id;
        if (!Filesystem::isDirectory(image_dir)) {
            continue;
        }
        std::string manifest_path = image_dir + "/manifest.json";
        if (!Filesystem::exists(manifest_path)) {
            continue;
        }

        ImageInfo info;
        if (!parseManifest(manifest_path, info)) {
            continue;
        }
        // 确保 id 一致
        if (info.id.empty()) {
            info.id = image_id;
        }
        std::string id = info.id;
        images_[id] = info;
        layers_[id] = parseLayersFromFile(manifest_path);
    }
}

// 析构函数
ImageManager::~ImageManager() = default;

// 拉取镜像 (模拟下载)
bool ImageManager::pullImage(const std::string& name, const std::string& tag) {
    std::string image_id = generateImageId(name, tag);

    // 镜像已存在则直接返回成功
    if (images_.find(image_id) != images_.end()) {
        return true;
    }

    std::string image_dir = storage_path_ + "/images/" + image_id;
    std::string layers_dir = image_dir + "/layers";
    if (!Filesystem::makeDirectory(layers_dir, 0755)) {
        return false;
    }

    // 模拟下载: 创建若干层 (此处模拟 3 层)
    const int layer_count = 3;
    std::vector<ImageLayer> image_layers;
    int64_t total_size = 0;

    for (int i = 0; i < layer_count; ++i) {
        // 生成层摘要
        std::string suffix = generateDigestSuffix(image_id + ":" + std::to_string(i));
        std::string digest = "sha256:" + suffix;
        std::string layer_dir = layers_dir + "/" + suffix;

        if (!Filesystem::makeDirectory(layer_dir, 0755)) {
            continue;
        }

        // 模拟层内容: 写入标记文件
        std::string marker = layer_dir + "/.layer";
        std::ofstream ofs(marker);
        if (ofs.is_open()) {
            ofs << "Layer " << i << " of " << name << ":" << tag << "\n";
            ofs.close();
        }

        int64_t layer_size = Filesystem::getFileSize(marker);
        if (layer_size < 0) layer_size = 0;
        total_size += layer_size;

        ImageLayer layer;
        layer.digest = digest;
        layer.path = layer_dir;
        layer.size = layer_size;
        layer.cached = true;
        image_layers.push_back(layer);
    }

    // 构建镜像信息
    ImageInfo info;
    info.id = image_id;
    info.name = name;
    info.tag = tag;
    info.os = "linux";
    info.arch = "amd64";
    info.binary_format = "elf";
    info.size = total_size;
    info.config = "{}";

    // 写入 manifest.json
    if (!writeManifest(image_dir, info, image_layers)) {
        return false;
    }

    images_[image_id] = info;
    layers_[image_id] = image_layers;
    return true;
}

// 从 tar 文件加载镜像
bool ImageManager::loadImage(const std::string& tar_path) {
    if (!Filesystem::exists(tar_path)) {
        return false;
    }

    // 创建临时目录用于解压 tar
    std::string temp_dir = storage_path_ + "/tmp/load_" +
                           std::to_string(static_cast<long long>(std::time(nullptr)));
    if (!Filesystem::makeDirectory(temp_dir, 0755)) {
        return false;
    }

    // 解压 tar 文件
    if (!Filesystem::extractTarball(tar_path, temp_dir)) {
        removeTree(temp_dir);
        return false;
    }

    // 解析 manifest.json (兼容 Docker 格式: 数组, 或自定义格式: 对象)
    std::string manifest_path = temp_dir + "/manifest.json";
    std::string content = readFile(manifest_path);
    if (content.empty()) {
        removeTree(temp_dir);
        return false;
    }

    JsonValue root;
    JsonParser parser(content);
    if (!parser.parse(root)) {
        removeTree(temp_dir);
        return false;
    }

    // Docker manifest 为数组, 取第一个条目; 兼容单对象格式
    JsonValue* entry = nullptr;
    if (root.type == JsonValue::Array && !root.arr.empty()) {
        entry = &root.arr[0];
    } else if (root.type == JsonValue::Object) {
        entry = &root;
    }
    if (!entry) {
        removeTree(temp_dir);
        return false;
    }

    // 从 RepoTags 提取镜像名和标签
    std::string name, tag = "latest";
    if (const JsonValue* tags = entry->find("RepoTags")) {
        if (tags->type == JsonValue::Array && !tags->arr.empty() &&
            tags->arr[0].type == JsonValue::String) {
            std::string repo_tag = tags->arr[0].str;
            size_t colon = repo_tag.find(':');
            if (colon != std::string::npos) {
                name = repo_tag.substr(0, colon);
                tag = repo_tag.substr(colon + 1);
            } else {
                name = repo_tag;
            }
        }
    }
    if (name.empty()) {
        removeTree(temp_dir);
        return false;
    }

    // 读取 Config 文件内容
    std::string config_content = "{}";
    if (const JsonValue* cfg = entry->find("Config")) {
        if (cfg->type == JsonValue::String) {
            std::string cfg_text = readFile(temp_dir + "/" + cfg->str);
            if (!cfg_text.empty()) {
                config_content = cfg_text;
            }
        }
    }

    // 获取层文件列表
    std::vector<std::string> layer_files;
    if (const JsonValue* layers_val = entry->find("Layers")) {
        if (layers_val->type == JsonValue::Array) {
            for (const auto& l : layers_val->arr) {
                if (l.type == JsonValue::String) {
                    layer_files.push_back(l.str);
                }
            }
        }
    }

    // 创建镜像目录
    std::string image_id = generateImageId(name, tag);
    std::string image_dir = storage_path_ + "/images/" + image_id;
    std::string layers_dir = image_dir + "/layers";
    if (!Filesystem::makeDirectory(layers_dir, 0755)) {
        removeTree(temp_dir);
        return false;
    }

    // 解压每一层到镜像层目录
    std::vector<ImageLayer> image_layers;
    int64_t total_size = 0;
    for (size_t i = 0; i < layer_files.size(); ++i) {
        std::string layer_tar = temp_dir + "/" + layer_files[i];
        std::string suffix = generateDigestSuffix(image_id + ":" + layer_files[i] +
                                                   ":" + std::to_string(i));
        std::string digest = "sha256:" + suffix;
        std::string layer_dir = layers_dir + "/" + suffix;

        if (!Filesystem::makeDirectory(layer_dir, 0755)) {
            continue;
        }
        if (!extractLayer(layer_tar, layer_dir)) {
            continue;
        }

        ImageLayer layer;
        layer.digest = digest;
        layer.path = layer_dir;
        layer.size = Filesystem::getFileSize(layer_tar);
        if (layer.size < 0) layer.size = 0;
        layer.cached = true;
        total_size += layer.size;
        image_layers.push_back(layer);
    }

    // 构建镜像信息
    ImageInfo info;
    info.id = image_id;
    info.name = name;
    info.tag = tag;
    info.os = "linux";
    info.arch = "amd64";
    info.binary_format = "elf";
    info.size = total_size;
    info.config = config_content;

    // 写入 manifest.json
    writeManifest(image_dir, info, image_layers);

    images_[image_id] = info;
    layers_[image_id] = image_layers;

    // 清理临时目录
    removeTree(temp_dir);
    return true;
}

// 删除镜像
bool ImageManager::removeImage(const std::string& name, const std::string& tag) {
    std::string image_id = generateImageId(name, tag);
    auto it = images_.find(image_id);
    if (it == images_.end()) {
        return false;
    }

    // 删除镜像目录
    std::string image_dir = storage_path_ + "/images/" + image_id;
    removeTree(image_dir);

    images_.erase(it);
    layers_.erase(image_id);
    return true;
}

// 列出所有本地镜像
std::vector<ImageInfo> ImageManager::listImages() {
    std::vector<ImageInfo> result;
    result.reserve(images_.size());
    for (const auto& kv : images_) {
        result.push_back(kv.second);
    }
    return result;
}

// 获取指定镜像的信息
ImageInfo ImageManager::getImageInfo(const std::string& name, const std::string& tag) {
    std::string image_id = generateImageId(name, tag);
    auto it = images_.find(image_id);
    if (it != images_.end()) {
        return it->second;
    }
    return ImageInfo{};  // 不存在时返回默认值
}

// 获取镜像的所有层
std::vector<ImageLayer> ImageManager::getLayers(const std::string& image_id) {
    auto it = layers_.find(image_id);
    if (it != layers_.end()) {
        return it->second;
    }
    return {};
}

// 挂载镜像为容器 rootfs
std::string ImageManager::mountRootfs(const std::string& image_id,
                                      const std::string& container_id) {
    // 校验镜像是否存在
    if (images_.find(image_id) == images_.end()) {
        return "";
    }
    auto layer_it = layers_.find(image_id);
    if (layer_it == layers_.end() || layer_it->second.empty()) {
        return "";
    }

    // 创建容器目录结构
    std::string container_dir = storage_path_ + "/containers/" + container_id;
    std::string rootfs = container_dir + "/rootfs";
    std::string upper = container_dir + "/upper";
    std::string work = container_dir + "/work";

    if (!Filesystem::makeDirectory(rootfs, 0755) ||
        !Filesystem::makeDirectory(upper, 0755) ||
        !Filesystem::makeDirectory(work, 0755)) {
        return "";
    }

    // 收集层路径作为 OverlayFS lowerdir (从上到下)
    std::vector<std::string> layer_paths;
    for (const auto& layer : layer_it->second) {
        if (!layer.path.empty()) {
            layer_paths.push_back(layer.path);
        }
    }
    if (layer_paths.empty()) {
        return "";
    }

    // 挂载 OverlayFS
    if (!mountOverlay(layer_paths, upper, work, rootfs)) {
        return "";
    }

    return rootfs;
}

// 卸载容器 rootfs
bool ImageManager::unmountRootfs(const std::string& container_id) {
    std::string rootfs = storage_path_ + "/containers/" + container_id + "/rootfs";
    return Filesystem::umount(rootfs);
}

// OverlayFS 挂载
bool ImageManager::mountOverlay(const std::vector<std::string>& layers,
                                const std::string& upper_dir,
                                const std::string& work_dir,
                                const std::string& mount_point) {
    if (layers.empty()) {
        return false;
    }

    // 构建 OverlayFS 挂载参数
    // lowerdir: 多个层用冒号分隔, 从上到下排列
    std::string lowerdir;
    for (size_t i = 0; i < layers.size(); ++i) {
        if (i > 0) lowerdir += ":";
        lowerdir += layers[i];
    }
    std::string data = "lowerdir=" + lowerdir +
                       ",upperdir=" + upper_dir +
                       ",workdir=" + work_dir;

    // TODO: 自定义 OS 需实现 OverlayFS 内核支持
    return Filesystem::mount("overlay", mount_point, FilesystemType::Overlay, 0, data);
}

// 解析 manifest.json, 填充镜像信息
bool ImageManager::parseManifest(const std::string& path, ImageInfo& info) {
    std::string content = readFile(path);
    if (content.empty()) {
        return false;
    }

    JsonValue root;
    JsonParser parser(content);
    if (!parser.parse(root)) {
        return false;
    }

    if (const JsonValue* v = root.find("id")) {
        if (v->type == JsonValue::String) info.id = v->str;
    }
    if (const JsonValue* v = root.find("name")) {
        if (v->type == JsonValue::String) info.name = v->str;
    }
    if (const JsonValue* v = root.find("tag")) {
        if (v->type == JsonValue::String) info.tag = v->str;
    }
    if (const JsonValue* v = root.find("os")) {
        if (v->type == JsonValue::String) info.os = v->str;
    }
    if (const JsonValue* v = root.find("arch")) {
        if (v->type == JsonValue::String) info.arch = v->str;
    }
    if (const JsonValue* v = root.find("binary_format")) {
        if (v->type == JsonValue::String) info.binary_format = v->str;
    }
    if (const JsonValue* v = root.find("size")) {
        if (v->type == JsonValue::Number) info.size = static_cast<int64_t>(v->num);
    }
    if (const JsonValue* v = root.find("config")) {
        if (v->type == JsonValue::String) info.config = v->str;
    }

    return true;
}

// 解压镜像层到目标路径
bool ImageManager::extractLayer(const std::string& layer_path,
                                const std::string& dest_path) {
    if (!Filesystem::makeDirectory(dest_path, 0755)) {
        return false;
    }
    return Filesystem::extractTarball(layer_path, dest_path);
}

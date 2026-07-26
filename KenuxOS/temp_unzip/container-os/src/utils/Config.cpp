#include "utils/Config.h"

#include <fstream>
#include <sstream>
#include <cctype>
#include <string>

Config& Config::getInstance() {
    static Config instance;
    return instance;
}

Config::Config() {}
Config::~Config() {}

std::string Config::trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() &&
           (s[start] == ' ' || s[start] == '\t' || s[start] == '\r' || s[start] == '\n')) {
        ++start;
    }
    size_t end = s.size();
    while (end > start &&
           (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r' || s[end - 1] == '\n')) {
        --end;
    }
    return s.substr(start, end - start);
}

std::string Config::stripQuotes(const std::string& s) {
    std::string t = trim(s);
    if (t.size() >= 2 && t.front() == '"' && t.back() == '"') {
        return t.substr(1, t.size() - 2);
    }
    return t;
}

bool Config::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    std::ostringstream oss;
    oss << file.rdbuf();
    return loadFromString(oss.str());
}

bool Config::loadFromString(const std::string& content) {
    // 自动识别格式：以 '{' 开头视为 JSON，否则按 key=value 解析
    std::string trimmed = trim(content);
    if (trimmed.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        values_.clear();
        return true;
    }
    if (trimmed.front() == '{') {
        return parseSimpleJson(content);
    }
    return parseKeyValue(content);
}

void Config::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    values_.clear();
}

void Config::set(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    values_[key] = value;
}

std::string Config::get(const std::string& key, const std::string& default_value) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = values_.find(key);
    if (it == values_.end()) {
        return default_value;
    }
    return it->second;
}

int Config::getInt(const std::string& key, int default_value) const {
    std::string v = get(key, "");
    if (v.empty()) {
        return default_value;
    }
    try {
        size_t pos = 0;
        int result = std::stoi(v, &pos);
        // 整个字符串（去除尾部空白后）应全部被消费
        std::string rest = trim(v.substr(pos));
        if (!rest.empty()) {
            return default_value;
        }
        return result;
    } catch (...) {
        return default_value;
    }
}

bool Config::getBool(const std::string& key, bool default_value) const {
    std::string v = get(key, "");
    if (v.empty()) {
        return default_value;
    }
    // 转小写比较
    std::string lower;
    lower.reserve(v.size());
    for (char c : v) {
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    if (lower == "true" || lower == "1" || lower == "yes" || lower == "on") {
        return true;
    }
    if (lower == "false" || lower == "0" || lower == "no" || lower == "off") {
        return false;
    }
    return default_value;
}

double Config::getDouble(const std::string& key, double default_value) const {
    std::string v = get(key, "");
    if (v.empty()) {
        return default_value;
    }
    try {
        size_t pos = 0;
        double result = std::stod(v, &pos);
        std::string rest = trim(v.substr(pos));
        if (!rest.empty()) {
            return default_value;
        }
        return result;
    } catch (...) {
        return default_value;
    }
}

bool Config::has(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return values_.find(key) != values_.end();
}

bool Config::parseSimpleJson(const std::string& content) {
    // 解析简单的扁平 JSON 对象，例如：
    //   {
    //       "name": "ubuntu",
    //       "port": 8080,
    //       "enabled": true,
    //       "ratio": 0.5
    //   }
    // 支持字符串、数字、布尔、null 类型；不支持嵌套对象与数组

    size_t i = 0;
    const size_t n = content.size();

    auto skipWs = [&]() {
        while (i < n && (content[i] == ' ' || content[i] == '\t' ||
                         content[i] == '\n' || content[i] == '\r')) {
            ++i;
        }
    };

    // 解析字符串字面量（调用时 content[i] 应为 '"'）
    auto parseString = [&](std::string& out) -> bool {
        if (i >= n || content[i] != '"') {
            return false;
        }
        ++i; // 跳过开头引号
        out.clear();
        while (i < n && content[i] != '"') {
            if (content[i] == '\\' && i + 1 < n) {
                char c = content[i + 1];
                switch (c) {
                    case 'n':  out += '\n'; break;
                    case 't':  out += '\t'; break;
                    case 'r':  out += '\r'; break;
                    case '\\': out += '\\'; break;
                    case '"':  out += '"';  break;
                    case '/':  out += '/';  break;
                    case 'b':  out += '\b'; break;
                    case 'f':  out += '\f'; break;
                    default:   out += c;    break;
                }
                i += 2;
            } else {
                out += content[i];
                ++i;
            }
        }
        if (i >= n) {
            return false; // 未找到结束引号
        }
        ++i; // 跳过结尾引号
        return true;
    };

    std::unordered_map<std::string, std::string> temp;

    skipWs();
    if (i >= n || content[i] != '{') {
        return false;
    }
    ++i;
    skipWs();

    // 空对象 {}
    if (i < n && content[i] == '}') {
        ++i;
        std::lock_guard<std::mutex> lock(mutex_);
        values_ = std::move(temp);
        return true;
    }

    bool closed = false;
    while (i < n) {
        skipWs();
        // 解析 key
        std::string key;
        if (!parseString(key)) {
            return false;
        }
        skipWs();
        if (i >= n || content[i] != ':') {
            return false;
        }
        ++i; // 跳过 ':'
        skipWs();

        // 解析 value
        std::string value;
        if (i < n && content[i] == '"') {
            if (!parseString(value)) {
                return false;
            }
        } else {
            // 非字符串值：数字、true、false、null
            std::string raw;
            while (i < n && content[i] != ',' && content[i] != '}' &&
                   content[i] != ' ' && content[i] != '\t' &&
                   content[i] != '\n' && content[i] != '\r') {
                raw += content[i];
                ++i;
            }
            value = (raw == "null") ? std::string() : raw;
        }

        temp[key] = value;

        skipWs();
        if (i < n && content[i] == ',') {
            ++i;
            skipWs();
            // 允许尾随逗号
            if (i < n && content[i] == '}') {
                ++i;
                closed = true;
                break;
            }
        } else if (i < n && content[i] == '}') {
            ++i;
            closed = true;
            break;
        } else {
            return false;
        }
    }

    if (!closed) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    values_ = std::move(temp);
    return true;
}

bool Config::parseKeyValue(const std::string& content) {
    // 解析 key=value 格式，每行一项，例如：
    //   name = ubuntu
    //   port = 8080
    //   enabled = true
    //   ratio = 0.5
    // 以 '#' 或 ';' 开头的行为注释，空行忽略
    // 值可用双引号包裹以保留首尾空白

    std::istringstream stream(content);
    std::string line;
    std::unordered_map<std::string, std::string> temp;

    while (std::getline(stream, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#' || t[0] == ';') {
            continue;
        }
        size_t eq = t.find('=');
        if (eq == std::string::npos) {
            continue; // 非法行，跳过
        }
        std::string key = trim(t.substr(0, eq));
        std::string value = stripQuotes(trim(t.substr(eq + 1)));
        if (key.empty()) {
            continue;
        }
        temp[key] = value;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    values_ = std::move(temp);
    return true;
}

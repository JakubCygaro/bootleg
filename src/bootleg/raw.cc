#include <algorithm>
#include <bootleg/game.hpp>
#include <cctype>
#include <charconv>
#include <optional>
#include <print>
#include <raylib.h>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace boot::raw {

enum struct ParseState {
    PASSING,
    READHEADER,
    READDATA,
};
using namespace std::string_view_literals;
static const std::unordered_map<std::string_view, unsigned int> COLORMAP = {
    { "red"sv, boot::colors::CRED },
    { "yellow"sv, boot::colors::CYELLOW },
    { "green"sv, boot::colors::CGREEN },
    { "blue"sv, boot::colors::CBLUE },
    { "pink"sv, boot::colors::CPINK },
};

using kvp = std::pair<std::string, std::string>;

static std::string_view trim(const std::string_view sv)
{
    size_t start;
    size_t end;
    for (start = 0; start < sv.size(); start++) {
        if (!std::isspace(sv[start]))
            break;
    }
    if (start == sv.size() - 1)
        return std::string_view(sv.begin() + start, sv.end());
    for (end = sv.size() - 1; end >= start; end--) {
        if (!std::isspace(sv[end]))
            break;
    }
    return std::string_view(sv.data() + start, end - start + 1);
}

static std::optional<kvp> read_kvp(const std::string& line)
{
    std::string key;
    std::string value;
    auto eq = line.find('=');
    if (eq == std::string::npos) {
        return std::nullopt;
    }
    key = line.substr(0, eq - 1);
    key = std::string(trim(key));
    if (line.size() > eq) {
        value = line.substr(eq + 1);
        value = std::string(trim(value));
    }
    // what the fuck is this syntax
    return { { key, value } };
}

static void set_property(LevelData& ldata, const kvp& kv)
{
    auto [key, val] = kv;
    if (key == "X") {
        ldata.X = std::stoi(val);
    }
    if (key == "Y") {
        ldata.Y = std::stoi(val);
    }
    if (key == "Z") {
        ldata.Z = std::stoi(val);
    }
}
static Color read_color(const std::string_view sv)
{
    if (sv.starts_with("0x")) {
        unsigned int v {};
        std::from_chars(sv.begin() + 2, sv.end(), v, 16);
        return boot::decode_color_from_hex(v);
    }
    if(COLORMAP.contains(sv)){
        return boot::decode_color_from_hex(COLORMAP.at(sv));
    }

    return BLANK;
}
/// a chunk is the part of the data section that represents the sigular Y level
/// while a slice is a single line of a chunk, that is terminated by a newline character
static std::vector<Color> read_chunk_slice(std::string_view slice)
{
    std::vector<Color> ret;
    size_t start = 0, end = 0;
    size_t i = start;
    while (i < slice.size() - 1) {
        while (!std::isalnum(slice[i]))
            i++;
        start = i;
        while (i < slice.size() && std::isalnum(slice[i]))
            i++;
        end = i - 1;
        const auto token = std::string_view(slice.data() + start, end - start + 1);
        ret.push_back(read_color(token));
    }
    return ret;
}

LevelData parse_level_data(std::string&& src)
{
    LevelData lvl = {};
    ParseState pstate = ParseState::PASSING;
    std::vector<std::vector<Color>> data {};
    std::vector<Color> chunk {};
    std::stringstream sstream;
    sstream << std::move(src);
    while (!sstream.eof()) {
        std::string line;
        std::getline(sstream, line);
        if (line == "header:") {
            pstate = ParseState::READHEADER;
            continue;
        } else if (line == "data:") {
            pstate = ParseState::READDATA;
            continue;
        }
        switch (pstate) {
        case ParseState::PASSING: {
        } break;
        case ParseState::READHEADER: {
            if (auto kvp = read_kvp(line); kvp) {
                // const auto& [k, v] = *kvp;
                // std::println("k: '{}' v: '{}'", k, v);
                set_property(lvl, *kvp);
            }
        } break;
        case ParseState::READDATA: {
            if (line.empty()) {
                data.push_back(std::move(chunk));
                chunk = {};
                continue;
            }
            chunk.append_range(read_chunk_slice(line));
        } break;
        }
    }
    lvl.solution = CubeData(lvl.X, lvl.Y, lvl.Z);
    std::vector<Color>* current_chunk = nullptr;
    // each chunk corresponds to y
    for (int y = 0; y < lvl.Y; y++) {
        if ((size_t)y >= data.size()) {
            current_chunk = nullptr;
        } else {
            current_chunk = &data[y];
        }
        // each slice corresponds to z
        for (int z = 0; z < lvl.Z; z++) {
            const auto base_offset = z * lvl.X;
            for (int x = 0; x < lvl.X; x++) {
                if (current_chunk) {
                    const auto pos = base_offset + x;
                    auto c = GREEN;
                    if ((size_t)pos < current_chunk->size()) {
                        c = (*current_chunk)[pos];
                    }
                    lvl.solution.color_data[x][y][z] = c;
                } else {
                    lvl.solution.color_data[x][y][z] = BLANK;
                }
            }
        }
    }
    return lvl;
}
}

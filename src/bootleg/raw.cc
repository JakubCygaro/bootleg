#include <bootleg/game.hpp>
#include <sstream>
#include <string>
#include <utility>

namespace boot::raw {

enum struct ParseState {
    PASSING,
    READHEADER,
    READDATA,
};

using kvp = std::pair<std::string, std::string>;

static kvp read_kvp(const std::string& line){
    std::string key;
    std::string value;

}

LevelData parse_level_data(std::string&& src)
{
    LevelData lvl = {};
    ParseState pstate = ParseState::PASSING;
    std::stringstream sstream;
    sstream << std::move(src);
    while (true) {
        switch (pstate) {
        if (sstream.eof())
                break;
        case ParseState::PASSING: {
            std::string line;
            std::getline(sstream, line);
            if (line == "header:")
                pstate = ParseState::READHEADER;
            else if (line == "data:")
                pstate = ParseState::READDATA;
        } break;
        case ParseState::READHEADER: {
            std::string line;
            std::getline(sstream, line);
        } break;
        }
    }
leave_loop:
}
}

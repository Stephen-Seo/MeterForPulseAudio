#include <iostream>
#include <cstdlib>
#include <string>

#include <SFML/Graphics.hpp>

#include <ADP/AnotherDangParser.hpp>
#include "MfPA/Meter.hpp"
#include "MfPA/GetSinkSourceInfo.hpp"

int main(int argc, char** argv)
{
    std::string sinkOrSourceName;
    bool isSink = true;
    unsigned int framerateLimit = 60;
    sf::Color color(sf::Color::Green);
    bool hideMarkings = false;

    ADP::AnotherDangParser parser;
    parser.addLongOptionFlag(
        "sink",
        [&sinkOrSourceName, &isSink] (std::string opt) {
            sinkOrSourceName = opt;
            isSink = true;
        },
        "Sets the sink to monitor (use default sink-monitor by default)");
    parser.addLongOptionFlag(
        "source",
        [&sinkOrSourceName, &isSink] (std::string opt) {
            sinkOrSourceName = opt;
            isSink = false;
        },
        "Sets the source to monitor");
    parser.addOptionFlag(
        "f",
        [&framerateLimit] (std::string opt) {
            try {
                framerateLimit = std::stoul(opt);
            } catch (const std::invalid_argument& e) {
                std::cerr << "ERROR: Got invalid argument for \"-f\"" << std::endl;
                std::exit(1);
            }
        },
        "Sets the framerate (default 60)");
    parser.addLongFlag(
        "red",
        [&color] () {
            color = sf::Color::Red;
        },
        "Sets the bar color to red");
    parser.addLongFlag(
        "green",
        [&color] () {
            color = sf::Color::Green;
        },
        "Sets the bar color to green (default)");
    parser.addLongFlag(
        "blue",
        [&color] () {
            color = sf::Color::Blue;
        },
        "Sets the bar color to blue");
    parser.addLongFlag(
        "magenta",
        [&color] () {
            color = sf::Color::Magenta;
        },
        "Sets the bar color to magenta");
    parser.addLongFlag(
        "yellow",
        [&color] () {
            color = sf::Color::Yellow;
        },
        "Sets the bar color to yellow");
    parser.addLongFlag(
        "cyan",
        [&color] () {
            color = sf::Color::Cyan;
        },
        "Sets the bar color to cyan");
    parser.addLongOptionFlag(
        "color",
        [&color] (std::string opt) {
            unsigned int c = std::stoul(opt, nullptr, 16);
            color.r = (c >> 16) & 0xFF;
            color.g = (c >> 8) & 0xFF;
            color.b = c & 0xFF;
        },
        "Sets the bar color to a specified color (hex input like 0xFFFFFF, "
        "red is most significant byte out of 3)");
    parser.addLongFlag("list-sinks",
        [] () {
            MfPA::GetSinkSourceInfo getInfo(true);
            getInfo.startMainLoop();
            std::exit(0);
        },
        "Lists available PulseAudio sinks");
    parser.addLongFlag("list-sources",
        [] () {
            MfPA::GetSinkSourceInfo getInfo(false);
            getInfo.startMainLoop();
            std::exit(0);
        },
        "Lists available PulseAudio sources");
    parser.addLongFlag("hide-markings",
        [&hideMarkings] () {
            hideMarkings = true;
        },
        "Hides the markings on the meter (default not hidden)");
    parser.addFlag(
        "h",
        [&parser] () {
            parser.printHelp();
            std::exit(0);
        },
        "Prints help text");
    parser.aliasFlag("-h", "--help");

    if(!parser.parse(argc, argv))
    {
        std::cerr << "ERROR: Got invalid arguments! Try '-h' for usage info."
            << std::endl;
        return 1;
    }

    MfPA::Meter meter(
        sinkOrSourceName.c_str(),
        isSink,
        framerateLimit,
        color,
        hideMarkings);
    meter.startMainLoop();

    return 0;
}

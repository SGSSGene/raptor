// -----------------------------------------------------------------------------------------------------
// Copyright (c) 2006-2021, Knut Reinert & Freie Universität Berlin
// Copyright (c) 2016-2021, Knut Reinert & MPI für molekulare Genetik
// This file may be used, modified and/or redistributed under the terms of the 3-clause BSD-License
// shipped with this file and also available at: https://github.com/seqan/raptor/blob/master/LICENSE.md
// -----------------------------------------------------------------------------------------------------

#include <raptor/argument_parsing/upgrade.hpp>
#include <raptor/upgrade/upgrade.hpp>

namespace raptor
{

void init_upgrade_parser(seqan3::argument_parser & parser, upgrade_arguments & arguments)
{
    init_shared_meta(parser);
    parser.add_option(arguments.bin_file,
                      '\0',
                      "bins",
                      "File containing one file per line per bin.",
                      seqan3::option_spec::required,
                      seqan3::input_file_validator{});
    parser.add_option(arguments.in_file,
                      '\0',
                      "input",
                      "The index to upgrade. Parts: Without suffix _0",
                      seqan3::option_spec::required);
    parser.add_option(arguments.out_file,
                      '\0',
                      "output",
                      "Path to new index.",
                      seqan3::option_spec::required,
                      seqan3::output_file_validator{});
    parser.add_option(arguments.window_size,
                      '\0',
                      "window",
                      "The original window size.",
                      seqan3::option_spec::required,
                      positive_integer_validator{});
    parser.add_option(arguments.kmer_size,
                      '\0',
                      "kmer",
                      "The original kmer size.",
                      seqan3::option_spec::required,
                      seqan3::arithmetic_range_validator{1, 32});
    parser.add_option(arguments.parts,
                      '\0',
                      "parts",
                      "Original index consisted of this many parts.",
                      seqan3::option_spec::standard,
                      power_of_two_validator{});
    parser.add_flag(arguments.compressed,
                    '\0',
                    "compressed",
                    "Original IBF was compressed.");
}

void run_upgrade(seqan3::argument_parser & parser)
{
    upgrade_arguments arguments{};
    init_upgrade_parser(parser, arguments);
    try_parsing(parser);

    // ==========================================
    // Various checks.
    // ==========================================

    if (arguments.parts == 1)
    {
        seqan3::input_file_validator{}(arguments.in_file);
    }
    else
    {
        seqan3::input_file_validator validator{};
        for (size_t part{0}; part < arguments.parts; ++part)
        {
            validator(arguments.in_file.string() + std::string{"_"} + std::to_string(part));
        }
    }

    // ==========================================
    // Process bin_path
    // ==========================================
    std::ifstream istrm{arguments.bin_file};
    std::string line;
    auto sequence_file_validator{bin_validator{}.sequence_file_validator};

    while (std::getline(istrm, line))
    {
        if (!line.empty())
        {
            sequence_file_validator(line);
            arguments.bin_path.emplace_back(std::vector<std::string>{line});
        }
    }

    // ==========================================
    // Dispatch
    // ==========================================
    raptor_upgrade(arguments);
};

} // namespace raptor

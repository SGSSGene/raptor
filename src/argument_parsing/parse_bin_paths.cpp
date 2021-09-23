// -----------------------------------------------------------------------------------------------------
// Copyright (c) 2006-2021, Knut Reinert & Freie Universität Berlin
// Copyright (c) 2016-2021, Knut Reinert & MPI für molekulare Genetik
// This file may be used, modified and/or redistributed under the terms of the 3-clause BSD-License
// shipped with this file and also available at: https://github.com/seqan/raptor/blob/master/LICENSE.md
// -----------------------------------------------------------------------------------------------------

#include <raptor/argument_parsing/parse_bin_paths.hpp>
#include <raptor/argument_parsing/validators.hpp>

namespace raptor
{

void parse_bin_paths(std::filesystem::path const & bin_file,
                     std::vector<std::vector<std::string>> & bin_paths,
                     bool const is_socks)
{
    std::ifstream istrm{bin_file};
    std::string line{};
    std::string color_name{};
    std::string file_name{};
    std::vector<std::string> tmp{};

    while (std::getline(istrm, line))
    {
        if (!line.empty())
        {
            tmp.clear();
            std::stringstream sstream{line};

            if (is_socks)
                sstream >> color_name;

            while (std::getline(sstream, file_name, ' '))
                if (!file_name.empty())
                    tmp.emplace_back(file_name);

            bin_paths.emplace_back(tmp);
        }
    }

    bin_validator{}(bin_paths);
}

} // namespace raptor

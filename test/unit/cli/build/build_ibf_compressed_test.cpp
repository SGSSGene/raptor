// --------------------------------------------------------------------------------------------------
// Copyright (c) 2006-2023, Knut Reinert & Freie Universität Berlin
// Copyright (c) 2016-2023, Knut Reinert & MPI für molekulare Genetik
// This file may be used, modified and/or redistributed under the terms of the 3-clause BSD-License
// shipped with this file and also available at: https://github.com/seqan/raptor/blob/main/LICENSE.md
// --------------------------------------------------------------------------------------------------

#include <raptor/test/cli_test.hpp>

struct build_ibf_compressed :
    public raptor_base,
    public testing::WithParamInterface<std::tuple<size_t, size_t, bool, size_t>>
{};

TEST_P(build_ibf_compressed, pipeline)
{
    auto const [number_of_repeated_bins, window_size, run_parallel_tmp, number_of_errors] = GetParam();
    bool const run_parallel = run_parallel_tmp && number_of_repeated_bins >= 32;

    std::stringstream header{};
    { // generate input file
        std::ofstream file{"raptor_cli_test.txt"};
        size_t usr_bin_id{0};
        for (auto && file_path : get_repeated_bins(number_of_repeated_bins))
        {
            header << '#' << usr_bin_id++ << '\t' << file_path << '\n';
            file << file_path << '\n';
        }
        header << "#QUERY_NAME\tUSER_BINS\n";
        file << '\n';
    }

    cli_test_result const result1 = execute_app("raptor",
                                                "build",
                                                "--kmer 19",
                                                "--window ",
                                                std::to_string(window_size),
                                                "--threads ",
                                                run_parallel ? "2" : "1",
                                                "--output raptor.index",
                                                "--compressed",
                                                "--quiet",
                                                "--input",
                                                "raptor_cli_test.txt");
    EXPECT_EQ(result1.out, std::string{});
    EXPECT_EQ(result1.err, std::string{});
    RAPTOR_ASSERT_ZERO_EXIT(result1);

    cli_test_result const result2 = execute_app("raptor",
                                                "search",
                                                "--output search.out",
                                                "--error ",
                                                std::to_string(number_of_errors),
                                                "--index ",
                                                "raptor.index",
                                                "--quiet",
                                                "--query ",
                                                data("query.fq"));
    EXPECT_EQ(result2.out, std::string{});
    EXPECT_EQ(result2.err, std::string{});
    RAPTOR_ASSERT_ZERO_EXIT(result2);

    compare_search(number_of_repeated_bins, number_of_errors, "search.out");
}

INSTANTIATE_TEST_SUITE_P(build_ibf_compressed_suite,
                         build_ibf_compressed,
                         testing::Combine(testing::Values(0, 16, 32),
                                          testing::Values(19, 23),
                                          testing::Values(true, false),
                                          testing::Values(0, 1)),
                         [](testing::TestParamInfo<build_ibf_compressed::ParamType> const & info)
                         {
                             std::string name = std::to_string(std::max<int>(1, std::get<0>(info.param) * 4)) + "_bins_"
                                              + std::to_string(std::get<1>(info.param)) + "_window_"
                                              + (std::get<2>(info.param) ? "parallel" : "serial")
                                              + std::to_string(std::get<3>(info.param)) + "_error";
                             return name;
                         });

TEST_F(build_ibf_compressed, wrong_compression)
{
    raptor::raptor_index<raptor::index_structure::ibf_compressed> index{};

    std::ifstream is{data("1bins19window.index"), std::ios::binary};
    cereal::BinaryInputArchive iarchive{is};
    EXPECT_THROW(iarchive(index), sharg::parser_error);
}

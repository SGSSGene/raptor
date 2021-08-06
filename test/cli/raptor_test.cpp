#include <fstream>
#include <seqan3/std/ranges>     // range comparisons
#include <string>                // strings
#include <vector>                // vectors

#include <seqan3/search/dream_index/interleaved_bloom_filter.hpp>
#include <seqan3/utility/views/zip.hpp>

#include "cli_test.hpp"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////// raptor build tests ///////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Good example for printing tables: https://en.cppreference.com/w/cpp/io/ios_base/width
std::string debug_ibfs(seqan3::interleaved_bloom_filter<seqan3::data_layout::uncompressed> const & expected_ibf,
                       seqan3::interleaved_bloom_filter<seqan3::data_layout::uncompressed> const & actual_ibf)
{
    std::stringstream result{};
    result << ">>>IBFs differ<<<\n";
    result.setf(std::ios::left, std::ios::adjustfield);

    result.width(22);
    result << "#Member accessor";
    result.width(15);
    result << "Expected value";
    result.width(13);
    result << "Actual value";
    result << '\n';

    result.width(22);
    result << "bin_count()";
    result.width(15);
    result << expected_ibf.bin_count();
    result.width(13);
    result << actual_ibf.bin_count();
    result << '\n';

    result.width(22);
    result << "bin_size()";
    result.width(15);
    result << expected_ibf.bin_size();
    result.width(13);
    result << actual_ibf.bin_size();
    result << '\n';

    result.width(22);
    result << "hash_function_count()";
    result.width(15);
    result << expected_ibf.hash_function_count();
    result.width(13);
    result << actual_ibf.hash_function_count();
    result << '\n';

    result.width(22);
    result << "bit_size()";
    result.width(15);
    result << expected_ibf.bit_size();
    result.width(13);
    result << actual_ibf.bit_size();
    result << '\n';

    return result.str();
}

void compare_results(std::filesystem::path const & expected_result, std::filesystem::path const & actual_result)
{
    uint8_t expected_kmer_size{}, actual_kmer_size{};
    uint32_t expected_window_size{}, actual_window_size{};
    std::vector<std::vector<std::string>> expected_bin_path{}, actual_bin_path{};
    seqan3::interleaved_bloom_filter<seqan3::data_layout::uncompressed> expected_ibf{}, actual_ibf{};

    {
        std::ifstream is{expected_result, std::ios::binary};
        cereal::BinaryInputArchive iarchive{is};
        iarchive(expected_kmer_size);
        iarchive(expected_window_size);
        iarchive(expected_bin_path);
        iarchive(expected_ibf);
    }
    {
        std::ifstream is{actual_result, std::ios::binary};
        cereal::BinaryInputArchive iarchive{is};
        iarchive(actual_kmer_size);
        iarchive(actual_window_size);
        iarchive(actual_bin_path);
        iarchive(actual_ibf);
    }

    EXPECT_EQ(expected_kmer_size, actual_kmer_size);
    EXPECT_EQ(expected_window_size, actual_window_size);
    EXPECT_TRUE(expected_ibf == actual_ibf) << debug_ibfs(expected_ibf, actual_ibf);
    EXPECT_EQ(std::ranges::distance(expected_bin_path), std::ranges::distance(actual_bin_path));
    for (auto const && [expected_list, actual_list] : seqan3::views::zip(expected_bin_path, actual_bin_path))
    {
        EXPECT_TRUE(std::ranges::distance(expected_list) > 0);
        for (auto const && [expected_file, actual_file] : seqan3::views::zip(expected_list, actual_list))
        {
            std::filesystem::path const expected_path(expected_file);
            std::filesystem::path const actual_path(actual_file);
            EXPECT_EQ(expected_path.filename(), actual_path.filename());
        }
    }
}

TEST_P(raptor_build, build_with_file)
{
    auto const [number_of_repeated_bins, window_size, run_parallel_tmp] = GetParam();
    bool const run_parallel = run_parallel_tmp && number_of_repeated_bins >= 32;

    {
        std::string const expanded_bins = repeat_bins(number_of_repeated_bins);
        std::ofstream file{"raptor_cli_test.txt"};
        auto split_bins = expanded_bins
                        | std::views::split(' ')
                        | std::views::transform([](auto &&rng) {
                            return std::string_view(&*rng.begin(), std::ranges::distance(rng));});
        for (auto && file_path : split_bins)
        {
            file << file_path << '\n';
        }
        file << '\n';
    }

    cli_test_result const result = execute_app("raptor", "build",
                                                         "--kmer 19",
                                                         "--window ", std::to_string(window_size),
                                                         "--size 64k",
                                                         "--threads ", run_parallel ? "2" : "1",
                                                         "--output index.ibf",
                                                         "raptor_cli_test.txt");
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_EQ(result.out, std::string{});
    EXPECT_EQ(result.err, std::string{});

    compare_results(ibf_path(number_of_repeated_bins, window_size), "index.ibf");
}

TEST_P(raptor_build, build_with_file_socks)
{
    auto const [number_of_repeated_bins, window_size, run_parallel_tmp] = GetParam();
    bool const run_parallel = run_parallel_tmp && number_of_repeated_bins >= 32;

    {
        std::string const expanded_bins = repeat_bins(number_of_repeated_bins);
        std::ofstream file{"raptor_cli_test.txt"};
        auto split_bins = expanded_bins
                        | std::views::split(' ')
                        | std::views::transform([](auto &&rng) {
                            return std::string_view(&*rng.begin(), std::ranges::distance(rng));});
        bool repeat{false};
        for (auto && file_path : split_bins)
        {
            file << "dummy_color: " << file_path;
            if (repeat)
                file << ' ' << file_path;
            file << '\n';
            repeat = !repeat;
        }
        file << '\n';
    }

    cli_test_result const result = execute_app("raptor", "socks", "build",
                                                         "--kmer 19",
                                                         "--window ", std::to_string(window_size),
                                                         "--size 64k",
                                                         "--threads ", run_parallel ? "2" : "1",
                                                         "--output index.ibf",
                                                         "raptor_cli_test.txt");
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_EQ(result.out, std::string{});
    EXPECT_EQ(result.err, std::string{});

    compare_results(ibf_path(number_of_repeated_bins, window_size), "index.ibf");
}

INSTANTIATE_TEST_SUITE_P(build_suite,
                         raptor_build,
                         testing::Combine(testing::Values(0, 16, 32), testing::Values(19, 23), testing::Values(true, false)),
                         [] (testing::TestParamInfo<raptor_build::ParamType> const & info)
                         {
                             std::string name = std::to_string(std::max<int>(1, std::get<0>(info.param) * 4)) + "_bins_" +
                                                std::to_string(std::get<1>(info.param)) + "_window_" +
                                                (std::get<2>(info.param) ? "parallel" : "serial");
                             return name;
                         });

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////// raptor search tests //////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_P(raptor_search, search)
{
    auto const [number_of_repeated_bins, window_size, number_of_errors] = GetParam();

    if (window_size == 23 && number_of_errors == 0)
        GTEST_SKIP() << "Needs dynamic threshold correction";

    cli_test_result const result = execute_app("raptor", "search",
                                                         "--output search.out",
                                                         "--error ", std::to_string(number_of_errors),
                                                         "--index ", ibf_path(number_of_repeated_bins, window_size),
                                                         "--query ", data("query.fq"));
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_EQ(result.out, std::string{});
    EXPECT_EQ(result.err, std::string{});

    std::string const expected = string_from_file(search_result_path(number_of_repeated_bins, window_size, number_of_errors), std::ios::binary);
    std::string const actual = string_from_file("search.out");

    EXPECT_EQ(expected, actual);
}

TEST_P(raptor_search, search_socks)
{
    auto const [number_of_repeated_bins, window_size, number_of_errors] = GetParam();

    if (window_size == 23 || number_of_errors != 0)
        GTEST_SKIP() << "SOCKS only supports exact kmers";

    cli_test_result const result = execute_app("raptor", "socks", "lookup-kmer",
                                                         "--output search.out",
                                                         "--index ", ibf_path(number_of_repeated_bins, window_size),
                                                         "--query ", data("query_socks.fq"));
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_EQ(result.out, std::string{});
    EXPECT_EQ(result.err, std::string{});

    std::string const expected = string_from_file(search_result_path(number_of_repeated_bins, window_size, number_of_errors, true), std::ios::binary);
    std::string const actual = string_from_file("search.out");

    EXPECT_EQ(expected, actual);
}

// Search with threshold
TEST_P(raptor_search, search_threshold)
{
    auto const [number_of_repeated_bins, window_size, number_of_errors] = GetParam();

    cli_test_result const result = execute_app("raptor", "search",
                                                         "--output search_threshold.out",
                                                         "--threshold 0.50",
                                                         "--index ", ibf_path(number_of_repeated_bins, window_size),
                                                         "--query ", data("query.fq"));
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_EQ(result.out, std::string{});
    EXPECT_EQ(result.err, std::string{});

    std::string const expected = [&] ()
    {
        std::string const bin_list = [&] ()
        {
            std::string result;
            for (size_t i = 0; i < std::max<size_t>(1, number_of_repeated_bins * 4u); ++i)
            {
                result += std::to_string(i);
                result += ',';
            }
            result.pop_back();
            return result;
        }();

        std::string header{};
        std::string line{};
        std::ifstream search_result{search_result_path(number_of_repeated_bins,
                                                       window_size,
                                                       window_size == 23 ? 1 : number_of_errors)};
        while (std::getline(search_result, line) && line.substr(0, 6) != "query1")
        {
            header += line;
            header += '\n';
        }

        return header + "query1\t" + bin_list + "\nquery2\t" + bin_list + "\nquery3\t" + bin_list + '\n';
    }();

    std::string const actual = string_from_file("search_threshold.out");

    EXPECT_EQ(expected, actual);
}

INSTANTIATE_TEST_SUITE_P(search_suite,
                         raptor_search,
                         testing::Combine(testing::Values(0, 16, 32), testing::Values(19, 23), testing::Values(0, 1)),
                         [] (testing::TestParamInfo<raptor_search::ParamType> const & info)
                         {
                             std::string name = std::to_string(std::max<int>(1, std::get<0>(info.param) * 4)) + "_bins_" +
                                                std::to_string(std::get<1>(info.param)) + "_window_" +
                                                std::to_string(std::get<2>(info.param)) + "_error";
                             return name;
                         });

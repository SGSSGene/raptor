// --------------------------------------------------------------------------------------------------
// Copyright (c) 2006-2023, Knut Reinert & Freie Universität Berlin
// Copyright (c) 2016-2023, Knut Reinert & MPI für molekulare Genetik
// This file may be used, modified and/or redistributed under the terms of the 3-clause BSD-License
// shipped with this file and also available at: https://github.com/seqan/raptor/blob/main/LICENSE.md
// --------------------------------------------------------------------------------------------------

#include <cassert>
#include <charconv>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <robin_hood.h>

#include <raptor/argument_parsing/validators.hpp>

#include <Genome_Biology/check_output_file.hpp>
#include <Genome_Biology/parse_user_bin_ids.hpp>

struct config
{
    double percentage{};
    // size_t kmer_size{};
    // size_t number_of_errors{};
    // size_t threshold_grace{};

    std::filesystem::path query_names_file{};
    std::filesystem::path user_bin_ids_file{};
    std::filesystem::path mantis_result_file{};
    std::filesystem::path output_file{};
};

std::vector<std::string> parse_query_names(std::filesystem::path const & query_names_file)
{
    std::string line_buffer{};
    std::vector<std::string> query_names;
    std::ifstream query_names_in{query_names_file};

    std::cerr << "Reading " << query_names_file << " ... " << std::flush;
    // Contains lines: "query_name"
    while (std::getline(query_names_in, line_buffer))
        query_names.push_back(line_buffer);
    std::cerr << "Done" << std::endl;
    return query_names;
}

// ## Threshold:
// Let:
//   * p patternsize
//   * k k-mer size
//   * e errors
//   * c k-mer count
//   * t threshold
// Then:
//   * c = p - k + 1 [Lemma A]
//   * p = c + k - 1 [Eq 1]
//   * t = p - (e + 1) * k + 1 [Lemma B]
//   * t = c + k - 1 - (e + 1) * k + 1 [Eq 1 + Lemma B]
//   * t = c + 1 - 1 + k - k - e * k
//   * t = c - e * k
// However, mantis counts unique kmers, not taking into account multiplicity.
// Hence, we may further substract a constant from the threshold.
class thresholder
{
private:
    // size_t const destroyed_kmers{};
    double const percentage{};

public:
    thresholder() = default;
    thresholder(thresholder const &) = default;
    thresholder(thresholder &&) = default;
    thresholder & operator=(thresholder const &) = default;
    thresholder & operator=(thresholder &&) = default;
    ~thresholder() = default;

    explicit thresholder(config const & cfg) : percentage(cfg.percentage)
    {}

    [[nodiscard]] constexpr size_t get(size_t const kmer_count) const noexcept
    {
        return static_cast<size_t>(percentage * kmer_count);
    }

    // explicit thresholder(config const & cfg) :
    //     destroyed_kmers(cfg.number_of_errors * cfg.kmer_size + cfg.threshold_grace)
    // {}

    // [[nodiscard]] constexpr size_t get(size_t const kmer_count) const noexcept
    // {
    //     return (kmer_count > destroyed_kmers) ? (kmer_count - destroyed_kmers) : 0u;
    // }
};

void normalise_output(config const & cfg)
{
    // All query names
    std::vector<std::string> const query_names{parse_query_names(cfg.query_names_file)};
    // map[reference_name] = number
    std::cerr << "Reading " << cfg.user_bin_ids_file << " ... " << std::flush;
    robin_hood::unordered_map<std::string, uint64_t> const ub_name_to_id{parse_user_bin_ids(cfg.user_bin_ids_file)};
    std::cerr << "Done" << std::endl;

    // Process mantis results
    std::ifstream mantis_result_in{cfg.mantis_result_file};
    std::ofstream mantis_result_out{cfg.output_file};
    thresholder const threshold{cfg}; // Helper for computing the threshold.
    size_t mantis_threshold{};        // Needs to be set for each query.
    size_t current_query_number{};
    std::vector<uint64_t> results;

    // Buffers for file I/O
    std::string ub_name_buffer{};
    std::string result_buffer{};
    std::string line_buffer{};

    // ## Mantis results ##
    // There is no query ID, instead they are enumerated: seq0 - seqX
    // For each read:
    //   * seqX <tab> kmers in query
    //   * For each user bin that has kmers, the unique kmer count is reported:
    //     * absolute_path/to/ub.squeakr <tab> count
    // E.g.:
    // seq0    219
    // [...]/GCF_016028855.1_ASM1602885v1_genomic.squeakr       173
    // [...]/GCF_016128175.1_ASM1612817v1_genomic.squeakr       205
    // [...]/GCF_020162095.1_ASM2016209v1_genomic.squeakr       1
    // seq1    219
    auto parse_user_bin_id = [&ub_name_buffer, &ub_name_to_id](std::string const & line)
    {
        ub_name_buffer.assign(line.begin() + line.find_last_of('/') + 1, // Skip absolute path
                              line.begin() + line.find_last_of('.'));    // Skip .squeakr extension
        return ub_name_to_id.at(ub_name_buffer);
    };

    auto parse_kmer_count = [](std::string const & line)
    {
        size_t result{};
        std::string_view const sv{line.begin() + line.find('\t') + 1, // Skip seqX
                                  line.end()};
        std::from_chars(sv.data(), sv.data() + sv.size(), result);
        return result;
    };

    auto process_results = [&results, &result_buffer, &mantis_result_out]()
    {
        if (!results.empty())
        {
            std::sort(results.begin(), results.end());
            for (size_t const ub : results)
                result_buffer += std::to_string(ub) + ',';
            result_buffer.back() = '\n';

            mantis_result_out << result_buffer;
            result_buffer.clear();
            results.clear();
        }
        else
        {
            mantis_result_out << '\n';
        }
    };

    std::cerr << "Processing " << cfg.mantis_result_file << " ... " << std::flush;

    // First line.
    if (std::getline(mantis_result_in, line_buffer))
    {
        assert(line_buffer.starts_with("seq"));
        assert(current_query_number < query_names.size());
        mantis_result_out << query_names[current_query_number++] << '\t';
        mantis_threshold = threshold.get(parse_kmer_count(line_buffer));
    }

    while (std::getline(mantis_result_in, line_buffer))
    {
        if (line_buffer.starts_with("seq")) // new query
        {
            // Process the results of the previous query.
            process_results();

            // Output new query name.
            assert(current_query_number < query_names.size());
            mantis_result_out << query_names[current_query_number++] << '\t';

            // Compute threshold for current query.
            mantis_threshold = threshold.get(parse_kmer_count(line_buffer));
        }
        else
        {
            if (parse_kmer_count(line_buffer) >= mantis_threshold)
                results.push_back(parse_user_bin_id(line_buffer));
        }
    }

    // Write last results.
    process_results();

    std::cerr << "Done" << std::endl;
}

void init_parser(sharg::parser & parser, config & cfg)
{
    parser.add_option(cfg.query_names_file,
                      sharg::config{.short_id = '\0',
                                    .long_id = "query_names",
                                    .description = "The file containing query names, e.g., \"query.names\".",
                                    .required = true,
                                    .validator = sharg::input_file_validator{}});
    parser.add_option(cfg.user_bin_ids_file,
                      sharg::config{.short_id = '\0',
                                    .long_id = "user_bin_ids",
                                    .description = "The file containing user bin ids, e.g., \"user_bin.ids\".",
                                    .required = true,
                                    .validator = sharg::input_file_validator{}});
    parser.add_option(cfg.mantis_result_file,
                      sharg::config{.short_id = '\0',
                                    .long_id = "mantis_results",
                                    .description = "The mantis result file, e.g., \"mantis.results\".",
                                    .required = true,
                                    .validator = sharg::input_file_validator{}});
    parser.add_option(cfg.output_file,
                      sharg::config{.short_id = '\0',
                                    .long_id = "output_file",
                                    .description = "Provide a path to the output.",
                                    .required = true});
    parser.add_option(
        cfg.percentage,
        sharg::config{.short_id = '\0',
                      .long_id = "percentage",
                      .description = "Percentage of kmers in a query that need to be found to qualify as a hit.",
                      .required = true,
                      .validator = sharg::arithmetic_range_validator{0, 1}});

    // parser.add_option(cfg.kmer_size, sharg::config{
    //                   .short_id='\0',
    //                   .long_id="kmer_size",
    //                   .description="The k-mer size.",
    //                   .required=true,
    //                   .validator=sharg::arithmetic_range_validator{1, 32}});
    // parser.add_option(cfg.number_of_errors, sharg::config{
    //                   .short_id='\0',
    //                   .long_id="errors",
    //                   .description="The number of errors.",
    //                   .required=true,
    //                   .validator=raptor::positive_integer_validator{true}});
    // parser.add_option(cfg.threshold_grace, sharg::config{
    //                   .short_id='\0',
    //                   .long_id="threshold_grace",
    //                   .description="Reduce kmer threshold by this much.",
    //                   .required=true,
    //                   .validator=raptor::positive_integer_validator{true}});
}

int main(int argc, char ** argv)
{
    sharg::parser parser{"normalise_mantis_output", argc, argv, sharg::update_notifications::off};
    parser.info.author = "Svenja Mehringer, Enrico Seiler";
    parser.info.email = "enrico.seiler@fu-berlin.de";
    parser.info.short_description = "Converts mantis results into raptor-like results.";
    parser.info.version = "0.0.1";

    config cfg{};
    init_parser(parser, cfg);

    try
    {
        parser.parse();
        check_output_file(cfg.output_file);
    }
    catch (sharg::parser_error const & ext)
    {
        std::cerr << "[Error] " << ext.what() << '\n';
        std::exit(-1);
    }

    normalise_output(cfg);
}

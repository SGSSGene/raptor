#include <iostream>
#include <random>

#include <robin_hood.h>

#include <seqan3/search/dream_index/interleaved_bloom_filter.hpp>

#include <raptor/argument_parsing/validators.hpp>

struct config
{
    size_t kmer_size{20};
    size_t elements{130};
    size_t splits{5};
    size_t hash{2};
    double fpr{0.05};
};

void init_parser(sharg::parser & parser, config & cfg)
{
    parser.add_option(cfg.kmer_size,
                      sharg::config{.short_id = '\0',
                                    .long_id = "kmer",
                                    .description = "The k-mer size.",
                                    .validator = sharg::arithmetic_range_validator{1, 32}});
    parser.add_option(cfg.elements,
                      sharg::config{.short_id = '\0',
                                    .long_id = "elements",
                                    .description = "Number of elements to insert.",
                                    .validator = raptor::positive_integer_validator{}});
    parser.add_option(cfg.splits,
                      sharg::config{.short_id = '\0',
                                    .long_id = "splits",
                                    .description = "Number of bins to split into.",
                                    .validator = raptor::positive_integer_validator{}});
    parser.add_option(cfg.hash,
                      sharg::config{.short_id = '\0',
                                    .long_id = "hash",
                                    .description = "The number of hash functions to use.",
                                    .validator = sharg::arithmetic_range_validator{1, 5}});
    parser.add_option(cfg.fpr,
                      sharg::config{.short_id = '\0',
                                    .long_id = "fpr",
                                    .description = "The desired false positive rate.",
                                    .validator = sharg::arithmetic_range_validator{0.0, 1.0}});
}

size_t bin_size_in_bits(config const & cfg, size_t const elements)
{
    double const numerator{-static_cast<double>(elements * cfg.hash)};
    double const denominator{std::log(1.0 - std::exp(std::log(cfg.fpr) / cfg.hash))};
    double const result{std::ceil(numerator / denominator)};
    return result;
}

size_t bin_size_in_bits(config const & cfg)
{
    return bin_size_in_bits(cfg, cfg.elements);
}

// New
double compute_fp_correction(config const & cfg)
{
    // std::log1p(arg) = std::log(1 + arg). More precise than std::log(1 + arg) if arg is close to zero.
    double const numerator = std::log1p(-std::exp(std::log(cfg.fpr) / cfg.hash));
    double const log_target_fpr = std::log1p(-std::exp(std::log1p(-cfg.fpr) / cfg.splits));
    return numerator / std::log1p(-std::exp(log_target_fpr / cfg.hash));
}

// Old
// double compute_fp_correction(config const & cfg)
// {
//     double const denominator = std::log(1.0 - std::exp(std::log(cfg.fpr) / cfg.hash));
//     double const tmp = 1.0 - std::pow(1.0 - cfg.fpr, cfg.splits);
//     return std::log(1 - std::exp(std::log(tmp) / cfg.hash)) / denominator;
// }

void print_results(size_t const fp_count, size_t const all_kmers, size_t const elements)
{
    std::cout << "fp_count: " << fp_count << '\n'
              << "fp_rate: " << std::fixed << std::setprecision(3)
              << static_cast<double>(fp_count) / (all_kmers - elements) << '\n';
}

void single_tb(config const & cfg)
{
    size_t const all_kmers{1ULL << cfg.kmer_size};
    seqan3::interleaved_bloom_filter ibf{seqan3::bin_count{1u}, seqan3::bin_size{bin_size_in_bits(cfg)}};
    auto agent = ibf.membership_agent();
    robin_hood::unordered_set<uint64_t> inserted_values{};
    size_t fp_count{};
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> distrib(0ULL, all_kmers);

    // Generate elements many random kmer values.
    for (; inserted_values.size() < cfg.elements;)
        inserted_values.emplace(distrib(gen));

    for (uint64_t const & value : inserted_values)
        ibf.emplace(value, seqan3::bin_index{0u});

    // Check all possible kmer values.
    for (uint64_t value{}; value < all_kmers; ++value)
    {
        auto & result = agent.bulk_contains(value);
        if (result[0] && !inserted_values.count(value))
            ++fp_count;
    }

    print_results(fp_count, all_kmers, cfg.elements);
}

void multiple_tb(config const & cfg, size_t const bin_size)
{
    size_t const all_kmers{1ULL << cfg.kmer_size};
    size_t const elements_per_bin{(cfg.elements + cfg.splits - 1) / cfg.splits}; // ceil for positive integers
    seqan3::interleaved_bloom_filter ibf{seqan3::bin_count{cfg.splits}, seqan3::bin_size{bin_size}};
    auto agent = ibf.membership_agent();
    robin_hood::unordered_set<uint64_t> all_values{};
    std::vector<robin_hood::unordered_set<uint64_t>> inserted_values(cfg.splits, robin_hood::unordered_set<uint64_t>{});
    size_t fp_count{};
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> distrib(0ULL, all_kmers);

    // Generate elements many random kmer values.
    for (; all_values.size() < cfg.elements;)
        all_values.emplace(distrib(gen));

    // Distribute across all bins.
    size_t counter{};
    for (uint64_t const value : all_values)
        inserted_values[counter++ / elements_per_bin].emplace(value);

#ifndef NDEBUG
    for (auto & values : inserted_values)
        assert(values.size() == cfg.elements);
#endif // NDEBUG

    for (size_t i{}; i < cfg.splits; ++i)
        for (uint64_t const value : inserted_values[i])
            ibf.emplace(value, seqan3::bin_index{i});

    // Check all possible kmer values.
    for (uint64_t value{}; value < all_kmers; ++value)
    {
        auto & result = agent.bulk_contains(value);
        if (!all_values.count(value))
        {
            for (size_t i{}; i < cfg.splits; ++i)
                if (result[i])
                {
                    ++fp_count;
                    break;
                }
        }
        else
        {
            size_t containing_bin{};
            for (size_t i{}; i < cfg.splits; ++i)
            {
                if (inserted_values[i].count(value))
                {
                    containing_bin = i;
                    break;
                }
            }
            for (size_t i{}; i < cfg.splits; ++i)
                if (i != containing_bin && result[i])
                {
                    ++fp_count;
                    break;
                }
        }
    }

    print_results(fp_count, all_kmers, cfg.elements);
}

int main(int argc, char ** argv)
{
    sharg::parser parser{"ibf_fpr", argc, argv, sharg::update_notifications::off};
    config cfg{};
    init_parser(parser, cfg);
    parser.parse();

    std::cout << "=== Single bin ===\n";
    single_tb(cfg);

    size_t const elements_per_bin{(cfg.elements + cfg.splits - 1) / cfg.splits}; // ceil for positive integers

    std::cout << "=== Split bin ===\n";
    multiple_tb(cfg, bin_size_in_bits(cfg, elements_per_bin));

    std::cout << "=== Split bin corrected ===\n";
    multiple_tb(cfg, bin_size_in_bits(cfg, elements_per_bin) * compute_fp_correction(cfg));
}

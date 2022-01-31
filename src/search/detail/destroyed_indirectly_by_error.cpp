// -----------------------------------------------------------------------------------------------------
// Copyright (c) 2006-2022, Knut Reinert & Freie Universität Berlin
// Copyright (c) 2016-2022, Knut Reinert & MPI für molekulare Genetik
// This file may be used, modified and/or redistributed under the terms of the 3-clause BSD-License
// shipped with this file and also available at: https://github.com/seqan/raptor/blob/master/LICENSE.md
// -----------------------------------------------------------------------------------------------------

#include <random>

#include <seqan3/alphabet/nucleotide/dna4.hpp>

#include <raptor/search/detail/destroyed_indirectly_by_error.hpp>
#include <raptor/search/detail/forward_strand_minimiser.hpp>

namespace raptor::detail
{

std::vector<double> destroyed_indirectly_by_error(size_t const pattern_size,
                                                  size_t const window_size,
                                                  seqan3::shape const shape)
{
    uint8_t const kmer_size{shape.size()};
    size_t const max_number_of_minimiser{pattern_size - window_size + 1};
    size_t const iterations{10'000};

    std::mt19937_64 gen{0x1D2B8284D988C4D0};
    std::uniform_int_distribution<size_t> random_error_position{0u, pattern_size - 1u};
    std::uniform_int_distribution<uint8_t> random_dna4_rank{0u, 3u};
    auto random_dna = [&random_dna4_rank, &gen] ()
    {
        return seqan3::assign_rank_to(random_dna4_rank(gen), seqan3::dna4{});
    };

    std::vector<seqan3::dna4> sequence(pattern_size);

    // Minimiser begin positions of original sequence
    std::vector<uint8_t> minimiser_positions(max_number_of_minimiser, false);
    // Minimiser begin positions after introducing one error into the sequence
    std::vector<uint8_t> minimiser_positions_error(max_number_of_minimiser, false);
    // One error affects at most all minimisers, there are pattern_size - window_size + 1 many
    std::vector<double> result(max_number_of_minimiser, 0.0);
    forward_strand_minimiser fwd_minimiser{window{static_cast<uint32_t>(window_size)}, shape};

    for (size_t iteration = 0; iteration < iterations; ++iteration)
    {
        std::ranges::fill(minimiser_positions, 0u);
        std::ranges::fill(minimiser_positions_error, 0u);
        std::ranges::generate(sequence, random_dna);

        // Minimiser begin positions of original sequence
        fwd_minimiser.compute(sequence);
        for (auto pos : fwd_minimiser.minimiser_begin)
            minimiser_positions[pos] = true;

        // Introduce one error
        size_t const error_position = random_error_position(gen);
        uint8_t new_rank{random_dna4_rank(gen)};
        while (new_rank == seqan3::to_rank(sequence[error_position]))
            new_rank = random_dna4_rank(gen);
        sequence[error_position] = seqan3::assign_rank_to(new_rank, seqan3::dna4{});

        // Minimiser begin positions after introducing one error into the sequence
        fwd_minimiser.compute(sequence);
        for (auto pos : fwd_minimiser.minimiser_begin)
            minimiser_positions_error[pos] = true;

        // Determine number of affected minimisers
        size_t affected_minimiser{};
        // An error destroyed a minimiser indirectly iff
        // (1) A minimiser begin position changed and
        // (2) The error occurs before the window or after the window
        for (size_t i = 0; i < max_number_of_minimiser; ++i)
        {
            affected_minimiser += (minimiser_positions[i] != minimiser_positions_error[i]) && // (1)
                                  ((error_position < i) || (i + kmer_size < error_position)); // (2)
        }

        ++result[affected_minimiser];
    }

    // Convert counts to a distribution
    for (auto & x : result)
        x /= iterations;

    return result;
}

} // namespace raptor::detail

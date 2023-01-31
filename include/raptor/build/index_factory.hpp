// --------------------------------------------------------------------------------------------------
// Copyright (c) 2006-2023, Knut Reinert & Freie Universität Berlin
// Copyright (c) 2016-2023, Knut Reinert & MPI für molekulare Genetik
// This file may be used, modified and/or redistributed under the terms of the 3-clause BSD-License
// shipped with this file and also available at: https://github.com/seqan/raptor/blob/main/LICENSE.md
// --------------------------------------------------------------------------------------------------

#pragma once

#include <seqan3/search/views/minimiser_hash.hpp>

#include <raptor/adjust_seed.hpp>
#include <raptor/build/partition_config.hpp>
#include <raptor/call_parallel_on_bins.hpp>
#include <raptor/dna4_traits.hpp>
#include <raptor/file_reader.hpp>
#include <raptor/index.hpp>

namespace raptor
{

class index_factory
{
public:
    index_factory() = default;
    index_factory(index_factory const &) = default;
    index_factory(index_factory &&) = default;
    index_factory & operator=(index_factory const &) = default;
    index_factory & operator=(index_factory &&) = default;
    ~index_factory() = default;

    explicit index_factory(build_arguments const & args) : arguments{std::addressof(args)}
    {
        if (arguments->input_is_minimiser)
            reader = file_reader<file_types::minimiser>{};
        else
            reader = file_reader<file_types::sequence>{arguments->shape, arguments->window_size};
    }

    explicit index_factory(build_arguments const & args, partition_config const & cfg) :
        arguments{std::addressof(args)},
        config{std::addressof(cfg)}
    {
        if (arguments->input_is_minimiser)
            reader = file_reader<file_types::minimiser>{};
        else
            reader = file_reader<file_types::sequence>{arguments->shape, arguments->window_size};
    }

    [[nodiscard]] raptor_index<> operator()(size_t const part = 0u) const
    {
        return construct(part);
    }

private:
    build_arguments const * const arguments{nullptr};
    partition_config const * const config{nullptr};
    std::variant<file_reader<file_types::sequence>, file_reader<file_types::minimiser>> reader;

    raptor_index<> construct(size_t const part) const
    {
        assert(arguments != nullptr);

        raptor_index<> index{*arguments};

        auto worker = [&](auto && zipped_view, auto &&)
        {
            std::vector<uint64_t> hashes{};
            auto & ibf = index.ibf();

            for (auto && [file_names, bin_number] : zipped_view)
            {
                hashes.clear();
                std::visit(
                    [&](auto const & reader)
                    {
                        if (config == nullptr)
                            reader.hash_into(file_names, std::back_inserter(hashes));
                        else
                            reader.hash_into_if(file_names,
                                                std::back_inserter(hashes),
                                                [&](uint64_t const hash)
                                                {
                                                    return config->hash_partition(hash) == part;
                                                });
                    },
                    reader);

                for (auto && value : hashes)
                    ibf.emplace(value, seqan3::bin_index{bin_number});
            }
        };

        call_parallel_on_bins(worker, arguments->bin_path, arguments->threads);

        return index;
    }
};

} // namespace raptor

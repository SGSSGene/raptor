// --------------------------------------------------------------------------------------------------
// Copyright (c) 2006-2023, Knut Reinert & Freie Universität Berlin
// Copyright (c) 2016-2023, Knut Reinert & MPI für molekulare Genetik
// This file may be used, modified and/or redistributed under the terms of the 3-clause BSD-License
// shipped with this file and also available at: https://github.com/seqan/raptor/blob/main/LICENSE.md
// --------------------------------------------------------------------------------------------------

/*!\file
 * \brief Implements raptor::search_ibf.
 * \author Enrico Seiler <enrico.seiler AT fu-berlin.de>
 */

#include <raptor/search/search_ibf.hpp>
#include <raptor/search/search_singular_ibf.hpp>

namespace raptor
{

template <bool compressed>
void search_ibf(search_arguments const & arguments)
{
    using index_structure_t = std::conditional_t<compressed, index_structure::ibf_compressed, index_structure::ibf>;
    auto index = raptor_index<index_structure_t>{};
    search_singular_ibf(arguments, std::move(index));
}

template void search_ibf<false>(search_arguments const & arguments);

template void search_ibf<true>(search_arguments const & arguments);

} // namespace raptor

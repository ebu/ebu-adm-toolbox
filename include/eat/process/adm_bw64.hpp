#pragma once

#include <adm/elements_fwd.hpp>

#include "chna.hpp"
#include "eat/framework/process.hpp"
#include "eat/framework/value_ptr.hpp"

namespace eat::process {

/// stores ADM information associated with some stream of audio
///
/// CHNA information is merged into document, with the channel number for each
/// audioChannelUID stored in channel_map
struct ADMData {
  framework::ValuePtr<adm::Document> document;
  channel_map_t channel_map;
};

/// read samples from a BW64 file
///
/// ports:
/// - out_samples (StreamPort<InterleavedBlockPtr>) : output samples
///
/// @param path path to the file to read
/// @param block_size maximum number of samples in each output block
framework::ProcessPtr make_read_bw64(const std::string &name, const std::string &path, size_t block_size);

/// write samples to a BW64 file
///
/// ports:
/// - in_samples (StreamPort<InterleavedBlockPtr>) : input samples
///
/// @param path path to the file to read
framework::ProcessPtr make_write_bw64(const std::string &name, const std::string &path);

/// read ADM data from a BW64 ADM file
///
/// ports:
/// - out_axml (DataPort<ADMData>) : output ADM data
///
/// @param path path to the file to read
framework::ProcessPtr make_read_adm(const std::string &name, const std::string &path);

/// read samples and ADM data from a BW64 file
///
/// ports:
/// - out_axml (DataPort<ADMData>) : output ADM data
/// - out_samples (StreamPort<InterleavedBlockPtr>) : output samples
///
/// @param path path to the file to read
/// @param block_size maximum number of samples in each output block
framework::ProcessPtr make_read_adm_bw64(const std::string &name, const std::string &path, size_t block_size);

/// write samples and ADM data to a BW64 file
///
/// ports:
/// - in_samples (StreamPort<InterleavedBlockPtr>) : input samples
///
/// @param path path to the file to read
framework::ProcessPtr make_write_adm_bw64(const std::string &name, const std::string &path);

}  // namespace eat::process

namespace eat::framework {
template <>
std::shared_ptr<adm::Document> copy_shared_ptr<adm::Document>(const std::shared_ptr<adm::Document> &value);

}  // namespace eat::framework

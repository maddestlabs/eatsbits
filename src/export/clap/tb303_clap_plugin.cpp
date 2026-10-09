#include "eatsbits/export/clap/tb303_clap_adapter.hpp"

/**
 * Eatsbits TB-303 Reference CLAP Plugin DSO Entry Point.
 *
 * Exports official C-ABI:
 * - clap_entry (clap_plugin_entry_t)
 * - clap.plugin-factory (clap_plugin_factory_t)
 */
EATS_CLAP_EXPORT_PLUGIN(::eatsbits::export_clap::Tb303ClapAdapter)

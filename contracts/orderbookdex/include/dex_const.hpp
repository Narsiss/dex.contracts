#pragma once

#include <cstdint>
#include <eosio/name.hpp>
#include <eosio/asset.hpp>

constexpr int64_t PRICE_PRECISION           = 100'000'000; // 10^8, the price precision
constexpr int64_t RATIO_PRECISION           = 10000;     // 10^4, the ratio precision
constexpr int64_t FEE_RATIO_MAX             = 4999;      // 49.99%, max fee ratio
constexpr int64_t DEX_MAKER_FEE_RATIO       = 4;         // 0.04%, dex maker fee ratio
constexpr int64_t DEX_TAKER_FEE_RATIO       = 8;         // 0.04%, dex taker fee ratio
constexpr uint32_t DEX_MATCH_COUNT_MAX      = 50;         // the max dex match count.
constexpr uint64_t DATA_RECYCLE_SEC         = 90 * 3600 * 24; // recycle time: 90 days, in seconds

constexpr int64_t MEMO_LEN_MAX              = 255;        // 0.001%, max memo length
constexpr int64_t URL_LEN_MAX               = 255;        // 0.001%, max url length

using namespace eosio;

#define SYMBOL(sym_code, precision) symbol(symbol_code(sym_code), precision)

static constexpr name   SYS_ACCOUNT     = name("amax");
static constexpr name   SYS_BANK        = name("amax.token");
static constexpr symbol SYS_TOKEN       = SYMBOL("AMAX",8);

static constexpr name   MIRROR_BANK     = name("amax.mtoken");
static constexpr symbol MIRROR_USDT     = SYMBOL("MUSDT",6);
static constexpr symbol MIRROR_BTC      = SYMBOL("MUSDT",6);
static constexpr symbol MIRROR_ETH      = SYMBOL("METH",6);
static constexpr symbol MIRROR_BNB      = SYMBOL("MBNB",8);

static constexpr name   APL_FARM        = "aplinkfarm"_n;
static constexpr name   APL_BANK        = "aplink.token"_n;
static constexpr symbol APL_SYMBOL      = SYMBOL("APL",4);


#define DEX_CONTRACT_PROP eosio::contract("orderbookdex")
#define DEX_CONTRACT [[DEX_CONTRACT_PROP]]
#define DEX_TABLE [[eosio::table, DEX_CONTRACT_PROP]]
#define DEX_TABLE_NAME(name) [[eosio::table(name), DEX_CONTRACT_PROP]]

#define CHECKC(exp, code, msg) \
   { if (!(exp)) eosio::check(false, std::string("$$$") + std::to_string((int)code) + std::string("$$$ ") + msg); }


#define CHECK_DEX_ENABLED() { \
    CHECKC(_config.dex_enabled, err::STATUS_ERROR, string("DEX is disabled! function=") + __func__) \
}
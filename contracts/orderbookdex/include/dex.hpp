#pragma once

#include <eosio/eosio.hpp>

#include "dex_const.hpp"
#include "dex_states.hpp"
#include "dex_match.hpp"

using namespace std;
using namespace eosio;

class [[eosio::contract("orderbookdex")]] dex_contract : public contract {
public:
    using contract::contract;

public:
    dex_contract(name receiver, name code, datastream<const char *> ds)
        : contract(receiver, code, ds), _conf_tbl(get_self(), get_self().value),
          _global(dex::global_state::make_global(get_self())) {
        _config = _conf_tbl.exists() ? _conf_tbl.get() : get_default_config();
    }

    ~dex_contract() {
        _global->save(get_self());
    }

    ACTION init();
    
    ACTION setconfig(const dex::config &conf);

    ACTION setsympair(  const extended_symbol &asset_symbol,
                        const extended_symbol &coin_symbol,
                        const asset &min_asset_quant, const asset &min_coin_quant,
                        bool only_accept_coin_fee, bool enabled);

    ACTION onoffsympair(const uint64_t& sympair_id, const bool& on_off);

    [[eosio::on_notify("*::transfer")]] 
    void ontransfer(const name& from, const name& to, const asset& quant, const string& memo);

    ACTION withdraw(const name& user, const name &bank, const asset& quant, const string& memo);

    /**
     * new order, should deposit by transfer first
     * @param user - user, owner of order
     * @param sympair_id - symbol pair id
     * @param order_side - order side, BUY | SELL
     * @param limit_quant - the limit quantity
     * @param frozen_quant - the frozen quantity, unused
     * @param price - the price
     * @param external_id - external id, always set by application
     * @param order_config_ex - optional extended config, must authenticate by admin if set
     */
    ACTION neworder(const name &user, const uint64_t &sympair_id,
            const name &order_side,
             const asset &limit_quant, const asset &frozen_quant,
             const asset &price, const uint64_t &external_id,
             const optional<dex::order_config_ex_t> &order_config_ex);

    ACTION buy( const name &user, const uint64_t &sympair_id,
                const asset &quantity, const asset &price, const uint64_t &external_id,
                const optional<dex::order_config_ex_t> &order_config_ex);

    ACTION sell(const name &user, const uint64_t &sympair_id,
                const asset &quantity, const asset &price,
                const uint64_t &external_id,
                const optional<dex::order_config_ex_t> &order_config_ex);

    /**
     *  @param max_count the max count of match item
     *  @param sym_pairs the symol pairs to match. is empty, match all
     */
    ACTION match(const name &matcher, uint32_t max_count, const string &memo);

    ACTION cancel(const uint64_t& pair_id, const name& side, const uint64_t &order_id);

    ACTION cleandata(const uint64_t &max_count);

    // using withdraw_action   = action_wrapper<"withdraw"_n, &dex_contract::withdraw>;
    using neworder_action   = action_wrapper<"neworder"_n,  &dex_contract::neworder>;
    using buy_action        = action_wrapper<"buy"_n,       &dex_contract::buy>;
    using sell_action       = action_wrapper<"sell"_n,      &dex_contract::sell>;
    using match_action      = action_wrapper<"match"_n,     &dex_contract::match>;
    using cancel_action     = action_wrapper<"cancel"_n,    &dex_contract::cancel>;

public:
    std::string to_hex(const char* d, uint32_t s){
        std::string r;
        const char* to_hex="0123456789abcdef";
        uint8_t* c = (uint8_t*)d;
        for( uint32_t i = 0; i < s; ++i )
            (r += to_hex[(c[i]>>4)]) += to_hex[(c[i] &0x0f)];
        return r;
    }

    std::string to_hex( const std::vector<char>& data ){
       if( data.size() )
          return to_hex( data.data(), data.size() );

       return "";
    }

    std::string to_hex( const std::array<uint8_t, 32> & bytes ){
        std::vector<char> chars(bytes.begin(), bytes.end());
        return to_hex(chars);
    } 

    inline std::string str_to_upper( string_view str ) {
        std::string ret(str.size(), 0);
        for (size_t i = 0; i < str.size(); i++) {
            ret[i] = std::toupper(str[i]);
        }
        return ret;
    }

    inline uint64_t parse_uint64( string_view str ) {
        safe<uint64_t> ret;
        to_int(str, ret);
        return ret.value;
    }

private:
    dex::config get_default_config();

    void _allot_fee( const name &from_user, const name& bank, const asset& fee, const uint64_t order_id );

    void match_sympair(const name &matcher, const dex::symbol_pair_t &sym_pair, uint32_t max_count,
                        uint32_t &matched_count, const string &memo);
    void update_latest_deal_price(const uint64_t& sympair_id, const asset& latest_deal_price);

    void new_order(const name &user, const uint64_t &sympair_id,
            const name &order_side,
            const asset &limit_quant,
            const optional<asset> &price,
            const uint64_t &external_id,
            const optional<dex::order_config_ex_t> &order_config_ex);

    void add_balance(const name &user, const name &bank, const asset &quantity, const name &type, const string& memo);

    bool check_data_outdated(const time_point &data_time, const time_point &now);

    bool check_dex_enabled();

    //Send deal action
    void _send_deal_action( const dex::deal_item_t& deal_item );

    dex::config_table _conf_tbl;
    dex::config _config;
    dex::global_state::ptr_t _global;
};

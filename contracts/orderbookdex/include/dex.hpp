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

                                
    ACTION setsympair(  const name&                 sympair_code, 
                        const extended_symbol&      asset_symbol,
                        const extended_symbol&      coin_symbol,
                        const asset&                min_asset_quant,
                        const asset&                min_coin_quant,
                        bool                        enabled,
                        const uint64_t&             taker_fee_ratio,
                        const uint64_t&             maker_fee_ratio,
                        const uint64_t              asset_precision,          //asset precision
                        const uint64_t              coin_precision,         //coin precision
                        const uint64_t              price_precision,        //price precision
                        const uint64_t              deal_precision         //成交精度
                        );

    ACTION onoffsympair(const name& sympair_code, const bool& on_off);

    ACTION delsympair(const name& sympair_code);

    [[eosio::on_notify("*::transfer")]] 
    void ontransfer(const name& from, const name& to, const asset& quant, const string& memo);

    ACTION withdraw(const name& user, const name &bank, const asset& quant, const string& memo);

    /**
     * create a new order
     * @param user - user, owner of order
     * @param sympair_code - symbol pair id
     * @param order_side - order side, BUY | SELL
     * @param total_asset_quant - the limit quantity
     * @param price - the price
     * @param ext_id - external id, always set by application
     * @param order_config_ex - optional extended config, must authenticate by admin if set
     */
    ACTION neworder(const name &user, const name &sympair_code,
            const name &order_side,
             const asset &total_asset_quant,
             const asset &price, const uint64_t &ext_id,
             const optional<dex::order_config_ex_t> &order_config_ex);


    /**
     * create buy new order
     * @param user - user, owner of order
     * @param sympair_code - symbol pair id
     * @param quantity - quantity
     * @param price - the price
     * @param ext_id - external id, always set by application
     */
    ACTION limitbuy( const name &user, const name &sympair_code,
                const asset &quantity, const asset &price,
                const uint64_t &ext_id);

    

    ACTION limitsell(const name &user, const name &sympair_code,
                const asset &quantity, const asset &price,
                const uint64_t &ext_id);

    ACTION marketbuy(const name &user, const name &sympair_code,
                const asset &quantity,
                const uint64_t &ext_id);

    ACTION marketsell(const name &user, const name &sympair_code,
                const asset &quantity,
                const uint64_t &ext_id);
    /**
     *  @param max_count the max count of match item
     *  @param sym_pairs the symol pairs to match. is empty, match all
     */
    ACTION match(const name &matcher, const name& pair_code, uint32_t max_count, const string &memo);

    /**
     * cancel order where order not finished
     * 
    */
    ACTION cancel(const name& pair_code, const name& type, const name& side, const uint64_t &order_id);


    /**
     * delete queue order
    */
    ACTION delqueueord(const name& user);

    /**
     * internal action for order matched.
     * deal items
     * 
    */
    ACTION adddexdeal(const std::list<dex::deal_item_t>& deal_items, const time_point_sec& curr_ts );

    ACTION orderchange( const uint64_t order_id, const dex::order_t& order);


    ACTION addaplconf( const extended_symbol &asset_symbol, const asset apl_amount);

    ACTION delaplconf( const extended_symbol &asset_symbol  );

    // using withdraw_action   = action_wrapper<"withdraw"_n, &dex_contract::withdraw>;
    using neworder_action           = action_wrapper<"neworder"_n,      &dex_contract::neworder>;
    using limit_buy_action          = action_wrapper<"limitbuy"_n,      &dex_contract::limitbuy>;
    using limit_sell_action         = action_wrapper<"limitsell"_n,     &dex_contract::limitsell>;
    using market_buy_action         = action_wrapper<"marketbuy"_n,     &dex_contract::marketbuy>;
    using market_sell_action        = action_wrapper<"marketsell"_n,    &dex_contract::marketsell>;
    using match_action              = action_wrapper<"match"_n,         &dex_contract::match>;
    using cancel_action             = action_wrapper<"cancel"_n,        &dex_contract::cancel>;

    using deal_action               = action_wrapper<"adddexdeal"_n,    &dex_contract::adddexdeal>;
    using orderchange_action        = action_wrapper<"orderchange"_n,   &dex_contract::orderchange>;


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

    void market_match_sympair(const name &matcher, const dex::symbol_pair_t &sym_pair,
                                  uint32_t max_count, uint32_t &matched_count, const string &memo);

    void update_latest_deal_price(const name& sympair_code, const asset& latest_deal_price);

    void new_order(const name &user, const name &sympair_code,
            const name &order_side, 
            const name &order_type,
            const asset &total_asset_quant,
            const optional<asset> &price,
            const uint64_t &ext_id,
            const optional<dex::order_config_ex_t> &order_config_ex);

    void add_balance(const name &user, const name &bank, const asset &quantity, const name &type, const string& memo);


    bool check_dex_enabled();

    dex::config_table _conf_tbl;
    dex::config _config;
    dex::global_state::ptr_t _global;
};

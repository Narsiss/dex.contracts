#pragma once

#include <map>
#include <set>
#include <eosio/eosio.hpp>
#include <eosio/name.hpp>
#include <eosio/asset.hpp>
#include <eosio/singleton.hpp>
#include "dex_const.hpp"
#include "dex_states.hpp"
#include "utils.hpp"


namespace dex {

    using namespace eosio;

    static constexpr eosio::name active_perm{"active"_n};

    typedef name order_side_t;
    typedef name order_status_t;

    enum class err: uint8_t {
        NONE                 = 0,
        RECORD_NOT_FOUND     = 1,
        RECORD_EXISTING      = 2,
        SYMBOL_MISMATCH      = 4,
        PARAM_ERROR          = 5,
        MEMO_FORMAT_ERROR    = 6,
        PAUSED               = 7,
        NO_AUTH              = 8,
        NOT_POSITIVE         = 9,
        NOT_STARTED          = 10,
        OVERSIZED            = 11,
        TIME_EXPIRED         = 12,
        NOTIFY_UNRELATED     = 13,
        ACTION_REDUNDANT     = 14,
        ACCOUNT_INVALID      = 15,
        FEE_INSUFFICIENT     = 16,
        FIRST_CREATOR        = 17,
        STATUS_ERROR         = 18,
        SCORE_NOT_ENOUGH     = 19,
        NEED_REQUIRED_CHECK  = 20
    };

    namespace balance_type {
        static constexpr name ordercancel   = "ordercancel"_n;
        static constexpr name ordermatched  = "ordermatched"_n;
        static constexpr name orderfee      = "orderfee"_n;
        static constexpr name orderrefund   = "orderrefund"_n;
        static constexpr name parentreward  = "parentreward"_n;
        static constexpr name grandreward   = "grandreward"_n;
    }

    namespace order_side {
        static const order_side_t BUY   = "buy"_n;
        static const order_side_t SELL  = "sell"_n;
        static const order_side_t NONE  = order_side_t();

        // name -> index
        static const std::map<order_side_t, uint8_t> ENUM_MAP = {
            {BUY,   1},
            {SELL,  2}
        };
        inline bool is_valid(const order_side_t &value) {
            return ENUM_MAP.count(value);
        }
        inline uint8_t index(const order_side_t &value) {
            if (value == NONE) return 0;
            auto it = ENUM_MAP.find(value);
            CHECKC(it != ENUM_MAP.end(), err::PARAM_ERROR, "Invalid order_side=" + value.to_string());
            return it->second;
        }
    }

    namespace order_status {
        static const order_status_t NONE        = order_status_t();
        static const order_status_t QUEUE       = "queue"_n;
        static const order_status_t MATCHABLE   = "matchable"_n;
        static const order_status_t COMPLETED   = "completed"_n;
        static const order_status_t CANCELED    = "canceled"_n;
        // name -> index
        static const std::map<order_status_t, uint8_t> ENUM_MAP = {
            {MATCHABLE, 1},
            {COMPLETED, 2},
            {CANCELED,  3}
        };

        inline uint8_t index(const order_status_t &value) {
            if (value == NONE) return 0;
            auto it = ENUM_MAP.find(value);
            CHECKC(it != ENUM_MAP.end(), err::STATUS_ERROR, "Invalid order_status=" + value.to_string());
            return it->second;
        }
    }

    struct order_config_ex_t {
        uint64_t taker_fee_ratio = 0;
        uint64_t maker_fee_ratio = 0;
    };

    struct DEX_TABLE config {
        bool        dex_enabled;           // if false, disable all operation of common user
        name        dex_admin;             // admin of this contract, permisions: manage sym_pairs, authorize order
        name        dex_fee_collector;     // dex_fee_collector of this contract
        int64_t     maker_fee_ratio;
        int64_t     taker_fee_ratio;
        uint32_t    max_match_count;        // the max match count for creating new order,  if 0 will forbid match
        bool        admin_sign_required;   // check the order must have the authorization by dex admin
        int64_t     data_recycle_sec;       // old data: canceled orders, deal items and related completed orders
        int8_t      deferred_matching_secs; // will auto matching after seconds for hudge order

        set<extended_symbol> support_quote_symbols;
        uint64_t    parent_reward_ratio;
        uint64_t    grand_reward_ratio;

        uint64_t    apl_farm_id;
        map<symbol_code, uint32_t> farm_scales;
    };

    typedef eosio::singleton< "config"_n, config > config_table;

    struct DEX_TABLE global {
        uint64_t        order_id    = 0;        // the auto-increament id of order
        uint64_t        sympair_id = 0;         // the auto-increament id of symbol pair
        uint64_t        deal_item_id = 0;       // the auto-increament id of deal item
        set<uint64_t>   matching_sympair;       // deferred send action to match
        bool            matching_sent = false;  // deferred send action to match
    };

    typedef eosio::singleton< "global"_n, global > global_table;

    struct global_state: public global {
    public:
        bool changed = false;

        using ptr_t = std::unique_ptr<global_state>;

        static ptr_t make_global(const name &contract) {
            std::shared_ptr<global_table> global_tbl;
            auto ret = std::make_unique<global_state>();
            ret->_global_tbl = std::make_unique<global_table>(contract, contract.value);

            if (ret->_global_tbl->exists()) {
                static_cast<global&>(*ret) = ret->_global_tbl->get();
            }
            return ret;
        }

        inline uint64_t new_auto_inc_id(uint64_t &id) {
            if (id == 0 || id == std::numeric_limits<uint64_t>::max()) {
                id = 1;
            } else {
                id++;
            }
            change();
            return id;
        }

        inline uint64_t new_order_id() {
            return new_auto_inc_id(order_id);
        }

        inline uint64_t new_sympair_id() {
            return new_auto_inc_id(sympair_id);
        }

        inline uint64_t new_deal_item_id() {
            return new_auto_inc_id(deal_item_id);
        }

        inline void change() {
            changed = true;
        }

        inline void save(const name &payer) {
            if (changed) {
                auto &g = static_cast<global&>(*this);
                _global_tbl->set(g, payer);
                changed = false;
            }
        }
    private:
        std::unique_ptr<global_table> _global_tbl;
    };

    using uint256_t = fixed_bytes<32>;

    static inline uint256_t make_symbols_idx(const extended_symbol &asset_symbol, const extended_symbol &coin_symbol) {
        return uint256_t::make_from_word_sequence<uint64_t>(
                asset_symbol.get_contract().value,
                asset_symbol.get_symbol().code().raw(),
                coin_symbol.get_contract().value,
                coin_symbol.get_symbol().code().raw());
    }

    struct DEX_TABLE symbol_pair_t {
        uint64_t        sympair_id; // PK: auto-increment
        extended_symbol asset_symbol;
        extended_symbol coin_symbol;
        asset           min_asset_quant;
        asset           min_coin_quant;
        asset           latest_deal_price;
        int64_t         taker_fee_ratio;
        int64_t         maker_fee_ratio;
        bool            only_accept_coin_fee;
        bool            enabled;
        uint64_t        farm_lease_id;
        int64_t         farm_ratio;
        int64_t         parent_fee_ratio;
        int64_t         grand_fee_ratio;

        uint64_t primary_key() const { return sympair_id; }
        inline uint256_t get_symbols_idx() const { return make_symbols_idx(asset_symbol, coin_symbol); }

    };

    using symbols_idx = indexed_by<"symbolsidx"_n, const_mem_fun<symbol_pair_t, uint256_t, &symbol_pair_t::get_symbols_idx>>;
    typedef eosio::multi_index<"sympair"_n, symbol_pair_t, symbols_idx> symbol_pair_table;

    inline static symbol_pair_table make_sympair_table(const name &self) {
        return symbol_pair_table(self, self.value/*scope*/);
    }

    using order_match_idx_key = uint64_t;
    inline static order_match_idx_key make_order_match_idx( const order_side_t& side, const uint64_t& price ) {
        uint64_t price_factor = (side == order_side::BUY) ? std::numeric_limits<uint64_t>::max() - price : price;
        return price_factor;
    }

    uint128_t make_uint128(uint64_t high_val, uint64_t low_val) {
        return uint128_t(high_val) << 64 | uint128_t(low_val);
    }



    struct DEX_TABLE order_t {
        uint64_t        order_id;           // auto-increment
        uint64_t        external_id;        // external id
        name            owner;
        uint64_t        sympair_id;         // id of symbol_pair_table
        order_side_t    order_side;
        asset           price;
        asset           limit_quant;
        asset           frozen_quant;
        int64_t         taker_fee_ratio;
        int64_t         maker_fee_ratio;
        asset           matched_assets;     //!< total matched asset quantity
        asset           matched_coins;      //!< total matched coin quantity
        asset           matched_fee;        //!< total matched fees
        order_status_t  status;
        time_point      created_at;
        time_point      last_updated_at;
        uint64_t        last_deal_id;

        uint64_t primary_key() const { return order_id; }
        uint64_t by_owner()const { return owner.value; }
        uint64_t by_external_id()const { return external_id; }
        uint64_t get_price()const { return  price.amount; }


        order_match_idx_key get_order_match_idx()const { 
            return make_order_match_idx( order_side, price.amount); 
        }

        void print() const {
            auto created_at = this->created_at.elapsed.count(); // print the ms value
            auto last_updated_at = this->last_updated_at.elapsed.count(); // print the ms value
            PRINT_PROPERTIES(
                PP(order_id),
                PP(external_id),
                PP(owner),
                PP0(sympair_id),
                PP(order_side),
                PP(price),
                PP(limit_quant),
                PP(frozen_quant),
                PP(taker_fee_ratio),
                PP(maker_fee_ratio),
                PP(matched_assets),
                PP(matched_coins),
                PP(matched_fee),
                PP(status),
                PP(created_at),
                PP(last_updated_at),
                PP(last_deal_id)
            );
        }
    };

    using order_match_idx = indexed_by<"ordermatch"_n, const_mem_fun<order_t, uint64_t, &order_t::get_price> >;
    using order_owner_idx = indexed_by<"orderowner"_n, const_mem_fun<order_t, uint64_t, &order_t::by_owner> >;

    typedef eosio::multi_index<"order"_n, order_t, order_match_idx> order_tbl;
    typedef eosio::multi_index<"queue"_n, order_t, order_owner_idx> queue_tbl;

    inline static order_tbl make_order_table(const name &self, const uint64_t& pair_id, const order_side_t& side ) { \
                        return order_tbl(self, pair_id << 8 | uint64_t(order_side::index(side))); \
                    }
    
    inline static queue_tbl make_queue_table(const name &self) { return queue_tbl(self, self.value/*scope*/); }
 

    struct deal_item_t {
        uint64_t    id;
        uint64_t    sympair_id;
        uint64_t    buy_order_id;
        uint64_t    sell_order_id;
        asset       deal_assets;
        asset       deal_coins;
        asset       deal_price;
        order_side_t taker_side;
        asset       buy_fee;
        asset       sell_fee;
        asset       buy_refund_coins;
        string      memo;
        time_point  deal_time;

        void print() const {
            auto deal_time = this->deal_time.elapsed.count(); // print the ms value
            PRINT_PROPERTIES(
                PP0(id),
                PP(sympair_id),
                PP(buy_order_id),
                PP(sell_order_id),
                PP(deal_assets),
                PP(deal_coins),
                PP(deal_price),
                PP(taker_side),
                PP(buy_fee),
                PP(sell_fee),
                PP(buy_refund_coins),
                PP(memo),
                PP(deal_time)
            );
        }
    };

    struct DEX_TABLE rewards_t
    {
        name owner;
        map<extended_symbol, uint64_t> rewards;

        uint64_t primary_key() const { return owner.value; }

        rewards_t() {}
        rewards_t(const uint64_t &powner) : owner(powner) {}

        EOSLIB_SERIALIZE(rewards_t, (owner)(rewards))
    };

    typedef eosio::multi_index<"rewards"_n, rewards_t> rewards_tbl;
        
    typedef eosio::multi_index<"sympair"_n, symbol_pair_t, symbols_idx> symbol_pair_table;
    inline static rewards_tbl make_reward_table(const name &self) { return rewards_tbl(self, self.value/*scope*/); }

}// namespace dex
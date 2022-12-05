#pragma once

#include "dex_states.hpp"
#include <utils.hpp>

namespace dex {

    inline int64_t power(int64_t base, int64_t exp) {
        int64_t ret = 1;
        while( exp > 0  ) {
            ret *= base; --exp;
        }
        return ret;
    }

    inline int64_t power10(int64_t exp) {
        return power(10, exp);
    }

    inline int64_t calc_precision(int64_t digit) {
        CHECK(digit >= 0 && digit <= 18, "precision digit " + std::to_string(digit) + " should be in range[0,18]");
        return power10(digit);
    }

    int64_t calc_asset_amount(const asset &coin_quant, const asset &price, const symbol &asset_symbol) {
        ASSERT(coin_quant.symbol.precision() == price.symbol.precision());
        int128_t precision = calc_precision(asset_symbol.precision());
        return divide_decimal64(coin_quant.amount, price.amount, precision);
    }

    int64_t calc_coin_amount(const asset &asset_quant, const asset &price, const symbol &coin_symbol) {
        ASSERT(coin_symbol.precision() == price.symbol.precision());
        int128_t precision = calc_precision(asset_quant.symbol.precision());
        return multiply_decimal64(asset_quant.amount, price.amount, precision);
    }

    asset calc_asset_quant(const asset &coin_quant, const asset &price, const symbol &asset_symbol) {
        return asset(calc_asset_amount(coin_quant, price, asset_symbol), asset_symbol);
    }

    asset calc_coin_quant(const asset &asset_quant, const asset &price, const symbol &coin_symbol) {
        return asset(calc_coin_amount(asset_quant, price, coin_symbol), coin_symbol);
    }

    inline asset calc_match_fee(int64_t ratio, const asset &quant) {
        if (quant.amount == 0) return asset{0, quant.symbol};
        int64_t fee = multiply_decimal64(quant.amount, ratio, RATIO_PRECISION);
        CHECK(fee < quant.amount, "the calc_fee is large than quantity=" + quant.to_string() + ", ratio=" + to_string(ratio));
        return asset{fee, quant.symbol};
    }

    inline asset calc_match_fee(const dex::order_t &order, const order_side_t &taker_side, const asset &quant) {
        int64_t ratio = (order.order_side == taker_side) ? order.taker_fee_ratio : order.maker_fee_ratio;
        return calc_match_fee(ratio, quant);
    }

    template<typename table_t, typename price_index_t>
    class matching_order_iterator {
    public:
        matching_order_iterator(const table_t& table, const price_index_t price_idx, uint64_t sympair_id, order_side_t side)
            : _sym_pair_id(sympair_id), _order_side(side), _table(table), _price_idx(price_idx)
        {
            _it             = _price_idx.begin();
            TRACE("creating matching order itr! sympair_id=", _sym_pair_id, ", side=", _order_side, "\n");
            process_data();
        };

        void complete_and_next() {
            // ASSERT(is_completed()); TODO
            const auto &store_order = *_it;
            _it++;
            _order_tbl.erase(store_order);
            // table.modify(store_order, same_payer, [&]( auto& a ) {
            //     a.matched_assets = _matched_assets;
            //     a.matched_coins = _matched_coins;
            //     a.matched_fee = _matched_fee;
            //     a.last_updated_at = current_block_time();
            //     a.last_deal_id = _last_deal_id;
            // });
            process_data();
        }

        void save_matching_order() {        //TODO check matching status
            _order_tbl.modify(*_it, same_payer, [&]( auto& a ) {
                a.matched_assets = _matched_assets;
                a.matched_coins = _matched_coins;
                a.matched_fee = _matched_fee;
                a.last_updated_at = current_block_time();
                a.last_deal_id = _last_deal_id;
            });
        }

        inline const order_t &stored_order() {
            return *_it;
        }

        inline void match(uint64_t deal_id,
                    const asset &new_matched_assets,
                    const asset &new_matched_coins,
                    const asset &new_matched_fee) {
    
            bool completed = false; 

            _last_deal_id   = deal_id;
            _matched_assets += new_matched_assets;
            _matched_coins  += new_matched_coins;
            _matched_fee    += new_matched_fee;
            const auto &order = *_it;

            CHECK(_matched_assets <= order.limit_quant,
                "The matched assets=" + _matched_assets.to_string() +
                " is overflow with limit_quant=" + order.limit_quant.to_string());
            completed = _matched_assets == order.limit_quant;
            
            if (order.order_side == order_side::BUY) {
                auto total_matched_coins = (_matched_coins.symbol == _matched_fee.symbol) ?
                    _matched_coins + _matched_fee // the buyer pay fee with coins
                    : _matched_coins;
                CHECK(total_matched_coins <= order.frozen_quant,
                        "The total_matched_coins=" + _matched_coins.to_string() +
                        " is overflow with frozen_quant=" + order.frozen_quant.to_string() + " for buy order");
                if (completed) {
                    if (order.frozen_quant > total_matched_coins) {
                        _refund_coins = order.frozen_quant - total_matched_coins;
                    }
                }
            }
        }
        

        inline asset get_free_limit_quant() const {
            ASSERT(_it != _price_index().end());
            asset ret = _it->limit_quant - _matched_assets;
            ASSERT(ret.amount >= 0);
            return ret;
        }

        inline asset get_refund_coins() const {
            ASSERT(_it != _price_index().end());
            return _refund_coins;
        }

        const order_side_t &order_side() const {
            return _order_side; 
        }

    private:
        void process_data() {
            if (_it == _price_index().end()) {
                TRACE("matching order itr end! sympair_id=", _sym_pair_id, ", side=", _order_side, "\n");
                return;
            }

            const auto &stored_order = *_it;
            TRACE("found order! order=", stored_order, "\n");

            _last_deal_id   = stored_order.last_deal_id;
            _matched_assets = stored_order.matched_assets;
            _matched_coins  = stored_order.matched_coins;
            _matched_fee    = stored_order.matched_fee;
            _refund_coins   = asset(0, _matched_coins.symbol);
        }

        table_t&                _order_tbl;
        price_index_t&          _price_idx;
        typename price_index_t::const_iterator _it;
        uint64_t                _sym_pair_id;
        order_side_t            _order_side;

        uint64_t                _last_deal_id = 0;
        asset                   _matched_assets;      //!< total matched asset amount
        asset                   _matched_coins;       //!< total matched coin amount
        asset                   _matched_fee;        //!< total matched fees
        asset                   _refund_coins;
    };

    template<typename table_t, typename price_index_t>
    class matching_pair_iterator {
    public:
        using order_iterator = matching_order_iterator<order_tbl,price_idx>;

        matching_pair_iterator(const name contract, const dex::symbol_pair_t &sym_pair )
            :  _sym_pair(sym_pair) {
            _order_buy_tbl  = make_order_table( contract, sym_pair.sympair_id, side );
            _order_sell_tbl = make_order_table( contract, sym_pair.sympair_id, side )

            _buy_price_idx  = _order_buy_tbl.get_index<"orderprice"_n>();
            _sell_price_idx = _order_sell_tbl.get_index<"orderprice"_n>();

            limit_buy_it(_order_buy_tbl, _order_buy_tbl, sym_pair.sympair_id, order_side::BUY),
            limit_sell_it(_order_sell_tbl, _order_sell_tbl, sym_pair.sympair_id, order_side::SELL),
            process_data();
        }

        void complete_and_next() {
            if (_taker_it->is_completed()) {
                _taker_it->complete_and_next();
            }
            if (_maker_it->is_completed()) {
                _maker_it->complete_and_next();
            }
            process_data();
        }

        void save_matching_order() {
            limit_buy_it.save_matching_order( );
            limit_sell_it.save_matching_order( );
        }

        bool can_match() const  {
            return _can_match;
        }

        order_iterator& maker_it() {
            ASSERT(_can_match);
            return *_maker_it;
        }
        order_iterator& taker_it() {
            ASSERT(_can_match);
            return *_taker_it;
        }

        void calc_matched_amounts(asset &matched_assets, asset &matched_coins) {
            const auto &asset_symbol = _sym_pair.asset_symbol.get_symbol();
            const auto &coin_symbol = _sym_pair.coin_symbol.get_symbol();
            ASSERT( _maker_it->stored_order().price.amount > 0);

            const auto &matched_price = _maker_it->stored_order().price;

            auto maker_free_assets = _maker_it->get_free_limit_quant();
            ASSERT(maker_free_assets.symbol == asset_symbol);
            CHECK(maker_free_assets.amount > 0, "MUST: maker_free_assets > 0");

            asset taker_free_assets;
    
            taker_free_assets = _taker_it->get_free_limit_quant();
            ASSERT(taker_free_assets.symbol == asset_symbol);
            CHECK(taker_free_assets.amount > 0, "MUST: taker_free_assets > 0");
        
            matched_assets = (taker_free_assets < maker_free_assets) ? taker_free_assets : maker_free_assets;
            matched_coins = calc_coin_quant(matched_assets, matched_price, coin_symbol);
        }

    private:
        const dex::symbol_pair_t &_sym_pair;

        order_iterator limit_buy_it;
        order_iterator limit_sell_it;
        order_iterator *_taker_it = nullptr;
        order_iterator *_maker_it = nullptr;
        table_t&        _order_buy_tbl;
        table_t&        _order_sell_tbl;
        match_index_t&  _buy_price_idx;
        match_index_t&  _sell_price_idx;

        bool _can_match = false;

        void process_data() {
            _taker_it = nullptr;
            _maker_it = nullptr;
            _can_match = false;
    
            if (!_can_match) {
                _can_match = true;
                if ( limit_buy_it.is_valid() && limit_sell_it.is_valid() &&
                     limit_buy_it.stored_order().price >= limit_sell_it.stored_order().price ) {
                    if ( limit_buy_it.stored_order().order_id > limit_sell_it.stored_order().order_id ) {
                        _taker_it = &limit_buy_it;
                        _maker_it = &limit_sell_it;
                    } else {
                        _taker_it = &limit_sell_it;
                        _maker_it = &limit_buy_it;
                    }
                } else {
                    _can_match = false;
                }
            }

            if (!_can_match) {
                _taker_it = nullptr;
            }
        }
    };

}// namespace dex